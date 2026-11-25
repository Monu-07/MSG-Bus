#ifndef MSGBUS_SYNC_H
#define MSGBUS_SYNC_H

#include <semaphore.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

// Milestone 7 & Milestone 6: Process-Shared Robust Synchronization Primitives

// Initialize mutex with PTHREAD_PROCESS_SHARED & PTHREAD_MUTEX_ROBUST
int msgbus_sync_init_mutex(pthread_mutex_t *mutex);

// Initialize condition variable with PTHREAD_PROCESS_SHARED
int msgbus_sync_init_cond(pthread_cond_t *cond);

// Lock robust mutex and automatically recover if the previous owner died
int msgbus_sync_lock_robust(pthread_mutex_t *mutex);
int msgbus_sync_unlock(pthread_mutex_t *mutex);

// POSIX Named Semaphores (Milestone 7 Bounded Producer/Consumer Coordination)
typedef struct {
    sem_t *sem_mutex; // Mutual exclusion for concurrent writers
    sem_t *sem_free;  // Counting semaphore: available free slots
    sem_t *sem_used;  // Counting semaphore: available unread slots
} msgbus_semaphores_t;

int  msgbus_sem_init(const char *topic, uint32_t capacity, msgbus_semaphores_t *sems);
int  msgbus_sem_open(const char *topic, msgbus_semaphores_t *sems);
void msgbus_sem_close(msgbus_semaphores_t *sems);
void msgbus_sem_unlink(const char *topic);

#endif // MSGBUS_SYNC_H
