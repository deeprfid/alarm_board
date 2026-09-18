/**
 * @file ota_agent.h
 * @brief OTA 升级 Agent：下载→校验→置标志→复位；启动时 commit（Phase 3 闭环）
 * @version V0.2  2026-08-14
 */
#ifndef OTA_AGENT_H
#define OTA_AGENT_H

/* 完整升级流程（App 态触发）：download → 校验 → ota_mark_ready → system_reset */
int  ota_agent_run(const char *url);

/* 启动检查：need_copy=1 → commit（QSPI→Bank B + SwapCmd）→ 复位从新固件启动 */
void ota_agent_boot(void);

/* v9.81p: 业务初始化完成（gIsFinInit=1）后调用——新固件自检确认（清 NEED_CONFIRM，防回滚） */
void ota_agent_confirm(void);

#endif /* OTA_AGENT_H */
