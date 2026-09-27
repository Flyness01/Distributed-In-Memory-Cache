# Partitioned In-Memory Cache

A compact C++ systems project for studying how partitioning changes lock contention and throughput in a concurrent in-memory key/value store.

This public implementation uses POSIX read–write locks and hash-based partition placement. Its purpose is to keep the synchronization boundary inspectable before adding distribution, eviction, persistence, or replication.

## Implemented

- String key/value `put`, `get`, and `erase`
- Configurable hash partitions
- One POSIX read–write lock per partition
- Concurrent correctness test with eight threads
- Configurable 80% read / 20% write benchmark
- CMake and CTest integration

The implementation is thread-safe within one process. It is **not yet a multi-node distributed database**. Local partitioning is the foundation for measuring contention and reasoning about later sharding work.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Benchmark

```bash
./build/cache_benchmark [threads] [operations_per_thread] [partitions] [key_space]
./build/cache_benchmark 8 100000 16 10000
```

The output reports total operations, wall-clock time, aggregate throughput, cache hits, and final entry count. Results are environment-specific and should be collected over repeated trials.

## Questions this makes testable

1. How does throughput change as thread count exceeds available cores?
2. When do additional partitions stop reducing contention?
3. How does key skew change the benefit of partitioning?
4. When do read–write locks help compared with mutexes?
5. Which costs appear when local partitions become remote shards?

## Current limitations

- No eviction or capacity bound
- No persistence, replication, or network protocol
- `size()` is safe but not a single atomic snapshot across partitions
- The benchmark uses a synthetic uniform key pattern
- No latency-percentile reporting yet

## Next experiments

- Sweep partition counts under fixed workloads
- Add Zipfian/skewed key access
- Compare mutex and read–write-lock variants
- Record latency percentiles
- Add bounded capacity and compare eviction policies

## Author

Flyness Namatama
