#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "topic_table.h"
#include "msgbus/msgbus.h"

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

int main(int argc, char *argv[]) {
    printf("=====================================================\n");
    printf("      MsgBus Broker Daemon (Weeks 1 to 12)\n");
    printf(" Decoupled Pub/Sub over POSIX Shared-Memory Rings\n");
    printf("=====================================================\n");

    // Milestone 6: Signal handling for clean teardown on exit or Ctrl+C
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    topic_table_init();

    bool durable = false;
    int topic_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--durable") == 0) {
            durable = true;
        }
    }

    // Register initial topics passed via CLI (excluding flags)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--durable") == 0) continue;
        if (topic_table_add_durable(argv[i], durable) == 0) {
            printf("[broker] Initialized topic: '%s'%s\n", argv[i], durable ? " (DURABLE WAL ENABLED)" : "");
            topic_count++;
        }
    }

    // Default topic if none provided
    if (topic_count == 0) {
        const char *default_topic = "default";
        topic_table_add_durable(default_topic, durable);
        printf("[broker] Initialized default topic: '%s'%s\n", default_topic, durable ? " (DURABLE WAL ENABLED)" : "");
    }

    printf("[broker] Broker running (PID: %d). Press Ctrl+C to terminate cleanly.\n", getpid());

    // Daemon loop (Wait for termination signal)
    while (g_running) {
        sleep(1);
    }

    printf("\n[broker] Caught shutdown signal! Unlinking all shared memory, semaphores & WAL...\n");
    topic_table_cleanup_all();
    printf("[broker] Teardown complete. Exiting.\n");

    return 0;
}
