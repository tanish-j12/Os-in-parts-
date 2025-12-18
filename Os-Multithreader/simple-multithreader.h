#include <iostream>
#include <functional>
#include <stdlib.h>
#include <cstring>
#include <pthread.h> 
#include <sys/time.h>

using namespace std; 


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


void* thread_func_1d(void* arg) {
    thread_args_1d* data =(thread_args_1d*)(arg);
    for (int i = data->low; i < data->high; i++) {
        (*(data->lambda))(i);
    }
    return NULL;
}

void* thread_func_2d(void* arg) {
    thread_args_2d* data = (thread_args_2d*)(arg);
    for (int i = data->low1; i < data->high1; i++) {
        for (int j = data->low2; j < data->high2; ++j) {
            (*(data->lambda))(i, j);
        }
    }
    return NULL;
}


void parallel_for(int low, int high, function<void(int)>&& lambda, int numThreads) {
    //  Start Timer
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

    //  Dividing  Work
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

    //  Main Thread Work
    thread_func_1d(&args[numThreads - 1]);

    //  Wait for other threads
    for (int i = 0; i < num_newThreads; ++i) {
        pthread_join(tids[i], NULL);
    }

    //  Stop Timer & Print
    gettimeofday(&end, NULL);
    double tot_time = (end.tv_sec - start.tv_sec) * 1000.0;
    tot_time += (end.tv_usec - start.tv_usec) / 1000.0;

    cout << "parallel_for execution time: " << tot_time << " ms" << endl;

    // Cleanup
    delete[] tids;
    delete[] args;
}


void parallel_for(int low1, int high1, int low2, int high2, 
                  function<void(int, int)>&& lambda, int numThreads) {
    //  Start Timer
    struct timeval start, end;
    gettimeofday(&start, NULL);

    int total_rows = high1 - low1;
    if (numThreads <= 0) {
        numThreads = 1;
    }
    //  Other threads
    int num_newThreads = numThreads - 1;
    pthread_t* tids = nullptr;
    if (num_newThreads > 0) {
        tids = new pthread_t[num_newThreads];
    }
    thread_args_2d* args = new thread_args_2d[numThreads];

    //  Dividing Rows
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

    //  Main Thread Work
    thread_func_2d(&args[numThreads - 1]);

    //  Wait for other threads
    for (int i = 0; i < num_newThreads; ++i) {
        pthread_join(tids[i], NULL);
    }

    //  Stop Timer & Print
    gettimeofday(&end, NULL);
    double tot_time = (end.tv_sec - start.tv_sec) * 1000.0;
    tot_time += (end.tv_usec - start.tv_usec) / 1000.0;

    cout << "parallel_for execution time: " << tot_time << " ms" << endl;

    // Cleanup
    delete[] tids;
    delete[] args;
}

int user_main(int argc, char** argv);

void demonstration(function<void()>&& lambda) {
    lambda();
}

int main(int argc, char** argv) {
    int x = 5, y = 1;
    auto lambda1 = [x, &y](void) {
        y = 5;
        cout << "====== Welcome to Assignment-" << y << " of the CSE231(A) ======\n";
    };
    demonstration(lambda1);
    int rc = user_main(argc, argv);
    auto lambda2 = []() {
        cout << "====== Hope you enjoyed CSE231(A) ======\n";
    };
    demonstration(lambda2);
    return rc;
}

#define main user_main