#ifndef MSGBUS_H
#define MSGBUS_H

#include "protocol.h"
#include "shm.h"
#include "sync.h"
#include <stddef.h>

// Opaque connection handle for client applications
typedef struct msgbus_handle {
    char topic_name[MSGBUS_MAX_TOPIC_NAME];
    int shm_fd;
    msgbus_topic_shm_t *shm_addr;
    int sub_index;             // Index in subscriber table (-1 if publisher only)
    msgbus_policy_t policy;    // Subscription policy
    msgbus_semaphores_t sems;  // POSIX semaphores
    int wal_fd;                // Replay file descriptor for durable topics (Week 9)
    bool in_replay;            // True while replaying historical messages
} msgbus_handle_t;

// Milestone 5: Public Client Library API

// Topic Lifecycle
int msgbus_topic_create(const char *topic_name);
int msgbus_topic_create_durable(const char *topic_name, bool durable);
int msgbus_topic_destroy(const char *topic_name);

// Connection Management (Milestone 4: Process Attach/Detach)
msgbus_handle_t* msgbus_connect(const char *topic_name);
int msgbus_disconnect(msgbus_handle_t *handle);

// Publishing (Milestone 5 & 7: Multi-Publisher Writing with Bounded Policies)
int msgbus_publish(msgbus_handle_t *handle, const void *payload, size_t len);

// Subscribing (Milestone 5 & 7: Zero Busy-Waiting Receiving)
int msgbus_subscribe(msgbus_handle_t *handle, msgbus_policy_t policy);
int msgbus_subscribe_replay(msgbus_handle_t *handle, msgbus_policy_t policy);
bool msgbus_is_replaying(const msgbus_handle_t *handle);
int msgbus_unsubscribe(msgbus_handle_t *handle);
int msgbus_recv(msgbus_handle_t *handle, msgbus_msg_t *msg_out, int timeout_ms);

#endif // MSGBUS_H
