#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "msgbus/protocol.h"
#include "msgbus/ring_buffer.h"

int main(void) {
    printf("[Test W2] Testing Topic Ring Buffer Layout & Memory Discipline...\n");

    // Allocate simulated shared memory region
    msgbus_topic_shm_t shm;
    memset(&shm, 0, sizeof(shm));
    strncpy(shm.topic_name, "test_ring", sizeof(shm.topic_name) - 1);
    shm.capacity = MSGBUS_RING_CAPACITY;
    shm.head_seq = 0;

    printf("  [Info] sizeof(msgbus_topic_shm_t) = %zu bytes\n", sizeof(msgbus_topic_shm_t));
    printf("  [Info] sizeof(msgbus_msg_t)       = %zu bytes\n", sizeof(msgbus_msg_t));
    printf("  [Info] Ring Capacity              = %u slots\n", shm.capacity);

    // Register a mock subscriber
    shm.subscribers[0].active = true;
    shm.subscribers[0].pid = 1234;
    shm.subscribers[0].policy = MSGBUS_POLICY_BLOCK;
    shm.subscribers[0].read_seq = 0;
    shm.active_subscribers = 1;

    // Test 1: Write up to capacity and read back
    for (int i = 0; i < (int)shm.capacity; i++) {
        char payload[64];
        snprintf(payload, sizeof(payload), "Message number %d", i);
        uint64_t seq = 0;
        int rc = msgbus_ring_write(&shm, payload, strlen(payload) + 1, 1000, &seq);
        assert(rc == 0);
        assert(seq == (uint64_t)i);
    }
    assert(shm.head_seq == shm.capacity);
    printf("  [W2] Filled buffer to capacity (%u messages) successfully.\n", shm.capacity);

    // Read back all messages
    for (int i = 0; i < (int)shm.capacity; i++) {
        msgbus_msg_t msg;
        int rc = msgbus_ring_read(&shm, 0, &msg);
        assert(rc == 1);
        assert(msg.seq == (uint64_t)i);
        char expected[64];
        snprintf(expected, sizeof(expected), "Message number %d", i);
        assert(strcmp(msg.payload, expected) == 0);
    }
    assert(shm.subscribers[0].read_seq == shm.head_seq);
    printf("  [W2] Read back all messages in sequential order.\n");

    // Test 2: Wrap-around test (write 50 more messages)
    printf("  [W2] Testing circular ring buffer wrap-around...\n");
    for (int i = 0; i < 50; i++) {
        char payload[64];
        snprintf(payload, sizeof(payload), "Wrap message %d", i);
        uint64_t seq = 0;
        assert(msgbus_ring_write(&shm, payload, strlen(payload) + 1, 1000, &seq) == 0);

        msgbus_msg_t msg;
        int rc = msgbus_ring_read(&shm, 0, &msg);
        assert(rc == 1);
        assert(msg.seq == seq);
        assert(strcmp(msg.payload, payload) == 0);
    }

    printf("[Test W2] PASSED: Circular ring buffer layout and wrap-around verified!\n\n");
    return 0;
}
