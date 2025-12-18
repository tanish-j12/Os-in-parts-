# SimpleSmartLoader - OS Assignment 4

**Authors:** Nalin Gupta, Tanish Jindal  
**Course:** CSE231: Operating Systems  

## Introduction

This project implements **SimpleSmartLoader**, an upgraded version of SimpleLoader that demonstrates lazy loading of program segments using demand paging. Unlike SimpleLoader which loaded all PT_LOAD segments upfront using mmap, SimpleSmartLoader allocates and loads memory pages only when they are accessed during program execution.

This approach mimics the demand paging mechanism used in modern operating systems like Linux, where physical memory is allocated lazily to optimize resource utilization. The loader handles segmentation faults as page faults, dynamically allocating 4KB pages and loading corresponding segment data from the ELF file seamlessly.

### Key Features
- Lazy loading of executable segments
- Signal-based page fault handling
- Page-by-page allocation (4KB granularity)
- Statistics tracking (page faults, allocations, internal fragmentation)
- Support for 32-bit ELF executables

## System Design

### Overall Architecture

The SimpleSmartLoader system consists of the following key components:

1. **ELF Parser**: Reads the ELF header and program headers to identify PT_LOAD segments without immediately allocating memory.
2. **Signal Handler**: Intercepts SIGSEGV signals and determines if they represent valid page faults that require memory allocation.
3. **Page Allocator**: Allocates 4KB pages on-demand using mmap with MAP_FIXED to place pages at specific virtual addresses.
4. **Segment Loader**: Reads segment data from the ELF file and copies it into newly allocated pages.
5. **Statistics Tracker**: Monitors page faults, allocations, and internal fragmentation throughout execution.

The loader does not allocate any memory upfront, not even for the segment containing the entry point. Execution begins immediately by jumping to the `_start` address, which triggers the first page fault and initiates the lazy loading process.

### Modules Implemented

- **Signal-Based Page Fault Handling**: Registers a SIGSEGV handler using `sigaction()` with `SA_SIGINFO` to capture fault addresses via `si_addr`.
- **Segment Metadata Storage**: Maintains an array of PT_LOAD segment headers read during ELF parsing for reference during fault handling.
- **Page-by-Page Allocation**: Allocates memory in 4KB chunks aligned to page boundaries, even if segments span multiple pages.
- **Page Tracking**: Records all allocated page addresses in an array to prevent duplicate allocations for the same page.
- **File-to-Memory Mapping**: Uses `lseek()` and `read()` to load segment data from the ELF file into allocated pages at the correct offsets.
- **Internal Fragmentation Calculation**: Computes wasted space when page allocation exceeds segment size boundaries.

## Implementation Details

### 1. ELF File Parsing

The loader begins by reading the ELF header to obtain metadata about the executable:

```c
if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
    perror("ehdr is faulty");
    loader_cleanup();
    exit(1);
}
```

The entry point address (`ehdr.e_entry`) is extracted, but no memory is allocated at this stage. The loader then reads all program headers and stores PT_LOAD segments:

```c
for (int i = 0; i < ehdr.e_phnum; i++) {
    Elf32_Phdr phdr;
    if (read(fd, &phdr, sizeof(phdr)) != sizeof(phdr)) {
        perror("Failed to read program header");
        loader_cleanup();
        exit(1);
    }
    if (phdr.p_type == PT_LOAD) {
        if (num_load_segment < MAX_SEGMENTS) {
            load_segment[num_load_segment++] = phdr;
        }
    }
}
```

Each PT_LOAD segment header contains:
- **p_vaddr**: Virtual address where the segment should be loaded
- **p_memsz**: Size of the segment in memory
- **p_offset**: Offset in the file where segment data begins
- **p_filesz**: Size of the segment in the file

### 2. Signal Handler Registration

Before jumping to the entry point, a SIGSEGV handler is registered:

```c
struct sigaction sa;
memset(&sa, 0, sizeof(sa));
sa.sa_sigaction = signal_handler;
sa.sa_flags = SA_SIGINFO;

if (sigaction(SIGSEGV, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
}
```

The `SA_SIGINFO` flag enables the handler to receive detailed fault information via the `siginfo_t` structure, including the faulting address in `si->si_addr`.

### 3. Entry Point Execution

The loader casts the entry point address to a function pointer and invokes it:

```c
void* entry_point = (void*)ehdr.e_entry;
typedef int (*start_func_t)();
start_func_t _start = (start_func_t)entry_point;
int result = _start();
```

Since no memory has been allocated, this immediately triggers a segmentation fault at the entry point address. The signal handler intercepts this fault and allocates the necessary page.

### 4. Page Fault Handling

The signal handler performs the following steps when a SIGSEGV occurs:

#### Step 1: Increment Page Fault Counter
```c
total_pageFaults++;
```

#### Step 2: Identify Target Segment
The handler searches through stored PT_LOAD segments to find which segment contains the faulting address:

```c
void *fault_addr = si->si_addr;
Elf32_Phdr *target_phdr = NULL;

for (int i = 0; i < num_load_segment; i++) {
    Elf32_Phdr *phdr = &load_segment[i];
    if ((uintptr_t)fault_addr >= phdr->p_vaddr && 
        (uintptr_t)fault_addr < phdr->p_vaddr + phdr->p_memsz) {
        target_phdr = phdr;
        break;
    }
}
```

If no segment contains the address, it's a genuine segmentation fault:
```c
if (!target_phdr) {
    write(STDERR_FILENO, "Segmentation fault (core dumped)\n", 33);
    exit(1);
}
```

#### Step 3: Calculate Page-Aligned Address
The faulting address is rounded down to the nearest 4KB boundary:

```c
uintptr_t addr = (uintptr_t)fault_addr;
uintptr_t align_addr = (addr / PAGE_SIZE) * PAGE_SIZE;
void *page_start = (void *)align_addr;
```

For example, if the fault occurs at address `0x8049123`, the page starts at `0x8049000`.

#### Step 4: Check for Duplicate Allocation
To avoid allocating the same page twice:

```c
if (is_page_mapped(page_start))
    return;
```

The `is_page_mapped()` function checks if the page address exists in the `mapped_pages` array.

#### Step 5: Allocate Page with mmap
A new 4KB page is allocated at the exact virtual address:

```c
void *page = mmap(page_start, PAGE_SIZE,
                  PROT_READ | PROT_WRITE | PROT_EXEC,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                  -1, 0);
if (page == MAP_FAILED) {
    perror("mmap failed in handler");
    exit(1);
}

total_pageAllocate++;
mapped_pages[num_mapped_pages++] = page_start;
```

The `MAP_FIXED` flag ensures the page is placed at the requested virtual address. The page is mapped with read, write, and execute permissions to support all segment types (.text, .data, .bss).

#### Step 6: Load Segment Data
The handler calculates which portion of the segment overlaps with the allocated page:

```c
uintptr_t page_end = align_addr + PAGE_SIZE;
uintptr_t file_end = target_phdr->p_vaddr + target_phdr->p_memsz;
uintptr_t read_until;

if (align_addr < file_end) {
    if (page_end < file_end) {
        read_until = page_end;
    } else {
        read_until = file_end;
    }
    size_t bytes_needed = read_until - align_addr;
    off_t file_offset = target_phdr->p_offset + 
                        (align_addr - target_phdr->p_vaddr);
    
    lseek(fd, file_offset, SEEK_SET);
    read(fd, page_start, bytes_needed);
}
```

This code handles edge cases where:
- The page extends beyond the segment (only load up to segment end)
- The segment is smaller than a page (zero-fill the rest)
- The page spans multiple segments (handled by subsequent faults)

#### Step 7: Calculate Internal Fragmentation
When a page allocation extends beyond the segment boundary, internal fragmentation occurs:

```c
uintptr_t segment_end = target_phdr->p_vaddr + target_phdr->p_memsz;
if (page_end > segment_end && align_addr < segment_end) {
    total_internal_fragmentation += (page_end - segment_end);
}
```

For example, if a segment ends at 0x8049500 but the last page extends to 0x804A000, 0xB00 bytes (2816 bytes) are wasted.

### 5. Program Execution and Return

After all page faults are handled and the program completes execution, the loader prints statistics:

```c
printf("User _start return value = %d\n", result);

printf("--- SimpleSmartLoader Statistics ---\n");
printf("Total Page Faults: %d\n", total_pageFaults);
printf("Total Page Allocations: %d\n", total_pageAllocate);
printf("Total Internal Fragmentation: %.2f KB\n", 
       (double)total_internal_fragmentation / 1024.0);
```

### 6. Cleanup

The `loader_cleanup()` function releases all allocated resources:

```c
void loader_cleanup() {
    for (int i = 0; i < num_mapped_pages; i++) {
        if (mapped_pages[i]) {
            munmap(mapped_pages[i], PAGE_SIZE);
        }
    }
    if (fd != -1) {
        close(fd);
    }
}
```

This unmaps all allocated pages and closes the ELF file descriptor.

## Capabilities

The SimpleSmartLoader successfully implements:

- ✅ **Lazy Loading**: Memory is allocated only when accessed, reducing initial memory footprint
- ✅ **Page-by-Page Allocation**: Segments are loaded in 4KB chunks, not all at once
- ✅ **Signal-Based Fault Handling**: SIGSEGV signals are intercepted and handled transparently
- ✅ **Multiple Segment Support**: Handles .text, .data, .bss, and other PT_LOAD segments
- ✅ **Accurate Statistics**: Tracks page faults, allocations, and internal fragmentation
- ✅ **Seamless Execution**: Programs run without knowing they're being lazily loaded
- ✅ **32-bit ELF Support**: Works with 32-bit executables compiled with -m32

## Limitations

While SimpleSmartLoader demonstrates demand paging concepts, it has limitations:

- ❌ **No Copy-on-Write**: All pages are allocated as private writable copies; no sharing between processes
- ❌ **No Page Replacement**: Once allocated, pages remain in memory until program termination
- ❌ **No Protection Bits**: All pages have RWX permissions; no enforcement of read-only .text segments
- ❌ **Fixed Page Size**: Only supports 4KB pages; no huge page support
- ❌ **32-bit Only**: Requires compilation with -m32; does not support 64-bit ELF files
- ❌ **No Dynamic Linking**: Only works with statically linked executables compiled with -nostdlib
- ❌ **Signal Handler Limitations**: Cannot handle recursive faults within the handler itself
- ❌ **No ASLR**: Segments are loaded at fixed virtual addresses from the ELF file

## Compilation and Execution

### Building the Project

The provided Makefile compiles the loader and test programs:

```bash
make
```

This produces:
- **loader**: The SimpleSmartLoader executable (32-bit)
- **fib**: Fibonacci test program (32-bit, statically linked)
- **sum**: Sum test program (32-bit, statically linked)

### Compilation Flags Explanation

Test programs are compiled with specific flags:

```bash
gcc -m32 -no-pie -nostdlib -o fib fib.c
```

- **-m32**: Generate 32-bit code (required for Elf32 format)
- **-no-pie**: Disable position-independent executable (use fixed addresses)
- **-nostdlib**: Do not link with standard C library (no glibc dependencies)

The loader itself is compiled as 32-bit to match:
```bash
gcc -m32 -o loader loader.c
```

### Running the Loader

Execute a test program through the loader:

```bash
./loader ./fib
```

Example output:
```
User _start return value = 55
--- SimpleSmartLoader Statistics ---
Total Page Faults: 3
Total Page Allocations: 3
Total Internal Fragmentation: 2.81 KB
```

### Creating Test Programs

Test programs must define `_start` as the entry point and avoid glibc:

```c
int _start() {
    // Program logic here
    int result = 0;
    for (int i = 0; i < 1000000; i++) {
        result += i;
    }
    return result;
}
```

Compile with:
```bash
gcc -m32 -no-pie -nostdlib -o myprogram myprogram.c
```

## Design Decisions

### Why Use Signal Handlers?

Signal-based page fault handling was chosen because:
- It mirrors how real operating systems handle page faults (via exception handlers)
- The kernel automatically suspends the faulting instruction until the signal handler returns
- It allows transparent lazy loading without modifying the executable
- The faulting address is provided by the kernel in `si->si_addr`

### Why Page-by-Page Instead of Segment-at-Once?

Page-level granularity provides:
- More realistic simulation of OS demand paging behavior
- Better memory efficiency for large segments where only portions are used
- Demonstration of multiple page faults per segment
- Alignment with actual page table granularity in hardware

### Why Track Mapped Pages?

The `mapped_pages` array prevents:
- Double allocation of the same page from multiple faults
- Memory leaks from orphaned mappings
- Incorrect page fault statistics
- Allows proper cleanup via munmap during termination

### Why Use MAP_FIXED?

The `MAP_FIXED` flag is essential because:
- ELF executables expect code/data at specific virtual addresses
- Relative addressing in compiled code depends on fixed addresses
- Function pointers and global variables would break without fixed mapping
- It simulates how the kernel's loader places segments at prescribed locations

## AI Generated Code Snippets

During the development of SimpleSmartLoader, several code snippets were generated with AI assistance to handle implementation challenges:

### Snippet 1: Page Alignment Calculation

```c
uintptr_t addr = (uintptr_t)fault_addr;
uintptr_t align_addr = (addr / PAGE_SIZE) * PAGE_SIZE;
void *page_start = (void *) align_addr;
```

This code calculates the page-aligned starting address for a given fault address. It divides the address by the page size (4096) and multiplies back to effectively round down to the nearest page boundary. For example, address 0x8049123 becomes 0x8049000. This alignment is critical because mmap requires page-aligned addresses, and we need to allocate entire pages even if the fault occurs in the middle of one.

### Snippet 2: File Offset Calculation

```c
off_t file_offset = target_phdr->p_offset + 
                    (align_addr - target_phdr->p_vaddr);
```

This single line computes where to read from in the ELF file to load data for a specific page. The segment's file offset (`p_offset`) gives us the starting position in the file, and we add the page's relative position within the segment (calculated as `align_addr - p_vaddr`). For instance, if a segment starts at file offset 0x1000 and virtual address 0x8048000, and we need to load page 0x8049000, the file offset becomes 0x1000 + (0x8049000 - 0x8048000) = 0x2000. This mapping is essential because virtual addresses in memory don't directly correspond to file positions.

### Snippet 3: Signal Handler Configuration

```c
sa.sa_flags = SA_SIGINFO;
```

Setting the `SA_SIGINFO` flag in the sigaction structure enables the extended signal handler interface. This allows our handler to receive three arguments instead of one: the signal number, a pointer to a `siginfo_t` structure, and a pointer to the context. The `siginfo_t` structure contains critical information including `si_addr`, which holds the exact memory address that caused the segmentation fault. Without this flag, we would only know that a SIGSEGV occurred, but not where, making it impossible to determine which page to allocate. This flag is fundamental to implementing demand paging in user space.

## Contributions

- **Nalin Gupta**: Implemented ELF parsing, signal handler, statistics reporting, and documentation
- **Tanish Jindal**: Page fault detection, page allocation, internal fragmentation calculation, segment loading, page tracking, and cleanup mechanisms

### GitHub Repository
🔗 Private Repository: [OS-Assignment-4](https://github.com/chill-nalin/OS-Assignment-4)

## References

### Course Materials
- Lecture Slides on Virtual Memory and Paging
- Assignment 1 codebase (SimpleLoader)

### System Calls

#### Memory Management
- [mmap](https://man7.org/linux/man-pages/man2/mmap.2.html) - Map files or devices into memory
- [munmap](https://man7.org/linux/man-pages/man2/munmap.2.html) - Unmap files or devices from memory

#### Signal Handling
- [sigaction](https://man7.org/linux/man-pages/man2/sigaction.2.html) - Examine and change signal action

#### File Operations
- [open](https://man7.org/linux/man-pages/man2/open.2.html) - Open and possibly create a file
- [read](https://man7.org/linux/man-pages/man2/read.2.html) - Read from a file descriptor
- [lseek](https://man7.org/linux/man-pages/man2/lseek.2.html) - Reposition file offset
- [close](https://man7.org/linux/man-pages/man2/close.2.html) - Close a file descriptor

### Library Functions

#### Standard I/O
- [printf](https://man7.org/linux/man-pages/man3/printf.3.html) - Formatted output
- [perror](https://man7.org/linux/man-pages/man3/perror.3.html) - Print system error message

#### Memory
- [memset](https://man7.org/linux/man-pages/man3/memset.3.html) - Fill memory with a constant byte

### Documentation
- [Linux man pages](https://man7.org/linux/man-pages/) - Complete system call documentation
- [ELF Specification](https://refspecs.linuxfoundation.org/elf/elf.pdf) - Executable and Linkable Format
- [GNU C Library Manual](https://www.gnu.org/software/libc/manual/)
