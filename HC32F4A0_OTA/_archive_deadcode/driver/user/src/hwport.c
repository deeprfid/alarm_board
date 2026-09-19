#include  "hwport.h"
#include "hc32f46_driver.h"
#include "bsp.h"
#include "uart.h"
#include "driverconfig.h"
void  flash_bytes_read(uint32_t addr, void *buf, uint16_t len)
{
	  en_int_status_t flag1,flag2;
    int ret;
	// __disable_irq();
	
  //  EFM_SetBusStatus(EFM_BUS_HOLD);

	 do {
        flag1 = EFM_GetStatus(EFM_FLAG_RDY);
        flag2 = EFM_GetStatus(EFM_FLAG_RDY1);
   } while ((SET != flag1) || (SET != flag2));



		ret=EFM_ReadByte(addr,(uint8_t*)buf,len);

	 
	//	 __enable_irq();
	 
		if(LL_OK==ret)
		{
		TRACE("LL_OK: Read successfully......%d\n",ret);
		}	
		if(LL_OK==LL_ERR_INVD_PARAM)
		{
		 TRACE("LL_ERR_INVD_PARAM: Invalid parameter.....%d\n",ret);
		}	
		if(LL_OK==LL_ERR_NOT_RDY)
		{
		 TRACE("LL_ERR_NOT_RDY: EFM is not ready......%d\n",ret);
		}	
		
}

int  flash_sector_erase(uint32_t dest)
{
    int ret;
    en_int_status_t flag1,flag2;

    __disable_irq();
	
	   do {
        flag1 = EFM_GetStatus(EFM_FLAG_RDY);
        flag2 = EFM_GetStatus(EFM_FLAG_RDY1);
    } while ((SET != flag1) || (SET != flag2));
		 
	  EFM_FWMC_Cmd(ENABLE);
    /* Release bus while erase & program */
    EFM_SetBusStatus(EFM_BUS_HOLD);
	
		  (void)EFM_SingleSectorOperateCmd(EFM_SECTOR_NUM, ENABLE);
      uint32_t  u32Addr = EFM_SECTOR_ADDR(EFM_SECTOR_NUM);
            /* Sector erase */
      ret=EFM_SectorErase(u32Addr);
      (void)EFM_SingleSectorOperateCmd(EFM_SECTOR_NUM, DISABLE);
            /* Compare */
    
		 EFM_FWMC_Cmd(DISABLE);
		 __enable_irq();
		if(LL_OK==ret)
		{
		TRACE("LL_OK: Erase successful......%d\n",ret);
		}	
		else
		{
		 TRACE("LL_ERR_NOT_RDY: EFM is not ready....%d\n",ret);
		}	
    return ret;
}

int  flash_bytes_write(uint32_t addr, void *buf, uint16_t len)
{
    int ret;
    en_int_status_t flag1,flag2;
	
    __disable_irq();

	  EFM_REG_Unlock();
	  EFM_FWMC_Cmd(ENABLE);
	  EFM_SetBusStatus(EFM_BUS_HOLD);
	
	
     do {
        flag1 = EFM_GetStatus(EFM_FLAG_RDY);
        flag2 = EFM_GetStatus(EFM_FLAG_RDY1);
    } while ((SET != flag1) || (SET != flag2));
   
    (void)EFM_SingleSectorOperateCmd(EFM_SECTOR_NUM, ENABLE);
		
    ret = EFM_Program(addr, buf, len);

		(void)EFM_SingleSectorOperateCmd(EFM_SECTOR_NUM, DISABLE);
		
    EFM_FWMC_Cmd(DISABLE);
		EFM_REG_Lock();
    __enable_irq();
		
		if(LL_OK==ret)
		{
		TRACE("Flash Program successful......%d\n",ret);
		}	
		else
    {
		  TRACE("LL_ERR_NOT_RDY: EFM if not ready.......%d\n",ret);
		}			

    return ret;
}


//void UART_DeInit(void)
//{
//    USART_DeInit(USART3_UNIT);
//}


//void USART3_IT_ENABLE(void)
//{
//    USART_FuncCmd(USART3_UNIT, USART_INT_RX, ENABLE);//使能接收中断
//}

//void USART3_IT_DISABLE(void)
//{
//    USART_FuncCmd(USART3_UNIT, USART_INT_RX, DISABLE);//不使能接收中断
//}




//void  GPIO_Configuration(void)
//{
//    Board_GPIO_Init();

//}


// drivelib test
//volatile uint8 gIsUsbAvailable;
//volatile uint8 gUsbCompType;
//int gAntNumber= -1;
//int is_enable_fwupdate = 2;

