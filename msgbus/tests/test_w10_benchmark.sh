#!/usr/bin/env bash
# Week 10 Test: Latency & Throughput Benchmark (SHM vs UNIX Domain Sockets)
set -e

echo "=========================================================="
echo " [Test W10] Running Latency & Throughput Benchmark        "
echo "=========================================================="

./bin/msgbus-bench 15000

echo "=========================================================="
echo " [Test W10] PASSED: Benchmark executed successfully!      "
echo "=========================================================="
