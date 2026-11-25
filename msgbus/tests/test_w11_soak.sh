#!/usr/bin/env bash
# Week 11 Test: Multi-Process Soak, Concurrency, Fan-Out & Durability Replay
set -e

TOPIC="w11_soak_topic"
echo "=========================================================="
echo " [Test W11] Starting Multi-Process Soak & Concurrency Test "
echo "=========================================================="

# 1. Clean previous state
./bin/msgbus-clean $TOPIC >/dev/null 2>&1 || true
rm -f soak_sub*.log soak_replay.log

# 2. Start Broker with Durability
echo "1. Starting Broker with --durable..."
./bin/msgbus-broker $TOPIC --durable >/dev/null 2>&1 &
BROKER_PID=$!
sleep 0.5

# 3. Start 3 Concurrent Subscribers expecting 30 messages each
echo "2. Spawning 3 Concurrent Subscribers (Fan-Out) expecting 30 messages each..."
./bin/msgbus-sub $TOPIC --policy block -n 30 > soak_sub1.log 2>&1 &
SUB1_PID=$!

./bin/msgbus-sub $TOPIC --policy block -n 30 > soak_sub2.log 2>&1 &
SUB2_PID=$!

./bin/msgbus-sub $TOPIC --policy block -n 30 > soak_sub3.log 2>&1 &
SUB3_PID=$!

sleep 0.5

# 4. Concurrently launch 3 Publishers (10 messages each = 30 total)
echo "3. Spawning 3 Concurrent Publishers (10 messages each = 30 total)..."
./bin/msgbus-pub $TOPIC "Stream-A" -n 10 -d 20 &
PUB1_PID=$!

./bin/msgbus-pub $TOPIC "Stream-B" -n 10 -d 20 &
PUB2_PID=$!

./bin/msgbus-pub $TOPIC "Stream-C" -n 10 -d 20 &
PUB3_PID=$!

# Wait for publishers
wait $PUB1_PID
wait $PUB2_PID
wait $PUB3_PID
echo "   All 3 publishers finished transmitting messages."

# Wait for subscribers
wait $SUB1_PID
wait $SUB2_PID
wait $SUB3_PID
echo "   All 3 subscribers finished consuming messages."

# 5. Verify Delivery Counts across all subscribers
COUNT1=$(grep -c "Received \[Seq" soak_sub1.log || true)
COUNT2=$(grep -c "Received \[Seq" soak_sub2.log || true)
COUNT3=$(grep -c "Received \[Seq" soak_sub3.log || true)

echo "4. Verifying message delivery counts:"
echo "   Subscriber 1: $COUNT1 / 30"
echo "   Subscriber 2: $COUNT2 / 30"
echo "   Subscriber 3: $COUNT3 / 30"

if [ "$COUNT1" -ne 30 ] || [ "$COUNT2" -ne 30 ] || [ "$COUNT3" -ne 30 ]; then
    echo " [FAIL] Message count mismatch in soak test!"
    kill -9 $BROKER_PID 2>/dev/null || true
    exit 1
fi
echo "   Fan-Out Verified: All 3 subscribers received every message with zero loss!"

# 6. Test Late-Subscriber Replay on durable topic after soak
echo "5. Spawning LATE Subscriber with --replay to verify durability..."
./bin/msgbus-sub $TOPIC --replay -n 30 > soak_replay.log 2>&1 &
REPLAY_PID=$!
wait $REPLAY_PID
REPLAY_COUNT=$(grep -c "Received (REPLAY)" soak_replay.log || true)
echo "   Late Subscriber Replayed: $REPLAY_COUNT / 30 messages from WAL!"

if [ "$REPLAY_COUNT" -ne 30 ]; then
    echo " [FAIL] Replay failed to deliver all 30 messages!"
    kill -9 $BROKER_PID 2>/dev/null || true
    exit 1
fi

# 7. Clean teardown
echo "6. Testing graceful teardown..."
kill -SIGINT $BROKER_PID
wait $BROKER_PID 2>/dev/null || true
rm -f soak_sub*.log soak_replay.log

echo "=========================================================="
echo " [Test W11] PASSED: Multi-Process Soak Test Succeeded!    "
echo "=========================================================="
