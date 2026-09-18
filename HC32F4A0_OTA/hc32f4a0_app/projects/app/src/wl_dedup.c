/* wl_dedup.c - 白名单 EPC 位图去重（v9.81cj-k） */
#include "wl_dedup.h"
#include <string.h>

static uint8_t  s_bitmap[WL_DEDUP_BYTES];
static uint32_t s_count = 0;

static uint32_t wl_hash(const uint8_t *epc, uint8_t len)
{
    uint32_t h = 2166136261u;
    uint8_t i;
    for (i = 0; i < len; i++) { h ^= epc[i]; h *= 16777619u; }
    return h;
}

void wl_dedup_reset(void)
{
    memset(s_bitmap, 0, sizeof(s_bitmap));
    s_count = 0;
}

int wl_dedup_check(const uint8_t *epc, uint8_t len)
{
    uint32_t idx = wl_hash(epc, len) % WL_DEDUP_BITS;
    uint32_t byte = idx >> 3;
    uint8_t  bit  = (uint8_t)(idx & 7);
    if (s_bitmap[byte] & (1u << bit))
        return 0;
    s_bitmap[byte] |= (1u << bit);
    s_count++;
    return 1;
}

uint32_t wl_dedup_count(void)
{
    return s_count;
}
