#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include "scheduler.h"

#define MAX_LINE 1024

static pid_t sched_pid = -1;
static SharedState *g_shared_state = NULL;
static int g_tslice = 0;

void parse_command(char *line, char **args) {
    char *newline = strchr(line, '\n');
    if (newline) *newline = '\0';

    int i = 0;
    char *token = strtok(line, " \t");
    while (token != NULL && i < MAX_LINE / 2) {
        args[i++] = token;
        token = strtok(NULL, " \t");
    }
    args[i] = NULL;
}

void submit_job(SharedState *S, const char *path) {
    if (S->njq_size >= MAX_JOBS) {
        printf("Error: Job submission queue is full.\n");
        return;
    }
    if (S->job_count + S->njq_size >= MAX_JOBS) {
        printf("Error: Maximum total jobs (%d) will be exceeded.\n", MAX_JOBS);
        return;
    }

    strncpy(S->new_job_q[S->njq_tail], path, sizeof(S->new_job_q[0]) - 1);
    S->new_job_q[S->njq_tail][sizeof(S->new_job_q[0]) - 1] = '\0';

    S->njq_tail = (S->njq_tail + 1) % MAX_JOBS;
    S->njq_size++;

    printf("Job submitted: %s\n", path);
}

void cleanup_and_exit() {
    if (sched_pid > 0) {
        int waited_ms = 0;
        while (g_shared_state && g_shared_state->njq_size > 0 && waited_ms < 1000) {
            usleep(100 * 1000); // 100 ms
            waited_ms += 100;
        }

        kill(sched_pid, SIGTERM);

        usleep(200 * 1000);

        waitpid(sched_pid, NULL, 0);
    }

    if (g_shared_state != NULL) {
        print_report(g_shared_state, g_tslice);
        munmap(g_shared_state, sizeof(SharedState));
        g_shared_state = NULL;
    }
}

void sigint_handler(int signum) {
    if(signum==SIGINT) {
        printf("\nCaught SIGINT. Exiting\n");
        cleanup_and_exit();
        exit(0);  
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <NCPU> <TSLICE(ms)>\n", argv[0]);
        return 1;
    }

    int NCPU = atoi(argv[1]);
    int TSLICE = atoi(argv[2]);
    g_tslice = TSLICE;

    if (NCPU <= 0 || TSLICE <= 0) {
        fprintf(stderr, "Error: NCPU and TSLICE must be positive integers\n");
        return 1;
    }

    SharedState *S = mmap(NULL, sizeof(SharedState),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (S == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }
    memset(S, 0, sizeof(SharedState));
    g_shared_state = S;

    struct sigaction sa = {0};
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    sched_pid = fork();
    if (sched_pid < 0) {
        perror("fork for scheduler failed");
        munmap(S, sizeof(SharedState));
        return 1;
    }

    if (sched_pid == 0) {
        signal(SIGINT, SIG_DFL);
        run_scheduler(S, NCPU, TSLICE);
        _exit(0);
    }

    char input_line[MAX_LINE];
    char *args[MAX_LINE / 2 + 1];

    printf("Simple Job Scheduler Shell\n");
    printf("Commands: submit <path>, exit \n\n");

    while (1) {
        printf("SimpleShell$ ");
        fflush(stdout);

        if (!fgets(input_line, MAX_LINE, stdin)) {
            printf("\nExiting...\n");
            cleanup_and_exit();
            break;
        }

        parse_command(input_line, args);

        if (args[0] == NULL) continue;

        if (strcmp(args[0], "exit") == 0) {
            cleanup_and_exit();
            break;
        }
        else if (strcmp(args[0], "submit") == 0) {
            if (args[1] == NULL) {
                printf("Usage: submit <path_to_executable>\n");
            } else {
                submit_job(S, args[1]);
            }
        }
        else {
            printf("Unknown command");
        }
    }
    return 0;
}
