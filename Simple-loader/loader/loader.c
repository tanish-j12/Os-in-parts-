#include "loader.h"

int fd = -1;

void *segmentMemory = NULL;
size_t segmentSize = 0;

/*
 * release memory and other cleanups
 */
void loader_cleanup() {
  if (segmentMemory) {
    munmap(segmentMemory, segmentSize);
  }

  if (fd != -1) {
    close(fd);
  }
}

/*
 * Load and run the ELF executable file
 */
void load_and_run_elf(char** exe) {
  // 1. Load entire binary content into the memory from the ELF file.
  fd = open(exe[1], O_RDONLY);

  if (fd < 0) {
    perror("open");
    exit(1);
  }

  Elf32_Ehdr ehdr;
  if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)){
      perror("ehdr is faulty");
      loader_cleanup();
      exit(1);
  }
  
  if(ehdr.e_ident[0]!=0x7f || ehdr.e_ident[1]!='E' || ehdr.e_ident[2]!='L' || ehdr.e_ident[3]!='F'){
    fprintf(stderr,"not a valid elf!!!!\n");
    loader_cleanup();
    exit(1);
  }

  off_t e_phoff = ehdr.e_phoff;
  lseek(fd, e_phoff, SEEK_SET);

  // 2. Iterate through the PHDR table and find the section of PT_LOAD type that contains the address of the entrypoint method in fib.c
  for (int i = 0; i < ehdr.e_phnum; i++) {
    Elf32_Phdr phdr;

    if (read(fd, &phdr, sizeof(phdr)) != sizeof(phdr)) {
      perror("Failed to read program header");
      loader_cleanup();
      exit(1);
    }

    if (phdr.p_type == PT_LOAD && ehdr.e_entry >= phdr.p_vaddr && ehdr.e_entry < (phdr.p_vaddr + phdr.p_memsz)) {

      // 3. Allocate memory of the size "p_memsz" using mmap function and then copy the segment content
      segmentSize = phdr.p_memsz;
      segmentMemory = mmap(NULL, segmentSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
      if (segmentMemory == MAP_FAILED) {
        perror("mmap");
        loader_cleanup();
        exit(1);
      }
    
      lseek(fd, phdr.p_offset, SEEK_SET);
      if (read(fd, segmentMemory, phdr.p_memsz) != phdr.p_memsz) {
        perror("Failed to read segment content");
        loader_cleanup();
        exit(1);
      }

      // 4. Navigate to the entrypoint address into the segment loaded in the memory in above step
      void* entry_point = (void*)((char*)segmentMemory + (ehdr.e_entry - phdr.p_vaddr));

      if (entry_point == NULL) {
        perror("Entry point not found");
        loader_cleanup();
        exit(1);
      }

      // 5. Typecast the address to that of function pointer matching "_start" method in fib.c.
      int (*_start)() = (int (*)())entry_point;

      // 6. Call the "_start" method and print the value returned from the "_start"
      int result = _start();
      printf("User _start return value = %d\n", result);

      break;
    }
  }
}