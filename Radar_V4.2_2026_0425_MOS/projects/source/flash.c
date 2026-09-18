/**
 *******************************************************************************
 * @file  flash.c
 * @brief 片内 Flash 擦/写/读原语实现 —— 算法与量产 boot_iap 一致, 只把 DDL 调用换成共享 DDL 的命名
 *
 * 【旧 DDL -> 共享 DDL 的调用映射(本文件唯一改动)】
 *     EFM_Unlock()  -> EFM_REG_Unlock()
 *     EFM_Lock()    -> EFM_REG_Lock()
 *     另外: 共享 DDL 的 EFM_Program/EFM_SectorErase 退出时会把 FWMC.PEMOD 复位成只读,
 *           故【每个擦写操作前都要 EFM_FWMC_Cmd(ENABLE)】, 操作后 DISABLE。
 *******************************************************************************
 */
#include "flash.h"

int32_t FLASH_CheckAddrAlign(uint32_t u32Addr)
{
    uint32_t u32Step = FLASH_SECTOR_SIZE;

    if (VECT_TAB_STEP > FLASH_SECTOR_SIZE) {
        u32Step = VECT_TAB_STEP;
    }
    if ((u32Addr % u32Step) != 0UL) {
        return LL_ERR;
    }
    return LL_OK;
}

int32_t FLASH_EraseSector(uint32_t u32Addr, uint32_t u32Size)
{
    uint32_t i;
    uint32_t u32PageNum;

    if (u32Addr >= (FLASH_BASE + FLASH_SIZE)) {
        return LL_ERR_INVD_PARAM;
    }

    EFM_REG_Unlock();

    if (u32Size == 0U) {
        EFM_FWMC_Cmd(ENABLE);
        if (LL_OK != EFM_SectorErase(u32Addr)) {
            EFM_FWMC_Cmd(DISABLE);
            EFM_REG_Lock();
            return LL_ERR;
        }
        EFM_FWMC_Cmd(DISABLE);
    } else {
        u32PageNum = u32Size / FLASH_SECTOR_SIZE;
        if ((u32Size % FLASH_SECTOR_SIZE) != 0UL) {
            u32PageNum += 1U;
        }
        for (i = 0; i < u32PageNum; i++) {
            EFM_FWMC_Cmd(ENABLE);
            if (LL_OK != EFM_SectorErase(u32Addr + (i * FLASH_SECTOR_SIZE))) {
                EFM_FWMC_Cmd(DISABLE);
                EFM_REG_Lock();
                return LL_ERR;
            }
            EFM_FWMC_Cmd(DISABLE);
        }
    }

    EFM_REG_Lock();
    return LL_OK;
}

int32_t FLASH_WriteData(uint32_t u32Addr, uint8_t *pu8Buff, uint32_t u32Len)
{
    int32_t i32Ret;

    if ((pu8Buff == NULL) || (u32Len == 0U) || ((u32Addr + u32Len) > (FLASH_BASE + FLASH_SIZE))) {
        return LL_ERR_INVD_PARAM;
    }
    if (0UL != (u32Addr % 4U)) {
        return LL_ERR_ADDR_ALIGN;
    }

    EFM_REG_Unlock();
    EFM_FWMC_Cmd(ENABLE);
    i32Ret = EFM_Program(u32Addr, pu8Buff, u32Len);
    EFM_FWMC_Cmd(DISABLE);
    EFM_REG_Lock();

    return i32Ret;
}

int32_t FLASH_ReadData(uint32_t u32Addr, uint8_t *pu8Buff, uint32_t u32Len)
{
    uint32_t i;
    uint32_t u32WordLength, u8ByteRemain;
    uint32_t *pu32ReadBuff;
    volatile uint32_t *pu32FlashAddr;
    uint8_t  *pu8Byte;
    volatile uint8_t  *pu8FlashAddr;

    if ((pu8Buff == NULL) || (u32Len == 0U) || ((u32Addr + u32Len) > (FLASH_BASE + FLASH_SIZE))) {
        return LL_ERR_INVD_PARAM;
    }
    if (0UL != (u32Addr % 4U)) {
        return LL_ERR_ADDR_ALIGN;
    }

    pu32ReadBuff  = (uint32_t *)(uint32_t)pu8Buff;
    pu32FlashAddr = (volatile uint32_t *)u32Addr;
    u32WordLength = u32Len / 4U;
    u8ByteRemain  = u32Len % 4U;

    for (i = 0UL; i < u32WordLength; i++) {
        *(pu32ReadBuff++) = *(pu32FlashAddr++);
    }
    if (0UL != u8ByteRemain) {
        pu8Byte      = (uint8_t *)pu32ReadBuff;
        pu8FlashAddr = (volatile uint8_t *)pu32FlashAddr;
        for (i = 0UL; i < u8ByteRemain; i++) {
            *(pu8Byte++) = *(pu8FlashAddr++);
        }
    }

    return LL_OK;
}
