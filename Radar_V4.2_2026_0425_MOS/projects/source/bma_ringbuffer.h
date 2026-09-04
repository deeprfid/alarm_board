/*
 * Copyright (c) 2022, Egahp
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CHRY_RINGBUFFER_H
#define CHRY_RINGBUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t in;   /*!< Define the write pointer.               */
    uint32_t out;  /*!< Define the read pointer.                */
    uint32_t mask; /*!< Define the write and read pointer mask. */
    void *pool;    /*!< Define the memory pointer.              */
} BMA_ringbuffer_t;

extern int BMA_ringbuffer_init(BMA_ringbuffer_t *rb, void *pool, uint32_t size);
extern void BMA_ringbuffer_reset(BMA_ringbuffer_t *rb);
extern void BMA_ringbuffer_reset_read(BMA_ringbuffer_t *rb);

extern uint32_t BMA_ringbuffer_get_size(BMA_ringbuffer_t *rb);
extern uint32_t BMA_ringbuffer_get_used(BMA_ringbuffer_t *rb);
extern uint32_t BMA_ringbuffer_get_free(BMA_ringbuffer_t *rb);

extern bool BMA_ringbuffer_check_full(BMA_ringbuffer_t *rb);
extern bool BMA_ringbuffer_check_empty(BMA_ringbuffer_t *rb);

extern bool BMA_ringbuffer_write_byte(BMA_ringbuffer_t *rb, uint8_t byte);
extern bool BMA_ringbuffer_overwrite_byte(BMA_ringbuffer_t *rb, uint8_t byte);
extern bool BMA_ringbuffer_peek_byte(BMA_ringbuffer_t *rb, uint8_t *byte);
extern bool BMA_ringbuffer_read_byte(BMA_ringbuffer_t *rb, uint8_t *byte);
extern bool BMA_ringbuffer_drop_byte(BMA_ringbuffer_t *rb);

extern uint32_t BMA_ringbuffer_write(BMA_ringbuffer_t *rb, void *data, uint32_t size);
extern uint32_t BMA_ringbuffer_overwrite(BMA_ringbuffer_t *rb, void *data, uint32_t size);
extern uint32_t BMA_ringbuffer_peek(BMA_ringbuffer_t *rb, void *data, uint32_t size);
extern uint32_t BMA_ringbuffer_read(BMA_ringbuffer_t *rb, void *data, uint32_t size);
extern uint32_t BMA_ringbuffer_drop(BMA_ringbuffer_t *rb, uint32_t size);

extern void *BMA_ringbuffer_linear_write_setup(BMA_ringbuffer_t *rb, uint32_t *size);
extern void *BMA_ringbuffer_linear_read_setup(BMA_ringbuffer_t *rb, uint32_t *size);
extern uint32_t BMA_ringbuffer_linear_write_done(BMA_ringbuffer_t *rb, uint32_t size);
extern uint32_t BMA_ringbuffer_linear_read_done(BMA_ringbuffer_t *rb, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif
