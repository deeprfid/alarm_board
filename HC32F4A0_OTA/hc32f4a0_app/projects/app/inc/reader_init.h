#ifndef _READER_INIT_H_
#define _READER_INIT_H_

#include "ModuleReader.h"
#include "hc32f46_driver.h"   /* v9.81ch: WorkMode_Code */

#ifdef __cplusplus
extern "C" {
#endif

READER_ERR OpenReader(void);
int HandleModErr(void);

/* v9.81ch: reader state access interface (replaces cross-file extern) */
int  rdr_get_handle(void);
int  rdr_get_ant_number(void);
ConnAnts_ST *rdr_get_connants(void);
int  rdr_can_async_inv(void);
int  rdr_is_unknown_mod(void);
int  rdr_get_uart0_bindex(void);
int  rdr_get_uart0_bauds(int idx);
int  rdr_get_tag_send_len(void);
int  rdr_get_cmd_recv_len(void);
int  rdr_get_handle_passive(void);
READER_ERR rdr_err(void);
void rdr_set_err(READER_ERR e);
WorkMode_Code rdr_workmode(void);
int rdr_async_inv_started(void);
void rdr_set_async_inv(int v);
int init_upload(void);

#ifdef __cplusplus
}
#endif

#endif


