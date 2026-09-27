#include "partitioned_cache.hpp"
#include <cassert>
#include <iostream>
#include <pthread.h>
#include <string>
#include <vector>

struct WorkerArgs { PartitionedCache* cache; int id; int operations; };
void* write_and_read(void* raw) {
  auto* args = static_cast<WorkerArgs*>(raw);
  for (int i = 0; i < args->operations; ++i) {
    const auto key = "worker-" + std::to_string(args->id) + "-" + std::to_string(i);
    const auto value = "value-" + std::to_string(i);
    args->cache->put(key, value);
    const auto found = args->cache->get(key);
    assert(found && *found == value);
  }
  return nullptr;
}
int main() {
  PartitionedCache cache(16);
  cache.put("hello", "world");
  assert(cache.get("hello") == std::optional<std::string>("world"));
  assert(!cache.get("missing"));
  assert(cache.erase("hello"));
  assert(!cache.erase("hello"));
  constexpr int threads_count = 8, operations = 2000;
  std::vector<pthread_t> threads(threads_count);
  std::vector<WorkerArgs> args(threads_count);
  for (int i = 0; i < threads_count; ++i) { args[i] = {&cache, i, operations}; assert(pthread_create(&threads[i], nullptr, write_and_read, &args[i]) == 0); }
  for (auto& thread : threads) assert(pthread_join(thread, nullptr) == 0);
  assert(cache.size() == static_cast<std::size_t>(threads_count * operations));
  std::cout << "All cache tests passed. Entries: " << cache.size() << '\n';
}
