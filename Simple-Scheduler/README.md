# OS-Assignment-3
**Authors:** Nalin Gupta, Tanish Jindal  
**Course:** CSE231: Operating System **(IIITD)**

## Table of Contents
- [Introduction](#introduction)
- [System Design](#system-design)
- [Implementation Details](#implementation-details)
- [Capabilities](#capabilities)
- [Limitations](#limitations)
- [Compilation and Execution](#compilation-and-execution)
- [Design Decisions](#design-decisions)
- [AI Generated Code Snippets](#ai-generated-code-snippets)
- [Contributions](#contributions)
- [References](#references)

## Introduction

This document provides a comprehensive overview of the **SimpleScheduler** project. The goal of Assignment 3 was to implement a process scheduler in C that manages multiple jobs using a **round-robin scheduling policy** with configurable CPU resources (NCPU) and time slices (TSLICE). The scheduler operates as a daemon process, coordinating job execution through shared memory and signal-based process control.

## System Design

### Overall Architecture

The SimpleScheduler system consists of three main components:

- **SimpleShell**: A command-line interface that accepts job submissions from users and manages the scheduler daemon process.
- **Scheduler Daemon**: A background process that implements round-robin scheduling, managing the ready queue and dispatching jobs to available CPU resources.
- **Job Processes**: User-submitted executables that are controlled by the scheduler through `SIGSTOP` and `SIGCONT` signals.

Communication between the shell and scheduler occurs via **shared memory** (`mmap`), which contains job metadata, ready queue, and a new job submission queue.

### Modules Implemented

- **Shared Memory Management**: Uses `mmap()` with `MAP_SHARED` and `MAP_ANONYMOUS` flags to create a shared memory region accessible by both the shell and scheduler processes.
- **Job Submission Queue**: A circular queue (`new_job_q`) where the shell enqueues job paths for the scheduler to process.
- **Ready Queue**: A circular queue managing jobs that are ready to execute, implementing FIFO ordering for round-robin scheduling.
- **Signal-Based Process Control**: Uses `SIGSTOP` to pause jobs and `SIGCONT` to resume them, allowing fine-grained control over execution.
- **Time Slice Management**: The scheduler operates in discrete time slices, preempting running jobs and scheduling new ones at each interval.
- **Execution Reporting**: Tracks turnaround time, wait time, and completion status for all submitted jobs.

## Implementation Details

### Shared State Structure

The `SharedState` structure defined in `scheduler.h` serves as the communication channel between the shell and scheduler:

```c
typedef struct {
    Job jobs[MAX_JOBS];
    int job_count;
    
    // Ready queue for jobs waiting for CPU time
    int ready_q[MAX_JOBS];
    int rq_head, rq_tail, rq_size;
    
    // New job submission queue
    char new_job_q[MAX_JOBS][256];
    int njq_head, njq_tail, njq_size;
} SharedState;
```

Each `Job` structure tracks:
- **pid**: Process identifier
- **name**: Executable path
- **state**: READY, RUNNING, or DONE
- **submission_slice**: When the job was submitted
- **completion_slice**: When the job finished
- **slices_ran**: Total time slices executed
- **slices_waited**: Total time slices spent waiting

### Job Submission Process

When a user submits a job via `submit <path>`:

1. The shell validates that the submission queue is not full and adds the job path to `new_job_q`.
2. The scheduler's `check_for_new_jobs()` function dequeues the job path.
3. A new process is created via `fork()` and `execvp()`.
4. The new process is immediately stopped with `SIGSTOP` before it executes user code.
5. Job metadata is initialized and the job index is added to the ready queue.

This approach ensures that jobs do not begin execution until the scheduler explicitly grants them CPU time.

### The dummy_main.h Header

User programs must include `dummy_main.h`, which contains:

```c
int dummy_main(int argc, char **argv);

int main(int argc, char **argv) {
    // Stop immediately so scheduler can control execution
    raise(SIGSTOP);
    
    int ret = dummy_main(argc, argv);
    return ret;
}

#define main dummy_main
```

This header performs two critical functions:

1. **Macro Redefinition**: The `#define main dummy_main` directive renames the user's `main()` function to `dummy_main()`, allowing the header to provide its own `main()`.
2. **Self-Stopping**: The injected `main()` calls `raise(SIGSTOP)` immediately, ensuring the process stops itself before executing any user code. This gives the scheduler complete control over when execution begins.

### Round-Robin Scheduling Algorithm

The scheduler implements classic round-robin scheduling through the `handle_time_slice()` function:

1. **Preemption**: All currently running jobs are sent `SIGSTOP` signals.
2. **Completion Check**: The scheduler uses `waitpid()` with `WNOHANG` to detect if any running job has terminated. Jobs that have finished are marked as DONE.
3. **Re-queuing**: Jobs that are still alive are moved back to the ready queue.
4. **Dispatching**: Up to NCPU jobs are dequeued from the ready queue and sent `SIGCONT` signals to resume execution.
5. **Wait Time Accounting**: Jobs remaining in the ready queue have their `slices_waited` counter incremented.

The scheduler sleeps for TSLICE milliseconds between each time slice, creating the illusion of concurrent execution through rapid context switching.

### Idle State Handling

When the scheduler has no jobs to run (no running jobs, empty ready queue, and empty submission queue), it enters an idle state:
- The scheduler sleeps for TSLICE milliseconds without incrementing the global time slice counter.
- This prevents artificial inflation of turnaround and wait times when the system is idle.
- As soon as a new job is submitted, the scheduler resumes normal operation.

### Termination

When the user types `exit`:

1. The shell waits up to 1 second for any pending jobs in the submission queue to be picked up by the scheduler.
2. A `SIGTERM` signal is sent to the scheduler process.
3. The scheduler completes its current time slice, then calls `cleanup_child_processes()`.
4. All remaining jobs are sent `SIGKILL` and reaped with `waitpid()`.
5. The shell prints the execution report showing turnaround time and wait time for all jobs.

### Execution Report

Upon termination, the shell displays:
- **Job Name**: The path of the submitted executable
- **PID**: Process identifier
- **Turnaround Time**: `(completion_slice - submission_slice) × TSLICE` milliseconds
- **Wait Time**: `slices_waited × TSLICE` milliseconds

For jobs that complete within a single time slice, turnaround time is still reported as `2 × TSLICE` as per the assignment specification.

## Capabilities

The SimpleScheduler successfully implements:

- ✅ **Configurable Parallelism**: Supports 1 to N CPUs through the NCPU parameter.
- ✅ **Round-Robin Scheduling**: Ensures fair CPU time distribution among all jobs.
- ✅ **Dynamic Job Submission**: Users can submit jobs at any time while the shell is running.
- ✅ **Signal-Based Process Control**: Fine-grained control over job execution without busy-waiting.
- ✅ **Performance Metrics**: Accurate tracking of turnaround time and wait time.
- ✅ **Shutdown**: Proper cleanup of all child processes and shared resources.

## Limitations

While SimpleScheduler demonstrates core scheduling concepts, it has several limitations:

- ❌ **No Priority Scheduling**: All jobs are treated equally; there is no support for priority levels or real-time scheduling.
- ❌ **No I/O Blocking Awareness**: The scheduler cannot distinguish between CPU-bound and I/O-bound jobs. A job performing I/O still consumes its full time slice.
- ❌ **Fixed Time Slice**: TSLICE is set at startup and cannot be adjusted dynamically.
- ❌ **No Job Control**: Users cannot pause, resume, or terminate individual jobs after submission.
- ❌ **No Command-Line Arguments**: Submitted jobs cannot receive command-line parameters.
- ❌ **No Standard Input**: Jobs with blocking calls like `scanf()` are not supported.
- ❌ **Memory Overhead**: All job metadata is stored in shared memory, limiting the maximum number of jobs to MAX_JOBS (100).

## Compilation and Execution

### Building the Project

The provided Makefile compiles both the scheduler and a test program:

```bash
make
```

This produces:
- **simple_shell**: The main scheduler executable
- **code**: A sample test program (compiled from code.c)

### Running the Scheduler

Launch the shell with desired parameters:

```bash
./simple_shell <NCPU> <TSLICE>
```

Example with 2 CPUs and 50ms time slices:

```bash
./simple_shell 2 50
```

### Submitting Jobs

At the `SimpleShell>` prompt:

```bash
SimpleShell> submit ./code
Job submitted: ./code
SimpleShell> submit ./another_program
Job submitted: ./another_program
SimpleShell> exit
```

The execution report will be displayed upon exit.

### Creating Test Programs

Any C program can be scheduled by including the `dummy_main.h` header:

```c
#include <stdio.h>
#include "dummy_main.h"

int main() {
    for (int i = 0; i < 1000000000; i++) {
        // CPU-intensive work
    }
    printf("Job completed!\n");
    return 0;
}
```

Compile with:

```bash
gcc -o code code.c
```

## Design Decisions

### Why Use Shared Memory?

Shared memory was chosen over pipes or message queues because:
- It provides low-latency, high-bandwidth communication between the shell and scheduler.
- Both processes can directly access and modify the shared data structure without copying.
- It simplifies synchronization since the scheduler is the only writer to most fields.

### Why Use SIGSTOP/SIGCONT?

These signals provide kernel-level process suspension:
- Jobs cannot ignore or handle these signals, ensuring scheduler control.
- Suspended processes consume no CPU time, allowing accurate time slice accounting.

### Why Use a Circular Queue?

The ready queue and submission queue use circular buffers because:
- Constant-time O(1) enqueue and dequeue operations.
- Memory-efficient: No dynamic allocation required.
- Cache-friendly: Linear memory layout improves performance.

## AI Generated Code Snippets

During the development of SimpleScheduler, several code snippets were generated with AI assistance to handle specific implementation challenges:

### Snippet 1: Cleanup Child Processes

```c
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
```

**Explanation:** This function performs cleanup when the scheduler terminates. It iterates through all jobs in the shared state and forcefully terminates any that haven't completed naturally. The `SIGKILL` signal is used because it cannot be ignored or caught, ensuring immediate termination. Each terminated process is then reaped with `waitpid()` to prevent zombie processes. Finally, job metadata is updated to reflect completion at the current time slice.

### Snippet 2: Job Completion Detection

```c
pid_t r = waitpid(j->pid, &status, WNOHANG);
if (r == j->pid || (kill(j->pid, 0) == -1 && errno == ESRCH)) {
    j->state = DONE;
    j->completion_slice = current_time_slice;
}
```

**Explanation:** This code detects whether a job has completed execution. It uses `waitpid()` with the `WNOHANG` flag to check for state changes without blocking. If the process has terminated, `waitpid()` returns its PID. As a fallback, the code sends signal 0 to the process (which doesn't actually deliver a signal but checks existence). If `kill()` returns -1 with errno set to `ESRCH` (No such process), the process has definitely terminated. This dual-check approach ensures reliable completion detection even if the job terminates between time slices.

### Snippet 3: Signal Disposition Reset

```c
signal(SIGINT, SIG_DFL);
signal(SIGTERM, SIG_DFL);
```

**Explanation:** When forking child processes (both for the scheduler daemon and user jobs), signal handlers are inherited from the parent. This code resets `SIGINT` and `SIGTERM` to their default dispositions (`SIG_DFL`) in child processes. For user jobs, this ensures they can be interrupted normally. For the scheduler daemon before calling `run_scheduler()`, custom handlers are installed after this reset. This prevents unwanted signal handling behavior in child processes.

### Snippet 4: Exit Flag Declaration

```c
static volatile int exit_requested = 0;
```

**Explanation:** This declares a global flag used to coordinate graceful shutdown of the scheduler. The `volatile` keyword is critical here because the variable is modified by a signal handler (when `SIGTERM` is received) and read by the main scheduler loop. Without `volatile`, the compiler might optimize away repeated reads of the variable, causing the loop to never detect the shutdown request. The `volatile` qualifier ensures the value is always fetched from memory, guaranteeing the signal handler's modification is visible to the main thread.

### Snippet 5: SharedState Structure Definition

```c
typedef struct {
    Job jobs[MAX_JOBS];
    int job_count;
    int ready_q[MAX_JOBS];
    int rq_head, rq_tail, rq_size;
    char new_job_q[MAX_JOBS][256];
    int njq_head, njq_tail, njq_size;
} SharedState;
```
**Explanation:** This structure defines the shared memory layout used by both the SimpleShell and Scheduler Daemon processes. It maintains global state for job management and inter-process communication. The jobs array stores metadata for all submitted jobs, including process identifiers, states, and timing information, while `job_count` keeps track of the total number of active jobs in the system. The `ready_q` array represents a circular queue of job indices that are ready for CPU execution, managed through the `rq_head`, `rq_tail`, and `rq_size` fields. Similarly, the `new_job_q` queue temporarily stores paths of newly submitted jobs from the shell, controlled by `njq_head`, `njq_tail`, and `njq_size`. This shared structure allows the shell and scheduler to communicate efficiently through a single memory-mapped region created with `mmap()` using the `MAP_SHARED` flag, ensuring all modifications are immediately visible across processes without requiring explicit message passing.

## Contributions

- **Nalin Gupta**: Implemented shell interface, Signal handling, Execution reporting, and Documentation.
- **Tanish Jindal**: Implemented scheduler daemon, Shared memory management, Time slice management, Job submission queue.

## References

### Course Materials
- Lecture Slides 12, 13
- Assignment 2 codebase (SimpleShell)

### System Calls

**Process Control:**
- [fork](https://man7.org/linux/man-pages/man2/fork.2.html) - Create a child process
- [execvp](https://man7.org/linux/man-pages/man3/exec.3.html) - Execute a program
- [waitpid](https://man7.org/linux/man-pages/man2/wait.2.html) - Wait for process to change state
- [kill](https://man7.org/linux/man-pages/man2/kill.2.html) - Send signal to a process
- [raise](https://man7.org/linux/man-pages/man3/raise.3.html) - Send signal to the calling process
- [getpid](https://man7.org/linux/man-pages/man2/getpid.2.html) - Get process identification

**Signal Handling:**
- [signal](https://man7.org/linux/man-pages/man2/signal.2.html) - ANSI C signal handling
- [sigaction](https://man7.org/linux/man-pages/man2/sigaction.2.html) - Examine and change signal action

**Memory Management:**
- [mmap](https://man7.org/linux/man-pages/man2/mmap.2.html) - Map files or devices into memory
- [munmap](https://man7.org/linux/man-pages/man2/munmap.2.html) - Unmap files or devices from memory

**Time Functions:**
- [usleep](https://man7.org/linux/man-pages/man3/usleep.3.html) - Suspend execution for microsecond intervals

### Library Functions

**String Operations:**
- [strncpy](https://man7.org/linux/man-pages/man3/strncpy.3.html) - Copy a string with length limit
- [strcmp](https://man7.org/linux/man-pages/man3/strcmp.3.html) - Compare two strings
- [strtok](https://man7.org/linux/man-pages/man3/strtok.3.html) - Extract tokens from strings
- [strlen](https://man7.org/linux/man-pages/man3/strlen.3.html) - Calculate the length of a string

**Standard I/O:**
- [fprintf](https://man7.org/linux/man-pages/man3/fprintf.3.html) - Formatted output to a stream
- [fgets](https://man7.org/linux/man-pages/man3/fgets.3.html) - Input of strings
- [fflush](https://man7.org/linux/man-pages/man3/fflush.3.html) - Flush a stream

**Memory:**
- [memset](https://man7.org/linux/man-pages/man3/memset.3.html) - Fill memory with a constant byte

**Utility:**
- [atoi](https://man7.org/linux/man-pages/man3/atoi.3.html) - Convert a string to an integer

### Documentation
- [Linux man pages](https://man7.org/linux/man-pages/) - Complete system call and library documentation
- [GNU C Library Reference Manual](https://www.gnu.org/software/libc/manual/)
