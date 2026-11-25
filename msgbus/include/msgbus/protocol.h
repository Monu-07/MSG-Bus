#ifndef MSGBUS_PROTOCOL_H
#define MSGBUS_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <pthread.h>

#define MSGBUS_MAX_TOPIC_NAME 64
#define MSGBUS_MAX_PAYLOAD    1024
#define MSGBUS_RING_CAPACITY  32
#define MSGBUS_MAX_SUBS       16

// Bounded queue policies (Week 7)
typedef enum {
    MSGBUS_POLICY_BLOCK = 0,      // Block publisher when ring buffer is full
    MSGBUS_POLICY_DROP_OLDEST = 1 // Drop oldest unconsumed message if subscriber is slow
} msgbus_policy_t;

// Milestone 3: Message Descriptor Format
typedef struct {
    uint64_t seq;                         // Monotonically increasing message sequence ID
    uint32_t len;                         // Payload length in bytes
    pid_t sender_pid;                     // PID of the publishing process (W4)
    uint64_t timestamp_ns;                // Timestamp in nanoseconds
    char payload[MSGBUS_MAX_PAYLOAD];     // Fixed-size payload slot (W2 memory discipline)
} msgbus_msg_t;

// Milestone 4: Subscriber Process Descriptor in Shared Memory
typedef struct {
    pid_t pid;                            // Subscriber process ID
    uint64_t read_seq;                    // Next sequence expected by this subscriber
    bool active;                          // Whether this slot is in use
    msgbus_policy_t policy;               // Overflow policy for this subscriber
} msgbus_subscriber_t;

// Milestone 2 & 7: Topic Ring Buffer Layout in Shared Memory
typedef struct {
    // Process-shared synchronization primitives (W6 & W7)
    pthread_mutex_t lock;                 // PTHREAD_PROCESS_SHARED & PTHREAD_MUTEX_ROBUST
    pthread_cond_t  has_data_cv;          // Broadcast wake-up for waiting subscribers
    pthread_cond_t  has_space_cv;         // Wake-up for blocked publishers

    char topic_name[MSGBUS_MAX_TOPIC_NAME];
    uint32_t capacity;                    // Ring capacity (power of 2: MSGBUS_RING_CAPACITY)
    uint64_t head_seq;                    // Next sequence number to be written by publishers
    uint32_t active_subscribers;          // Count of active attached subscribers
    bool durable;                         // Message durability enabled (Week 9)
    char wal_path[256];                   // Path to on-disk write-ahead log file (Week 9)

    // Subscriber table (W4 process tracking)
    msgbus_subscriber_t subscribers[MSGBUS_MAX_SUBS];

    // Circular ring buffer storage (W2 dynamic data layout)
    msgbus_msg_t ring[MSGBUS_RING_CAPACITY];
} msgbus_topic_shm_t;

#endif // MSGBUS_PROTOCOL_H
