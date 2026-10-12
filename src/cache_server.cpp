#include "partitioned_cache.hpp"

#include <arpa/inet.h>
#include <csignal>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <pthread.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/resource.h>
#include <unistd.h>

namespace {
struct ClientContext { int socket; PartitionedCache* cache; };

bool send_all(int socket, const std::string& message) {
  std::size_t sent = 0;
  while (sent < message.size()) {
    const auto count = send(socket, message.data() + sent, message.size() - sent, 0);
    if (count <= 0) return false;
    sent += static_cast<std::size_t>(count);
  }
  return true;
}

std::string handle_command(const std::string& line, PartitionedCache& cache) {
  const auto first = line.find('\t');
  const std::string operation = line.substr(0, first);
  if (operation == "PUT" && first != std::string::npos) {
    const auto second = line.find('\t', first + 1);
    if (second == std::string::npos) return "ERROR\tPUT requires key and value\n";
    cache.put(line.substr(first + 1, second - first - 1), line.substr(second + 1));
    return "OK\n";
  }
  if (operation == "GET" && first != std::string::npos) {
    const auto value = cache.get(line.substr(first + 1));
    return value ? "VALUE\t" + *value + "\n" : "NOT_FOUND\n";
  }
  if (operation == "DELETE" && first != std::string::npos) {
    return cache.erase(line.substr(first + 1)) ? "DELETED\n" : "NOT_FOUND\n";
  }
  if (operation == "STATS") {
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
    const double peak_rss_mb = usage.ru_maxrss / (1024.0 * 1024.0);
#else
    const double peak_rss_mb = usage.ru_maxrss / 1024.0;
#endif
    std::ostringstream stats;
    stats << "STATS\tentries=" << cache.size() << "\tpartitions=" << cache.partition_count()
          << "\tpeak_rss_mb=" << peak_rss_mb << '\n';
    return stats.str();
  }
  if (operation == "PING") return "PONG\n";
  return "ERROR\tunknown command\n";
}

void* serve_client(void* raw) {
  auto* context = static_cast<ClientContext*>(raw);
  const int client = context->socket;
  PartitionedCache* cache = context->cache;
  delete context;
  std::string pending;
  char buffer[4096];
  while (true) {
    const auto received = recv(client, buffer, sizeof(buffer), 0);
    if (received <= 0) break;
    pending.append(buffer, static_cast<std::size_t>(received));
    std::size_t newline;
    while ((newline = pending.find('\n')) != std::string::npos) {
      std::string line = pending.substr(0, newline);
      pending.erase(0, newline + 1);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (!send_all(client, handle_command(line, *cache))) break;
    }
  }
  close(client);
  return nullptr;
}
}  // namespace

int main(int argc, char** argv) {
  const int port = argc > 1 ? std::stoi(argv[1]) : 7000;
  const std::size_t partitions = argc > 2 ? std::stoul(argv[2]) : 16;
  PartitionedCache cache(partitions);
  std::signal(SIGPIPE, SIG_IGN);

  const int server = socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) { std::cerr << "socket failed\n"; return 1; }
  int reuse = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(static_cast<uint16_t>(port));
  if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(server, 128) != 0) {
    std::cerr << "failed to bind/listen on port " << port << ": " << std::strerror(errno) << '\n';
    return 1;
  }
  std::cout << "cache_server port=" << port << " partitions=" << partitions << std::endl;
  while (true) {
    const int client = accept(server, nullptr, nullptr);
    if (client < 0) continue;
    pthread_t thread;
    auto* context = new ClientContext{client, &cache};
    if (pthread_create(&thread, nullptr, serve_client, context) == 0) pthread_detach(thread);
    else { close(client); delete context; }
  }
}
