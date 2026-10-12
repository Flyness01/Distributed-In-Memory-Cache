# Distributed In-Memory Data Processing & Caching Engine

A small distributed-systems prototype for studying sharding, concurrent access, synchronization, lookup latency, throughput, and memory behavior.

The project began as an exploration of partition-aware in-memory processing. The current public implementation extends that idea into a working two-layer system:

1. **C++ cache nodes** store key/value data in configurable hash partitions protected by POSIX read–write locks.
2. A **Python client** applies stable client-side hashing to distribute keys across independent node processes that may run on different hosts.

This is a distributed sharded cache, not a production database. It does not yet provide replication, automatic membership, rebalancing, persistence, or failure recovery.

## Implemented

- C++17 `put`, `get`, and `erase` operations
- Configurable partitions with one POSIX read–write lock per partition
- Concurrent TCP server using one POSIX thread per client connection
- Persistent client connections and a small request protocol
- Python partition-aware distributed client
- Deterministic SHA-256 key routing across independent nodes
- Local and cross-node correctness tests
- Mixed 80% read / 20% write benchmark
- Mean, p50, and p95 end-to-end latency
- Aggregate throughput and per-node entry/RSS statistics
- Repeated, alternating-order comparison of one versus sixteen lock domains

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

On systems without CMake:

```bash
mkdir -p build
clang++ -std=c++17 -O2 -pthread -Iinclude \
  src/partitioned_cache.cpp src/cache_server.cpp -o build/cache_server
```

## Start two nodes

```bash
./build/cache_server 7000 16
./build/cache_server 7001 16
```

The server binds to all interfaces. Use appropriate firewall controls outside local development; this prototype has no authentication or encryption.

## Test and benchmark

```bash
PYTHONPATH=python CACHE_SERVER=build/cache_server \
  python3 -m unittest tests/distributed_test.py -v

PYTHONPATH=python python3 benchmarks/distributed_benchmark.py \
  --nodes 127.0.0.1:7000,127.0.0.1:7001 \
  --threads 8 --operations 2000 --key-space 2000

PYTHONPATH=python python3 benchmarks/compare_lock_scope.py \
  --repeats 5 --operations 2000
```

## Initial comparison

Five alternating-order trials compared two nodes with one lock domain per node against two nodes with sixteen partitions per node. Each trial used eight client threads, 16,000 total operations, a 2,000-key space, and an 80% read / 20% write workload.

| Configuration | Throughput, mean | Mean latency | p50 latency | p95 latency |
|---|---:|---:|---:|---:|
| One partition | 29,500 ops/s | 0.267 ms | 0.238 ms | 0.546 ms |
| Sixteen partitions | 29,781 ops/s | 0.265 ms | 0.236 ms | 0.541 ms |

The difference was small relative to trial variation. On this localhost workload, additional partitions did **not** produce a material performance improvement. Network/protocol costs and the short critical sections may dominate lock contention at this scale.

Peak RSS remained around 1.66–1.70 MB per one-partition node and 1.70–1.75 MB per sixteen-partition node. These values are descriptive, not a general memory comparison. Full trials are in `results/lock_scope_comparison.json`.

## Current limitations

- No replication, failover, persistence, eviction, authentication, or encryption
- No automatic discovery, membership, or key rebalancing
- Modulo hashing remaps many keys when node count changes
- Thread-per-connection server rather than an event-driven design
- Synthetic uniform workload on localhost
- `size()` is safe but not one atomic snapshot across a node's partitions

## Next experiments

- Increase concurrency and critical-section work to create measurable contention
- Add Zipfian/skewed keys and identify hot partitions
- Compare global mutexes, partitioned mutexes, and read–write locks
- Replace modulo routing with consistent hashing
- Measure cross-host behavior and network failure
- Add replication and define consistency semantics

## Project history

The research direction was explored in Spring 2026. The public distributed implementation, validation, and comparative benchmark were completed in Fall 2026.

## Author

Flyness Namatama
