# 💻 OS-in-parts

A modular exploration of Operating System internals. This repository breaks down complex kernel-level responsibilities—loading, scheduling, and concurrency—into distinct, functional components implemented in C and C++.

---

## 🛠️ Core Modules

### `Simple-loader`
A custom execution engine that parses ELF (Executable and Linkable Format) binaries, maps segments into memory, and transfers control to the program entry point.

### `Simple-shell`
A robust command-line interface featuring process forking, command execution, and support for system-level features like I/O redirection and piping.

### `Simple-Scheduler`
An implementation of fundamental CPU scheduling policies (FCFS, Round Robin, etc.) designed to manage process lifecycles and evaluate throughput.

### `Smart-Scheduler`
An advanced iteration of the scheduler, incorporating more sophisticated logic for task prioritization and resource allocation.

### `Os-Multithreader`
A deep dive into parallelism, utilizing the pthreads library to manage concurrent execution, synchronization primitives, and thread-safe data handling.

---

## 🚀 Technical Overview

This project bridges the gap between OS theory and systems programming by focusing on:

- **Binary Interaction:** Directly handling ELF headers and memory mapping
- **Process Lifecycle:** Managing `fork()`, `exec()`, and signal handling
- **Concurrency:** Navigating the complexities of race conditions and thread synchronization
- **Performance:** Analyzing the efficiency of different scheduling heuristics

---

## ⚙️ Quick Start

### 1. Clone the environment

```bash
git clone https://github.com/tanish-j12/Os-in-parts-.git
cd Os-in-parts-
```

### 2. Build a specific module

Each directory contains a `Makefile` for streamlined compilation.

```bash
# Example for the shell module
cd Simple-shell
make
./shell
```

> [!IMPORTANT]  
> These implementations are designed for Linux environments and require `gcc`/`g++` and `make` utilities.

---

## 📝 Purpose

Created for academic research and hands-on learning in the field of Systems Programming and Operating System Architecture.

---

## 📧 Contact

**Tanish Jindal**  
Indraprastha Institute of Information Technology, Delhi

- 📧 Email: tanish24579@iiitd.ac.in
- 💼 LinkedIn: [linkedin.com/in/tanishjindal]((https://www.linkedin.com/in/tanish-jindal-a21104344/))
- 🐙 GitHub: [github.com/tanish-j12](https://github.com/tanish-j12)

---

⭐ **Star this repository if you find it useful for learning OS concepts!**
