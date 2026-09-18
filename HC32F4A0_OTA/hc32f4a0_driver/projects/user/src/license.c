

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "hc32_ll.h"
#include "hc32f46_driver.h"


uint32_t Ucode_read(uint8_t *inbuf, uint16_t u32len)
{
      uint32_t uuid = 0;
      stc_efm_unique_id_t  efm_unique_id={0};
      EFM_GetUID(&efm_unique_id);

    uint32_t idkey = 0;
    uint32_t u32temp = 0x12011201; //God Father's Day

     uuid = efm_unique_id.u32UniqueID0 ^ efm_unique_id.u32UniqueID1 ^ efm_unique_id.u32UniqueID2;

    idkey = uuid ^ u32temp;

    return idkey;
}

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
