#ifndef __REGISTER_H__
#define __REGISTER_H__

#include "stddef.h"

// supervisor-level timer interrupts
#define REGISTER_SIP_STIE (1 << 5)
// the privilege level at which a hart was executing previous entering supervisor mode
#define REGISTER_SSTATUS_SPP (1 << 8)
// whether supervisor interrupts were enabled prior to trapping into supervisor mode
#define REGISTER_SSTATUS_SPIE (1 << 5)
// Permit Supervisor User Memory access
#define REGISTER_SSTATUS_SUM (1 << 18)
// Global Interrupt Enable
#define REGISTER_SSTATUS_SIE (1 << 1)

size_t register_read_sip();
size_t register_read_sstatus();
size_t register_read_sp();
size_t register_read_satp();
size_t register_read_pc();

#endif

