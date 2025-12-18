# OS Assignment 5
## SimpleMultithreader - Lambda-Based Parallel Programming Library

**Authors:** Nalin Gupta, Tanish Jindal<br>
**Course:** CSE231: Operating Systems

---

## Introduction

This document provides an overview of the SimpleMultithreader project. The goal of the assignment was to abstract the complexity of pthread-based parallel programming into an easy-to-use, header-only library that supports C++11 lambda expressions. The SimpleMultithreader provides two `parallel_for` APIs that allow programmers to parallelize one-dimensional and two-dimensional loops with minimal code changes, eliminating the boilerplate code typically required for thread creation, work distribution, and synchronization.

Traditional pthread programming requires significant effort: creating thread structures, dividing work manually, passing arguments through void pointers, joining threads, and managing memory. SimpleMultithreader encapsulates all these operations behind clean functional interfaces, reducing code complexity by approximately 3x compared to raw pthread implementations while maintaining full control over thread count and execution timing.

---

## System Design

### Overall Architecture

The SimpleMultithreader system consists of the following components:

- **Lambda-Based API**: Two overloaded `parallel_for` functions that accept C++11 lambda expressions as loop bodies.
- **Work Distribution**: Automatic division of iteration space across threads with load balancing to handle non-divisible workloads.
- **Thread Management**: Dynamic creation and joining of pthreads without any thread pooling mechanism.
- **Main Thread Participation**: The calling thread executes work alongside newly created threads to avoid idle CPU cycles.
- **Timing Infrastructure**: Built-in execution time measurement for each `parallel_for` invocation.

The design follows a fork-join pattern: threads are created at each `parallel_for` call, execute their assigned work in parallel, and are joined before the function returns.

### Modules Implemented

- **1D Parallel Loop**: Parallelizes single-dimensional loops with signature `parallel_for(low, high, lambda, numThreads)`.
- **2D Parallel Loop**: Parallelizes nested loops with signature `parallel_for(low1, high1, low2, high2, lambda, numThreads)`.
- **Thread Argument Structures**: `thread_args_1d` and `thread_args_2d` structures that package work ranges and lambda references for thread functions.
- **Thread Functions**: `thread_func_1d` and `thread_func_2d` wrapper functions that execute lambda bodies over assigned iteration ranges.
- **Load Balancing**: Distributes remainder iterations evenly across threads when total work is not evenly divisible.
- **Lambda Demonstration**: Helper `demonstration` function showcasing lambda capture and usage.

---

## Implementation Details

### Data Structures

#### Thread Argument Structures

Two structures are defined to pass work assignments to threads:

```c
struct thread_args_1d {
    int low;
    int high;
    function<void(int)>* lambda; 
};

struct thread_args_2d {
    int low1;
    int high1; 
    int low2;
    int high2; 
    function<void(int, int)>* lambda;
};
```

These structures encapsulate iteration bounds and a pointer to the lambda function. Using a pointer to `std::function` avoids copying the lambda, which could be expensive if it captures large objects.

### Thread Wrapper Functions

#### 1D Thread Function

```c
void* thread_func_1d(void* arg) {
    thread_args_1d* data = (thread_args_1d*)(arg);
    for (int i = data->low; i < data->high; i++) {
        (*(data->lambda))(i);
    }
    return NULL;
}
```

This function serves as the pthread entry point for 1D loops. It casts the void pointer argument to `thread_args_1d*`, extracts the iteration range, and invokes the lambda for each index in the range.

#### 2D Thread Function

```c
void* thread_func_2d(void* arg) {
    thread_args_2d* data = (thread_args_2d*)(arg);
    for (int i = data->low1; i < data->high1; i++) {
        for (int j = data->low2; j < data->high2; ++j) {
            (*(data->lambda))(i, j);
        }
    }
    return NULL;
}
```

Similar to the 1D version, but executes nested loops for 2D parallelization. The outer loop range is divided across threads, while the inner loop executes completely for each outer iteration.

### Parallel For Implementation

#### 1D Parallel For

The complete implementation follows this structure:

```c
void parallel_for(int low, int high, function<void(int)>&& lambda, 
                  int numThreads) {
    // Start Timer
    struct timeval start, end;
    gettimeofday(&start, NULL);

    int total_items = high - low;
    if (numThreads <= 0) {
        numThreads = 1;
    }
    int num_newThreads = numThreads - 1;
    pthread_t* tids = nullptr;
    if (num_newThreads > 0) {
        tids = new pthread_t[num_newThreads];
    }
    thread_args_1d* args = new thread_args_1d[numThreads];

    // Dividing Work
    int chunk = total_items / numThreads;
    int remainder = total_items % numThreads;
    int curr_low = low;

    for (int i = 0; i < numThreads; i++) {
        int curr_size = chunk; 
        if (i < remainder) {
            curr_size = curr_size + 1;
        }
        args[i].low = curr_low;
        args[i].high = curr_low + curr_size;
        args[i].lambda = &lambda;

        if (i < num_newThreads) {
            pthread_create(&tids[i], NULL, thread_func_1d, &args[i]);
        }
        curr_low += curr_size;
    }

    // Main Thread Work
    thread_func_1d(&args[numThreads - 1]);

    // Wait for other threads
    for (int i = 0; i < num_newThreads; ++i) {
        pthread_join(tids[i], NULL);
    }

    // Stop Timer & Print
    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0;
    elapsed += (end.tv_usec - start.tv_usec) / 1000.0;
    cout << "parallel_for execution time: " << elapsed << " ms" << endl;

    // Cleanup
    delete[] tids;
    delete[] args;
}
```

**Key Implementation Details:**

1. **Work Distribution**: The iteration space is divided into `chunk = total_items / numThreads` iterations per thread. The remainder iterations (`total_items % numThreads`) are distributed one each to the first `remainder` threads, ensuring balanced load.

2. **Thread Creation**: Only `numThreads - 1` new threads are created via `pthread_create`, because the main thread will execute the last chunk of work.

3. **Main Thread Participation**: After creating worker threads, the main thread executes `thread_func_1d(&args[numThreads - 1])`, processing its assigned work chunk. This avoids leaving the main thread idle.

4. **Synchronization**: All worker threads are joined using `pthread_join`, ensuring all work completes before the function returns.

5. **Timing**: Execution time is measured from before thread creation to after all threads join, providing accurate parallel execution time.

#### 2D Parallel For

The 2D implementation follows a similar pattern but divides only the outer loop dimension:

```c
void parallel_for(int low1, int high1, int low2, int high2, 
                  function<void(int, int)>&& lambda, int numThreads) {
    // Start Timer
    struct timeval start, end;
    gettimeofday(&start, NULL);

    int total_rows = high1 - low1;
    if (numThreads <= 0) {
        numThreads = 1;
    }
    int num_newThreads = numThreads - 1;
    pthread_t* tids = nullptr;
    if (num_newThreads > 0) {
        tids = new pthread_t[num_newThreads];
    }
    thread_args_2d* args = new thread_args_2d[numThreads];

    // Dividing Rows
    int chunk = total_rows / numThreads;
    int remainder = total_rows % numThreads;
    int current_row = low1;

    for (int i = 0; i < numThreads; i++) {
        int row_size = chunk;
        if (i < remainder) {
            row_size++;
        }
        
        args[i].low1 = current_row;
        args[i].high1 = current_row + row_size;
        args[i].low2 = low2;
        args[i].high2 = high2;
        args[i].lambda = &lambda;

        if (i < num_newThreads) {
            pthread_create(&tids[i], NULL, thread_func_2d, &args[i]);
        }
        current_row += row_size;
    }

    // Main Thread Work
    thread_func_2d(&args[numThreads - 1]);

    // Wait for other threads
    for (int i = 0; i < num_newThreads; ++i) {
        pthread_join(tids[i], NULL);
    }

    // Stop Timer & Print
    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0;
    elapsed += (end.tv_usec - start.tv_usec) / 1000.0;
    cout << "parallel_for execution time: " << elapsed << " ms" << endl;

    // Cleanup
    delete[] tids;
    delete[] args;
}
```

The outer loop rows (`low1` to `high1`) are distributed across threads, while each thread executes the complete inner loop (`low2` to `high2`) for its assigned rows. This row-wise parallelization is common in matrix operations.

### Example Usage

#### Vector Addition (1D Loop)

From `vector.cpp`:

```c
int* A = new int[size];
int* B = new int[size];
int* C = new int[size];

std::fill(A, A+size, 1);
std::fill(B, B+size, 1);
std::fill(C, C+size, 0);

parallel_for(0, size, [&](int i) {
    C[i] = A[i] + B[i];
}, numThread);
```

The lambda captures arrays by reference `[&]` and performs element-wise addition. With 2 threads and size=48000000, this distributes 24000000 iterations to each thread.

#### Matrix Multiplication (2D Loop)

From `matrix.cpp`:

```c
parallel_for(0, size, 0, size, [&](int i, int j) {
    for(int k=0; k<size; k++) {
        C[i][j] += A[i][k] * B[k][j];
    }
}, numThread);
```

The 2D `parallel_for` distributes rows across threads. Each thread computes complete rows of the result matrix, with the k-loop executing sequentially within each thread. For a 1024x1024 matrix with 2 threads, each thread processes 512 rows.

---

## Capabilities

The SimpleMultithreader successfully implements:

- **Easy Parallelization**: Convert sequential loops to parallel with minimal code changes.
- **Lambda Support**: Full C++11 lambda expression support including capture lists.
- **Load Balancing**: Automatic distribution of work with remainder handling.
- **Configurable Threads**: User-specified thread count for tuning parallelism.
- **Main Thread Participation**: Efficient use of the calling thread to avoid idle cores.
- **Execution Timing**: Built-in performance measurement for each parallel region.
- **Header-Only**: No linking required; simple `#include` integration.
- **No Thread Pool**: Fresh threads for each call, avoiding state management complexity.

---

## Limitations

While SimpleMultithreader simplifies parallel programming, it has limitations:

- **Thread Creation Overhead**: Creating new threads for each `parallel_for` call incurs overhead. A thread pool would amortize this cost across multiple calls.
- **No Nested Parallelism**: Calling `parallel_for` from within a parallel lambda would create excessive threads and likely degrade performance.
- **Fixed Partitioning**: Work distribution is static (block partitioning). Dynamic scheduling would handle load imbalance better when iteration costs vary.
- **No Reduction Support**: Accumulating values (like sum or max) across threads requires manual synchronization.
- **Memory Allocation**: Uses dynamic allocation for thread IDs and arguments on every call, adding overhead.
- **Row-Only 2D Parallelization**: The 2D version only parallelizes the outer loop. True 2D tiling would enable better cache locality.
- **No Exception Handling**: Exceptions thrown in lambdas may not be properly caught across thread boundaries.
- **Limited Error Checking**: pthread errors are not comprehensively handled.

---

## Compilation and Execution

### Building the Project

The provided Makefile compiles test programs:

```bash
make
```

This produces:
- **vector**: Vector addition test program
- **matrix**: Matrix multiplication test program

### Compilation Command

Programs are compiled with:

```bash
g++ -O3 -std=c++11 -o vector vector.cpp -lpthread
g++ -O3 -std=c++11 -o matrix matrix.cpp -lpthread
```

**Flag Explanations:**
- **-O3**: Enables aggressive optimizations for maximum performance.
- **-std=c++11**: Enables C++11 features including lambda expressions.
- **-lpthread**: Links the pthread library for thread creation and management.

### Running the Programs

**Vector Addition:**
```bash
./vector <numThreads> <size>
./vector 4 48000000
```

**Matrix Multiplication:**
```bash
./matrix <numThreads> <size>
./matrix 4 1024
```

Both programs print execution time and verify correctness with assertions.

---

## AI Generated Code Snippets

During the development of SimpleMultithreader, several code snippets were generated with AI assistance:

### Snippet 1: High-Resolution Time Measurement

```c
struct timeval start, end;
gettimeofday(&start, NULL);

// ... parallel execution ...

gettimeofday(&end, NULL);
double elapsed = (end.tv_sec - start.tv_sec) * 1000.0;
elapsed += (end.tv_usec - start.tv_usec) / 1000.0;
```

This code measures execution time with microsecond precision using the `gettimeofday` system call. The `timeval` structure contains two fields: `tv_sec` (seconds) and `tv_usec` (microseconds). The elapsed time calculation first computes the difference in seconds and converts to milliseconds by multiplying by 1000.0. Then it adds the microsecond difference converted to milliseconds by dividing by 1000.0. This two-step approach handles the wraparound of microseconds correctly. For example, if the start time is 5.999900 seconds and end time is 6.000100 seconds, the calculation yields (6-5)*1000.0 + (100-999900)/1000.0 = 1000.0 - 999.8 = 0.2 ms. This precise timing is essential for benchmarking parallel code and understanding speedup characteristics.

### Snippet 2: Conditional Thread Array Allocation

```c
if (num_newThreads > 0) {
    tids = new pthread_t[num_newThreads];
}
```

This snippet performs conditional memory allocation for thread IDs. Since the main thread participates in work execution, only `numThreads - 1` new pthreads need to be created (stored in `num_newThreads`). When `numThreads` equals 1, no additional threads are created and `tids` remains `nullptr`, avoiding unnecessary allocation of a zero-length array. This guard prevents allocating memory when `num_newThreads` is 0, which could cause undefined behavior or waste memory. The conditional also makes the subsequent thread creation loop cleaner, as it naturally handles the single-threaded case without special conditions.

---

## Contributions

- **Nalin Gupta**: Thread wrapper functions, data structures, test program integration and documentation.
- **Tanish Jindal**: Implemented 1D parallel_for, load balancing algorithm, timing infrastructure, and implemented 2D parallel_for.

### GitHub Repository

Private Repository Link: [https://github.com/chill-nalin/OS-Assignment-5](https://github.com/chill-nalin/OS-Assignment-5)

---

## References

The following references were consulted during the implementation:

### Course Materials

- Lecture 21: Introduction to Multithreading
- Lecture 22: Race Condition in Multithreading
- Lecture 23: Deadlock Avoidance

### Threading APIs

- [pthread_create](https://man7.org/linux/man-pages/man3/pthread_create.3.html) - Create a new thread
- [pthread_join](https://man7.org/linux/man-pages/man3/pthread_join.3.html) - Join with a terminated thread

### Time Functions

- [gettimeofday](https://man7.org/linux/man-pages/man2/gettimeofday.2.html) - Get time with microsecond precision

### C++11 Features

- [Lambda Expressions](https://en.cppreference.com/w/cpp/language/lambda) - C++11 anonymous functions
- [std::function](https://en.cppreference.com/w/cpp/utility/functional/function) - General-purpose function wrapper
- [Lambda Tutorial](https://riptutorial.com/cplusplus/example/1854/what-is-a-lambda-expression-) - RIP Tutorial on lambda basics
- [Lambda for Beginners](https://blogs.embarcadero.com/lambda-expressions-for-beginners/) - Embarcadero blog post

### Documentation

- [Linux man pages](https://man7.org/linux/man-pages/) - System call documentation
- [C++ Reference](https://en.cppreference.com/) - C++ standard library reference
