CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -Iinclude -D_GNU_SOURCE
LDFLAGS = -pthread -lrt

SRC_LIB = src/lib/shm.c src/lib/sync.c src/lib/ring_buffer.c src/lib/client.c
OBJ_LIB = $(patsubst src/%.c, build/%.o, $(SRC_LIB))

SRC_BROKER = src/broker/broker_main.c src/broker/topic_table.c
OBJ_BROKER = $(patsubst src/%.c, build/%.o, $(SRC_BROKER))

TARGET_LIB = lib/libmsgbus.a
TARGET_BROKER = bin/msgbus-broker
TARGET_PUB = bin/msgbus-pub
TARGET_PUBLISHER = bin/msgbus-publisher
TARGET_SUB = bin/msgbus-sub
TARGET_CLEAN = bin/msgbus-clean
TARGET_BENCH = bin/msgbus-bench
TARGET_TEST_W1 = bin/test_w1_single_slot
TARGET_TEST_W2 = bin/test_w2_ring_layout

ALL_BINS = $(TARGET_BROKER) $(TARGET_PUBLISHER) $(TARGET_PUB) $(TARGET_SUB) $(TARGET_CLEAN) $(TARGET_BENCH) $(TARGET_TEST_W1) $(TARGET_TEST_W2)

.PHONY: all clean test shm-clean directories

all: directories $(TARGET_LIB) $(ALL_BINS)

directories:
	@mkdir -p build/lib build/broker build/tools bin lib

# Client Library Archive
$(TARGET_LIB): $(OBJ_LIB)
	@echo " [AR] $@"
	@ar rcs $@ $^

# Broker Daemon
$(TARGET_BROKER): $(OBJ_BROKER) $(TARGET_LIB)
	@echo " [LD] $@"
	@$(CC) $(CFLAGS) -o $@ $(OBJ_BROKER) $(TARGET_LIB) $(LDFLAGS)
	@cp -f $@ broker
	@chmod +x broker

# CLI Tools
$(TARGET_PUBLISHER): src/tools/publisher.c $(TARGET_LIB)
	@echo " [LD] $@"
	@$(CC) $(CFLAGS) -o $@ src/tools/publisher.c $(TARGET_LIB) $(LDFLAGS)
	@cp -f $@ $(TARGET_PUB)
	@cp -f $@ publisher
	@chmod +x $@ $(TARGET_PUB) publisher

$(TARGET_PUB): $(TARGET_PUBLISHER)

$(TARGET_SUB): src/tools/subscriber.c $(TARGET_LIB)
	@echo " [LD] $@"
	@$(CC) $(CFLAGS) -o $@ src/tools/subscriber.c $(TARGET_LIB) $(LDFLAGS)
	@cp -f $@ subscriber
	@chmod +x subscriber

$(TARGET_CLEAN): src/tools/cleaner.c $(TARGET_LIB)
	@echo " [LD] $@"
	@$(CC) $(CFLAGS) -o $@ src/tools/cleaner.c $(TARGET_LIB) $(LDFLAGS)
	@cp -f $@ cleaner
	@chmod +x cleaner

$(TARGET_BENCH): src/tools/benchmark.c $(TARGET_LIB)
	@echo " [LD] $@"
	@$(CC) $(CFLAGS) -o $@ src/tools/benchmark.c $(TARGET_LIB) $(LDFLAGS)
	@cp -f $@ benchmark
	@chmod +x benchmark

# Tests
$(TARGET_TEST_W1): tests/test_w1_single_slot.c
	@echo " [LD] $@"
	@$(CC) $(CFLAGS) -o $@ tests/test_w1_single_slot.c $(LDFLAGS)

$(TARGET_TEST_W2): tests/test_w2_ring_layout.c src/lib/ring_buffer.c
	@echo " [LD] $@"
	@$(CC) $(CFLAGS) -o $@ tests/test_w2_ring_layout.c src/lib/ring_buffer.c $(LDFLAGS)

# Object compilation rule
build/%.o: src/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

test: all
	@echo "=== Running Week 1 & Week 2 Unit Tests ==="
	@./$(TARGET_TEST_W1)
	@./$(TARGET_TEST_W2)
	@echo "=== Running Week 7 Multi-Process Integration Test ==="
	@bash tests/test_w7_multiprocess.sh
	@echo "=== Running Zero Busy-Waiting CPU Verification ==="
	@bash tests/verify_zero_cpu.sh
	@echo "=== Running Week 8 Crash Recovery & Cleanup Test ==="
	@bash tests/test_w8_cleanup.sh
	@echo "=== Running Week 9 Message Durability & Replay Test ==="
	@bash tests/test_w9_durability.sh
	@echo "=== Running Week 10 Performance Benchmark ==="
	@bash tests/test_w10_benchmark.sh
	@echo "=== Running Week 11 Multi-Process Concurrency Soak Test ==="
	@bash tests/test_w11_soak.sh
	@echo "=== Running ShellForge Orchestration Integration Test ==="
	@bash tests/test_in_shellforge.sh

shm-clean:
	@echo "Cleaning up any leftover POSIX shared memory, semaphores and WAL files..."
	@rm -f /dev/shm/msgbus_* /dev/shm/sem.msgbus_* /tmp/msgbus_*.wal 2>/dev/null || true

clean: shm-clean
	@rm -rf build bin lib broker publisher subscriber cleaner benchmark
	@echo "Clean completed."
