import argparse
import json
import os
import statistics
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "python"))
sys.path.insert(0, os.path.join(ROOT, "benchmarks"))

from distributed_benchmark import run
from distributed_cache import DistributedCache


def start_cluster(binary, base_port, partitions):
    processes = [subprocess.Popen([binary, str(base_port + index), str(partitions)], stdout=subprocess.DEVNULL) for index in range(2)]
    endpoints = [("127.0.0.1", base_port + index) for index in range(2)]
    deadline = time.time() + 5
    while time.time() < deadline:
        try:
            probe = DistributedCache(endpoints); probe.stats(); probe.close()
            return processes, endpoints
        except OSError:
            time.sleep(0.05)
    raise RuntimeError("cluster did not start")


def stop_cluster(processes):
    for process in processes:
        process.terminate()
    for process in processes:
        process.wait(timeout=3)


def summarize(trials):
    metrics = ("throughput_ops_s", "latency_ms_mean", "latency_ms_p50", "latency_ms_p95")
    return {metric: {"mean": statistics.mean(trial[metric] for trial in trials), "stdev": statistics.stdev(trial[metric] for trial in trials)} for metric in metrics}


def main():
    parser = argparse.ArgumentParser(description="Compare one lock domain with partitioned locking across two cache nodes.")
    parser.add_argument("--server", default=os.path.join(ROOT, "build", "cache_server"))
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--operations", type=int, default=2000)
    parser.add_argument("--output", default=os.path.join(ROOT, "results", "lock_scope_comparison.json"))
    args = parser.parse_args()
    configurations = [("one_partition", 1), ("sixteen_partitions", 16)]
    trials = {name: [] for name, _ in configurations}
    for repeat in range(args.repeats):
        order = configurations if repeat % 2 == 0 else list(reversed(configurations))
        for offset, (name, partitions) in enumerate(order):
            processes, endpoints = start_cluster(args.server, 17800 + repeat * 10 + offset * 2, partitions)
            try:
                result = run(endpoints, threads=8, operations=args.operations, key_space=2000, write_ratio=0.2, seed=100 + repeat)
                trials[name].append(result)
            finally:
                stop_cluster(processes)
    output = {"method": {"nodes": 2, "threads": 8, "operations_per_thread": args.operations, "write_ratio": 0.2, "repeats": args.repeats, "alternated_order": True}, "trials": trials, "summary": {name: summarize(values) for name, values in trials.items()}}
    with open(args.output, "w", encoding="utf-8") as handle:
        json.dump(output, handle, indent=2); handle.write("\n")
    print(json.dumps(output["summary"], indent=2))


if __name__ == "__main__":
    main()
