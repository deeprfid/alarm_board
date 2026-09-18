/**
 * @file  ota_app.c
 * @brief App 侧 OTA 自检确认实现
 */
#include <string.h>
#include "ota_app.h"
#include "ota_flash.h"

int32_t ota_app_boot_confirm(uint32_t slot)
{
    ota_flag_t flag;
    int32_t    ret = 0;

    if (0 != ota_flag_read(&flag)) {
        /* 标志无效（首次烧录未写标志区）：建一份，直接把本槽置 RUNNABLE */
        (void)memset(&flag, 0, sizeof(flag));
        flag.magic   = OTA_FLAG_MAGIC;
        flag.active  = slot & 1UL;
        flag.state_a = (uint32_t)OTA_SLOT_EMPTY;
        flag.state_b = (uint32_t)OTA_SLOT_EMPTY;
    }

    flag.active = slot & 1UL;
    if (flag.active == OTA_SLOT_A) {
        flag.state_a = (uint32_t)OTA_SLOT_RUNNABLE;
        flag.fail_a  = 0UL;
    } else {
        flag.state_b = (uint32_t)OTA_SLOT_RUNNABLE;
        flag.fail_b  = 0UL;
    }
    flag.boot_count = 0UL;
    flag.flags     &= ~OTA_FLAG_NEED_CONFIRM;

    if (0 != ota_flag_write(&flag)) {
        ret = -1;
    }
    return ret;
}
