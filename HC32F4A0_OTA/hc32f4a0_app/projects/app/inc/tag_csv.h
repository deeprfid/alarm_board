#ifndef _TAG_CSV_H_
#define _TAG_CSV_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* v9.81cj-k: 白名单 TAG.CSV 生成（TSDB 去重 -> FAT 卷文件）
 * - add 推送后调用 tag_csv_mark_dirty() 触发
 * - 后台空闲时 tag_csv_task() 遍历 TSDB 去重写 TAG.CSV
 * - 生成完 USB 虚拟盘可直接读取 */

/* add/del 推送完成后调用（置脏标记） */
void tag_csv_mark_dirty(void);
/* 后台任务入口（空闲时调用，检测脏标记后生成） */
void tag_csv_task(void);
/* v9.81cl: FAT 卷互斥（生成 vs HTTP 下载），0=拿到锁，-1=忙 */
int  tag_csv_fat_trylock(void);
void tag_csv_fat_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* _TAG_CSV_H_ */
