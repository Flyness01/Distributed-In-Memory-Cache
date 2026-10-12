import os
import subprocess
import sys
import time
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))
from distributed_cache import DistributedCache


class DistributedCacheTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        binary = os.environ.get("CACHE_SERVER", "build/cache_server")
        cls.processes = [subprocess.Popen([binary, str(port), "8"], stdout=subprocess.DEVNULL) for port in (17400, 17401)]
        deadline = time.time() + 5
        while time.time() < deadline:
            try:
                probe = DistributedCache([("127.0.0.1", 17400), ("127.0.0.1", 17401)])
                probe.stats(); probe.close(); break
            except OSError:
                time.sleep(0.05)
        else:
            raise RuntimeError("cache servers did not start")

    @classmethod
    def tearDownClass(cls):
        for process in cls.processes:
            process.terminate()
        for process in cls.processes:
            process.wait(timeout=3)

    def test_keys_are_routed_and_retrieved(self):
        cache = DistributedCache([("127.0.0.1", 17400), ("127.0.0.1", 17401)])
        routed = set()
        for index in range(100):
            key = f"item-{index}"
            routed.add(cache.node_index(key))
            cache.put(key, f"value-{index}")
            self.assertEqual(cache.get(key), f"value-{index}")
        self.assertEqual(routed, {0, 1})
        cache.close()


if __name__ == "__main__":
    unittest.main()
