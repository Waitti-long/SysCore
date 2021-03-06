#ifndef OS_RISC_V_SCHEDULER_H
#define OS_RISC_V_SCHEDULER_H

#include "../lib/stl/stl.h"
#include "elf_loader.h"
#include "interrupt.h"
#include "register.h"
#include "../lib/stl/list.h"
#include "../lib/stl/vector.h"
#include "../lib/stl/map.h"
#include "../lib/stl/RefCountPtr.h"
#include "memory/brk_control.h"
#include "memory/elf_control.h"
#include "memory/mmap_control.h"

#define MAX_PATH_LENGTH 32

extern int global_pid;

extern int memory_dirty;

void init_thread();

class PCB{
public:

    PCB()=default; // used by new PCB()
    PCB& operator=(const PCB& other)=delete; // copy not allowed
    PCB(const PCB& other)=delete; // copy not allowed

    int pid;
    int ppid;
    size_t stack;
    size_t stack_size;

    RefCountPtr<size_t> page_table;

    RefCountPtr<Elf_Control> elf_control;

    RefCountPtr<Elf64_Phdr> kernel_phdr;

    RefCountPtr<BrkControl> brk_control;

    RefCountPtr<MmapControl> mmap_control;

    Context * thread_context;
    char cwd[MAX_PATH_LENGTH];
    int pipe_read_count = 0;
    int pipe_write_count = 0;

    RefCountPtr<Map<size_t,size_t>> occupied_file_describer;
    RefCountPtr<List<size_t>> occupied_kernel_heap;
    RefCountPtr<List<size_t>> occupied_pages;

    // thread local
    List<pair<int,int>> signal_list;
    int* wstatus=nullptr;
    size_t wait_pid;

    void kill(int exit_ret);

};

extern Vector<PCB*> runnable,blocked;
extern PCB* running;

void file_describer_bind(size_t file_id,size_t real_file_describer);

void file_describer_erase(size_t file_id);

bool file_describer_exists(size_t file_id);

size_t file_describer_convert(size_t file_id);

void bind_kernel_heap(size_t addr);

void bind_pages(size_t addr);

int get_new_pid();

char* get_running_cwd();

int& get_running_pipe_read_count();

int& get_running_pipe_write_count();

void init_scheduler();

size_t get_running_elf_page();

int get_running_pid();

int get_running_ppid();

PCB *search_by_pid(Vector<PCB*>** list,int pid);

void create_process(const char *elf_path);

void create_process(const char *elf_path,const char* []);

void clone(int flags,size_t stack,int* parent_tid, size_t tls,int* child_tid);

void __yield();

void yield_with_return(size_t a0);

void yield();

int wait(int* wstatus);

void execute(const char* exec_path);

void exit_process(int exit_ret);

void schedule();

#endif //OS_RISC_V_SCHEDULER_H
