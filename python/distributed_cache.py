import hashlib
import socket


class CacheNode:
    def __init__(self, host, port, timeout=3.0):
        self.host, self.port, self.timeout = host, port, timeout
        self._socket = None
        self._file = None

    def _connect(self):
        if self._socket is None:
            self._socket = socket.create_connection((self.host, self.port), self.timeout)
            self._file = self._socket.makefile("rwb")

    def request(self, command):
        self._connect()
        self._file.write(command.encode("utf-8") + b"\n")
        self._file.flush()
        response = self._file.readline()
        if not response:
            raise ConnectionError(f"node {self.host}:{self.port} closed the connection")
        return response.decode("utf-8").rstrip("\r\n")

    def close(self):
        if self._file:
            self._file.close()
        if self._socket:
            self._socket.close()
        self._file = self._socket = None


class DistributedCache:
    """Client-side hash partitioning across independent cache nodes."""

    def __init__(self, endpoints):
        if not endpoints:
            raise ValueError("at least one cache endpoint is required")
        self.nodes = [CacheNode(host, port) for host, port in endpoints]

    def node_index(self, key):
        digest = hashlib.sha256(key.encode("utf-8")).digest()
        return int.from_bytes(digest[:8], "big") % len(self.nodes)

    def put(self, key, value):
        self._validate(key, value)
        response = self.nodes[self.node_index(key)].request(f"PUT\t{key}\t{value}")
        if response != "OK":
            raise RuntimeError(response)

    def get(self, key):
        self._validate(key)
        response = self.nodes[self.node_index(key)].request(f"GET\t{key}")
        if response == "NOT_FOUND":
            return None
        if not response.startswith("VALUE\t"):
            raise RuntimeError(response)
        return response.split("\t", 1)[1]

    def delete(self, key):
        self._validate(key)
        return self.nodes[self.node_index(key)].request(f"DELETE\t{key}") == "DELETED"

    def stats(self):
        return [node.request("STATS") for node in self.nodes]

    def close(self):
        for node in self.nodes:
            node.close()

    @staticmethod
    def _validate(*parts):
        if any("\t" in part or "\n" in part or "\r" in part for part in parts):
            raise ValueError("keys and values cannot contain tabs or newlines")
