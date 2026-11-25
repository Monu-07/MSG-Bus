#!/usr/bin/env bash
# Week 9 Test: On-Disk Durability Spill & Late-Subscriber Replay
set -e

TOPIC="w9_durable_topic"
echo "=========================================================="
echo " [Test W9] Starting Message Durability & Replay Test      "
echo "=========================================================="

# 1. Clean previous state
./bin/msgbus-clean $TOPIC >/dev/null 2>&1 || true
rm -f replay_sub.log

# 2. Start Broker with Durability Enabled
echo "1. Starting Broker with --durable option..."
./bin/msgbus-broker $TOPIC --durable >/dev/null 2>&1 &
BROKER_PID=$!
sleep 0.5

# 3. Publish 6 messages to the topic BEFORE any subscriber exists
echo "2. Publishing 6 messages BEFORE any subscriber exists (spilling to WAL)..."
./bin/msgbus-pub $TOPIC "Historical Event" -n 6 -d 30
echo "   Publisher finished and disconnected."

# Check that WAL file exists and is non-empty
WAL_FILE="/tmp/msgbus_${TOPIC}.wal"
if [ ! -s "$WAL_FILE" ]; then
    echo " [FAIL] Expected write-ahead log file $WAL_FILE to exist and have content!"
    kill -9 $BROKER_PID 2>/dev/null || true
    exit 1
fi
echo "   Verified on-disk WAL file created: $(ls -l $WAL_FILE)"

# 4. Spawn a LATE Subscriber with --replay flag expecting 7 messages (6 replayed + 1 live)
echo "3. Spawning LATE Subscriber with --replay flag..."
./bin/msgbus-sub $TOPIC --replay -n 7 > replay_sub.log 2>&1 &
SUB_PID=$!
sleep 0.5

# 5. Publish 1 more LIVE message
echo "4. Publishing 1 new LIVE message to verify seamless transition..."
./bin/msgbus-pub $TOPIC "Live Event #7"

# Wait for subscriber to receive all 7
wait $SUB_PID
echo "   Subscriber exited after receiving all target messages."

# 6. Verify replay log content
REPLAYED_COUNT=$(grep -c "Received (REPLAY)" replay_sub.log || true)
LIVE_COUNT=$(grep -c "Received \[Seq" replay_sub.log || true)
TOTAL_COUNT=$(grep -c "Received" replay_sub.log || true)

echo "5. Verifying message delivery breakdown:"
echo "   Historical replayed messages: $REPLAYED_COUNT / 6"
echo "   Live real-time messages:      $LIVE_COUNT / 1"
echo "   Total messages received:      $TOTAL_COUNT / 7"

if [ "$REPLAYED_COUNT" -ne 6 ] || [ "$TOTAL_COUNT" -ne 7 ]; then
    echo " [FAIL] Replay count mismatch!"
    cat replay_sub.log
    kill -9 $BROKER_PID 2>/dev/null || true
    exit 1
fi

echo "   Replay Verified: Late subscriber received all 6 historical messages + 1 live message!"

# Teardown
kill -SIGINT $BROKER_PID
wait $BROKER_PID 2>/dev/null || true
rm -f replay_sub.log

echo "=========================================================="
echo " [Test W9] PASSED: Message Durability & Replay Verified!  "
echo "=========================================================="
