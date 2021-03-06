#ifndef __SDCARD_H
#define __SDCARD_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

int sdcard_init(void);

void sdcard_read_sector(uint8 *buf, int sectorno);

void sdcard_write_sector(uint8 *buf, int sectorno);

#ifdef __cplusplus
}
#endif

#endif
