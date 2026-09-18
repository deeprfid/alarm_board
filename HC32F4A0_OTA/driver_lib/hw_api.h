/* hw_api.h - driver layer hardware config unified access (v9.81ci)
 * 替代业务层裸 extern 访问 driver_lib 全局变量。
 * 实现位于各定义文件（port.c / io_stream.c / usr_mod.c / custom_ee_commond.c）
 */
#ifndef HW_API_H
#define HW_API_H

#include "hc32f46_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 网络配置 (gNetConf, wizchip/port.c) */
networkParaConfig *hw_netconf(void);

/* 广播设备信息 (gBrdCstDevInfo, io_stream.c) */
BRDCST_DevInfo *hw_brdcst_info(void);

/* WLAN 配置 (gWlanNet, usr_mod.c) */
networkParaConfig *hw_wlan(void);

/* GPO 指令集 (gEEcmdGpoSet, custom_ee_commond.c) */
EERCmdGpoSet_ST *hw_gpo_set(void);

#ifdef __cplusplus
}
#endif

#endif /* HW_API_H */
