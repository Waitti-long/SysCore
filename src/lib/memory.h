#ifndef OS_RISC_V_MEMORY_H
#define OS_RISC_V_MEMORY_H


#include "stdbool.h"
#include "stddef.h"
#include "stdio.h"
#include "stl.h"

#define L(x) (x*2)
#define R(x) (x*2+1)

#define __page_size (4096)
#define __kernel_start (0x80020000)
#define __memory_end  0x80000000+6*1024*1024
extern size_t __kernel_end;
#define __page_num ((__memory_end-__kernel_start) / __page_size)

typedef struct {
    // left  node : [l,mid]
    // right node : [mid+1,r]
    int l, r;
    int mid;

    int left_boarder_space;
    int left_right_space;

    int right_left_space;
    int right_boarder_space;

    int left_max_space;
    int right_max_space;

    int max_space;

    // 0:not set; 1:set to occupied; -1:set to unoccupied
    int set;

} __Memory_SegmentTreeNode;

extern __Memory_SegmentTreeNode global_pages[];
extern int __memory_alloc_length[];

int __memory_get_size(int node);

bool __memory_isEmpty(int node);

void __memory_push_up(int node);

void __memory_init_pages(int node, int l, int r);

int __memory_find_space(int node, int size);

void __memory_push_down(int node);

void __memory_update(int node, int l, int r, int set);

void init_memory();

size_t alloc_page(size_t size);

void dealloc_page(size_t p);

#endif //OS_RISC_V_MEMORY_H
