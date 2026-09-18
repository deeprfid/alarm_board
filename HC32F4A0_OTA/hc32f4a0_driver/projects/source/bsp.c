

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "bsp.h"

/* Test data size */
#define TEST_DATA_SIZE                  (0x1000UL)


static __align(4) uint8_t m_au8WriteData[TEST_DATA_SIZE];
static __align(4) uint8_t m_au8ReadData[TEST_DATA_SIZE];

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/


static void QSPI_TestDataInit(void)
{
    uint32_t i;

    for (i = 0UL; i < TEST_DATA_SIZE; i++) {
        m_au8WriteData[i] = (uint8_t)(i % 256U);
        m_au8ReadData[i]  = 0U;
    }
}

/**
 * @brief  Check the erase result.
 * @param  None
 * @retval None
 */
static int32_t QSPI_CheckEraseResult(void)
{
    uint32_t i;
    int32_t i32Ret = LL_OK;

    for (i = 0UL; i < TEST_DATA_SIZE; i++) {
        if (0xFFU != m_au8ReadData[i]) {
            i32Ret = LL_ERR;
            break;
        }
    }

    return i32Ret;
}

/**
 * @brief  Check the program result.
 * @param  None
 * @retval None
 */
static int32_t QSPI_CheckProgramResult(void)
{
    uint32_t i;
    int32_t i32Ret = LL_OK;

    for (i = 0UL; i < TEST_DATA_SIZE; i++) {
        if (m_au8ReadData[i] != m_au8WriteData[i]) {
            i32Ret = LL_ERR;
            break;
        }
    }

    return i32Ret;
}






int32_t BSP_init(void)
{
    uint8_t  u8UID[W25Q64_UNIQUE_ID_SIZE] = {0U};
		uint8_t  u8mfrID[8]={0};
		uint8_t  u8jedecID[4]={0};
    uint32_t u32FlashAddr = W25Q64_MAX_ADDR-16384;
    uint32_t startT=0,endT=0,flag=0;
    /* Peripheral registers write unprotected */
    LL_PERIPH_WE(EXAMPLE_PERIPH_WE);
    /* Configure BSP */
    BSP_CLK_Init();
	//  (void)SysTick_Init(1000U);
   // BSP_KEY_Init();
    /* Configure UART */
    DDL_PrintfInit(BSP_PRINTF_DEVICE, BSP_PRINTF_BAUDRATE, BSP_PRINTF_Preinit);
    /* Configure QSPI */
    QSPI_FLASH_Init();
		TrngConfig();
		(void)Board_GPIO_Init();
		(void)Timera_init();
    /* Peripheral registers write protected */
    LL_PERIPH_WP(EXAMPLE_PERIPH_WP);
    /* Get flash UID */
    QSPI_FLASH_GetUniqueID(u8UID,u8mfrID,u8jedecID);
    DDL_Printf("UID     value: %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\r\n",
               u8UID[0], u8UID[1], u8UID[2], u8UID[3], u8UID[4], u8UID[5], u8UID[6], u8UID[7]);
		DDL_Printf("MFRID   value: %02x-%02x-%02x-%02x\r\n",u8mfrID[0],u8mfrID[1],u8mfrID[2],u8mfrID[3]);
		DDL_Printf("JEDECID value: %02x-%02x-%02x\r\n",u8jedecID[0],u8jedecID[1],u8jedecID[2]);
    QSPI_TestDataInit();
		u32FlashAddr=0;
    for (;;) {
            /* Erase sector */
					  startT=SysTick_GetTick();
            (void)QSPI_FLASH_EraseSector(u32FlashAddr);
            (void)QSPI_FLASH_Read(u32FlashAddr, m_au8ReadData, TEST_DATA_SIZE);
           if (LL_OK == QSPI_CheckEraseResult())
							{
                /* Write data */
					      (void)TRNG_GenerateRandom((uint32_t*)m_au8WriteData, TEST_DATA_SIZE/4);
                (void)QSPI_FLASH_Write(u32FlashAddr, m_au8WriteData, sizeof(m_au8WriteData));
							  endT=SysTick_GetTick();
							  DDL_Printf("diff time:%dms\r\n",(endT-startT));
								DDL_Printf("Sector ID:%ld ",(u32FlashAddr/W25Q64_SECTOR_SIZE));
                (void)QSPI_FLASH_Read(u32FlashAddr, m_au8ReadData, TEST_DATA_SIZE);
                if (LL_OK == QSPI_CheckProgramResult()) {
                  DDL_Printf("Programmed success :%d\r\n",flag++);
                 } 

            }
            /* Flash address offset */
            u32FlashAddr += W25Q64_SECTOR_SIZE;
            if ((u32FlashAddr + TEST_DATA_SIZE) >= W25Q64_MAX_ADDR/32) {
                u32FlashAddr = 0U;
							  while(u32FlashAddr+1);
            }
        }
				

}


/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
