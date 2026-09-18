/**
 *******************************************************************************
 * @file  aes/aes_base/source/main.c
 * @brief Main program of AES for the Device Driver Library.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2023-09-30       CDT             Set XTAL as system clock source
                                    Modify printf baudrate
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
#include "bsp.h"

/**
 * @addtogroup HC32F4A0_DDL_Examples
 * @{
 */

/**
 * @addtogroup AES_Base
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
/* AES key size. @ref AES_Key_Size */
#define AES_KEY_SIZE                        (AES_KEY_SIZE_16BYTE)

/* Size of plaintext and ciphertext. */
#define AES_PLAINTEXT_BYTE_SIZE             (16UL * 1UL)
#define AES_CIPHERTEXT_BYTE_SIZE            (AES_PLAINTEXT_BYTE_SIZE)



static void AesConfig(void);
static int32_t AesVerify(const uint8_t *pu8Data1, const uint8_t *pu8Data2, uint32_t u32NumByte);

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
const static char *m_strPlaintext = "abcdefghijklmnop";

#if (AES_KEY_SIZE == AES_KEY_SIZE_16BYTE)
const static char *m_strAesKey = "1234567890abcdef";
const static uint8_t m_au8Ciphertext[AES_CIPHERTEXT_BYTE_SIZE] = {
    0x2E, 0xE0, 0xF9, 0x5A, 0x84, 0x51, 0x70, 0x7A,
    0xB5, 0xB6, 0xE1, 0x16, 0x65, 0x01, 0xCB, 0x1F,
};

#elif (AES_KEY_SIZE == AES_KEY_SIZE_24BYTE)
const static char *m_strAesKey = "1234567890abcdefghijklmn";
const static uint8_t m_au8Ciphertext[AES_CIPHERTEXT_BYTE_SIZE] = {
    0x6C, 0x9C, 0xDA, 0x97, 0x40, 0xB0, 0x7E, 0x49,
    0xFE, 0x92, 0x1C, 0x03, 0x57, 0xCE, 0x51, 0x5B,
};

#elif (AES_KEY_SIZE == AES_KEY_SIZE_32BYTE)
const static char *m_strAesKey = "1234567890abcdefghijklmnopqrstuv";
const static uint8_t m_au8Ciphertext[AES_CIPHERTEXT_BYTE_SIZE] = {
    0x1C, 0xD5, 0x68, 0x5B, 0x81, 0x1E, 0x43, 0x18,
    0x20, 0xF6, 0x91, 0x3E, 0x39, 0xA4, 0xB1, 0x96,
};

#else
#error "ERROR key size!!!"
#endif


/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  Main function of example project
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t aes_init(void)
{
    uint8_t au8Plaintext[AES_PLAINTEXT_BYTE_SIZE];
    uint8_t au8Ciphertext[AES_CIPHERTEXT_BYTE_SIZE];


    /* AES configuration. */
    AesConfig();
    /* Initializes UART for debug printing. Baudrate is 115200. */
   
    for (;;) {
        /* AES encryption. */
        (void)AES_Encrypt((uint8_t *)m_strPlaintext, AES_PLAINTEXT_BYTE_SIZE, \
                          (uint8_t *)m_strAesKey, AES_KEY_SIZE, \
                          au8Ciphertext);
        if (AesVerify(au8Ciphertext, m_au8Ciphertext, AES_CIPHERTEXT_BYTE_SIZE) != LL_OK) {
            DDL_Printf("AES encryption FAIL.\r\n");
            for (;;) {
                /* rsvd */
            }
        }
        DDL_Printf("AES encryption OK.\r\n");

        /* AES decryption */
        (void)AES_Decrypt(m_au8Ciphertext, AES_CIPHERTEXT_BYTE_SIZE, \
                          (uint8_t *)m_strAesKey, AES_KEY_SIZE, \
                          au8Plaintext);
        if (AesVerify(au8Plaintext, (uint8_t *)m_strPlaintext, AES_PLAINTEXT_BYTE_SIZE) != LL_OK) {
            DDL_Printf("AES decryption FAIL.\r\n");
            for (;;) {
                /* rsvd */
            }
        }
        DDL_Printf("AES decryption OK.\r\n");

        DDL_DelayMS(500U);
    }
}



/**
 * @brief  AES configuration.
 * @param  None
 * @retval None
 */
static void AesConfig(void)
{
    /* Enable AES peripheral clock. */
    FCG_Fcg0PeriphClockCmd(PWC_FCG0_AES, ENABLE);
}

/**
 * @brief  AES verification.
 * @param  [in]  pu8Data1               Pointer to data 1.
 * @param  [in]  pu8Data2               Pointer to data 2.
 * @param  [in]  u32NumByte             Number of byte to verify.
 * @retval int32_t:
 *         - LL_OK:                     Verification OK.
 *         - LL_ERR:                    Verification FAIL.
 */
static int32_t AesVerify(const uint8_t *pu8Data1, const uint8_t *pu8Data2, uint32_t u32NumByte)
{
    uint32_t i;
    int32_t i32Ret = LL_OK;

    for (i = 0UL; i < u32NumByte; i++) {
        if (pu8Data1[i] != pu8Data2[i]) {
            i32Ret = LL_ERR;
            break;
        }
    }

    return i32Ret;
}
/**
 * @}
 */

/**
 * @}
 */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
