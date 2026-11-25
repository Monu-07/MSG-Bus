CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude
SRC := $(wildcard src/*.c)
TARGET = ShellForge

.PHONY: all clean msgbus_tools test

all: $(TARGET) msgbus_tools

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -lreadline -o $(TARGET)
	@chmod +x $(TARGET)

msgbus_tools:
	@$(MAKE) -C msgbus
	@cp -f msgbus/bin/msgbus-* .
	@chmod +x msgbus-* ShellForge run_msgbus_demo.sh 2>/dev/null || true

test: all
	@$(MAKE) -C msgbus test

clean:
	rm -f $(TARGET) shellforge msgbus-*
	@$(MAKE) -C msgbus clean
