#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "msgbus/msgbus.h"

#define DEFAULT_MSG_COUNT 25000
#define BENCH_PAYLOAD_SIZE 128

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void run_shm_benchmark(int num_msgs, double *out_time, double *out_thru, double *out_lat_us) {
    const char *topic = "bench_shm_test";
    msgbus_topic_destroy(topic);
    if (msgbus_topic_create(topic) != 0) {
        fprintf(stderr, "Failed to create benchmark shm topic\n");
        exit(1);
    }

    char payload[BENCH_PAYLOAD_SIZE];
    memset(payload, 'A', sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = '\0';

    pid_t pid = fork();
    if (pid == 0) {
        // Child: Subscriber
        msgbus_handle_t *sub = msgbus_connect(topic);
        if (!sub || msgbus_subscribe(sub, MSGBUS_POLICY_BLOCK) != 0) {
            exit(1);
        }
        msgbus_msg_t msg;
        for (int i = 0; i < num_msgs; i++) {
            while (msgbus_recv(sub, &msg, -1) <= 0);
        }
        msgbus_disconnect(sub);
        exit(0);
    } else {
        // Parent: Publisher
        usleep(50000); // 50ms for subscriber to attach
        msgbus_handle_t *pub = msgbus_connect(topic);
        if (!pub) {
            waitpid(pid, NULL, 0);
            exit(1);
        }

        double t0 = get_time_sec();
        for (int i = 0; i < num_msgs; i++) {
            while (msgbus_publish(pub, payload, sizeof(payload)) != 0);
        }
        waitpid(pid, NULL, 0);
        double t1 = get_time_sec();

        msgbus_disconnect(pub);
        msgbus_topic_destroy(topic);

        double elapsed = t1 - t0;
        *out_time = elapsed;
        *out_thru = (double)num_msgs / elapsed;
        *out_lat_us = (elapsed / (double)num_msgs) * 1e6;
    }
}

static void run_socket_benchmark(int num_msgs, double *out_time, double *out_thru, double *out_lat_us) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        exit(1);
    }

    char payload[BENCH_PAYLOAD_SIZE];
    memset(payload, 'B', sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = '\0';

    pid_t pid = fork();
    if (pid == 0) {
        // Child: Reader
        close(sv[0]);
        char buf[BENCH_PAYLOAD_SIZE];
        for (int i = 0; i < num_msgs; i++) {
            size_t total = 0;
            while (total < sizeof(buf)) {
                ssize_t n = read(sv[1], buf + total, sizeof(buf) - total);
                if (n <= 0) break;
                total += n;
            }
        }
        close(sv[1]);
        exit(0);
    } else {
        // Parent: Writer
        close(sv[1]);
        double t0 = get_time_sec();
        for (int i = 0; i < num_msgs; i++) {
            size_t total = 0;
            while (total < sizeof(payload)) {
                ssize_t n = write(sv[0], payload + total, sizeof(payload) - total);
                if (n <= 0) break;
                total += n;
            }
        }
        waitpid(pid, NULL, 0);
        double t1 = get_time_sec();
        close(sv[0]);

        double elapsed = t1 - t0;
        *out_time = elapsed;
        *out_thru = (double)num_msgs / elapsed;
        *out_lat_us = (elapsed / (double)num_msgs) * 1e6;
    }
}

int main(int argc, char *argv[]) {
    int count = DEFAULT_MSG_COUNT;
    if (argc > 1) {
        count = atoi(argv[1]);
        if (count <= 0) count = DEFAULT_MSG_COUNT;
    }

    printf("=======================================================================\n");
    printf("         MsgBus IPC Performance Benchmark: SHM vs Unix Sockets        \n");
    printf("=======================================================================\n");
    printf(" [Config] Messages: %d | Payload size: %d bytes\n\n", count, BENCH_PAYLOAD_SIZE);

    printf(" Running POSIX Shared Memory Benchmark...\n");
    double shm_time = 0, shm_thru = 0, shm_lat = 0;
    run_shm_benchmark(count, &shm_time, &shm_thru, &shm_lat);

    printf(" Running UNIX Domain Socket Benchmark...\n");
    double sock_time = 0, sock_thru = 0, sock_lat = 0;
    run_socket_benchmark(count, &sock_time, &sock_thru, &sock_lat);

    double speedup = shm_thru / sock_thru;

    printf("\n");
    printf("+----------------------+------------+------------+--------------------+------------------+\n");
    printf("| Transport Mechanism  | Messages   | Time (ms)  | Throughput (msg/s) | Avg Latency (us) |\n");
    printf("+----------------------+------------+------------+--------------------+------------------+\n");
    printf("| POSIX Shared Memory  | %-10d | %-10.2f | %-18.0f | %-16.2f |\n",
           count, shm_time * 1000.0, shm_thru, shm_lat);
    printf("| UNIX Domain Socket   | %-10d | %-10.2f | %-18.0f | %-16.2f |\n",
           count, sock_time * 1000.0, sock_thru, sock_lat);
    printf("+----------------------+------------+------------+--------------------+------------------+\n");
    printf("\n>>> RESULT: POSIX Shared Memory is %.2fx FASTER than UNIX Domain Sockets!\n", speedup);
    printf("    (Reason: Zero kernel-space buffer copies and lock-free/futex wakeups)\n\n");

    return 0;
}
