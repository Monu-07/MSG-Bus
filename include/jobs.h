#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_JOBS 64

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} job_state_t;

typedef struct job {
    int id;             /* Job ID (1, 2, 3...) */
    pid_t pgid;         /* Process group ID */
    job_state_t state;  /* JOB_RUNNING, JOB_STOPPED, JOB_DONE */
    char *command;      /* Command line string */
} job_t;

void jobs_init(void);
job_t *job_add(pid_t pgid, const char *command, job_state_t state);
job_t *job_find(int id);
job_t *job_find_by_pgid(pid_t pgid);
int job_remove(int id);
void jobs_print(void);
void job_stop(job_t *job);
void job_continue(job_t *job);
void job_done(job_t *job);

void jobs_update_status(void);
int jobs_get_most_recent_id(void);
int jobs_get_most_recent_stopped_id(void);
int fg_command(int job_id);
int bg_command(int job_id);
void set_foreground_pgid(pid_t pgid);
pid_t get_foreground_pgid(void);

#endif /* JOBS_H */
