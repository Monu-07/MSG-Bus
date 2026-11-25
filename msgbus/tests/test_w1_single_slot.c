#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <assert.h>

#define TEST_SHM_NAME "/msgbus_test_w1"
#define TEST_MSG "Hello from Week 1 Shared Memory Slot!"

typedef struct {
    int ready;
    char buffer[256];
} single_slot_t;

int main(void) {
    printf("[Test W1] Testing Syscall Boundary: Single Shared Memory Slot...\n");

    // Clean up any stale segment
    shm_unlink(TEST_SHM_NAME);

    // 1. Syscall boundary: shm_open
    int fd = shm_open(TEST_SHM_NAME, O_CREAT | O_RDWR, 0666);
    assert(fd >= 0);

    // 2. Syscall: ftruncate
    assert(ftruncate(fd, sizeof(single_slot_t)) == 0);

    // 3. Syscall: mmap (map kernel shm page into user address space)
    single_slot_t *slot = mmap(NULL, sizeof(single_slot_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(slot != MAP_FAILED);

    slot->ready = 0;
    memset(slot->buffer, 0, sizeof(slot->buffer));

    // 4. Milestone 4 preview: fork a child process to verify IPC across process boundaries
    pid_t pid = fork();
    assert(pid >= 0);

    if (pid == 0) {
        // Child process acts as publisher
        printf("  [Child Process PID:%d] Writing message to shared memory slot...\n", getpid());
        strncpy(slot->buffer, TEST_MSG, sizeof(slot->buffer) - 1);
        slot->ready = 1; // Mark ready
        munmap(slot, sizeof(single_slot_t));
        close(fd);
        exit(0);
    } else {
        // Parent process acts as subscriber
        wait(NULL); // Wait for child to finish write
        printf("  [Parent Process PID:%d] Reading message from shared memory slot...\n", getpid());
        assert(slot->ready == 1);
        printf("  [Parent] Received: \"%s\"\n", slot->buffer);
        assert(strcmp(slot->buffer, TEST_MSG) == 0);

        // Teardown: munmap & shm_unlink
        munmap(slot, sizeof(single_slot_t));
        close(fd);
        shm_unlink(TEST_SHM_NAME);
        printf("[Test W1] PASSED: User/Kernel boundary & IPC shared memory verified!\n\n");
    }

    return 0;
}
