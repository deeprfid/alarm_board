/**
 *******************************************************************************
 * @file  usb/usb_dev_cdc/source/usb_dev_user.c
 * @brief User callback function for USB example
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
 @endverbatim
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by XHSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "usb_dev_user.h"

/**
 * @addtogroup HC32F4A0_DDL_Applications
 * @{
 */

/**
 * @addtogroup USB_Dev_Cdc
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
void usb_dev_user_init(void);
void usb_dev_user_rst(void);
void usb_dev_user_devcfg(void);
void usb_dev_user_devsusp(void);
void usb_dev_user_devresume(void);
void usb_dev_user_conn(void);
void usb_dev_user_disconn(void);

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/
usb_dev_user_func user_cb = {
    &usb_dev_user_init,
    &usb_dev_user_rst,
    &usb_dev_user_devcfg,
    &usb_dev_user_devsusp,
    &usb_dev_user_devresume,
    &usb_dev_user_conn,
    &usb_dev_user_disconn,
};

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  usb_dev_user_init
 * @param  None
 * @retval None
 */
void usb_dev_user_init(void)
{
    /* Add initial code here */
}

/**
 * @brief  usb_dev_user_rst
 * @param  None
 * @retval None
 */
void usb_dev_user_rst(void)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    /* v9.81cm: IRQ打印已移除——3ms阻塞致误判suspend */
#endif
}

/**
 * @brief  usb_dev_user_devcfg
 * @param  None
 * @retval None
 */
extern volatile int gIsUsbAvailable;
void usb_dev_user_devcfg(void)
{
    gIsUsbAvailable = 1;   /* USB enumerated: io_stream can use USB */
#if (LL_PRINT_ENABLE == DDL_ON)
    /* v9.81cm: IRQ打印已移除——3ms阻塞致误判suspend */
#endif
}

/**
 * @brief  usb_dev_user_conn
 * @param  None
 * @retval None
 */
void usb_dev_user_conn(void)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    /* v9.81cm: IRQ打印已移除——3ms阻塞致误判suspend */
#endif
}

/**
 * @brief  USBD_USR_DeviceDisonnected
 * @param  None
 * @retval None
 */
void usb_dev_user_disconn(void)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    /* v9.81cm: IRQ打印已移除——3ms阻塞致误判suspend */
#endif
}

/**
 * @brief  usb_dev_user_devsusp
 * @param  None
 * @retval None
 */
void usb_dev_user_devsusp(void)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    /* v9.81cm: IRQ打印已移除——3ms阻塞致误判suspend */
#endif
}

/**
 * @brief  usb_dev_user_devresume
 * @param  None
 * @retval None
 */
void usb_dev_user_devresume(void)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    /* v9.81cm: IRQ打印已移除——3ms阻塞致误判suspend */
#endif
}

/**
 * @}
 */

/**
 * @}
 */

/******************************************************************************
 * EOF (not truncated)
 *****************************************************************************/

