#include "register.h"

size_t register_read_sip() {
    size_t res;
    asm volatile(
    "csrr %0, sip"
    :"=r"(res)
    );
    return res;
}

size_t register_read_sstatus() {
    size_t res;
    asm volatile(
    "csrr %0, sstatus"
    :"=r"(res)
    );
    return res;
}

size_t register_read_satp() {
    size_t res;
    asm volatile(
    "csrr %0, satp"
    :"=r"(res)
    );
    return res;
}

size_t register_read_sscratch() {
    size_t res;
    asm volatile(
    "csrr %0, sscratch"
    :"=r"(res)
    );
    return res;
}

size_t register_read_sp() {
    size_t res;
    asm volatile(
    "mv %0, sp"
    :"=r"(res)
    );
    return res;
}

size_t register_read_pc() {
    asm volatile ("auipc t0, 0");
    size_t res;
    asm volatile(
    "mv %0, t0"
    :"=r"(res)
    );
    return res;
}