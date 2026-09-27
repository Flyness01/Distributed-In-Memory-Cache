#include "partitioned_cache.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <string>
#include <vector>

struct Args { PartitionedCache* cache; int id; int operations; int keys; std::atomic<long long>* hits; };
void* run_workload(void* raw) {
  auto* args = static_cast<Args*>(raw);
  for (int i = 0; i < args->operations; ++i) {
    const auto key = "key-" + std::to_string((i * 31 + args->id * 17) % args->keys);
    if (i % 5 == 0) args->cache->put(key, "value-" + std::to_string(i));
    else if (args->cache->get(key)) ++(*args->hits);
  }
  return nullptr;
}
int main(int argc, char** argv) {
  const int threads_count = argc > 1 ? std::atoi(argv[1]) : 8;
  const int operations = argc > 2 ? std::atoi(argv[2]) : 100000;
  const int partitions = argc > 3 ? std::atoi(argv[3]) : 16;
  const int keys = argc > 4 ? std::atoi(argv[4]) : 10000;
  if (threads_count <= 0 || operations <= 0 || partitions <= 0 || keys <= 0) return 2;
  PartitionedCache cache(partitions);
  std::atomic<long long> hits{0};
  std::vector<pthread_t> threads(threads_count);
  std::vector<Args> args(threads_count);
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < threads_count; ++i) { args[i] = {&cache, i, operations, keys, &hits}; pthread_create(&threads[i], nullptr, run_workload, &args[i]); }
  for (auto& thread : threads) pthread_join(thread, nullptr);
  const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  const auto total = static_cast<long long>(threads_count) * operations;
  std::cout << "threads=" << threads_count << " partitions=" << partitions << " operations=" << total
            << " seconds=" << seconds << " throughput_ops_s=" << static_cast<long long>(total / seconds)
            << " hits=" << hits << " entries=" << cache.size() << '\n';
}
