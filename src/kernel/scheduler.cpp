#include "scheduler.h"
#include "self_test.h"
#include "fs/VFS.h"
#include "memory/kernel_stack.h"
#include "memory/memory.h"
#include "memory/elf_control.h"

extern "C" {
#include "../driver/fatfs/ff.h"
}

int global_pid = 1;

// Warning: when do sth with running, sync with latest running_context first
Context *running_context;

PCB *search_by_pid(List<PCB *> **list, int pid) {
    // search in running
    if (running != nullptr && running->pid == pid) {
        *list = nullptr;
        return running;
    }
    // search in runnable
    auto cnt = runnable.start;
    while (cnt != nullptr) {
        // printf(">> runnable pid=%d\n",cnt->PCB->pid);
        if (cnt->data->pid == pid) {
            *list = &runnable;
            return cnt->data;
        }
        cnt = cnt->next;
    }
    // search in blocked
    cnt = blocked.start;
    while (cnt != nullptr) {
        // printf(">> blocked pid=%d\n",cnt->PCB->pid);
        if (cnt->data->pid == pid) {
            *list = &blocked;
            return cnt->data;
        }
        cnt = cnt->next;
    }
    return nullptr;
}

int get_new_pid() {
    return ++global_pid;
}

char *get_running_cwd() {
    return running->cwd;
}

void file_describer_bind(size_t file_id, size_t real_file_describer) {
    running->occupied_file_describer->put(file_id, real_file_describer);
}

void file_describer_erase(size_t file_id) {
    running->occupied_file_describer->erase(file_id);
}

bool file_describer_exists(size_t file_id) {
    return running->occupied_file_describer->exists(file_id);
}

size_t file_describer_convert(size_t file_id) {
    return running->occupied_file_describer->get(file_id);
}

void bind_kernel_heap(size_t addr) {
    running->occupied_kernel_heap->push_back(addr);
}

void bind_pages(size_t addr) {
    running->occupied_pages->push_back(addr);
}

List<PCB *> runnable, blocked;
PCB *running;

void init_scheduler() {
#ifndef QEMU
    running_context = reinterpret_cast<Context *>(0x80000000 + 8 * 1024 * 1024 - sizeof(Context));
#else
    running_context= reinterpret_cast<Context *>(0x89000000 + 8 * 1024 * 1024 - sizeof(Context));
#endif
    runnable.start = runnable.end = nullptr;
    blocked.start = blocked.end = nullptr;
    running = nullptr;
}

void clone(int flags, size_t stack,int* parent_tid, size_t tls,int* child_tid) {

    printf("[clone ]flag=0x%x\n", flags);

    // sync with running_context
    *running->thread_context = *running_context;

    // copy context
    Context *child_context = new Context();
    *child_context = *running->thread_context;

    // copy PCB
    PCB *child_pcb = new PCB();

    // set pid and ppid
    child_pcb->ppid = running->pid;
    child_pcb->pid = get_new_pid();
    child_pcb->stack=running->stack;
    child_pcb->stack_size=running->stack_size;

    child_pcb->page_table=running->page_table;
    child_pcb->elf_control=running->elf_control;
    child_pcb->kernel_phdr=running->kernel_phdr;
    child_pcb->brk_control=running->brk_control;
    child_pcb->mmap_control=running->mmap_control;

    child_pcb->thread_context = child_context;

    child_pcb->occupied_file_describer=running->occupied_file_describer;
    child_pcb->occupied_kernel_heap=running->occupied_kernel_heap;
    child_pcb->occupied_pages=running->occupied_pages;

    // set syscall return value
    running->thread_context->a0 = child_pcb->pid;
    child_context->a0 = 0;

    if (stack != 0) {
        // fixed stack, will not copy spaces
        child_pcb->thread_context->sp = stack;
        child_pcb->thread_context->a0 = 0;
        child_context->sepc += 4;
        runnable.push_back(child_pcb);
    } else {
        // fork, generate a new stack
        size_t stack = alloc_page(running->stack_size);
        memcpy(reinterpret_cast<void *>(stack), reinterpret_cast<const void *>(running->stack), running->stack_size);

        size_t page_table = alloc_page(4096);
        memset(reinterpret_cast<void *>(page_table), 0, 4096);
        PageTableUtil::init_pageTable(page_table);
        child_context->satp = (page_table >> 12) | (8LL << 60);
        child_context->sepc += 4;

        child_pcb->thread_context->a0 = 0;
        child_pcb->stack = stack;
        child_pcb->thread_context->sp = running->thread_context->sp - running->stack + stack;
        child_pcb->page_table = RefCountPtr<size_t>((size_t *) page_table);
        child_pcb->kernel_phdr = running->kernel_phdr;

        child_pcb->elf_control= RefCountPtr<Elf_Control>(new Elf_Control(*running->elf_control, page_table));
        child_pcb->brk_control = RefCountPtr<BrkControl>(new BrkControl(*running->brk_control, page_table));
        child_pcb->mmap_control=RefCountPtr<MmapControl>(new MmapControl(*running->mmap_control,page_table));

        runnable.push_back(child_pcb);
    }
    // sync with running_context
    *running_context = *running->thread_context;
}

int get_running_pid() {
    return running->pid;
}

int get_running_ppid() {
    return running->ppid;
}

void copy_to_stack(char *&sp, const void *_source, int length) {
    const char *source = static_cast<const char *>(_source);
    sp -= length;
    for (int i = 0; i < length; i++) {
        sp[i] = source[i];
    }
}

void put_aux(size_t **sp, size_t aux_id, size_t aux_val) {
    *(--*sp) = aux_val;
    *(--*sp) = aux_id;
}

void put_envp(size_t **sp, size_t aux_val) {
    *(--*sp) = aux_val;
}

void check_stack_preparation(size_t st) {
    size_t *sp = reinterpret_cast<size_t *>(st);
    int argc = *(sp++);
    printf("argc= %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d]: %s\n", i, *(sp++));
    }
    // zero
    printf("zero: %d\n", *(sp++));
    // envp
    while (*sp != 0) {
        printf("[envp] %s\n", *(sp++));
    }
    // zero
    printf("zero: %d\n", *(sp++));
    // aux
    while (*sp != 0) {
        if (*sp == 31 || *sp == 33) {
            printf("[aux] %d  ", *(sp++));
            printf("[aux] %s\n", *(sp++));
        } else {
            printf("[aux] %d  ", *(sp++));
            printf("[aux] 0x%x\n", *(sp++));
        }
    }
    printf("stack check end\n");
}

void create_process(const char *_command) {
    char *command = new char[strlen(_command) + 1];
    strcpy(command, _command);
    // split command with space
    List<char *> splits;
    char *p = command;

    bool in_space = true;
    while (*p) {
        if (*p == '"') {
            p++;
            splits.push_back(p);
            while (*p != '"')p++;
            // *p = '"'
            *p = '\0';
            p++;
            in_space = true;
            continue;
        }
        if (*p == ' ') {
            if (!in_space) {
                in_space = true;
                *p = '\0';
            }
        } else {
            if (in_space) {
                in_space = false;
                splits.push_back(p);
            }
        }
        p++;
    }

    int argc = splits.length();
    char **argv = new char *[argc];

    auto i = splits.start;
    char *elf = i->data;
    i = i->next;

    int pos = 0;
    while (i) {
        argv[pos++] = i->data;
        i = i->next;
    }
    argv[pos] = nullptr;

    create_process(elf, (const char **) argv);
}

void CreateSoftLink(const char *elf_path) {
    auto *exe = fs->root->first_child->search("/proc/self/exe");
    assert(exe != nullptr);
    auto *elf = fs->root->first_child->search(elf_path);
    assert(elf != nullptr);
    exe->CreateSoftLink(elf);
}

size_t rrr = 12345678;

void create_process(const char *elf_path, const char *argv[]) {
    /**
     * 页表处理
     * 1. satp应由物理页首地址右移12位并且或上（8 << 60），表示开启sv39分页模式
     * 2. 未使用的页表项应该置0
     */
    size_t page_table_base = PageTableUtil::GetClearPage();
    PageTableUtil::init_pageTable(page_table_base);

    FIL elf_file;
//    printf("elf %s\n", elf_path);
    int res = f_open(&elf_file, elf_path, FA_READ);
    if (res != FR_OK) {
        panic("read error")
    }
    size_t entry, ph_off;
    Elf64_Phdr *kernel_phdr;
    int ph_num;
    RefCountPtr<Elf_Control> elf_control(new Elf_Control(page_table_base));
    load_elf(&elf_file, elf_control.getPtr(), &entry, &ph_off, &ph_num, &kernel_phdr);
    f_close(&elf_file);

    Context *thread_context = new Context();
    thread_context->sstatus = register_read_sstatus();
    /**
     * 用户栈
     * 栈通常向低地址方向增长，故此处增加__page_size
     */
    size_t stack_page = (size_t) alloc_page(4096);
    memset(reinterpret_cast<void *>(stack_page), 0, 4096);
    thread_context->sp = stack_page + __page_size - 10 * 8;

    const char *env[] = {
            "SHELL=ash",
            "PWD=/",
            "HOME=/",
            "USER=root",
            "MOTD_SHOWN=pam",
            "LANG=C.UTF-8",
            "INVOCATION_ID=e9500a871cf044d9886a157f53826684",
            "TERM=vt220",
            "SHLVL=2",
            "JOURNAL_STREAM=8:9265",
            "PATH=/",
            "OLDPWD=/",
            "_=busybox"
    };

    // copy envp strings
    size_t sp = (thread_context->sp);
    List<size_t> envp_strings;
    char filename[] = "busybox";
    copy_to_stack(reinterpret_cast<char *&>(sp), filename, strlen(filename) + 1);
    size_t filename_addr = sp;
    for (auto item:env) {
        copy_to_stack(reinterpret_cast<char *&>(sp), item, strlen(item) + 1);
        envp_strings.push_back(sp);
    }
    List<size_t> argv_strings;
    // copy argv strings
    if (argv) {
        // if has argv
        for (int i = 0;; i++) {
            if (argv[i]) {
                // copy argv
                copy_to_stack(reinterpret_cast<char *&>(sp), argv[i], strlen(argv[i]) + 1);
                argv_strings.push_front(sp); // reverse
            } else {
                break;
            }
        }
    }
    sp -= sp % 16; //align
    // aux environments

    put_aux((size_t **) &sp, AT_NULL, 0);
    put_aux((size_t **) &sp, 0x28, 0);
    put_aux((size_t **) &sp, 0x29, 0);
    put_aux((size_t **) &sp, 0x2a, 0);
    put_aux((size_t **) &sp, 0x2b, 0);
    put_aux((size_t **) &sp, 0x2c, 0);
    put_aux((size_t **) &sp, 0x2d, 0);

    put_aux((size_t **) &sp, AT_PHDR, (size_t) kernel_phdr);               // 3
    put_aux((size_t **) &sp, AT_PHENT, sizeof(Elf64_Phdr));  // 4
    put_aux((size_t **) &sp, AT_PHNUM, ph_num);              // 5
    put_aux((size_t **) &sp, AT_PAGESZ, 0x1000);                 // 6
    put_aux((size_t **) &sp, AT_BASE, 0);                        // 7
    put_aux((size_t **) &sp, AT_FLAGS, 0);                       // 8
    put_aux((size_t **) &sp, AT_ENTRY, entry);              // 9
    put_aux((size_t **) &sp, AT_UID, 0);                         // 11
    put_aux((size_t **) &sp, AT_EUID, 0);                        // 12
    put_aux((size_t **) &sp, AT_GID, 0);                         // 13
    put_aux((size_t **) &sp, AT_EGID, 0);                        // 14
    put_aux((size_t **) &sp, AT_HWCAP, 0x112d);                  // 16
    put_aux((size_t **) &sp, AT_CLKTCK, 64);                     // 17
    put_aux((size_t **) &sp, AT_RANDOM, (size_t) &rrr);                     // 17
    put_aux((size_t **) &sp, AT_EXECFN, filename_addr);       // 31


//    put_aux((size_t**)&sp,0, 0);
//    put_aux((size_t**)&sp,0, 0);
//    put_aux((size_t**)&sp,AT_EXECFN, filename_addr); //31
//    put_aux((size_t**)&sp,AT_CLKTCK, 0x64); //17
//    put_aux((size_t**)&sp,AT_HWCAP, 0x112d); //16
//    put_aux((size_t**)&sp,AT_EGID, 0); //14
//    put_aux((size_t**)&sp,AT_GID, 0); //13
//    put_aux((size_t**)&sp,AT_EUID, 0); //12
//    put_aux((size_t**)&sp,AT_UID, 0); //11
//    put_aux((size_t**)&sp,AT_ENTRY, entry); //9
//    put_aux((size_t**)&sp,AT_FLAGS, 0); //8
//    put_aux((size_t**)&sp,AT_BASE, 0); //7
//    put_aux((size_t**)&sp,AT_PAGESZ, 0x1000); //6
//    put_aux((size_t**)&sp,AT_PHNUM, ph_num); //5
//    put_aux((size_t**)&sp,AT_PHENT, sizeof(Elf64_Phdr)); //4 -> 0x38(ss_new)
//    put_aux((size_t**)&sp,AT_PHDR, ph_off); //3 !
//    put_aux((size_t**)&sp,45, 0);
//    put_aux((size_t**)&sp,44, 0);
//    put_aux((size_t**)&sp,43, 0);
//    put_aux((size_t**)&sp,42, 0);
//    put_aux((size_t**)&sp,41, 0);
//    put_aux((size_t**)&sp,40, 0);
//    put_aux((size_t**)&sp,33, elf_str_addr);
    sp -= 8; // 0 word
    // envp
    for (auto i = envp_strings.start; i; i = i->next) {
        put_envp((size_t **) &sp, i->data);
    }
    sp -= 8; // 0 word
    // argument pointers: argv
    for (auto i = argv_strings.start; i; i = i->next) {
        put_envp((size_t **) &sp, i->data);
    }
    put_envp((size_t **) &sp, filename_addr);
    size_t argv_start = sp;
    // argc
    put_envp((size_t **) &sp, argv_strings.length() + 1);

//    check_stack_preparation(sp);

    thread_context->sp = reinterpret_cast<size_t>(sp);

    /**
     * 此处spp应为0,表示user-mode
     */
    thread_context->sstatus |= REGISTER_SSTATUS_SPP; // spp = 1
    thread_context->sstatus ^= REGISTER_SSTATUS_SPP; // spp = 0
    /**
     * 此处spie应为1,表示user-mode允许中断
     */
    thread_context->sstatus |= REGISTER_SSTATUS_SPIE; // spie = 1
    /**
     * 此处sepc为中断后返回地址
     */
    thread_context->sepc = entry;

//    *((size_t *) page_table_base + 2) = (0x80000 << 10) | 0xdf;
    thread_context->satp = (page_table_base >> 12) | (8LL << 60);

    // push into runnable list
    PCB *new_pcb = new PCB();

    new_pcb->pid = get_new_pid();
    new_pcb->ppid = 1;

    new_pcb->stack = stack_page;
    new_pcb->stack_size = 4096;

    new_pcb->page_table = RefCountPtr<size_t>((size_t *) page_table_base);
    new_pcb->elf_control = elf_control;
    new_pcb->kernel_phdr = RefCountPtr<Elf64_Phdr>(kernel_phdr);
    new_pcb->brk_control = RefCountPtr<BrkControl>(new BrkControl(page_table_base));
    new_pcb->mmap_control=RefCountPtr<MmapControl>(new MmapControl(page_table_base));

    new_pcb->thread_context = thread_context;

    // TODO: 初始化工作目录为/，这不合理
    memset(new_pcb->cwd, 0, sizeof(new_pcb->cwd));
    new_pcb->cwd[0] = '/';

    new_pcb->occupied_file_describer=RefCountPtr<Map<size_t,size_t>>(new Map<size_t,size_t>());
    new_pcb->occupied_kernel_heap=RefCountPtr<List<size_t>>(new List<size_t>);
    new_pcb->occupied_pages=RefCountPtr<List<size_t>>(new List<size_t>);

    runnable.push_back(new_pcb);
}

void __yield() {
    // sync with running_context
    *running->thread_context = *running_context;

    runnable.push_back(running);
    running = nullptr;
    schedule();
}

void yield() {
    // sync with running_context
    *running->thread_context = *running_context;
    running->thread_context->sepc += 4;

    runnable.push_back(running);
    running = nullptr;
    schedule();
}

void execute(const char *exec_path) {
    // deal with path "/"
    if (exec_path[0] != '/') {
        char buf[strlen(exec_path) + strlen(running->cwd) + 2];
        if (strcmp(running->cwd, "/") == 0) {
            sprintf(buf, "/%s", exec_path);
        } else {
            sprintf(buf, "%s/%s", running->cwd, exec_path);
        }
        create_process(exec_path);
        exit_process(0);
    } else {
        create_process(exec_path);
        exit_process(0);
    }
}

void exit_process(int exit_ret) {
    // printf("process %d start to exit\n",running->pid);
    running->kill(exit_ret);

    delete running;
    running = nullptr;
    schedule();
}

void schedule() {
    if (running != nullptr) {
        // sync with running_context
        *running_context = *running->thread_context;

        __restore();
    } else {
        if (!runnable.is_empty()) {
            // pick one to run
            running = runnable.start->data;lty(running);lty(running->elf_page_base);
            runnable.pop_front();

            lty(running->thread_context->satp);lty(running->thread_context->sepc);

            // printf("trying to restore\n");

            // sync with running_context
            *running_context = *running->thread_context;

            __restore();
        } else if (!blocked.is_empty()) {
            // search one to unfreeze
            auto cnt = blocked.start;
            while (cnt != nullptr) {
                if (!cnt->data->signal_list.is_empty()) {
                    // has signal, wake up
                    running = cnt->data;
                    // remove it from blocked list
                    blocked.erase(cnt);
                    // get signal to return value
                    running->thread_context->a0 = running->signal_list.start->data.first;
                    if (running->wstatus) {
                        *running->wstatus = running->signal_list.start->data.second << 8;
                    }
                    running->signal_list.pop_front();

                    schedule();
                }
                cnt = cnt->next;
            }
            panic("There exists a dead waiting loop. All processes are blocked.");
        } else {
            if (has_next_test()) {
                create_process(get_next_test());
                schedule();
            } else {
                printf(">>> Nothing to run, shutdown. <<<\n");
                shutdown();
            }
        }
    }
}

int wait(int *wstatus) {
//    printf("process %d start to wait\n",running->pid);
    if (!running->signal_list.is_empty()) {
        // return immediately
        int ret = running->signal_list.start->data.first;
//        if(wstatus){
//            *wstatus=running->signal_list.start->data.second<<8;
//        }
        running->signal_list.pop_front();
        return ret;
    }

    // move to blocked list

    // sync with running_context
    *running->thread_context = *running_context;
    running->thread_context->sepc += 4;
    running->wait_pid = 0;
    running->wstatus = wstatus;

    blocked.push_back(running);
    running = nullptr;
    schedule();
}

void PCB::kill(int exit_ret) {
    // send signal
    List<PCB *> *list;
    PCB *parent = search_by_pid(&list, ppid);
    if (parent != nullptr) {
        // printf("unfreeze pid=%d\n",parent->pid);
        parent->signal_list.push_back(make_pair(pid, exit_ret));
    }
    // TODO: dealloc_page(running->page_table);
    dealloc_page(stack);
    delete thread_context;

    // free file describer
    // TODO: free file describer

}