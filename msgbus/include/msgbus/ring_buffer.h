#ifndef MSGBUS_RING_BUFFER_H
#define MSGBUS_RING_BUFFER_H

#include "msgbus/protocol.h"

// Milestone 2 & 7: Circular Ring Buffer Read / Write Operations
int msgbus_ring_write(msgbus_topic_shm_t *shm, const void *payload, size_t len, pid_t pid, uint64_t *out_seq);
int msgbus_ring_read(msgbus_topic_shm_t *shm, int sub_idx, msgbus_msg_t *msg_out);

#endif // MSGBUS_RING_BUFFER_H
