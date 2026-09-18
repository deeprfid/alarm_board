/* json_api.h - json-parser 兼容 API 封装层（基于 cJSON 实现）
 *
 * v9.81by: 统一 JSON 库到 cJSON——对外保持 json-parser 的 API 签名与
 * json_value 结构布局（含 u.array.length/values[i]、u.object.values、点路径
 * "a.b.c"、数组索引 "arr[0]" 访问），内部由 json_api.c 用 cJSON_Parse 解析后
 * 递归转换为 json_value 树。66 处调用方（readercfg.c/HttpModuleAPI.cpp/
 * http_callback.c）零改动。
 *
 * 类型定义与 json-parser.h (https://github.com/udp/json-parser) 保持一致。
 */
#ifndef _JSON_API_H
#define _JSON_API_H

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef json_char
   #define json_char char
#endif

#ifndef json_int_t
   #include <inttypes.h>
   #define json_int_t int64_t
#endif

typedef struct
{
   unsigned long max_memory;
   int settings;

   void * (* mem_alloc) (size_t, int zero, void * user_data);
   void (* mem_free) (void *, void * user_data);

   void * user_data;

   size_t value_extra;

} json_settings;

#define json_enable_comments  0x01

typedef enum
{
   json_none,
   json_object,
   json_array,
   json_integer,
   json_double,
   json_string,
   json_boolean,
   json_null

} json_type;

extern const struct _json_value json_value_none;

typedef struct _json_object_entry
{
    json_char * name;
    unsigned int name_length;

    struct _json_value * value;

} json_object_entry;

typedef struct _json_value
{
   struct _json_value * parent;

   json_type type;

   union
   {
      int boolean;
      json_int_t integer;
      double dbl;

      struct
      {
         unsigned int length;
         json_char * ptr; /* null terminated */

      } string;

      struct
      {
         unsigned int length;

         json_object_entry * values;

      } object;

      struct
      {
         unsigned int length;
         struct _json_value ** values;

      } array;

   } u;

} json_value;

/* ---- json-parser 兼容 API（内部用 cJSON 实现）---- */

json_value * json_parse (const json_char * json, size_t length);

#define json_error_max 128
json_value * json_parse_ex (json_settings * settings,
                            const json_char * json,
                            size_t length,
                            char * error);

void json_value_free (json_value *);

void json_value_free_ex (json_settings * settings, json_value *);

#define MaxNestDepth 10
#define MaxNameLenth 30

int json_getobject(json_value* value, const char *name, json_value**ppobj);
int json_getstring(json_value* value, const char *name, char *val);
/* len:字符串长度 cmpmode:0=len一致;1=<=len;2(或-1? 见实现)=>=len */
int json_getstring_len(json_value* value, const char *name,
                       int len, int cmpmode, char *val);
int json_getint(json_value* value, const char *name, int *val);
int json_getint64(json_value* value, const char *name, long long *val);
int json_getbool(json_value* value, const char *name, int *val);

#ifdef __cplusplus
   } /* extern "C" */
#endif

#endif /* _JSON_API_H */
