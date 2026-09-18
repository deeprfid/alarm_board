/**
 * @file msg_bus.h
 * @brief 消息总线（独立模块，v1.0）
 *
 * 设计目标：
 *   1. 模块间解耦：发布者不知道谁在收，订阅者不知道谁在发
 *   2. 消灭跨文件裸全局变量：状态/事件走总线，模块内部 static 化
 *   3. 基于 RTX5 osMessageQueue（线程安全），纯 C，无新依赖
 *
 * 两种通信原语：
 *   A. 主题队列（Topic Queue）—— 数据通路（高频，标签/命令/日志）
 *      msg_bus_publish(topic, data, len)  → 订阅者 msg_bus_subscribe(topic, cb)
 *   B. 信号槽（Signal/Slot）—— 事件路由（低频，状态/配置变化通知）
 *      msg_bus_signal(sig)  → 槽 msg_bus_connect(sig, cb)
 *
 * 线程模型：
 *   - publish/signal 可从任意线程/中断调用（内部 osMessageQueuePut 线程安全）
 *   - 订阅回调在"消费线程"执行（调用方自己保证），默认每个主题一个消费队列
 *
 * 许可：内部使用（参考 MicroBoot signals_slots Apache-2.0 思想，独立实现）
 */
#ifndef __MSG_BUS_H__
#define __MSG_BUS_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== A. 主题队列 ==================== */

/* 主题定义（新增业务主题在此扩展） */
typedef enum {
    MSG_TOPIC_NONE = 0,
    MSG_TOPIC_TAG,          /* 标签数据（高频） */
    MSG_TOPIC_EVENT,        /* 事件上报（TagComing/HeartBeat...） */
    MSG_TOPIC_CMD,          /* 控制命令（上位机→设备） */
    MSG_TOPIC_STATE,        /* 状态变化通知 */
    MSG_TOPIC_CFG,          /* 配置变化通知 */
    MSG_TOPIC_LOG,          /* 日志 */
    MSG_TOPIC_MAX
} msg_topic_t;

/* 消息包：固定头 + 变长数据（数据拷贝进队列，无指针悬挂） */
typedef struct {
    msg_topic_t topic;
    uint16_t    len;
    uint8_t     data[64];   /* 小消息内联；大消息用 MSG_FLAG_PTR 传指针 */
} msg_bus_msg_t;

/* 订阅回调：返回 0 已处理，非 0 未处理（可转发） */
typedef int (*msg_bus_handler_t)(const msg_bus_msg_t *msg, void *arg);

/* 初始化总线（创建内部队列，启动消费线程） */
int msg_bus_init(void);

/* 发布消息到主题（任意线程可调；成功 0） */
int msg_bus_publish(msg_topic_t topic, const void *data, uint16_t len);

/* 订阅主题：注册回调（每个主题可多个订阅者） */
int msg_bus_subscribe(msg_topic_t topic, msg_bus_handler_t cb, void *arg);

/* 取消订阅 */
int msg_bus_unsubscribe(msg_topic_t topic, msg_bus_handler_t cb, void *arg);

/* 查询主题队列深度/积压（调试用） */
uint32_t msg_bus_pending(msg_topic_t topic);

/* ==================== B. 信号槽 ==================== */

/* 信号定义（新增事件在此扩展） */
typedef enum {
    MSG_SIG_NONE = 0,
    MSG_SIG_STATE_CHANGED,   /* 设备状态变化（gRdrStateFlag 类） */
    MSG_SIG_CFG_CHANGED,     /* 配置变化（写 KV 后） */
    MSG_SIG_TAG_ARRIVED,     /* 标签到达（旁路通知，非数据通路） */
    MSG_SIG_GPIO_TRIGGERED,  /* GPIO 触发 */
    MSG_SIG_OTA_PROGRESS,    /* OTA 进度 */
    MSG_SIG_MAX
} msg_sig_t;

/* 信号槽回调：携带信号参数（无数据，纯通知） */
typedef void (*msg_sig_handler_t)(msg_sig_t sig, void *arg);

/* 连接信号到槽（一个信号可多个槽） */
int msg_bus_connect(msg_sig_t sig, msg_sig_handler_t cb, void *arg);

/* 断开 */
int msg_bus_disconnect(msg_sig_t sig, msg_sig_handler_t cb, void *arg);

/* 发射信号（任意线程可调；同步调用所有已连接槽） */
void msg_bus_emit(msg_sig_t sig);

#ifdef __cplusplus
}
#endif

#endif /* __MSG_BUS_H__ */
