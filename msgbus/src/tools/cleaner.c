#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>
#include "msgbus/msgbus.h"
#include "msgbus/shm.h"
#include "msgbus/sync.h"

int main(int argc, char *argv[]) {
    printf("=====================================================\n");
    printf("       MsgBus POSIX IPC Resource Cleaner (W8)        \n");
    printf("=====================================================\n");

    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("Usage: %s [topic_name | --all]\n", argv[0]);
        printf("Cleans orphaned shared-memory segments, semaphores, and WAL files.\n");
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--all") != 0) {
        // Clean specific topic
        const char *topic = argv[1];
        printf("[cleaner] Unlinking resources for topic: '%s'...\n", topic);
        msgbus_topic_destroy(topic);
        char wal_path[256];
        snprintf(wal_path, sizeof(wal_path), "/tmp/msgbus_%s.wal", topic);
        unlink(wal_path);
        printf("[cleaner] Completed cleanup for topic: '%s'\n", topic);
        return 0;
    }

    // Clean all /dev/shm msgbus entries
    int cleaned_shm = 0;
    int cleaned_wal = 0;

    DIR *dir = opendir("/dev/shm");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "msgbus_", 7) == 0 ||
                strncmp(entry->d_name, "sem.msgbus_", 11) == 0) {
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "/dev/shm/%s", entry->d_name);
                if (unlink(full_path) == 0) {
                    printf("  [unlinked] %s\n", full_path);
                    cleaned_shm++;
                }
            }
        }
        closedir(dir);
    }

    // Clean all /tmp msgbus WAL files
    dir = opendir("/tmp");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "msgbus_", 7) == 0 && strstr(entry->d_name, ".wal")) {
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "/tmp/%s", entry->d_name);
                if (unlink(full_path) == 0) {
                    printf("  [unlinked WAL] %s\n", full_path);
                    cleaned_wal++;
                }
            }
        }
        closedir(dir);
    }

    printf("[cleaner] Finished: Unlinked %d shm/semaphore files and %d WAL files.\n", cleaned_shm, cleaned_wal);
    return 0;
}
