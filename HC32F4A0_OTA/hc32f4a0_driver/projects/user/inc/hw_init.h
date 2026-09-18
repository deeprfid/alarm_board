/* hw_init.h - hardware layer unified init entry (v9.81ci)
 * driver_hw_init():    上电硬件初始化（时钟/IO/RTC/LED/定时器/TRNG/堆/打印/QSPI）— 在 main() 调用
 * driver_hw_rtos_init(): RTOS 环境就绪后的硬件配置（KV/板卡探测/网络）— 在 init_thread 调用
 */
#ifndef HW_INIT_H
#define HW_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/* 上电硬件初始化（main() 内、osKernelInitialize 之前调用）
 * 含: 时钟/IO/RTC/LED/定时器/TRNG/堆准备 */
void driver_hw_init(void);

/* 上电硬件初始化-第二阶段（堆初始化 _init_alloc 之后调用）
 * 含: 打印串口 DDL_PrintfInit + QSPI 闪存 QSPI_FLASH_Init */
void driver_hw_init_late(void);

/* RTOS 环境就绪后的硬件配置（init_thread 内调用；返回 0=成功） */
int driver_hw_rtos_init(void);

#ifdef __cplusplus
}
#endif

#endif /* HW_INIT_H */
