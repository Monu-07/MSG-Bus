#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "msgbus/msgbus.h"

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <topic> [--policy block|drop] [-n count] [--replay]\n", prog);
    fprintf(stderr, "Example: %s sports --policy block -n 10 --replay\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *topic = argv[1];
    msgbus_policy_t policy = MSGBUS_POLICY_BLOCK;
    int expected_count = -1; // -1 means run until stopped
    bool replay = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--policy") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "drop") == 0) {
                policy = MSGBUS_POLICY_DROP_OLDEST;
            } else {
                policy = MSGBUS_POLICY_BLOCK;
            }
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            expected_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--replay") == 0) {
            replay = true;
        }
    }

    // Milestone 6: Signal-safe graceful exit
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("[sub:%d] Connecting to topic '%s' (policy: %s%s)...\n",
           getpid(), topic, (policy == MSGBUS_POLICY_BLOCK ? "BLOCK" : "DROP_OLDEST"),
           replay ? " | REPLAY ENABLED" : "");

    msgbus_handle_t *handle = msgbus_connect(topic);
    if (!handle) {
        // Auto-create topic if not exists
        if (msgbus_topic_create(topic) != 0) {
            fprintf(stderr, "[sub:%d] Error: Cannot connect to topic '%s'\n", getpid(), topic);
            return 1;
        }
        handle = msgbus_connect(topic);
        if (!handle) {
            fprintf(stderr, "[sub:%d] Failed to connect to topic\n", getpid());
            return 1;
        }
    }

    int sub_rc = replay ? msgbus_subscribe_replay(handle, policy) : msgbus_subscribe(handle, policy);
    if (sub_rc != 0) {
        fprintf(stderr, "[sub:%d] Failed to subscribe to topic '%s'\n", getpid(), topic);
        msgbus_disconnect(handle);
        return 1;
    }

    printf("[sub:%d] Subscribed successfully. Waiting for messages (zero busy-waiting)...\n", getpid());

    int received = 0;
    while (g_running) {
        msgbus_msg_t msg;
        bool was_replaying = msgbus_is_replaying(handle);
        // Milestone 7: Block with 500ms timeout so we periodically check g_running on Ctrl+C
        int res = msgbus_recv(handle, &msg, 500);
        if (res > 0) {
            received++;
            if (was_replaying) {
                printf("[sub:%d] Received (REPLAY) [Seq #%lu | From PID %d]: \"%s\"\n",
                       getpid(), msg.seq, msg.sender_pid, msg.payload);
            } else {
                printf("[sub:%d] Received [Seq #%lu | From PID %d]: \"%s\"\n",
                       getpid(), msg.seq, msg.sender_pid, msg.payload);
            }

            if (expected_count > 0 && received >= expected_count) {
                printf("[sub:%d] Reached target count of %d messages. Exiting.\n", getpid(), expected_count);
                break;
            }
        }
    }

    printf("[sub:%d] Disconnecting and unregistering...\n", getpid());
    msgbus_disconnect(handle);
    printf("[sub:%d] Done. Total messages received: %d\n", getpid(), received);
    return 0;
}
