/**
 * @file msg_bus.c
 * @brief 消息总线实现（RTX5 队列 + 订阅/信号槽）
 *
 * 结构：
 *   - 每个主题一个 osMessageQueue（深度可配，默认 MSG_BUS_Q_DEPTH）
 *   - 订阅者链表（同一主题多订阅者，消息分发到每个）
 *   - 信号槽链表（msg_bus_emit 同步调用）
 *   - 消费线程：每个主题一个（msg_bus_consume_task）
 *
 * 线程安全：
 *   - publish/emit 任意线程可调（osMessageQueuePut 内部互斥）
 *   - 订阅注册表用 osMutex 保护（运行期注册，极少冲突）
 */
#include <string.h>
#include "cmsis_os2.h"
#include "msg_bus.h"

/* ---------- 配置 ---------- */
#define MSG_BUS_Q_DEPTH      16      /* 每个主题队列深度 */
#define MSG_BUS_MAX_SUB      8       /* 每个主题最大订阅者 */
#define MSG_BUS_MAX_SLOTS    8       /* 每个信号最大槽数 */

/* ---------- 内部结构 ---------- */
typedef struct {
    msg_bus_handler_t cb;
    void *arg;
    uint8_t used;
} msg_bus_sub_t;

typedef struct {
    osMessageQueueId_t q;
    msg_bus_sub_t subs[MSG_BUS_MAX_SUB];
} msg_bus_topic_t;

typedef struct {
    msg_sig_handler_t cb;
    void *arg;
    uint8_t used;
} msg_bus_slot_t;

/* ---------- 静态状态（模块内部，不对外） ---------- */
static msg_bus_topic_t s_topics[MSG_TOPIC_MAX];
static msg_bus_slot_t  s_slots[MSG_SIG_MAX][MSG_BUS_MAX_SLOTS];
static osMutexId_t     s_reg_mux;      /* 订阅注册表锁 */
static uint8_t         s_inited = 0;

/* ---------- 主题消费任务 ---------- */
static void msg_bus_consume_task(void *arg)
{
    msg_topic_t topic = (msg_topic_t)(uintptr_t)arg;
    msg_bus_msg_t msg;

    for (;;) {
        /* 阻塞取（替代轮询；可配超时做看门狗） */
        if (osMessageQueueGet(s_topics[topic].q, &msg, NULL, osWaitForever) != osOK)
            continue;

        /* 分发到所有订阅者 */
        for (int i = 0; i < MSG_BUS_MAX_SUB; i++) {
            if (s_topics[topic].subs[i].used) {
                s_topics[topic].subs[i].cb(&msg, s_topics[topic].subs[i].arg);
            }
        }
    }
}

/* ---------- 初始化 ---------- */
int msg_bus_init(void)
{
    if (s_inited) return 0;

    s_reg_mux = osMutexNew(NULL);
    if (s_reg_mux == NULL) return -1;

    for (int t = 1; t < MSG_TOPIC_MAX; t++) {
        s_topics[t].q = osMessageQueueNew(MSG_BUS_Q_DEPTH, sizeof(msg_bus_msg_t), NULL);
        if (s_topics[t].q == NULL) return -1;

        /* 每个主题一个消费线程 */
        osThreadAttr_t attr = { .name = "msg_bus", .stack_size = 2048, .priority = osPriorityNormal };
        osThreadNew(msg_bus_consume_task, (void *)(uintptr_t)t, &attr);
    }

    s_inited = 1;
    return 0;
}

/* ---------- 发布 ---------- */
int msg_bus_publish(msg_topic_t topic, const void *data, uint16_t len)
{
    if (!s_inited || topic <= MSG_TOPIC_NONE || topic >= MSG_TOPIC_MAX)
        return -1;
    if (len > sizeof(((msg_bus_msg_t *)0)->data))
        return -1;   /* 超长：调用方改用指针消息或增大 data */

    msg_bus_msg_t msg;
    msg.topic = topic;
    msg.len = len;
    if (data && len) memcpy(msg.data, data, len);

    return (osMessageQueuePut(s_topics[topic].q, &msg, 0, 0) == osOK) ? 0 : -1;
}

/* ---------- 订阅 ---------- */
int msg_bus_subscribe(msg_topic_t topic, msg_bus_handler_t cb, void *arg)
{
    if (!s_inited || topic <= MSG_TOPIC_NONE || topic >= MSG_TOPIC_MAX || cb == NULL)
        return -1;

    osMutexAcquire(s_reg_mux, osWaitForever);
    int rc = -1;
    for (int i = 0; i < MSG_BUS_MAX_SUB; i++) {
        if (!s_topics[topic].subs[i].used) {
            s_topics[topic].subs[i].cb = cb;
            s_topics[topic].subs[i].arg = arg;
            s_topics[topic].subs[i].used = 1;
            rc = 0;
            break;
        }
    }
    osMutexRelease(s_reg_mux);
    return rc;
}

int msg_bus_unsubscribe(msg_topic_t topic, msg_bus_handler_t cb, void *arg)
{
    if (!s_inited || topic <= MSG_TOPIC_NONE || topic >= MSG_TOPIC_MAX)
        return -1;

    osMutexAcquire(s_reg_mux, osWaitForever);
    int rc = -1;
    for (int i = 0; i < MSG_BUS_MAX_SUB; i++) {
        if (s_topics[topic].subs[i].used &&
            s_topics[topic].subs[i].cb == cb &&
            s_topics[topic].subs[i].arg == arg) {
            s_topics[topic].subs[i].used = 0;
            rc = 0;
            break;
        }
    }
    osMutexRelease(s_reg_mux);
    return rc;
}

uint32_t msg_bus_pending(msg_topic_t topic)
{
    if (!s_inited || topic <= MSG_TOPIC_NONE || topic >= MSG_TOPIC_MAX)
        return 0;
    return osMessageQueueGetCount(s_topics[topic].q);
}

/* ---------- 信号槽 ---------- */
int msg_bus_connect(msg_sig_t sig, msg_sig_handler_t cb, void *arg)
{
    if (!s_inited || sig <= MSG_SIG_NONE || sig >= MSG_SIG_MAX || cb == NULL)
        return -1;

    osMutexAcquire(s_reg_mux, osWaitForever);
    int rc = -1;
    for (int i = 0; i < MSG_BUS_MAX_SLOTS; i++) {
        if (!s_slots[sig][i].used) {
            s_slots[sig][i].cb = cb;
            s_slots[sig][i].arg = arg;
            s_slots[sig][i].used = 1;
            rc = 0;
            break;
        }
    }
    osMutexRelease(s_reg_mux);
    return rc;
}

int msg_bus_disconnect(msg_sig_t sig, msg_sig_handler_t cb, void *arg)
{
    if (!s_inited || sig <= MSG_SIG_NONE || sig >= MSG_SIG_MAX)
        return -1;

    osMutexAcquire(s_reg_mux, osWaitForever);
    int rc = -1;
    for (int i = 0; i < MSG_BUS_MAX_SLOTS; i++) {
        if (s_slots[sig][i].used &&
            s_slots[sig][i].cb == cb &&
            s_slots[sig][i].arg == arg) {
            s_slots[sig][i].used = 0;
            rc = 0;
            break;
        }
    }
    osMutexRelease(s_reg_mux);
    return rc;
}

void msg_bus_emit(msg_sig_t sig)
{
    if (!s_inited || sig <= MSG_SIG_NONE || sig >= MSG_SIG_MAX)
        return;

    /* 快照遍历（发射中注册变化不在此锁内处理，简单场景够用） */
    osMutexAcquire(s_reg_mux, osWaitForever);
    for (int i = 0; i < MSG_BUS_MAX_SLOTS; i++) {
        if (s_slots[sig][i].used) {
            msg_sig_handler_t cb = s_slots[sig][i].cb;
            void *arg = s_slots[sig][i].arg;
            osMutexRelease(s_reg_mux);   /* 回调内不持锁（防死锁） */
            cb(sig, arg);
            osMutexAcquire(s_reg_mux, osWaitForever);
        }
    }
    osMutexRelease(s_reg_mux);
}
