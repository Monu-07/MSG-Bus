#ifndef MSGBUS_SHM_H
#define MSGBUS_SHM_H

#include <stddef.h>
#include <stdbool.h>

// Format shm path name: "/msgbus_<topic>"
void msgbus_get_shm_name(const char *topic, char *dest, size_t max_len);

// Format semaphore names for topic
void msgbus_get_sem_names(const char *topic, char *sem_mutex, char *sem_free, char *sem_used, size_t max_len);

// Milestone 1: POSIX Shared Memory Syscall Wrappers
void* msgbus_shm_create(const char *name, size_t size, int *out_fd);
void* msgbus_shm_attach(const char *name, size_t size, int *out_fd);
int   msgbus_shm_detach(void *addr, size_t size, int fd);
int   msgbus_shm_unlink(const char *name);

#endif // MSGBUS_SHM_H
