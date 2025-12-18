#include <sys/types.h>

#define MAX_JOBS 100

// Job states
#define READY   0
#define RUNNING 1
#define DONE    2

typedef struct {
    pid_t pid;
    char name[256];
    int state;             // READY, RUNNING, DONE
    int started;           // 0 = never started, 1 = already started
    int completion_slice;  // slice when finished
    int slices_ran;
    int submission_slice;
    int slices_waited;    // total slices spent waiting
} Job;

typedef struct {
    Job jobs[MAX_JOBS];
    int job_count;

    int ready_q[MAX_JOBS];
    int rq_head, rq_tail, rq_size;

    char new_job_q[MAX_JOBS][256];
    int njq_head, njq_tail, njq_size;

} SharedState;

// Queue operations
void enqueue(SharedState *S, int idx);
int dequeue(SharedState *S);

// Scheduler
void run_scheduler(SharedState *S, int NCPU, int TSLICE);
void print_report(SharedState *S, int TSLICE);
