/*
 * Copyright (c) 2022, Egahp
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "main.h"
/*****************************************************************************
* @brief        init ringbuffer
* 
* @param[in]    rb          ringbuffer instance
* @param[in]    pool        memory pool address
* @param[in]    size        memory size in byte,
*                           must be power of 2 !!!
* 
* @retval int               0:Success -1:Error
*****************************************************************************/
int BMA_ringbuffer_init(BMA_ringbuffer_t *rb, void *pool, uint32_t size)
{
    if (NULL == rb) {
        return -1;
    }

    if (NULL == pool) {
        return -1;
    }

    if ((size < 2) || (size & (size - 1))) {
        return -1;
    }

    rb->in = 0;
    rb->out = 0;
    rb->mask = size - 1;
    rb->pool = pool;

    return 0;
}

/*****************************************************************************
* @brief        reset ringbuffer, clean all data, 
*               should be add lock in multithread
* 
* @param[in]    rb          ringbuffer instance
* 
*****************************************************************************/
void BMA_ringbuffer_reset(BMA_ringbuffer_t *rb)
{
     uint32_t primask = __get_PRIMASK();
    __disable_irq();
    rb->in  = 0;
    rb->out = 0;
         /* Atomic end */
  if (primask == 0U) {
    __enable_irq();
  }
}

/*****************************************************************************
* @brief        reset ringbuffer, clean all data,
*               should be add lock in multithread,
*               in single read thread not need lock
* 
* @param[in]    rb          ringbuffer instance
* 
*****************************************************************************/
void BMA_ringbuffer_reset_read(BMA_ringbuffer_t *rb)
{
     uint32_t primask = __get_PRIMASK();
    __disable_irq();
    rb->out = rb->in;
          /* Atomic end */
  if (primask == 0U) {
    __enable_irq();
  }
}

/*****************************************************************************
* @brief        get ringbuffer total size in byte
* 
* @param[in]    rb          ringbuffer instance
* 
* @retval uint32_t          total size in byte
*****************************************************************************/
uint32_t BMA_ringbuffer_get_size(BMA_ringbuffer_t *rb)
{
    return rb->mask + 1;
}

/*****************************************************************************
* @brief        get ringbuffer used size in byte
* 
* @param[in]    rb          ringbuffer instance
* 
* @retval uint32_t          used size in byte
*****************************************************************************/
uint32_t BMA_ringbuffer_get_used(BMA_ringbuffer_t *rb)
{
    return rb->in - rb->out;
}

/*****************************************************************************
* @brief        get ringbuffer free size in byte
* 
* @param[in]    rb          ringbuffer instance
* 
* @retval uint32_t          free size in byte
*****************************************************************************/
uint32_t BMA_ringbuffer_get_free(BMA_ringbuffer_t *rb)
{
    return (rb->mask + 1) - (rb->in - rb->out);
}

/*****************************************************************************
* @brief        check if ringbuffer is full
* 
* @param[in]    rb          ringbuffer instance
* 
* @retval true              full
* @retval false             not full
*****************************************************************************/
bool BMA_ringbuffer_check_full(BMA_ringbuffer_t *rb)
{
    return BMA_ringbuffer_get_used(rb) > rb->mask;
}

/*****************************************************************************
* @brief        check if ringbuffer is empty
* 
* @param[in]    rb          ringbuffer instance
* 
* @retval true              empty
* @retval false             not empty
*****************************************************************************/
bool BMA_ringbuffer_check_empty(BMA_ringbuffer_t *rb)
{
    return rb->in == rb->out;
}

/*****************************************************************************
* @brief        write one byte to ringbuffer,
*               should be add lock in multithread,
*               in single write thread not need lock
* 
* @param[in]    rb          ringbuffer instance
* @param[in]    byte        data
* 
* @retval true              Success
* @retval false             ringbuffer is full
*****************************************************************************/
bool BMA_ringbuffer_write_byte(BMA_ringbuffer_t *rb, uint8_t byte)
{
    if (BMA_ringbuffer_check_full(rb)) {
        return false;
    }
     uint32_t primask = __get_PRIMASK();
    __disable_irq();
    ((uint8_t *)(rb->pool))[rb->in & rb->mask] = byte;
    rb->in++;
          /* Atomic end */
  if (primask == 0U) {
    __enable_irq();
  }
    return true;
}

/*****************************************************************************
* @brief        overwrite one byte to ringbuffer, drop oldest data,
*               should be add lock always
*
* @param[in]    rb          ringbuffer instance
* @param[in]    byte        data
* 
* @retval true              Success
* @retval false             always return true
*****************************************************************************/
bool BMA_ringbuffer_overwrite_byte(BMA_ringbuffer_t *rb, uint8_t byte)
{
     uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (BMA_ringbuffer_check_full(rb)) {
        rb->out++;
    }

    ((uint8_t *)(rb->pool))[rb->in & rb->mask] = byte;
    rb->in++;
    /* Atomic end */
  if (primask == 0U) {
    __enable_irq();
  }
    return true;
}

/*****************************************************************************
* @brief        peek one byte from ringbuffer,
*               should be add lock in multithread,
*               in single read thread not need lock
* 
* @param[in]    rb          ringbuffer instance
* @param[in]    byte        pointer to save data
* 
* @retval true              Success
* @retval false             ringbuffer is empty
*****************************************************************************/
bool BMA_ringbuffer_peek_byte(BMA_ringbuffer_t *rb, uint8_t *byte)
{
    if (BMA_ringbuffer_check_empty(rb)) {
        return false;
    }
     uint32_t primask = __get_PRIMASK();
    __disable_irq();
    *byte = ((uint8_t *)(rb->pool))[rb->out & rb->mask];
          /* Atomic end */
  if (primask == 0U) {
    __enable_irq();
  }
    return true;
}

/*****************************************************************************
* @brief        read one byte from ringbuffer,
*               should be add lock in multithread,
*               in single read thread not need lock
* 
* @param[in]    rb          ringbuffer instance
* @param[in]    byte        pointer to save data
* 
* @retval true              Success
* @retval false             ringbuffer is empty
*****************************************************************************/
bool BMA_ringbuffer_read_byte(BMA_ringbuffer_t *rb, uint8_t *byte)
{
    bool ret;
     uint32_t primask = __get_PRIMASK();
    __disable_irq();
    ret = BMA_ringbuffer_peek_byte(rb, byte);
    rb->out += ret;
          /* Atomic end */
  if (primask == 0U) {
    __enable_irq();
  }
    return ret;
}

/*****************************************************************************
* @brief        drop one byte from ringbuffer,
*               should be add lock in multithread,
*               in single read thread not need lock
* 
* @param[in]    rb          ringbuffer instance
* 
* @retval true              Success
* @retval false             ringbuffer is empty
*****************************************************************************/
bool BMA_ringbuffer_drop_byte(BMA_ringbuffer_t *rb)
{
    if (BMA_ringbuffer_check_empty(rb)) {
        return false;
    }
     uint32_t primask = __get_PRIMASK();
    __disable_irq();
    rb->out += 1;
          /* Atomic end */
  if (primask == 0U) {
    __enable_irq();
  }
    return true;
}

/*****************************************************************************
* @brief        write data to ringbuffer,
*               should be add lock in multithread,
*               in single write thread not need lock
* 
* @param[in]    rb          ringbuffer instance
* @param[in]    data        data pointer
* @param[in]    size        size in byte
* 
* @retval uint32_t          actual write size in byte
*****************************************************************************/
uint32_t BMA_ringbuffer_write(BMA_ringbuffer_t *rb, void *data, uint32_t size)
{
    uint32_t unused;
    uint32_t offset;
    uint32_t remain;
    uint32_t primask = __get_PRIMASK();
    __disable_irq(); 
    unused = (rb->mask + 1) - (rb->in - rb->out);

    if (size > unused) {
        size = unused;
    }

    offset = rb->in & rb->mask;

    remain = rb->mask + 1 - offset;
    remain = remain > size ? size : remain;

    memcpy(((uint8_t *)(rb->pool)) + offset, data, remain);
    memcpy(rb->pool, (uint8_t *)data + remain, size - remain);

    rb->in += size;
          /* Atomic end */
  if (primask == 0U) {
    __enable_irq();
  }
    return size;
}

/*****************************************************************************
* @brief        write data to ringbuffer,
*               should be add lock always
* 
* @param[in]    rb          ringbuffer instance
* @param[in]    data        data pointer
* @param[in]    size        size in byte
* 
* @retval uint32_t          actual write size in byte
*****************************************************************************/
uint32_t BMA_ringbuffer_overwrite(BMA_ringbuffer_t *rb, void *data, uint32_t size)
{
    uint32_t unused;
    uint32_t offset;
    uint32_t remain;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    unused = (rb->mask + 1) - (rb->in - rb->out);

    if (size > unused) {
        if (size > (rb->mask + 1)) {
            size = rb->mask + 1;
        }

        rb->out += size - unused;
    }

    offset = rb->in & rb->mask;

    remain = rb->mask + 1 - offset;
    remain = remain > size ? size : remain;

    memcpy(((uint8_t *)(rb->pool)) + offset, data, remain);
    memcpy(rb->pool, (uint8_t *)data + remain, size - remain);

    rb->in += size;
          /* Atomic end */
  if (primask == 0U) {
    __enable_irq();
  }
    return size;
}

/*****************************************************************************
* @brief        peek data from ringbuffer
*               should be add lock in multithread,
*               in single read thread not need lock
* 
* @param[in]    rb          ringbuffer instance
* @param[in]    data        data pointer
* @param[in]    size        size in byte
* 
* @retval uint32_t          actual peek size in byte
*****************************************************************************/
uint32_t BMA_ringbuffer_peek(BMA_ringbuffer_t *rb, void *data, uint32_t size)
{
    uint32_t used;
    uint32_t offset;
    uint32_t remain;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    used = rb->in - rb->out;
    if (size > used) {
        size = used;
    }

    offset = rb->out & rb->mask;

    remain = rb->mask + 1 - offset;
    remain = remain > size ? size : remain;

    memcpy(data, ((uint8_t *)(rb->pool)) + offset, remain);
    memcpy((uint8_t *)data + remain, rb->pool, size - remain);
     /* Atomic end */
  if (primask == 0U) {
    __enable_irq();
  }
    return size;
}

/*****************************************************************************
* @brief        read data from ringbuffer
*               should be add lock in multithread,
*               in single read thread not need lock
* 
* @param[in]    rb          ringbuffer instance
* @param[in]    data        data pointer
* @param[in]    size        size in byte
* 
* @retval uint32_t          actual read size in byte
*****************************************************************************/
uint32_t BMA_ringbuffer_read(BMA_ringbuffer_t *rb, void *data, uint32_t size)
{
     uint32_t primask = __get_PRIMASK();
    __disable_irq();
    size = BMA_ringbuffer_peek(rb, data, size);
    rb->out += size;
          /* Atomic end */
  if (primask == 0U) {
    __enable_irq();
  }
    return size;
}

/*****************************************************************************
* @brief        drop data from ringbuffer
*               should be add lock in multithread,
*               in single read thread not need lock
* 
* @param[in]    rb          ringbuffer instance
* @param[in]    size        size in byte
* 
* @retval uint32_t          actual drop size in byte
*****************************************************************************/
uint32_t BMA_ringbuffer_drop(BMA_ringbuffer_t *rb, uint32_t size)
{
    uint32_t used;
     
     uint32_t primask = __get_PRIMASK();
    __disable_irq();
    used = rb->in - rb->out;
    if (size > used) {
        size = used;
    }

    rb->out += size;
          /* Atomic end */
  if (primask == 0U) {
    __enable_irq();
  }
    return size;
}

/*****************************************************************************
* @brief        linear write setup, get write pointer and max linear size.
*               
* @param[in]    rb          ringbuffer instance
* @param[in]    size        pointer to store max linear size in byte
* 
* @retval void*             write memory pointer
*****************************************************************************/
void *BMA_ringbuffer_linear_write_setup(BMA_ringbuffer_t *rb, uint32_t *size)
{
    uint32_t unused;
    uint32_t offset;
    uint32_t remain;

    unused = (rb->mask + 1) - (rb->in - rb->out);

    offset = rb->in & rb->mask;

    remain = rb->mask + 1 - offset;
    remain = remain > unused ? unused : remain;

    if (remain) {
        *size = remain;
        return ((uint8_t *)(rb->pool)) + offset;
    } else {
        *size = unused - remain;
        return rb->pool;
    }
}

/*****************************************************************************
* @brief        linear read setup, get read pointer and max linear size.
* 
* @param[in]    rb          ringbuffer instance
* @param[in]    size        pointer to store max linear size in byte
* 
* @retval void*             
*****************************************************************************/
void *BMA_ringbuffer_linear_read_setup(BMA_ringbuffer_t *rb, uint32_t *size)
{
    uint32_t used;
    uint32_t offset;
    uint32_t remain;

    used = rb->in - rb->out;

    offset = rb->out & rb->mask;

    remain = rb->mask + 1 - offset;
    remain = remain > used ? used : remain;

    if (remain) {
        *size = remain;
        return ((uint8_t *)(rb->pool)) + offset;
    } else {
        *size = used - remain;
        return rb->pool;
    }
}

/*****************************************************************************
* @brief        linear write done, add write pointer only
* 
* @param[in]    rb          ringbuffer instance
* @param[in]    size        write size in byte
* 
* @retval uint32_t          actual write size in byte
*****************************************************************************/
uint32_t BMA_ringbuffer_linear_write_done(BMA_ringbuffer_t *rb, uint32_t size)
{
    uint32_t unused;

    unused = (rb->mask + 1) - (rb->in - rb->out);
    if (size > unused) {
        size = unused;
    }
    rb->in += size;

    return size;
}

/*****************************************************************************
* @brief        linear read done, add read pointer only
* 
* @param[in]    rb          ringbuffer instance
* @param[in]    size        read size in byte
* 
* @retval uint32_t          actual read size in byte
*****************************************************************************/
uint32_t BMA_ringbuffer_linear_read_done(BMA_ringbuffer_t *rb, uint32_t size)
{
    return BMA_ringbuffer_drop(rb, size);
}
