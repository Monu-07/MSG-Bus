#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "topic_table.h"
#include "msgbus/msgbus.h"
#include <string.h>
#include <stdio.h>

static broker_topic_entry_t g_topics[MAX_BROKER_TOPICS];

void topic_table_init(void) {
    memset(g_topics, 0, sizeof(g_topics));
}

int topic_table_add_durable(const char *name, bool durable) {
    if (!name || strlen(name) >= MSGBUS_MAX_TOPIC_NAME) return -1;

    // Check if already exists
    for (int i = 0; i < MAX_BROKER_TOPICS; i++) {
        if (g_topics[i].in_use && strcmp(g_topics[i].topic_name, name) == 0) {
            return 0; // Already registered
        }
    }

    // Find free slot
    for (int i = 0; i < MAX_BROKER_TOPICS; i++) {
        if (!g_topics[i].in_use) {
            strncpy(g_topics[i].topic_name, name, sizeof(g_topics[i].topic_name) - 1);
            g_topics[i].in_use = true;
            g_topics[i].durable = durable;
            return msgbus_topic_create_durable(name, durable);
        }
    }

    return -1; // Table full
}

int topic_table_add(const char *name) {
    return topic_table_add_durable(name, false);
}

int topic_table_remove(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < MAX_BROKER_TOPICS; i++) {
        if (g_topics[i].in_use && strcmp(g_topics[i].topic_name, name) == 0) {
            g_topics[i].in_use = false;
            return msgbus_topic_destroy(name);
        }
    }
    return -1;
}

void topic_table_cleanup_all(void) {
    // Milestone 6: Signal-safe cleanup of all topics and semaphores
    for (int i = 0; i < MAX_BROKER_TOPICS; i++) {
        if (g_topics[i].in_use) {
            printf("[broker] Cleaning up topic: %s\n", g_topics[i].topic_name);
            msgbus_topic_destroy(g_topics[i].topic_name);
            g_topics[i].in_use = false;
        }
    }
}
