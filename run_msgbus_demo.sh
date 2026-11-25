#!/usr/bin/env bash
# Demonstration Script: Running MsgBus inside ShellForge
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

echo "=========================================================="
echo "    Running MsgBus Demonstration inside ShellForge Shell  "
echo "=========================================================="

./ShellForge << 'COMMANDS'
./msgbus-clean --all
./msgbus-broker sports &
./msgbus-sub sports --policy block -n 3 &
./msgbus-publisher sports "Goal-1"
./msgbus-publisher sports "Goal-2"
./msgbus-publisher sports "Goal-3"
./msgbus-bench 5000
exit
COMMANDS

echo "=========================================================="
echo "    ShellForge MsgBus Demonstration Completed!            "
echo "=========================================================="
