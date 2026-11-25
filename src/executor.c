#define _POSIX_C_SOURCE 200809L
#include "executor.h"
#include "builtin.h"
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

static char current_cmd_line[1024] = "";

void executor_set_command_line(const char *cmd_line) {
    if (cmd_line) {
        strncpy(current_cmd_line, cmd_line, sizeof(current_cmd_line) - 1);
        current_cmd_line[sizeof(current_cmd_line) - 1] = '\0';
    } else {
        current_cmd_line[0] = '\0';
    }
}

static void reap_zombies(void) {
    jobs_update_status();
}

static int setup_redirection(const command_t *cmd) {
    if (cmd->input_file != NULL) {
        int fd_in = open(cmd->input_file, O_RDONLY);
        if (fd_in < 0) {
            perror(cmd->input_file);
            return -1;
        }
        if (dup2(fd_in, STDIN_FILENO) < 0) {
            perror("dup2");
            close(fd_in);
            return -1;
        }
        close(fd_in);
    }

    if (cmd->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT | (cmd->append_mode ? O_APPEND : O_TRUNC);
        int fd_out = open(cmd->output_file, flags, 0644);
        if (fd_out < 0) {
            perror(cmd->output_file);
            return -1;
        }
        if (dup2(fd_out, STDOUT_FILENO) < 0) {
            perror("dup2");
            close(fd_out);
            return -1;
        }
        close(fd_out);
    }

    return 0;
}

static int execute_single_builtin(command_t *cmd) {
    int saved_stdin = -1;
    int saved_stdout = -1;

    if (cmd->input_file != NULL) {
        saved_stdin = dup(STDIN_FILENO);
        if (saved_stdin < 0) {
            perror("dup");
            return -1;
        }
        int fd_in = open(cmd->input_file, O_RDONLY);
        if (fd_in < 0) {
            perror(cmd->input_file);
            close(saved_stdin);
            return -1;
        }
        if (dup2(fd_in, STDIN_FILENO) < 0) {
            perror("dup2");
            close(fd_in);
            close(saved_stdin);
            return -1;
        }
        close(fd_in);
    }

    if (cmd->output_file != NULL) {
        saved_stdout = dup(STDOUT_FILENO);
        if (saved_stdout < 0) {
            perror("dup");
            if (saved_stdin != -1) {
                dup2(saved_stdin, STDIN_FILENO);
                close(saved_stdin);
            }
            return -1;
        }
        int flags = O_WRONLY | O_CREAT | (cmd->append_mode ? O_APPEND : O_TRUNC);
        int fd_out = open(cmd->output_file, flags, 0644);
        if (fd_out < 0) {
            perror(cmd->output_file);
            close(saved_stdout);
            if (saved_stdin != -1) {
                dup2(saved_stdin, STDIN_FILENO);
                close(saved_stdin);
            }
            return -1;
        }
        if (dup2(fd_out, STDOUT_FILENO) < 0) {
            perror("dup2");
            close(fd_out);
            close(saved_stdout);
            if (saved_stdin != -1) {
                dup2(saved_stdin, STDIN_FILENO);
                close(saved_stdin);
            }
            return -1;
        }
        close(fd_out);
    }

    int result = execute_builtin(cmd);

    if (saved_stdin != -1) {
        if (dup2(saved_stdin, STDIN_FILENO) < 0) {
            perror("dup2 restore stdin");
        }
        close(saved_stdin);
    }

    if (saved_stdout != -1) {
        if (dup2(saved_stdout, STDOUT_FILENO) < 0) {
            perror("dup2 restore stdout");
        }
        close(saved_stdout);
    }

    return result;
}

static int execute_single_command(command_t *cmd) {
    if (cmd == NULL || cmd->argc == 0) {
        return 0;
    }

    // Built-in commands execute directly in the parent process
    if (is_builtin(cmd)) {
        return execute_single_builtin(cmd);
    }

    // External command: fork child, execvp, parent waitpid
    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // Child process creates its own process group
        setpgid(0, 0);

        signal(SIGTTOU, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGINT, SIG_DFL);

        if (setup_redirection(cmd) < 0) {
            exit(1);
        }
        execvp(cmd->argv[0], cmd->argv);
        perror(cmd->argv[0]);
        exit(127);
    } else {
        // Parent process avoids race condition by also setting child PGID
        setpgid(pid, pid);

        if (cmd->background) {
            job_t *job = job_add(pid, current_cmd_line, JOB_RUNNING);
            if (job) {
                printf("[%d] %d\n", job->id, pid);
            } else {
                printf("[1] %d\n", pid);
            }
            return 0;
        }

        if (isatty(STDIN_FILENO)) {
            tcsetpgrp(STDIN_FILENO, pid);
        }
        set_foreground_pgid(pid);

        int status = 0;
        if (waitpid(pid, &status, WUNTRACED) < 0) {
            perror("waitpid");
            status = -1;
        }

        if (isatty(STDIN_FILENO)) {
            tcsetpgrp(STDIN_FILENO, getpgrp());
        }
        set_foreground_pgid(0);

        if (WIFSTOPPED(status)) {
            job_t *job = job_add(pid, current_cmd_line, JOB_STOPPED);
            if (job) {
                printf("\n[%d]  Stopped    %s\n", job->id, job->command);
            }
            return 148;
        }

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            return 128 + WTERMSIG(status);
        }
        return 0;
    }
}

int execute_command(command_t *cmd) {
    reap_zombies();
    return execute_single_command(cmd);
}

int execute_pipeline(pipeline_t *pipeline) {
    reap_zombies();

    if (pipeline == NULL || pipeline->command_count == 0) {
        return 0;
    }

    int N = pipeline->command_count;

    // Single command execution
    if (N == 1) {
        return execute_single_command(&pipeline->commands[0]);
    }

    // Multi-command pipeline: N >= 2 commands connected by pipes
    int pipes[MAX_COMMANDS][2];
    for (int i = 0; i < N - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return -1;
        }
    }

    pid_t pids[MAX_COMMANDS];
    pid_t pgid = 0;

    fflush(NULL);
    for (int i = 0; i < N; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            for (int j = 0; j < N - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            for (int j = 0; j < i; j++) {
                waitpid(pids[j], NULL, 0);
            }
            return -1;
        }

        if (pids[i] == 0) {
            // In child: first child establishes PGID, others join
            if (i == 0) {
                setpgid(0, 0);
            } else {
                setpgid(0, pgid);
            }

            signal(SIGTTOU, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGINT, SIG_DFL);

            command_t *cmd = &pipeline->commands[i];

            // Connect stdin from previous pipe if not the first command
            if (i > 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                    perror("dup2");
                    exit(1);
                }
            }

            // Connect stdout to next pipe if not the last command
            if (i < N - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                    perror("dup2");
                    exit(1);
                }
            }

            // Command-specific redirection overrides pipeline stdin/stdout if present
            if (setup_redirection(cmd) < 0) {
                exit(1);
            }

            // Close all pipe file descriptors in child
            for (int j = 0; j < N - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Execute command: builtin or external
            if (is_builtin(cmd)) {
                int ret = execute_builtin(cmd);
                exit(ret == 0 ? 0 : 1);
            } else {
                execvp(cmd->argv[0], cmd->argv);
                perror(cmd->argv[0]);
                exit(127);
            }
        }

        // In parent: first child creates pgid, subsequent children join pgid
        if (i == 0) {
            pgid = pids[0];
            setpgid(pids[0], pids[0]);
        } else {
            setpgid(pids[i], pgid);
        }
    }

    // Parent process: close all pipe file descriptors immediately
    for (int i = 0; i < N - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // If pipeline is in the background, add to job table and return immediately
    int is_bg = pipeline->commands[N - 1].background;
    if (is_bg) {
        job_t *job = job_add(pgid, current_cmd_line, JOB_RUNNING);
        if (job) {
            printf("[%d] %d\n", job->id, pgid);
        } else {
            printf("[1] %d\n", pgid);
        }
        return 0;
    }

    if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, pgid);
    }
    set_foreground_pgid(pgid);

    // Wait for all child processes and capture exit status of the last command
    int last_status = 0;
    int pipeline_stopped = 0;
    for (int i = 0; i < N; i++) {
        int status = 0;
        if (waitpid(pids[i], &status, WUNTRACED) < 0) {
            perror("waitpid");
        }
        if (WIFSTOPPED(status)) {
            pipeline_stopped = 1;
        }
        if (i == N - 1) {
            last_status = status;
        }
    }

    if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, getpgrp());
    }
    set_foreground_pgid(0);

    if (pipeline_stopped) {
        job_t *job = job_add(pgid, current_cmd_line, JOB_STOPPED);
        if (job) {
            printf("\n[%d]  Stopped    %s\n", job->id, job->command);
        }
        return 148;
    }

    if (WIFEXITED(last_status)) {
        return WEXITSTATUS(last_status);
    } else if (WIFSIGNALED(last_status)) {
        return 128 + WTERMSIG(last_status);
    }

    return 0;
}
