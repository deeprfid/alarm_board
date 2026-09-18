#ifndef _WL_DEDUP_H_
#define _WL_DEDUP_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* v9.81cj-k: 白名单 EPC 位图去重模块
 * - 位图 64KB（512K bit）：2 万条 ~3.5% 碰撞
 * - 用法: wl_dedup_reset() 开始; wl_dedup_check(epc,len) 1=新,0=重复 */

#define WL_DEDUP_BITS   (1UL << 19)   /* 512K bit = 64KB */
#define WL_DEDUP_BYTES  (WL_DEDUP_BITS / 8)

void     wl_dedup_reset(void);
int      wl_dedup_check(const uint8_t *epc, uint8_t len);
uint32_t wl_dedup_count(void);

#ifdef __cplusplus
}
#endif

#endif /* _WL_DEDUP_H_ */

