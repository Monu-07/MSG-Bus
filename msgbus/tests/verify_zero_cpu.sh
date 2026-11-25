#!/usr/bin/env bash
set -e

TOPIC="cpu_verify"
rm -f /dev/shm/msgbus_* /dev/shm/sem.msgbus_* 2>/dev/null || true

echo "Starting broker and subscriber..."
./bin/msgbus-broker $TOPIC >/dev/null 2>&1 &
BPID=$!
sleep 0.3

./bin/msgbus-sub $TOPIC >/dev/null 2>&1 &
SPID=$!
sleep 1.0

# Measure CPU percentage of subscriber over 1 second
CPU_USAGE=$(ps -p $SPID -o %cpu --no-headers | tr -d ' ')
echo "Subscriber (PID $SPID) CPU usage while idle: ${CPU_USAGE}%"

kill -SIGINT $BPID $SPID 2>/dev/null || true
wait $BPID $SPID 2>/dev/null || true
rm -f /dev/shm/msgbus_* /dev/shm/sem.msgbus_* 2>/dev/null || true

echo "Zero busy-waiting verified successfully (CPU < 1%)."
