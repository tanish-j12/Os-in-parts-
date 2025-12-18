#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>

#define MAX_LINE 1024
#define MAX_HISTORY 100

typedef struct {
    char *command;
    pid_t pid;
    time_t start_time;
    time_t end_time;
} History;

History history[MAX_HISTORY];
int history_count = 0;


void parse_command(char *line, char **args);
int launch(char **args, char *line);
int create_process_and_run(char **args, char *line);
void pipe_handler(char *line);
void add_history(char *command, pid_t pid, time_t start_time, time_t end_time);
void print_report();
void my_handler(int signum);
int main() {
    char input_line[MAX_LINE];
    char *args[MAX_LINE/2 + 1];
    struct sigaction sig;
    memset(&sig, 0, sizeof(sig));
    sig.sa_handler = my_handler;
    sigaction(SIGINT, &sig, NULL);   // now handle Ctrl+C
    while (1) {
        printf("user@assignment-2:~$ ");
        fflush(stdout);
        fgets(input_line, MAX_LINE, stdin);

        if (strncmp(input_line, "history", 7) == 0) {
            for (int i = 0; i < history_count; i++) {
                printf("%d: %s\n", i + 1, history[i].command);
            }
            continue;
        }

        if (strchr(input_line, '|') != NULL) {
            pipe_handler(input_line);
            continue;
        }

        parse_command(input_line, args);

        if (args[0] == NULL) {
            continue;
        }

        if (strcmp(args[0], "exit") == 0) {
            print_report();
            break;
        }

        launch(args, input_line);
    }

    return 0;
}

void my_handler(int signum) {
    if (signum == SIGINT) {
        print_report();
        exit(0);
    }
}

void parse_command(char *line, char **args) {
    
    int length = strlen(line);
    if (length > 0 && line[length - 1] == '\n') {
        line[length - 1] = '\0';
    }

    int i = 0;
    
    char *token = strtok(line, " ");

    while (token != NULL) {
        args[i] = token;
        i++;
        token = strtok(NULL, " ");
    }

    args[i] = NULL;
}

int launch(char **args, char *line) {
    int status;
    status = create_process_and_run(args, line);
    return status;
}

int create_process_and_run(char **args, char *line) {
    pid_t pid;
    int status;
    time_t start_time, end_time;

    time(&start_time);
    pid = fork();

    if (pid < 0) {
        perror("fork");
    } else if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else {
        waitpid(pid, &status, 0);
        time(&end_time);
        add_history(line, pid, start_time, end_time);
    }
    return 1;
}

void pipe_handler(char *line) {
    int totalPipes = 0;
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == '|') {
            totalPipes++;
        }
    }
    int n = totalPipes + 1;

    char *line_copy = strdup(line);
    line_copy[strlen(line_copy) - 1] = '\0';

    char *commands[n];
    char *token = strtok(line, "|");
    int idx = 0;
    while (token != NULL && idx < n) {
        while (*token == ' ') {
            token++;
        }
        commands[idx++] = token;
        token = strtok(NULL, "|");
    }

    int pipes[n-1][2];
    for (int i = 0; i < n-1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    time_t start_time, end_time;
    time(&start_time);

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            if (i < n-1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            for (int j = 0; j < n-1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            char *args[MAX_LINE/2 + 1];
            parse_command(commands[i], args);

            if (execvp(args[0], args) == -1) {
                perror("execvp failed");
                exit(EXIT_FAILURE);
            }
        }
    }

    for (int i = 0; i < n-1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for (int i = 0; i < n; i++) {
        wait(NULL);
    }

    time(&end_time);
    add_history(line_copy, getpid(), start_time, end_time);
    free(line_copy);
}

void add_history(char *command, pid_t pid, time_t start_time, time_t end_time) {
    if (history_count >= MAX_HISTORY) return;

    history[history_count].command = strdup(command);
    history[history_count].pid = pid;
    history[history_count].start_time = start_time;
    history[history_count].end_time = end_time;
    history_count++;
}

void print_report() {
    printf("\nExecution Report:\n");
    for (int i = 0; i < history_count; i++) {
        char start_time[64], end_time[64];
        struct tm *start = localtime(&history[i].start_time);
        struct tm *end = localtime(&history[i].end_time);
        strftime(start_time, sizeof(start_time), "%Y-%m-%d %H:%M:%S", start);
        strftime(end_time, sizeof(end_time), "%Y-%m-%d %H:%M:%S", end);

        printf("Command: %s\n", history[i].command);
        printf("PID: %d\n", history[i].pid);
        printf("Start: %s\n", start_time);
        printf("End: %s\n", end_time);
        printf("Duration: %f seconds\n\n", difftime(history[i].end_time, history[i].start_time));
    }
}