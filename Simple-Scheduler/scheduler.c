#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

static SharedState *shared_state = NULL;
static int num_cpu;
static int time_slice_ms;            // milliseconds
static int running_job_indices[MAX_JOBS];
static int num_running_jobs = 0;
static int current_time_slice = 0;
static volatile int exit_requested = 0;

static void cleanup_child_processes(void);

static void sigterm_handler(int signum) {
    if(signum==SIGTERM) {
        exit_requested=1;
    }
}

void enqueue(SharedState *S, int idx) {
    S->ready_q[S->rq_tail] = idx;
    S->rq_tail = (S->rq_tail + 1) % MAX_JOBS;
    S->rq_size++;
}

int dequeue(SharedState *S) {
    if (S->rq_size == 0) return -1;
    int idx = S->ready_q[S->rq_head];
    S->rq_head = (S->rq_head + 1) % MAX_JOBS;
    S->rq_size--;
    return idx;
}

static void check_for_new_jobs(void) {
    while (shared_state->njq_size > 0 && shared_state->job_count < MAX_JOBS) {
        char *path = shared_state->new_job_q[shared_state->njq_head];
        shared_state->njq_head = (shared_state->njq_head + 1) % MAX_JOBS;
        shared_state->njq_size--;

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            continue;
        }

        if (pid == 0) { 
            signal(SIGINT, SIG_DFL);
            signal(SIGTERM, SIG_DFL);

            char *argv[2];
            argv[0] = path;
            argv[1] = NULL;

            execvp(argv[0], argv);
            perror("execvp failed");
            _exit(127);
        }

        // Stop child until scheduled
        kill(pid, SIGSTOP);

        // Initialize job struct
        int idx = shared_state->job_count++;
        Job *j = &shared_state->jobs[idx];
        j->pid = pid;
        strncpy(j->name, path, sizeof(j->name) - 1);
        j->name[sizeof(j->name) - 1] = '\0';
        j->state = READY;
        j->started = 0;
        j->completion_slice = 0;
        j->slices_ran = 0;
        j->slices_waited = 0;
        j->submission_slice = current_time_slice;

        enqueue(shared_state, idx);
    }
}


static void handle_time_slice(void) {
    current_time_slice++;

    int currently_running_count = num_running_jobs;
    num_running_jobs = 0;

    for (int i = 0; i < currently_running_count; i++) {
        int job_idx = running_job_indices[i];
        Job *j = &shared_state->jobs[job_idx];

        j->slices_ran++;
        kill(j->pid, SIGSTOP);

        int status;
        pid_t r = waitpid(j->pid, &status, WNOHANG);

        if (r == j->pid || (kill(j->pid, 0) == -1 && errno == ESRCH)) {
            j->state = DONE;
            j->completion_slice = current_time_slice;
        } else {
            j->state = READY;
            enqueue(shared_state, job_idx);
        }
    }

    while (num_running_jobs < num_cpu && shared_state->rq_size > 0) {
        int idx_to_run = dequeue(shared_state);
        if (idx_to_run == -1) break;

        Job *j = &shared_state->jobs[idx_to_run];
        if (j->state == DONE) continue;

        kill(j->pid, SIGCONT);
        j->state = RUNNING;
        j->started = 1;
        running_job_indices[num_running_jobs++] = idx_to_run;
    }

    for (int i = 0; i < shared_state->rq_size; i++) {
        int idx_in_queue = shared_state->ready_q[(shared_state->rq_head + i) % MAX_JOBS];
        shared_state->jobs[idx_in_queue].slices_waited++;
    }

}

void run_scheduler(SharedState *S, int NCPU, int TSLICE) {
    shared_state = S;
    num_cpu = NCPU;
    time_slice_ms = TSLICE;

    struct sigaction sa = {0};
    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);

    while (!exit_requested) {
        check_for_new_jobs();

        if (num_running_jobs == 0 && shared_state->rq_size == 0 && shared_state->njq_size == 0) {
            usleep(TSLICE * 1000);
            continue;
        }

        handle_time_slice();
        usleep(TSLICE * 1000);
    }

    cleanup_child_processes();
}



static void cleanup_child_processes(void) {
    if (!shared_state) return;

    for (int i = 0; i < shared_state->job_count; i++) {
        if (shared_state->jobs[i].state != DONE) {
            kill(shared_state->jobs[i].pid, SIGKILL);
            waitpid(shared_state->jobs[i].pid, NULL, 0);
            shared_state->jobs[i].state = DONE;
            shared_state->jobs[i].completion_slice = current_time_slice;
        }
    }
}

void print_report(SharedState *S, int TSLICE) {
    printf("\nExecution Report:\n");
    printf("%-20s\t%-10s\t%-15s\t\t%-15s\n", "Name", "PID", "Turnaround Time", "Wait Time");

    for (int i = 0; i < S->job_count; i++) {
        Job j = S->jobs[i];
        int wait_time_ms = j.slices_waited ;
        int turnaround_time_ms;

        if (j.state == DONE) {
            turnaround_time_ms = (j.completion_slice - j.submission_slice);
            if (turnaround_time_ms < 0 || turnaround_time_ms > 60000)
                turnaround_time_ms = j.slices_ran ;
        } else {
            turnaround_time_ms = j.slices_ran;
        }

        printf("%-20s\t%-10d\t%-5d TSLICES\t\t%-5d TSLICES\n",
               j.name, j.pid, turnaround_time_ms, wait_time_ms);
    }
    fflush(stdout);
}

