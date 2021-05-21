#ifndef __SBI_H__
#define __SBI_H__

#include "stddef.h"

#define SBI_SET_TIMER 0
#define SBI_CONSOLE_PUTCHAR 1
#define SBI_CONSOLE_GETCHAR 2
#define SBI_CLEAR_IPI 3
#define SBI_SEND_IPI 4
#define SBI_REMOTE_FENCE_I 5
#define SBI_REMOTE_SFENCE_VMA 6
#define SBI_REMOTE_SFENCE_VMA_ASID 7
#define SBI_SHUTDOWN 8

extern size_t sbi_call(size_t which, size_t arg0, size_t arg1, size_t arg2);

extern size_t read_time();

extern size_t get_kernel_end();

extern void interrupt_timer_init();

extern void* get_boot_page_table();

extern size_t get_restore();

extern void flush_tlb();

void shutdown();

void set_timer(size_t time);

size_t getchar();

#endif