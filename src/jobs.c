#define _POSIX_C_SOURCE 200809L
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

static job_t job_table[MAX_JOBS];
static int job_count = 0;
static volatile pid_t current_fg_pgid = 0;

void set_foreground_pgid(pid_t pgid) {
    current_fg_pgid = pgid;
}

pid_t get_foreground_pgid(void) {
    return current_fg_pgid;
}

static void sigtstp_handler(int sig) {
    (void)sig;
    if (current_fg_pgid > 0) {
        kill(-current_fg_pgid, SIGTSTP);
    }
}

static void sigint_handler(int sig) {
    (void)sig;
    if (current_fg_pgid > 0) {
        kill(-current_fg_pgid, SIGINT);
    }
}

void jobs_init(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        job_table[i].id = 0;
        job_table[i].pgid = 0;
        job_table[i].state = JOB_DONE;
        job_table[i].command = NULL;
    }
    job_count = 0;
    current_fg_pgid = 0;

    // Ignore terminal stop and background signals in the shell process
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigtstp_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa, NULL);

    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, NULL);
}

job_t *job_add(pid_t pgid, const char *command, job_state_t state) {
    int max_id = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id > max_id) {
            max_id = job_table[i].id;
        }
    }
    int id = max_id + 1;
    if (id > MAX_JOBS) {
        id = 1;
        while (job_find(id) != NULL && id <= MAX_JOBS) {
            id++;
        }
    }

    int slot = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id == 0) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        fprintf(stderr, "jobs: job table full\n");
        return NULL;
    }

    job_table[slot].id = id;
    job_table[slot].pgid = pgid;
    job_table[slot].state = state;
    job_table[slot].command = command ? strdup(command) : strdup("");
    job_count++;

    return &job_table[slot];
}

int jobs_get_most_recent_id(void) {
    int max_id = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id > max_id && job_table[i].state != JOB_DONE) {
            max_id = job_table[i].id;
        }
    }
    return max_id > 0 ? max_id : -1;
}

int jobs_get_most_recent_stopped_id(void) {
    int max_id = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id > max_id && job_table[i].state == JOB_STOPPED) {
            max_id = job_table[i].id;
        }
    }
    if (max_id > 0) return max_id;
    return jobs_get_most_recent_id();
}

job_t *job_find(int id) {
    if (id <= 0) return NULL;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id == id) {
            return &job_table[i];
        }
    }
    return NULL;
}

job_t *job_find_by_pgid(pid_t pgid) {
    if (pgid <= 0) return NULL;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id != 0 && job_table[i].pgid == pgid) {
            return &job_table[i];
        }
    }
    return NULL;
}

int job_remove(int id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id == id) {
            if (job_table[i].command) {
                free(job_table[i].command);
                job_table[i].command = NULL;
            }
            job_table[i].id = 0;
            job_table[i].pgid = 0;
            job_table[i].state = JOB_DONE;
            job_count--;
            return 0;
        }
    }
    return -1;
}

void job_stop(job_t *job) {
    if (job) {
        job->state = JOB_STOPPED;
    }
}

void job_continue(job_t *job) {
    if (job) {
        job->state = JOB_RUNNING;
    }
}

void job_done(job_t *job) {
    if (job) {
        job->state = JOB_DONE;
    }
}

void jobs_update_status(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        job_t *job = job_find_by_pgid(pid);
        if (!job) {
            for (int i = 0; i < MAX_JOBS; i++) {
                if (job_table[i].id != 0) {
                    if (kill(-job_table[i].pgid, 0) == -1 && errno == ESRCH) {
                        job_table[i].state = JOB_DONE;
                    }
                }
            }
        } else {
            if (WIFSTOPPED(status)) {
                job->state = JOB_STOPPED;
            } else if (WIFCONTINUED(status)) {
                job->state = JOB_RUNNING;
            } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                if (kill(-job->pgid, 0) == -1 && errno == ESRCH) {
                    job->state = JOB_DONE;
                }
            }
        }
    }

    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id != 0 && job_table[i].state == JOB_RUNNING) {
            if (kill(-job_table[i].pgid, 0) == -1 && errno == ESRCH) {
                job_table[i].state = JOB_DONE;
            }
        }
    }
}

void jobs_print(void) {
    jobs_update_status();
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id != 0) {
            const char *state_str = "Unknown";
            switch (job_table[i].state) {
                case JOB_RUNNING: state_str = "Running"; break;
                case JOB_STOPPED: state_str = "Stopped"; break;
                case JOB_DONE:    state_str = "Done";    break;
            }
            printf("[%d]  %-10s %s\n", job_table[i].id, state_str, job_table[i].command);
        }
    }

    // Remove jobs that completed after they have been printed
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id != 0 && job_table[i].state == JOB_DONE) {
            job_remove(job_table[i].id);
        }
    }
}

int fg_command(int job_id) {
    jobs_update_status();
    job_t *job = job_find(job_id);
    if (!job) {
        fprintf(stderr, "fg: job %d not found\n", job_id);
        return -1;
    }

    printf("%s\n", job->command);
    fflush(stdout);

    if (job->state == JOB_STOPPED) {
        if (kill(-job->pgid, SIGCONT) < 0) {
            perror("kill (SIGCONT)");
            return -1;
        }
        job_continue(job);
    }

    if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, job->pgid);
    }
    set_foreground_pgid(job->pgid);

    int status = 0;
    int job_stopped = 0;
    pid_t wpid;

    while ((wpid = waitpid(-job->pgid, &status, WUNTRACED)) > 0) {
        if (WIFSTOPPED(status)) {
            job_stopped = 1;
            break;
        }
    }

    if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, getpgrp());
    }
    set_foreground_pgid(0);

    if (job_stopped) {
        job_stop(job);
        printf("\n[%d]  Stopped    %s\n", job->id, job->command);
    } else {
        job_done(job);
        job_remove(job->id);
    }

    return 0;
}

int bg_command(int job_id) {
    jobs_update_status();
    job_t *job = job_find(job_id);
    if (!job) {
        fprintf(stderr, "bg: job %d not found\n", job_id);
        return -1;
    }

    if (job->state == JOB_STOPPED) {
        if (kill(-job->pgid, SIGCONT) < 0) {
            perror("kill (SIGCONT)");
            return -1;
        }
        job_continue(job);
        size_t len = strlen(job->command);
        while (len > 0 && (job->command[len - 1] == ' ' || job->command[len - 1] == '\t')) {
            len--;
        }
        if (len > 0 && job->command[len - 1] == '&') {
            printf("[%d]  %s\n", job->id, job->command);
        } else {
            printf("[%d]  %s &\n", job->id, job->command);
        }
    } else {
        fprintf(stderr, "bg: job %d already running\n", job_id);
    }

    return 0;
}
