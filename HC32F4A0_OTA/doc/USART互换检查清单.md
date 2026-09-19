# USART1 / USART4 互换失败分析与检查清单

> 版本：v1.0
> 背景：模块口（UART0）与命令口（UART1）物理外设互换（模块口 → USART1，命令口 → USART4），
> 模块初始化失败（init_rfidmodle failed:43975/0xABA7、5768/0x1688），中断号修复 + 注释 OTA 后仍失败。
> 结论：**互换不是"重映射"，是牵一发动全身——至少 7 处物理外设绑定散落各文件，漏改任何一处即失败。**

---

## 1. 目标与现状

| | 逻辑口 | 物理外设 | 引脚 | 波特率 | 用途 |
|---|---|---|---|---|---|
| 现状 | UART0 | USART4（0x4001D800）| PB04/PB03 FUNC39/38 | 921600 | 模块口（RFID 模块）|
| 现状 | UART1 | USART1（0x4001CC00）| PB01/PB00 FUNC33/32 | 115200 | 命令口（上位机 CH340 + printf）|
| 互换目标 | UART0 → USART1 | UART1 → USART4 | 引脚不变 | 不变 | 交换物理外设 |

---

## 2. 🔴 7 处绑定点（全部有代码证据）

### ① uart.h L19-21 —— 逻辑口 → 物理外设宏映射
```c
#define USART1_UNIT  (CM_USART4)   /* 模块口 */
#define USART2_UNIT  (CM_USART1)   /* 命令口 */
```
互换需对调。**注意：这是"意图层"，下面所有文件内部还有各自硬编码，宏互换 ≠ 完成互换。**

### ② uart1.c L10-27 —— 模块口驱动（引脚/时钟/中断全绑定 USART4）
```c
#define USART1_RX_PIN      (GPIO_PIN_04)  #define USART1_RX_GPIO_FUNC (GPIO_FUNC_39)
#define USART1_TX_PIN      (GPIO_PIN_03)  #define USART1_TX_GPIO_FUNC (GPIO_FUNC_38)
#define USART1_FCG_ENABLE() (FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_USART4, ENABLE))
#define USART1_RX_ERR_IRQn  (INT000_IRQn)  #define USART1_RX_ERR_INT_SRC (INT_SRC_USART4_EI)
#define USART1_RX_FULL_IRQn (INT001_IRQn)  #define USART1_RX_FULL_INT_SRC (INT_SRC_USART4_RI)
```
互换后全部改为 USART1 对应值：FUNC33/32（或 USART1 实际功能号）、FCG3_PERIPH_USART1、INT086/087、INT_SRC_USART1_EI/RI。

### ③ uart2.c L10-27 —— 命令口驱动（同理，绑 USART1）
```c
#define USART2_RX_PIN      (GPIO_PIN_01)  #define USART2_RX_GPIO_FUNC (GPIO_FUNC_33)
#define USART2_TX_PIN      (GPIO_PIN_00)  #define USART2_TX_GPIO_FUNC (GPIO_FUNC_32)
#define USART2_FCG_ENABLE() (FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_USART1, ENABLE))
#define USART2_RX_ERR_IRQn  (INT086_IRQn)  #define USART2_RX_ERR_INT_SRC (INT_SRC_USART1_EI)
#define USART2_RX_FULL_IRQn (INT087_IRQn)  #define USART2_RX_FULL_INT_SRC (INT_SRC_USART1_RI)
```
互换后改为 USART4 对应值。

### ④ 🔴 uart2.c L105 —— 硬编码 CM_USART1（不随宏走，互换头号雷）
```c
CM_USART1->CR1 = (USART_CR1_OVER8 | USART_CR1_TE | USART_CR1_RE | USART_CR1_RIE);
```
**硬编码物理 USART1！** 互换后命令口=USART4，这行仍写 USART1（模块口）——**命令口 CR1 永远不会被正确配置，且会干扰模块口**。必须改为 `USART2_UNIT->CR1`（随宏）。

### ⑤ 🔴 board.h L175 —— printf 设备硬编码 CM_USART1（深层绑定，最易漏）
```c
#define BSP_PRINTF_DEVICE  (CM_USART1)
```
**printf/TRACE 永远映射到物理 USART1**（LL_PrintfInit → LL_SetPrintDevice，hc32_ll_utility.c L378）。
互换后模块口=USART1 → **BSP_PRINTF_Preinit 的 USART_UART_Init 会把模块口当 printf 设备重配（全写 CR1）→ 模块口配置被踩**。
必须改为 USART4（命令口新归属），或把 printf 设备与命令口解耦。

### ⑥ 🔴 Lan2Uart.c L250 —— 硬编码 0x4001CC0C（USART1 CR1）
```c
*(volatile uint32_t *)0x4001CC0CUL |= 0x24UL;   /* RE|RIE */
```
硬编码 USART1 CR1 地址。互换后语义反转（原补命令口，现变补模块口）。应删或改用宏。

### ⑦ USART_GetBusClockFreq（hc32_ll_usart.c L666-675）—— 时钟域缺陷
```c
static uint32_t USART_GetBusClockFreq(const CM_USART_TypeDef *USARTx)
{
    (void)USARTx;   /* ← 忽略外设！ */
    u32BusClock = SystemCoreClock >> (READ_REG32_BIT(CM_CMU->SCFGR, CMU_SCFGR_PCLK1S) >> CMU_SCFGR_PCLK1S_POS);
    return u32BusClock;
}
```
**所有 USART 一律按 PCLK1 算波特率**。若 USART1/USART4 实际挂不同 APB（PCLK1 vs PCLK4），互换后波特率算错 → 模块无响应。互换前必须先确认两外设时钟域。

### ⑧ RTO（接收超时）能力差异 —— 手册 P689
RTO 仅 **USART1/2/6/7** 支持，**USART4 不支持**。命令口 read 超时若依赖 RTO，换到 USART4 后失效（当前 read 是轮询计数 + sleep，未见 RTO 依赖，但互换前必须确认）。

---

## 3. 当时排查历史（为什么中断号修复了仍失败）

1. 引脚确认：模块在 PB3/PB4 未动、CH340 在 PB01/PB00 ✓
2. 中断号修复：uart1.c 应为 INT086/087 而非 INT000/001 —— **已修复**
3. u32ClockDiv：diff 显示 L81 从注释变启用（与 HEAD 不同）—— 需对比
4. 硬编码 CM_USART1->CR1（uart2.c L105）—— **漏改**
5. 硬编码 0x4001CC0C（Lan2Uart.c L250）—— **漏改**
6. 注释 OTA 4 处 —— 仍失败
7. **BSP_PRINTF_DEVICE=CM_USART1（board.h）—— 当时未识别：printf 初始化踩模块口**

**结论**：中断只是 7 项之一。模块口初始化失败的直接原因大概率是 ④⑤⑥ 任一漏改，
尤其 ⑤（printf 设备固定初始化 USART1，而互换后 USART1 是模块口）。

---

## 4. 正确互换步骤（按清单全量执行）

1. **时钟验证**：确认 USART1/USART4 时钟域；若不同，修复 USART_GetBusClockFreq（按外设取 PCLKx）或接受固定域
2. **uart.h**：USART1_UNIT / USART2_UNIT 宏对调
3. **uart1.c**：引脚 FUNC、FCG3_PERIPH_USART1、INT086/087、INT_SRC_USART1_EI/RI —— 全改
4. **uart2.c**：引脚 FUNC、FCG3_PERIPH_USART4、INT000/001、INT_SRC_USART4_EI/RI —— 全改；**L105 去硬编码 → USART2_UNIT->CR1**
5. **board.h**：BSP_PRINTF_DEVICE → CM_USART4（或 printf 解耦到独立口）
6. **Lan2Uart.c L250**：删除或改用宏（互换后语义反转，必须处理）
7. **RTO 检查**：命令口换 USART4 后确认 read 超时机制不依赖 RTO
8. **全量编译**：driver lib 重建（xcopy）+ app 重建，map 验证符号
9. **硬件验证**：模块初始化（InitReader 探测）+ 命令口 IOGET + TRACE 输出

---

## 5. 相关发现（本次排查顺带确认）

- **printf 设备 = CM_USART1 = 命令口**（board.h L175 + LL_SetPrintDevice），映射在 RTOS 启动前
  main() → driver_hw_init_late() → DDL_PrintfInit → LL_PrintfInit → LL_SetPrintDevice 完成，运行期 TRACE 只写 TDR 不碰 CR1
- BSP_PRINTF_Preinit 的波特率匹配循环（遍历 PR 0~64）是官方 BSP 模板写法，对 printf 无必要；
  v9.81ai 的 RX 恢复藏在 break 分支——波特率不匹配时不恢复（残留缺陷，建议恢复移出循环）
- 命令口 RX 中断曾被 BSP_PRINTF_Preinit 杀（v9.81ah 时代），v9.81ai 已恢复；
  Lan2Uart.c L250 是当时遗留的每轮循环自愈（现无防御对象，可删/可换观察版）

---

*文档结束。依据：hc32f4a0_driver / hc32f4a0_app 源码逐行核对。*
