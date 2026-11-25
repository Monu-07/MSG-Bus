#!/usr/bin/env bash
# Integration Test: Running MsgBus inside ShellForge Unix Shell
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

echo "=========================================================="
echo " [ShellForge Integration] Running MsgBus inside ShellForge"
echo "=========================================================="

SHELLFORGE_BIN="../ShellForge"
if [ ! -f "$SHELLFORGE_BIN" ]; then
    SHELLFORGE_BIN="/mnt/c/Users/asolo/.gemini/antigravity/scratch/ShellForge/ShellForge"
fi

if [ ! -f "$SHELLFORGE_BIN" ]; then
    echo " [FAIL] ShellForge binary not found at $SHELLFORGE_BIN"
    exit 1
fi

rm -f sf_output.log

echo "1. Feeding Pub/Sub commands into ShellForge..."

# Run commands inside ShellForge and capture full shell session
"$SHELLFORGE_BIN" > sf_output.log 2>&1 << 'EOF'
./bin/msgbus-clean --all
./bin/msgbus-broker shellforge_topic &
./bin/msgbus-sub shellforge_topic --policy block -n 3 &
./bin/msgbus-pub shellforge_topic Goal-1
./bin/msgbus-pub shellforge_topic Goal-2
./bin/msgbus-pub shellforge_topic Goal-3
exit
EOF

sleep 0.5

echo "2. ShellForge execution output:"
cat sf_output.log

COUNT=$(grep -c "Received \[Seq" sf_output.log || true)
echo "3. Verified: ShellForge successfully orchestrated MsgBus ($COUNT / 3 messages received)!"

# Clean topic
./bin/msgbus-clean shellforge_topic >/dev/null 2>&1 || true
rm -f sf_output.log

if [ "$COUNT" -ne 3 ]; then
    echo " [FAIL] Expected 3 messages, got $COUNT"
    exit 1
fi

echo "=========================================================="
echo " [ShellForge Integration] PASSED SUCCESSFULLY!            "
echo "=========================================================="
