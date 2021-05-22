#ifndef OS_RISC_V_SCHEDULER_H
#define OS_RISC_V_SCHEDULER_H

#include "stl.h"
#include "fat32.h"
#include "elf_loader.h"
#include "interrupt.h"
#include "register.h"

extern int global_pid;

typedef struct{
    int pid;
    size_t stack;
    size_t elf_page_base;
    size_t page_table;
    Context * thread_context;
} pcb;

typedef struct pcb_listNode{
    pcb* pcb;
    struct pcb_listNode* previous;
    struct pcb_listNode* next;
} pcb_listNode;

typedef struct {
    pcb_listNode* start;
    pcb_listNode* end;
} pcb_List;

extern pcb_List runnable,blocked;
extern pcb* running;

int get_new_pid();

bool pcb_list_is_empty(pcb_List* list);

void pcb_push_back(pcb_List* list,pcb* pcb);

void pcb_push_front(pcb_List* list,pcb* pcb);

void pcb_list_pop_front(pcb_List* list);

void init_scheduler();

size_t get_running_elf_page();

void create_process(const char *elf_path);

void exit_process();

void schedule();

#endif //OS_RISC_V_SCHEDULER_H
