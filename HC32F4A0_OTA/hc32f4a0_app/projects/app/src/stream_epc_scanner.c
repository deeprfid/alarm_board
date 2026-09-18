#include "stream_epc_scanner.h"
#include <string.h>
#include <stdlib.h>

/* ================================================================
 *  Stream EPC Scanner - 流式解析，输入格式：method=add/del, epc=[hex,...]
 *
 *  状态机：
 *    ST_INIT          等待左花括号
 *    ST_KEY_PRE       跳过 key 前空白
 *    ST_METHOD_PRE    等待 method 关键字后的冒号与双引号
 *    ST_METHOD_VAL    读取 add/del 值
 *    ST_EPC_KEY       等待 epc 关键字后的左方括号
 *    ST_EPC_SKIP      跳过 EPC 之间分隔符
 *    ST_EPC_STRING    读取 EPC hex 字符串
 *    ST_DONE          等待右方括号
 * ================================================================ */

enum {
    ST_INIT         = 0,
    ST_KEY_PRE      = 1,
    ST_METHOD_PRE   = 2,
    ST_METHOD_VAL   = 3,
    ST_EPC_KEY      = 4,
    ST_EPC_SKIP     = 5,
    ST_EPC_STRING   = 6,
    ST_DONE         = 7,
    ST_ERROR        = 8,
    ST_METHOD_COLON = 9
};

#define MAX_EPC_HEX_LEN 64

struct StreamEpcScanner {
    int state;
    int is_add;
    char hex_buf[MAX_EPC_HEX_LEN];
    int  hex_len;
    stream_epc_cb_t cb;
    void *user;
    int done_flag;
};

static int is_ws(char c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static int hex_char_to_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int hex_to_bin(const char *hex, int hex_len, uint8_t *out, int max_out)
{
    int i, pos = 0;
    for (i = 0; i + 1 < hex_len && pos < max_out; i += 2)
    {
        int hi = hex_char_to_nibble(hex[i]);
        int lo = hex_char_to_nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[pos++] = (uint8_t)((hi << 4) | lo);
    }
    return pos;
}

int stream_epc_scanner_feed(StreamEpcScanner *s, const char *data, size_t len)
{
    size_t i;
    if (!s || s->state == ST_DONE || s->state == ST_ERROR)
        return 0;

    for (i = 0; i < len; i++)
    {
        char c = data[i];

        switch (s->state)
        {
        /* ===== ST_INIT: skip until '{' ===== */
        case ST_INIT:
            if (c == '{')
                s->state = ST_KEY_PRE;
            continue;

        /* ===== ST_KEY_PRE: inside object, scan key names ===== */
        case ST_KEY_PRE:
            if (is_ws(c) || c == ':') continue;
            if (c == ',') continue;
            if (c == '"')
            {
                /* peek ahead for known keys: "method" or "epc" */
                if (i + 7 <= len && memcmp(data + i + 1, "method", 6) == 0)
                {
                    s->state = ST_METHOD_PRE;
                    i += 6;  /* skip past 'method' */
                    continue;
                }
                if (i + 4 <= len && memcmp(data + i + 1, "epc", 3) == 0)
                {
                    s->state = ST_EPC_KEY;
                    i += 3;  /* skip past 'epc' */
                    continue;
                }
                /* unknown key: skip to its closing '"' */
                {
                    size_t j = i + 1;
                    while (j < len && data[j] != '"') j++;
                    if (j < len) i = j - 1;   /* v9.81cc: off-by-one——for 循环 i++ 后从引号处重新处理，
                                               * 否则跳到引号后一位，下一个 key 的前引号被跳过（美化 JSON 丢 "epc"） */
                }
                continue;
            }
            if (c == '}') { s->state = ST_DONE; s->done_flag = 1; return 0; }
            continue;

        /* ===== ST_METHOD_PRE: after "method", skip closing quote, wait ':' ===== */
        case ST_METHOD_PRE:
            if (is_ws(c) || c == '"') continue;   /* v9.81cc: method 结束引号跳过（此前被误当值引号 → is_add 误判） */
            if (c == ':') { s->state = ST_METHOD_COLON; }
            continue;

        /* ===== ST_METHOD_COLON: after ':', wait for value opening quote ===== */
        case ST_METHOD_COLON:
            if (is_ws(c)) continue;
            if (c == '"')
            {
                s->state = ST_METHOD_VAL;
                s->hex_len = 0;
            }
            continue;

        /* ===== ST_METHOD_VAL: collect "add"/"del" string ===== */
        case ST_METHOD_VAL:
            if (c == '"')
            {
                /* v9.82h: 只有 "del" 判 DEL，其余（add/writetag 等）一律 ADD。
                 * 原只认 "add"——writetag 被误判 is_add=0 -> EPC 进 DELlist -> 同步时先 ADD 后 DEL 删光 */
                s->is_add = !(s->hex_len == 3 &&
                              s->hex_buf[0] == 'd' &&
                              s->hex_buf[1] == 'e' &&
                              s->hex_buf[2] == 'l');
                s->hex_len = 0;
                s->state = ST_KEY_PRE;
                continue;
            }
            if (s->hex_len < MAX_EPC_HEX_LEN - 1)
                s->hex_buf[s->hex_len++] = c;
            continue;

        /* ===== ST_EPC_KEY: after "epc", skip to '[' ===== */
        case ST_EPC_KEY:
            if (is_ws(c) || c == ':') continue;
            if (c == '[')
            {
                s->state = ST_EPC_SKIP;
                s->hex_len = 0;
            }
            continue;

        /* ===== ST_EPC_SKIP: inside epc array, between strings ===== */
        case ST_EPC_SKIP:
            if (is_ws(c) || c == ',') continue;
            if (c == '"')
            {
                s->state = ST_EPC_STRING;
                s->hex_len = 0;
                continue;
            }
            if (c == ']')
            {
                s->state = ST_DONE;
                s->done_flag = 1;
                return 0;
            }
            continue;

        /* ===== ST_EPC_STRING: collecting one hex EPC string ===== */
        case ST_EPC_STRING:
            if (c == '"')
            {
                if (s->hex_len > 0 && s->cb)
                {
                    uint8_t bin[32];
                    int bin_len = hex_to_bin(s->hex_buf, s->hex_len, bin, sizeof(bin));
                    if (bin_len > 0)
                        s->cb(bin, (uint8_t)bin_len, s->is_add, s->user);
                }
                s->hex_len = 0;
                s->state = ST_EPC_SKIP;
                continue;
            }
            if (s->hex_len < MAX_EPC_HEX_LEN - 1)
                s->hex_buf[s->hex_len++] = c;
            continue;

        case ST_DONE:
            return 0;

        default:
            s->state = ST_ERROR;
            return -1;
        }
    }
    return 0;
}

int stream_epc_scanner_done(const StreamEpcScanner *s)
{
    return s ? s->done_flag : 1;
}

int stream_epc_scanner_get_method(const StreamEpcScanner *s)
{
    return s ? s->is_add : -1;
}

void stream_epc_scanner_reset(StreamEpcScanner *s)
{
    if (s)
    {
        s->state = ST_INIT;
        s->is_add = 1;
        s->hex_len = 0;
        s->done_flag = 0;
    }
}

StreamEpcScanner *stream_epc_scanner_new(stream_epc_cb_t cb, void *user)
{
    StreamEpcScanner *s = (StreamEpcScanner *)malloc(sizeof(StreamEpcScanner));
    if (s)
    {
        memset(s, 0, sizeof(*s));
        s->state = ST_INIT;
        s->is_add = 1;
        s->cb = cb;
        s->user = user;
        s->done_flag = 0;
    }
    return s;
}

void stream_epc_scanner_free(StreamEpcScanner *s)
{
    free(s);
}
