import argparse
import concurrent.futures
import json
import math
import random
import time

from distributed_cache import DistributedCache


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, math.ceil(fraction * len(ordered)) - 1)]


def worker(endpoints, worker_id, operations, key_space, write_ratio, seed):
    cache = DistributedCache(endpoints)
    rng = random.Random(seed + worker_id)
    latencies = []
    hits = 0
    for index in range(operations):
        key = f"key-{rng.randrange(key_space)}"
        started = time.perf_counter_ns()
        if rng.random() < write_ratio:
            cache.put(key, f"worker-{worker_id}-value-{index}")
        elif cache.get(key) is not None:
            hits += 1
        latencies.append((time.perf_counter_ns() - started) / 1_000_000)
    cache.close()
    return latencies, hits


def run(endpoints, threads=8, operations=5000, key_space=10000, write_ratio=0.2, seed=11):
    warmup = DistributedCache(endpoints)
    for index in range(key_space):
        warmup.put(f"key-{index}", "warm")
    warmup.close()
    started = time.perf_counter()
    with concurrent.futures.ThreadPoolExecutor(max_workers=threads) as executor:
        results = list(executor.map(lambda worker_id: worker(endpoints, worker_id, operations, key_space, write_ratio, seed), range(threads)))
    seconds = time.perf_counter() - started
    latencies = [latency for result, _ in results for latency in result]
    total = threads * operations
    probe = DistributedCache(endpoints)
    stats = probe.stats()
    probe.close()
    return {
        "nodes": len(endpoints), "threads": threads, "operations": total,
        "write_ratio": write_ratio, "seconds": seconds,
        "throughput_ops_s": total / seconds, "latency_ms_mean": sum(latencies) / len(latencies),
        "latency_ms_p50": percentile(latencies, 0.50), "latency_ms_p95": percentile(latencies, 0.95),
        "hits": sum(hits for _, hits in results), "node_stats": stats,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--nodes", default="127.0.0.1:7000,127.0.0.1:7001")
    parser.add_argument("--threads", type=int, default=8)
    parser.add_argument("--operations", type=int, default=5000, help="operations per worker")
    parser.add_argument("--key-space", type=int, default=10000)
    parser.add_argument("--write-ratio", type=float, default=0.2)
    parser.add_argument("--output")
    args = parser.parse_args()
    endpoints = [(item.rsplit(":", 1)[0], int(item.rsplit(":", 1)[1])) for item in args.nodes.split(",")]
    result = run(endpoints, args.threads, args.operations, args.key_space, args.write_ratio)
    rendered = json.dumps(result, indent=2)
    print(rendered)
    if args.output:
        with open(args.output, "w", encoding="utf-8") as handle:
            handle.write(rendered + "\n")


if __name__ == "__main__":
    main()
