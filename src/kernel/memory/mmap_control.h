#ifndef SYSCORE_MMAP_CONTROL_H
#define SYSCORE_MMAP_CONTROL_H

#include "memory_keeper.h"
#include "memory.h"
#include "../../lib/stl/stl.h"
#include "../../lib/stl/PageTableUtil.h"
#include "../../lib/stl/vector.h"
#include "../../driver/fatfs/ff.h"
#include "../time/time.h"

#define MMAP_VIRT_BEGIN 0x100000000

extern FIL* LastOpenedFile;

struct mmap_unit{
    size_t mapped_to;
    size_t source_addr;
};

class MmapControl {
private:

    MmapControl &operator=(const MmapControl &other) {}

    MmapControl(const MmapControl &other) {}

public:

    Vector<mmap_unit> memKeeper;

    size_t pageTable;

    size_t mmap_start=MMAP_VIRT_BEGIN;

    explicit MmapControl(size_t pageTable) : pageTable(pageTable) {}

    MmapControl(const MmapControl &other, size_t pageTable) : pageTable(pageTable) {

        mmap_start=other.mmap_start;

        // copy pages
        for (auto i=0;i<other.memKeeper.size;i++) {
            size_t new_page = alloc_page();
            memcpy((void *) new_page, (const void*)(other.memKeeper[i].source_addr), 4096);

            memKeeper.push_back({new_page,other.memKeeper[i].mapped_to});
            PageTableUtil::CreateMapping(
                    pageTable,
                    other.memKeeper[i].mapped_to,
                    new_page,
                    PAGE_TABLE_LEVEL::SMALL,
                    PRIVILEGE_LEVEL::USER);
        }

    }

    size_t mmap(size_t length,int fd) {
        size_t need_page=(length+4096-1)/4096; //align
        size_t ret=mmap_start;
        for(int i=0;i<need_page;i++){
            size_t new_page = alloc_page();
            memKeeper.push_back({mmap_start,new_page});
            PageTableUtil::CreateMapping(
                    pageTable,
                    mmap_start,
                    new_page,
                    PAGE_TABLE_LEVEL::SMALL,
                    PRIVILEGE_LEVEL::USER);
            mmap_start+=4096;
        }
        PageTableUtil::FlushCurrentPageTable();
        UINT read_bytes;
//        size_t start=timer();
        f_read(LastOpenedFile,(void*)ret,length,&read_bytes);
//        printf("<read cost %d>\n",timer()-start);
        f_lseek(LastOpenedFile,0);
        return ret;
    }

    int munmap(size_t addr,size_t length){
        if(addr%0x1000!=0){
            panic("munmap address not aligned!")
        }
//        for(size_t p=addr;p<addr+length;p+=4096){
        for(size_t p=(addr+length-1)/4096*4096;p>=addr;p-=4096){
            // free
            for(auto i=memKeeper.size-1;i>=0;i--){
                if(memKeeper[i].mapped_to==p){
                    dealloc_page(memKeeper[i].source_addr);
                    //todo: free page table
                    memKeeper.erase(i);
                    goto munmap_next;
                }
            }
            panic("munmap address is not mapped!")
            munmap_next:;
        }
        if(memKeeper.is_empty())mmap_start=MMAP_VIRT_BEGIN;
        PageTableUtil::FlushCurrentPageTable();
        return 0;
    }

};

#endif //SYSCORE_MMAP_CONTROL_H
