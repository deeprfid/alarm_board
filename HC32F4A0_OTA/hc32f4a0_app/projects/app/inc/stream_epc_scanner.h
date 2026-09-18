#ifndef _STREAM_EPC_SCANNER_H_
#define _STREAM_EPC_SCANNER_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Callback type for each extracted EPC.
 *  @param epc_bin  Binary EPC bytes
 *  @param epc_len  Length in bytes
 *  @param is_add   1 = ADD to whitelist, 0 = DEL
 *  @param user     User data pointer
 */
typedef void (*stream_epc_cb_t)(const uint8_t *epc_bin, uint8_t epc_len,
                                int is_add, void *user);

/** Opaque scanner state (size defined in .c) */
typedef struct StreamEpcScanner StreamEpcScanner;

/** Create a new scanner instance.
 *  @return Pointer, or NULL if allocation failed.
 */
StreamEpcScanner *stream_epc_scanner_new(stream_epc_cb_t cb, void *user);

/** Feed a chunk of data to the scanner.
 *  @param s   Scanner instance
 *  @param data   Raw bytes from HTTP body
 *  @param len    Number of bytes
 *  @return 0 on success, -1 on parse error
 */
int stream_epc_scanner_feed(StreamEpcScanner *s,
                            const char *data, size_t len);

/** Check if the scanner has finished (hit end of epc array).
 *  @return 1 if done, 0 if still expecting more data.
 */
int stream_epc_scanner_done(const StreamEpcScanner *s);
/** Return the method detected: 1=ADD, 0=DEL, -1=unknown */
int stream_epc_scanner_get_method(const StreamEpcScanner *s);

/** Reset scanner for reuse.
 *  @param s  Scanner instance
 */
void stream_epc_scanner_reset(StreamEpcScanner *s);

/** Destroy scanner.
 *  @param s  Scanner instance
 */
void stream_epc_scanner_free(StreamEpcScanner *s);

#ifdef __cplusplus
}
#endif

#endif /* _STREAM_EPC_SCANNER_H_ */
