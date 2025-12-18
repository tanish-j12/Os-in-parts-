#include "loader.h"
#include <signal.h>
#include <sys/mman.h> 
#include <unistd.h>
#include <errno.h>   

#define PAGE_SIZE 4096
#define MAX_SEGMENTS 16  
#define MAX_MAPPED_PAGES 1024

int fd = -1; 
Elf32_Ehdr ehdr; 

int total_pageFaults = 0;
int total_pageAllocate = 0;
double total_internal_fragmentation = 0;

Elf32_Phdr load_segment[MAX_SEGMENTS]; 
int num_load_segment = 0;

void* mapped_pages[MAX_MAPPED_PAGES];
int num_mapped_pages = 0;

int is_page_mapped(void* page_addr) {
    for (int i = 0; i < num_mapped_pages; i++) {
        if (mapped_pages[i] == page_addr) {
            return 1; 
        }
    }
    return 0;
}


static void signal_handler(int signum, siginfo_t *info, void *parameter) {
    total_pageFaults++;

    void *fault_addr = info->si_addr;
    Elf32_Phdr *target_phdr = NULL;

    // which segment contains the fault address
    for (int i = 0; i < num_load_segment; i++) {
        Elf32_Phdr *phdr = &load_segment[i];
        if ((uintptr_t)fault_addr >= phdr->p_vaddr && 
            (uintptr_t)fault_addr < phdr->p_vaddr + phdr->p_memsz) {
            target_phdr = phdr;
            break;
        }
    }

    if (!target_phdr) {
        write(STDERR_FILENO, "Segmentation fault (core dumped)\n", 33);
        exit(1);
    }

    uintptr_t addr = (uintptr_t)fault_addr;
    uintptr_t align_addr = (addr / PAGE_SIZE) * PAGE_SIZE;
    void *page_start = (void *)align_addr;

    if (is_page_mapped(page_start))
        return;

    // Allocating page
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

    // Load data from file for this page
    uintptr_t page_end = align_addr + PAGE_SIZE;
    uintptr_t file_end = target_phdr->p_vaddr + target_phdr->p_memsz;
    uintptr_t read_until;
        if (align_addr < file_end) {
            if (page_end < file_end) {
                read_until = page_end;
            }
            else {
                read_until = file_end;
            }
            size_t bytes_needed = read_until - align_addr;
            off_t file_offset = target_phdr->p_offset + (align_addr - target_phdr->p_vaddr);

            if(lseek(fd, file_offset, SEEK_SET) < 0 ) {
                perror("lseek failed");
                exit(1);
            }
            if (read(fd, page_start, bytes_needed) <0 ) {
                perror("Read failed");
                exit(1);
            }
    }

    uintptr_t segment_end = target_phdr->p_vaddr + target_phdr->p_memsz;
    if (page_end > segment_end && align_addr < segment_end) {
        total_internal_fragmentation += (page_end - segment_end);
    }
}


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


void load_and_run_elf(char** exe) {
    fd = open(exe[1], O_RDONLY);

    if (fd < 0) {
        perror("open");
        exit(1);
    }

    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        perror("ehdr is faulty");
        loader_cleanup();
        exit(1);
    }
  
    off_t e_phoff = ehdr.e_phoff;
    lseek(fd, e_phoff, SEEK_SET);

    // Read all program headers and store the PT_LOAD ones
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

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    void* entry_point = (void*)ehdr.e_entry;

    typedef int (*start_func_t)();
    start_func_t _start = (start_func_t)entry_point;

    int result = _start();
    
    printf("User _start return value = %d\n", result);
    printf("--- SimpleSmartLoader Statistics ---\n");
    printf("Total Page Faults: %d\n", total_pageFaults);
    printf("Total Page Allocations: %d\n", total_pageAllocate);
    printf("Total Internal Fragmentation: %.2f KB\n", (double)total_internal_fragmentation / 1024.0);
}

int main(int argc, char** argv)
{
    if(argc != 2) {
        printf("Usage: %s <ELF Executable> \n",argv[0]);
        exit(1);
    }
    
    load_and_run_elf(argv);
    
    loader_cleanup();
    return 0;
}