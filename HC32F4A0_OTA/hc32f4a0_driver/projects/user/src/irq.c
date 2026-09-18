#include "irq.h"
#include "timer.h"
#include "uart.h"
#include "bsp.h"
#include "hc32_ll.h"
#include "hc32f46_driver.h"
//中断分配

//Int001_IRQn  uart1 rec
//Int002_IRQn  uart1 err

//Int003_IRQn  uart2 rec
//Int004_IRQn  uart2 err

//Int005_IRQn  uart3 rec
//Int006_IRQn  uart3 err

//Int007_IRQn  uart4 rec
//Int008_IRQn  uart4 err

//Int009_IRQn  time4


volatile uint32 gSysTickCnt = 0;
volatile int gIsRunDhcpTimeHandler = 0;
void DHCP_time_handler(void);
volatile int gIsRunDnsTimeHandler = 0;
void DNS_time_handler(void);
//int testcnt = 0;
volatile uint32 gResetKeyDownCnt = 0,gFuncKeyDownCnt = 0;
BtnResetCallback gBtnResetCb = NULL;
uint8_t  Keyvalue = 0;
void TMR2_Cmp_IrqCallback(void)
{
  TMR2_ClearStatus(TMR2_UNIT, TMR2_FLAG); 
	gSysTickCnt++;
    SWDT_FeedDog();
	 
	 if (gIsRunDhcpTimeHandler == 1)
	 {
		 if (gSysTickCnt % 2 == 0)
		 DHCP_time_handler();
	 }
	 if (gIsRunDnsTimeHandler == 1)
	 {
		 if (gSysTickCnt % 2 == 0)
		 DNS_time_handler();
	 }
	 
	 if (get_ipreset_key_value() == 0)
	 {
		 gResetKeyDownCnt++;
		 if (gResetKeyDownCnt == 10)
		 {
			 BtParams_ST btParams;
			 getBtParams(&btParams);
			 if (btParams.updateflag != 0 && (btParams.updatemode == FwUpdateMode_ByFtp_FmEth || 
				 btParams.updatemode == FwUpdateMode_ByFtp_Fm4G || 
				btParams.updatemode == FwUpdateMode_ByHttp_FmUartEx || 
				btParams.updatemode == FwUpdateMode_ByHttp_FmEth))
			 {
				 btParams.updatemode = FwUpdateMode_Default;
				 setBtParams(&btParams);
				 sleep_ms(50);
			 }
			 set_default_network_config();
			 erase_multi_config(ERASE_FLS_CFG_BIT_ACTMODE | ERASE_FLS_CFG_BIT_WKMODEPARA | ERASE_FLS_CFG_BIT_PSVMODE);
			 Erase_eastag_to_flash();
			 sleep_ms(50);
			 if (get_uart_ex_dev() == Uart_Ex_Wlan)
			 {
				 erase_wlan_config();
				 sleep_ms(50);
			 }
			 
			 if (get_uart_ex_dev() == Uart_Ex_Bluetooth)
			 {
				 erase_bluetooth_config();
				 sleep_ms(50);
			 }
			 
			 if (gBtnResetCb != NULL)
			 {
				 gBtnResetCb();
       }
				
			 system_reset();
		 }
	 }
	 else
	 {
		 gResetKeyDownCnt = 0;
	 }

  if(gpi_get(4) == 0) // FuncKey scanning
    {
        if(gFuncKeyDownCnt++ == 1)
        {
            Keyvalue = 1;
        }

    }
    else
    {
        Keyvalue = 0;
        gFuncKeyDownCnt = 0;
    }	 
	 
	// board_ledtoggle();	
}



//void Usart2RxIrqCallback(void) //1us
//{
//    if (Set == USART_GetStatus(M4_USART2, UsartRxNoEmpty ))
//    {
//        gUart1RecvBuf[uart2reccount] = (0xff&USART_RecData(M4_USART2));
//        uart2reccount++;
//        if(uart2reccount >= MAX_UART1_BUF_SIZE)
//			  uart2reccount = 0;
//    }
//}

//void Usart3RxIrqCallback(void) //1us
//{
//    if (Set == USART_GetStatus(M4_USART3, UsartRxNoEmpty ))
//    {
//        gUart2RecvBuf[uart3reccount] = (0xff&USART_RecData(M4_USART3));
//        uart3reccount++;
//        if(uart3reccount >= MAX_UART2_BUF_SIZE) 
//			  uart3reccount = 0;
//    }
//}

//void Usart4RxIrqCallback(void) //1us
//{
//    if (Set == USART_GetStatus(M4_USART4, UsartRxNoEmpty ))
//    {
//        gUart3RecvBuf[uart4reccount]=(0xff&USART_RecData(M4_USART4));
//        uart4reccount++;
//        if(uart4reccount>=MAX_UART3_BUF_SIZE) uart4reccount=0;
//    }
//}






