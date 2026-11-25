#!/usr/bin/env bash
# Week 8 Test: Crash Recovery, Memory Discipline & Orphaned Resource Cleanup
set -e

TOPIC="w8_test_cleanup"
echo "=========================================================="
echo " [Test W8] Starting Crash Recovery & Cleaner Verification"
echo "=========================================================="

# 1. Clean previous state
./bin/msgbus-clean --all >/dev/null 2>&1 || true

# 2. Simulate orphaned resources by creating topic without clean broker teardown
echo "1. Creating topic and simulating abnormal process termination..."
./bin/msgbus-pub $TOPIC "Pre-crash message" >/dev/null 2>&1 || true

# Verify resources exist in /dev/shm
BEFORE_COUNT=$(ls /dev/shm/msgbus_* /dev/shm/sem.msgbus_* 2>/dev/null | wc -l || true)
echo "   Active resources in /dev/shm: $BEFORE_COUNT"

if [ "$BEFORE_COUNT" -eq 0 ]; then
    echo " [FAIL] Expected shared memory resources to be present before cleanup!"
    exit 1
fi

# 3. Run the W8 msgbus-clean tool
echo "2. Executing bin/msgbus-clean tool to reclaim all orphaned resources..."
./bin/msgbus-clean $TOPIC

# 4. Verify no leaked shm or semaphore files remain
AFTER_COUNT=$(ls /dev/shm/msgbus_${TOPIC}* /dev/shm/sem.msgbus_${TOPIC}* 2>/dev/null | wc -l || true)
echo "   Remaining resources for topic '$TOPIC': $AFTER_COUNT"

if [ "$AFTER_COUNT" -ne 0 ]; then
    echo " [FAIL] Orphaned shared memory segments or semaphores still found!"
    exit 1
fi

echo "=========================================================="
echo " [Test W8] PASSED: Crash recovery and cleanup verified!   "
echo "=========================================================="
