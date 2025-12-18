# OS-Assignment-2
**Authors:** Nalin Gupta, Tanish Jindal  
**Course:** CSE231: Operating System **(IIITD)**

## Introduction

This document provides an overview of the SimpleShell project. The goal of the assignment was to implement a minimal Unix-like shell in C that supports command execution, pipes, command history, and execution reports.

## System Design

### Overall Architecture

The shell reads input from the user, parses the command, and executes it using `fork()` and `execvp()`. Built-in commands like `history` and `exit` are handled directly.

### Modules Implemented

- **Input Handling:** Uses `fgets()` to read commands.
- **Command Parsing:** Implements `parse_command()` with `strtok()`.
- **Process Creation:** Uses `fork()`, `execvp()`, and `waitpid()`.
- **Pipes:** Multiple commands are connected using `dup2()` and `pipe()`.
- **History Handling:** Commands and metadata are stored in an array of `History` structures.
- **Execution Report:** On termination, the shell displays execution details.

## Implementation Details

This section describes how the core parts of the SimpleShell were implemented.

### Command Parsing

When the user types a command, the shell first removes the trailing newline character left by `fgets`. The input string is then split into tokens using delimiters such as spaces. The first token is treated as the program name, while the remaining tokens are considered command-line arguments. This array of tokens is terminated with a `NULL` pointer so it can be directly passed to the `execvp` system call.

### Executing Commands

To execute a command, the shell creates a new process using `fork()`. The child process replaces its image with the requested program using `execvp()`, while the parent process waits for the child to complete using `waitpid()`. Execution time is measured using `time()` before and after the command runs, and this information, along with the process ID, is stored in the command history.

Built-in commands such as `exit` and `history` are handled without creating a new process, as they are managed internally by the shell.

### Pipe Handling

When a command line contains one or more `|` symbols, the shell splits the line into multiple sub-commands. A pipe is created for each connection between commands. The standard output of one command is redirected to the write end of a pipe, while the standard input of the next command is redirected to the read end of the same pipe using `dup2()`.

Each sub-command is executed in its own child process. The parent process closes unused pipe ends and waits for all children to finish. The entire pipeline is recorded as a single entry in the command history, with its overall start and end times.

## Limitations

Although SimpleShell implements basic functionality, it has several limitations compared to a full-featured Unix shell:

- **I/O Redirection (`>`, `<`, `>>`):** 
  Not supported because the shell does not parse special symbols beyond the pipe operator. 
  Implementing this would require opening files with `open()` and replacing standard input/output with `dup2()`.

- **Background Processes (`&`):** 
  The shell always waits for each child process to complete before accepting a new command. 
  Running background processes would require detecting `&` and skipping the `waitpid()` call.

- **Job Control (e.g., `fg`, `bg`, `kill`):** 
  Not supported because the shell does not manage process groups or signals.

- **Quoted Arguments and Escaping:** 
  Commands containing spaces inside quotes (e.g., `echo "hello world"`) or escape characters are not parsed correctly. 
  This is due to the simplistic use of `strtok()`, which splits only on spaces.

- **Persistent History:** 
  The history is maintained only in memory during a single session. 
  Once the shell exits, all history entries are lost. 
  Implementing persistent history would require reading/writing a file.

- **Shell Built-ins (e.g., `cd`, `export`, `alias`):** 
  Most built-in shell commands are not implemented. 
  For example, `cd` must be handled by the parent process since changing directory in a child has no effect on the shell.

## Running Instructions

The SimpleShell program can be compiled on any Unix/Linux system with the GNU C Compiler (`gcc`). Ensure that the source file (`simple_shell.c`) is present in the current directory.

**To compile the program, run:** `gcc -o simple_shell simple_shell.c`

**To run the shell, execute:** `./simple_shell`

**To terminate the shell, type:** `exit`

Upon exit, the shell will display the execution report, including the **command history**, **process IDs**, **start/end times**, and **duration** for each executed command.

## AI Generated Code Snippets

### 1. Pipe Command Splitting

```c
while (token != NULL && idx < n) {
    while (*token == ' ') token++;
    commands[idx++] = token;
    token = strtok(NULL, "|");
}
```

This loop splits the input line into sub-commands whenever a pipe symbol (`|`) is found. Leading spaces are removed, and each sub-command is stored in the `commands` array.

### 2. Time Formatting

```c
struct tm *start = localtime(&history[i].start_time)
struct tm *end = localtime(&history[i].end_time);
strftime(start_time, sizeof(start_time),
    "%Y-%m-%d %H:%M:%S", start);
strftime(end_time, sizeof(end_time),
    "%Y-%m-%d %H:%M:%S", end);
```

This code converts raw time values into human-readable strings. The `localtime()` function breaks down the time into components, and `strftime()` formats them into a standard `YYYY-MM-DD HH:MM:SS` representation.

## Contributions

- **Nalin Gupta:** Implemented History Handling, Execution Report, Process Creation, Documentation.
- **Tanish Jindal:** Implemented Input Handling, Command Parsing, Pipe Handling.

## References

The following references were consulted during the implementation:

- **Lecture Slides 06, 07, 08**

- **String Functions:**
  - [strdup](https://man7.org/linux/man-pages/man3/strdup.3.html)
  - [strcmp](https://man7.org/linux/man-pages/man3/strcmp.3.html)
  - [strncmp](https://man7.org/linux/man-pages/man3/strncmp.3.html)
  - [strlen](https://man7.org/linux/man-pages/man3/strlen.3.html)
  - [strtok](https://man7.org/linux/man-pages/man3/strtok.3.html)

- **Process and Execution:**
  - [fork](https://man7.org/linux/man-pages/man2/fork.2.html)
  - [execvp](https://man7.org/linux/man-pages/man3/execvp.3.html)
  - [wait](https://man7.org/linux/man-pages/man2/wait.2.html)
  - [waitpid](https://man7.org/linux/man-pages/man2/waitpid.2.html)
  - [getpid](https://man7.org/linux/man-pages/man2/getpid.2.html)

- **Pipes and File Descriptors:**
  - [pipe](https://man7.org/linux/man-pages/man2/pipe.2.html)
  - [dup2](https://man7.org/linux/man-pages/man2/dup2.2.html)

- **Time Functions:**
  - [time](https://man7.org/linux/man-pages/man2/time.2.html)
  - [localtime](https://man7.org/linux/man-pages/man3/localtime.3.html)
  - [strftime](https://man7.org/linux/man-pages/man3/strftime.3.html)
