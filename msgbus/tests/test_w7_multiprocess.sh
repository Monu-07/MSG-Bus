#!/usr/bin/env bash
# Week 7 Integration Test: Multi-publisher, Fan-out Subscribers, Semaphore Sleep/Wakeup & Teardown
set -e

TOPIC="integration_test"
echo "=========================================================="
echo " [Test W7] Starting Multi-Process Pub/Sub Integration Test"
echo "=========================================================="

# 1. Clean environment
rm -f /dev/shm/msgbus_* /dev/shm/sem.msgbus_* 2>/dev/null || true
rm -f sub1.log sub2.log

# 2. Start Broker Daemon
echo "1. Starting MsgBus Broker Daemon..."
./bin/msgbus-broker $TOPIC > broker.log 2>&1 &
BROKER_PID=$!
sleep 0.5

# 3. Start 2 Independent Subscribers (Fan-Out test: each should get every message)
echo "2. Spawning Subscriber 1 (PID $$) expecting 10 messages..."
./bin/msgbus-sub $TOPIC --policy block -n 10 > sub1.log 2>&1 &
SUB1_PID=$!

echo "3. Spawning Subscriber 2 (PID $$) expecting 10 messages..."
./bin/msgbus-sub $TOPIC --policy block -n 10 > sub2.log 2>&1 &
SUB2_PID=$!

sleep 0.5

# 4. Concurrently launch 2 Publisher processes posting 5 messages each
echo "4. Spawning 2 Concurrent Publishers (5 messages each = 10 total)..."
./bin/msgbus-pub $TOPIC "Alpha" -n 5 -d 50 &
PUB1_PID=$!

./bin/msgbus-pub $TOPIC "Beta" -n 5 -d 50 &
PUB2_PID=$!

# Wait for publishers
wait $PUB1_PID
wait $PUB2_PID
echo "   Both publishers finished sending messages."

# Wait for subscribers to receive all 10 messages
wait $SUB1_PID
wait $SUB2_PID
echo "   Both subscribers successfully received all expected messages."

# 5. Verify Content of Both Subscribers (Fan-Out Correctness)
SUB1_COUNT=$(grep -c "Received \[Seq" sub1.log || true)
SUB2_COUNT=$(grep -c "Received \[Seq" sub2.log || true)

echo "5. Verifying message delivery:"
echo "   Subscriber 1 received: $SUB1_COUNT / 10"
echo "   Subscriber 2 received: $SUB2_COUNT / 10"

if [ "$SUB1_COUNT" -ne 10 ] || [ "$SUB2_COUNT" -ne 10 ]; then
    echo " [FAIL] Message count mismatch in fan-out delivery!"
    kill -9 $BROKER_PID 2>/dev/null || true
    exit 1
fi

echo "   Fan-Out Verified: Every subscriber received all messages!"

# 6. Test Milestone 6 Signal-Safe Teardown
echo "6. Testing Signal-Safe Teardown (Sending SIGINT to broker)..."
kill -SIGINT $BROKER_PID
wait $BROKER_PID 2>/dev/null || true

# 7. Check for orphaned resources in /dev/shm
LEFTOVERS=$(ls /dev/shm/msgbus_* /dev/shm/sem.msgbus_* 2>/dev/null || true)
if [ -n "$LEFTOVERS" ]; then
    echo " [FAIL] Orphaned shared memory segments or semaphores detected:"
    echo "$LEFTOVERS"
    exit 1
else
    echo "   Clean Teardown Verified: No orphaned shared memory or semaphores in /dev/shm!"
fi

# Cleanup logs
rm -f sub1.log sub2.log broker.log

echo "=========================================================="
echo " [Test W7] ALL ACCEPTANCE TESTS PASSED SUCCESSFULLY! "
echo "=========================================================="
