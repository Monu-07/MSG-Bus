#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "msgbus/msgbus.h"

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <topic> <message> [-n count] [-d delay_ms] [--durable]\n", prog);
    fprintf(stderr, "Example: %s sports \"Goal scored!\" -n 5 -d 200 --durable\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *topic = argv[1];
    const char *message = argv[2];
    int count = 1;
    int delay_ms = 0;
    bool durable = false;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            delay_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--durable") == 0) {
            durable = true;
        }
    }

    printf("[pub:%d] Connecting to topic '%s'...\n", getpid(), topic);
    msgbus_handle_t *handle = msgbus_connect(topic);
    if (!handle) {
        // If topic does not exist yet, auto-create it
        printf("[pub:%d] Topic '%s' not found. Creating it%s...\n", getpid(), topic, durable ? " (durable)" : "");
        if (msgbus_topic_create_durable(topic, durable) != 0) {
            fprintf(stderr, "[pub:%d] Error: Failed to create or connect to topic '%s'\n", getpid(), topic);
            return 1;
        }
        handle = msgbus_connect(topic);
        if (!handle) {
            fprintf(stderr, "[pub:%d] Error connecting to created topic\n", getpid());
            return 1;
        }
    }

    char buf[MSGBUS_MAX_PAYLOAD];
    for (int i = 1; i <= count; i++) {
        if (count > 1) {
            snprintf(buf, sizeof(buf), "%s #%d", message, i);
        } else {
            snprintf(buf, sizeof(buf), "%s", message);
        }

        if (msgbus_publish(handle, buf, strlen(buf) + 1) == 0) {
            printf("[pub:%d] Published: \"%s\"\n", getpid(), buf);
        } else {
            fprintf(stderr, "[pub:%d] Error publishing message!\n", getpid());
        }

        if (delay_ms > 0 && i < count) {
            struct timespec req = {
                .tv_sec = delay_ms / 1000,
                .tv_nsec = (delay_ms % 1000) * 1000000L
            };
            nanosleep(&req, NULL);
        }
    }

    msgbus_disconnect(handle);
    return 0;
}
