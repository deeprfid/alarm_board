#ifndef _HWPORT_H_
#define _HWPORT_H_
#include "type.h"
#include "hc32_ll.h"

void flash_bytes_read(uint32_t addr, void *buf, uint16_t len);
int  flash_sector_erase(uint32_t dest);
int  flash_bytes_write(uint32_t addr, void *buf, uint16_t len);
#endif








