#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"
#include "jobs.h"


/* =========================================================
   BUILTIN: cd
   ========================================================= */

static int builtin_cd(command_t *cmd)
{
    const char *directory;

    /*
     * cd with no argument
     * changes to the user's HOME directory.
     */
    if (cmd->argc == 1)
    {
        directory = getenv("HOME");

        if (directory == NULL)
        {
            fprintf(stderr,
                    "cd: HOME not set\n");

            return -1;
        }
    }
    else if (cmd->argc == 2)
    {
        /*
         * cd has one directory argument.
         */
        directory = cmd->argv[1];
    }
    else
    {
        /*
         * Too many arguments.
         */
        fprintf(stderr,
                "cd: too many arguments\n");

        return -1;
    }

    /*
     * Change the current working directory.
     */
    if (chdir(directory) != 0)
    {
        perror("cd");

        return -1;
    }

    return 0;
}


/* =========================================================
   BUILTIN: pwd
   ========================================================= */

static int builtin_pwd(command_t *cmd)
{
    char current_directory[4096];

    /*
     * pwd does not need arguments.
     */
    if (cmd->argc > 1)
    {
        fprintf(stderr,
                "pwd: too many arguments\n");

        return -1;
    }

    /*
     * Get current working directory.
     */
    if (getcwd(current_directory,
               sizeof(current_directory)) == NULL)
    {
        perror("pwd");

        return -1;
    }

    /*
     * Display current directory.
     */
    printf("%s\n", current_directory);

    return 0;
}


/* =========================================================
   BUILTIN: echo
   ========================================================= */

static int builtin_echo(command_t *cmd)
{
    /*
     * Start from argv[1].
     *
     * argv[0] contains "echo".
     */
    for (int i = 1; i < cmd->argc; i++)
    {
        printf("%s", cmd->argv[i]);

        /*
         * Print a space between arguments.
         */
        if (i < cmd->argc - 1)
        {
            printf(" ");
        }
    }
    printf("\n");

    return 0;
}


/* =========================================================
   BUILTIN: exit
   ========================================================= */

static int builtin_exit(command_t *cmd)
{
    /*
     * Basic version:
     *
     * exit
     */
    if (cmd->argc > 1)
    {
        fprintf(stderr,
                "exit: too many arguments\n");

        return -1;
    }

    /*
     * Tell the main shell loop to terminate.
     */
    return 1;
}


/* =========================================================
   BUILTIN: jobs
   ========================================================= */

static int builtin_jobs(command_t *cmd)
{
    (void)cmd;
    jobs_print();
    return 0;
}


/* =========================================================
   BUILTIN: fg
   ========================================================= */

static int builtin_fg(command_t *cmd)
{
    int job_id = -1;
    if (cmd->argc < 2)
    {
        job_id = jobs_get_most_recent_id();
        if (job_id <= 0)
        {
            fprintf(stderr, "fg: current: no such job\n");
            return -1;
        }
    }
    else
    {
        const char *arg = cmd->argv[1];
        if (arg[0] == '%')
        {
            arg++;
        }
        job_id = atoi(arg);
        if (job_id <= 0)
        {
            fprintf(stderr, "fg: invalid job id: %s\n", cmd->argv[1]);
            return -1;
        }
    }
    return fg_command(job_id);
}


/* =========================================================
   BUILTIN: bg
   ========================================================= */

static int builtin_bg(command_t *cmd)
{
    int job_id = -1;
    if (cmd->argc < 2)
    {
        job_id = jobs_get_most_recent_stopped_id();
        if (job_id <= 0)
        {
            fprintf(stderr, "bg: current: no such job\n");
            return -1;
        }
    }
    else
    {
        const char *arg = cmd->argv[1];
        if (arg[0] == '%')
        {
            arg++;
        }
        job_id = atoi(arg);
        if (job_id <= 0)
        {
            fprintf(stderr, "bg: invalid job id: %s\n", cmd->argv[1]);
            return -1;
        }
    }
    return bg_command(job_id);
}


/* =========================================================
   CHECK WHETHER COMMAND IS A BUILTIN
   ========================================================= */

int is_builtin(const command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
    {
        return 0;
    }

    if (strcmp(cmd->argv[0], "cd") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "pwd") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "echo") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "exit") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "jobs") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "fg") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "bg") == 0)
        return 1;

    return 0;
}


/* =========================================================
   EXECUTE BUILTIN
   ========================================================= */

int execute_builtin(command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
    {
        return -1;
    }

    if (strcmp(cmd->argv[0], "cd") == 0)
    {
        return builtin_cd(cmd);
    }

    if (strcmp(cmd->argv[0], "pwd") == 0)
    {
        return builtin_pwd(cmd);
    }

    if (strcmp(cmd->argv[0], "echo") == 0)
    {
        return builtin_echo(cmd);
    }

    if (strcmp(cmd->argv[0], "exit") == 0)
    {
        return builtin_exit(cmd);
    }

    if (strcmp(cmd->argv[0], "jobs") == 0)
    {
        return builtin_jobs(cmd);
    }

    if (strcmp(cmd->argv[0], "fg") == 0)
    {
        return builtin_fg(cmd);
    }

    if (strcmp(cmd->argv[0], "bg") == 0)
    {
        return builtin_bg(cmd);
    }

    return -1;
}
