/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-01-26     armink       the first version
 */

#include <fal.h>
#include <sfud.h>
#include "string.h"
#include "qspi_flash.h"

#define LL_SUCCESS     (0)
#define LL_FAILED      (-1)
#ifdef FAL_USING_SFUD_PORT
#ifdef RT_USING_SFUD
#include <spi_flash_sfud.h>
#endif

#ifndef FAL_USING_NOR_FLASH_DEV_NAME
#define FAL_USING_NOR_FLASH_DEV_NAME             "norflash0"
#endif

static int init(void);
static int read(long offset, uint8_t *buf, size_t size);
static int write(long offset, const uint8_t *buf, size_t size);
static int erase(long offset, size_t size);


struct fal_flash_dev nor_flash0 =
{
    .name       = FAL_USING_NOR_FLASH_DEV_NAME,
    .addr       = 0x000000,
    .len        = 16 * 1024 * 1024,
    .blk_size   = 4*1024,
    .ops        = {init, read, write, erase},
    .write_gran = 1
};

static int init(void)
{

//#ifdef RT_USING_SFUD
//    /* RT-Thread RTOS platform */
//    sfud_dev = rt_sfud_flash_find_by_dev_name(FAL_USING_NOR_FLASH_DEV_NAME);
//#else
//    /* bare metal platform */
//    extern sfud_flash sfud_norflash0;
//    sfud_dev = &sfud_norflash0;
//#endif

//    if (NULL == sfud_dev)
//    {
//        return -1;
//    }

//    /* update the flash chip information */
//    nor_flash0.blk_size = sfud_dev->chip.erase_gran;
//    nor_flash0.len      = sfud_dev->chip.capacity;

    return 0;
}

static int read(long offset, uint8_t *buf, size_t size)
{
//    assert(sfud_dev);
//    assert(sfud_dev->init_ok);
   // sfud_read(sfud_dev, nor_flash0.addr + offset, size, buf);
   // memcpy((char *)buf,(char*)(nor_flash0.addr + offset),size);
	  QSPI_FLASH_Read((uint32_t)(nor_flash0.addr + offset),buf,size);
    return size;
}

static int write(long offset, const uint8_t *buf, size_t size)
{
//    assert(sfud_dev);
//    assert(sfud_dev->init_ok);
//    if (sfud_write(sfud_dev, nor_flash0.addr + offset, size, buf) != SFUD_SUCCESS)
//    {
//        return -1;
//    }
	  // memcpy((char*)(nor_flash0.addr + offset),(char *)buf,size);
   if (LL_SUCCESS==QSPI_FLASH_Write((uint32_t)(nor_flash0.addr + offset),( uint8_t *)buf,size))
    return size;
	 else
		 return LL_FAILED; 
}

static int erase(long offset, size_t size)
{
//    assert(sfud_dev);
//    assert(sfud_dev->init_ok);
//    if (sfud_erase(sfud_dev, nor_flash0.addr + offset, size) != SFUD_SUCCESS)
//    {
//        return -1;
//    }
	    if (LL_SUCCESS==QSPI_FLASH_EraseSector((uint32_t)(nor_flash0.addr+offset)))
			 return size;
	     else
		   return LL_FAILED; 	
	  // memset((char*)(nor_flash0.addr+offset),0xFF,size);

   
}
#endif /* FAL_USING_SFUD_PORT */

