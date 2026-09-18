/* json_api.c - json-parser 兼容 API 的 cJSON 实现（v9.81by）
 *
 * 统一 JSON 库到 cJSON：json_parse() 内部用 cJSON_Parse() 解析入站 JSON，
 * 递归转换为 json_value 树（兼容 json-parser 的结构布局），getter 在转换
 * 后的树上做点路径/数组索引查找。调用方（readercfg.c / HttpModuleAPI.cpp /
 * http_callback.c）保持零改动。
 *
 * 依赖：cJSON（MDK-Packs cJSON 1.7.7，头文件 cJSON.h）
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "json_api.h"
#include "cJSON.h"

/* ---------- 常量/声明 ---------- */
const struct _json_value json_value_none = { 0, json_none, {{0}} };

/* ARMCC 无 strdup——自行实现 */
static char *dup_str(const char *s)
{
    size_t n;
    char *p;
    if (!s)
        return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p)
        return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* json_splitname: 把 "a.b.c" / "arr[0].x" 拆成逐级 name；数组索引存于 idx 对应位 */
typedef struct {
    char name[MaxNameLenth];
    int  isarray;
    int  arrindex;
} path_seg_t;

static int split_name(const char *name, path_seg_t *segs, int *cnt)
{
    const char *p = name;
    int n = 0;

    while (*p && n < MaxNestDepth)
    {
        char *dot, *brk;
        int seglen;

        dot = strchr(p, '.');
        brk = strchr(p, '[');
        if (dot && (!brk || dot < brk))
            seglen = (int)(dot - p);
        else if (brk && (!dot || brk < dot))
            seglen = (int)(brk - p);
        else
            seglen = (int)strlen(p);

        if (seglen >= MaxNameLenth)
            return -1;
        memcpy(segs[n].name, p, seglen);
        segs[n].name[seglen] = 0;
        segs[n].isarray = 0;
        segs[n].arrindex = -1;

        p += seglen;
        if (*p == '[')
        {
            const char *q = p + 1;
            int idx = 0, dig = 0;
            while (*q >= '0' && *q <= '9') { idx = idx * 10 + (*q - '0'); dig = 1; q++; }
            if (!dig || *q != ']')
                return -1;
            segs[n].isarray = 1;
            segs[n].arrindex = idx;
            p = q + 1;
        }
        if (*p == '.')
            p++;
        n++;
    }
    *cnt = n;
    return (*p == 0) ? 0 : -1;
}

/* ---------- cJSON -> json_value 递归转换 ---------- */
static json_value *convert_node(cJSON *c, json_value *parent)
{
    json_value *v;

    if (!c)
        return NULL;

    v = (json_value *)calloc(1, sizeof(json_value));
    if (!v)
        return NULL;
    v->parent = parent;

    if (cJSON_IsObject(c))
    {
        cJSON *child;
        int n = 0, i = 0;
        v->type = json_object;
        for (child = c->child; child; child = child->next)
            n++;
        if (n > 0)
        {
            v->u.object.length = (unsigned int)n;
            v->u.object.values = (json_object_entry *)calloc(n, sizeof(json_object_entry));
            if (!v->u.object.values) { free(v); return NULL; }
            for (child = c->child; child; child = child->next)
            {
                v->u.object.values[i].name = dup_str(child->string ? child->string : "");
                v->u.object.values[i].name_length = child->string ? (unsigned int)strlen(child->string) : 0;
                v->u.object.values[i].value = convert_node(child, v);
                i++;
            }
        }
    }
    else if (cJSON_IsArray(c))
    {
        int n = cJSON_GetArraySize(c), i;
        v->type = json_array;
        v->u.array.length = (unsigned int)n;
        if (n > 0)
        {
            v->u.array.values = (json_value **)calloc(n, sizeof(json_value *));
            if (!v->u.array.values) { free(v); return NULL; }
            for (i = 0; i < n; i++)
                v->u.array.values[i] = convert_node(cJSON_GetArrayItem(c, i), v);
        }
    }
    else if (cJSON_IsString(c))
    {
        const char *s = cJSON_GetStringValue(c);
        v->type = json_string;
        v->u.string.ptr = dup_str(s ? s : "");
        v->u.string.length = s ? (unsigned int)strlen(s) : 0;
    }
    else if (cJSON_IsNumber(c))
    {
        double d = c->valuedouble;
        if (d == (double)(json_int_t)d && (d >= -9.2e18 && d <= 9.2e18))
        {
            v->type = json_integer;
            v->u.integer = (json_int_t)d;
        }
        else
        {
            v->type = json_double;
            v->u.dbl = d;
        }
    }
    else if (cJSON_IsBool(c))
    {
        v->type = json_boolean;
        v->u.boolean = c->type == cJSON_True ? 1 : 0;
    }
    else if (cJSON_IsNull(c))
    {
        v->type = json_null;
    }
    else
    {
        v->type = json_none;
    }
    return v;
}

/* ---------- json_value 树释放 ---------- */
static void free_value(json_value *v)
{
    int i;
    if (!v)
        return;
    switch (v->type)
    {
    case json_object:
        for (i = 0; i < (int)v->u.object.length; i++)
        {
            if (v->u.object.values[i].name)
                free(v->u.object.values[i].name);
            free_value(v->u.object.values[i].value);
        }
        if (v->u.object.values)
            free(v->u.object.values);
        break;
    case json_array:
        for (i = 0; i < (int)v->u.array.length; i++)
            free_value(v->u.array.values[i]);
        if (v->u.array.values)
            free(v->u.array.values);
        break;
    case json_string:
        if (v->u.string.ptr)
            free(v->u.string.ptr);
        break;
    default:
        break;
    }
    free(v);
}

/* ---------- 点路径/数组索引查找 ---------- */
static json_value *find_value(json_value *value, const char *name)
{
    path_seg_t segs[MaxNestDepth];
    int cnt = 0, j;
    json_value *cur = value;

    if (split_name(name, segs, &cnt) < 0)
        return NULL;

    for (j = 0; j < cnt; j++)
    {
        int found = 0;

        if (segs[j].isarray)
        {
            if (cur && cur->type == json_array && segs[j].arrindex >= 0 &&
                segs[j].arrindex < (int)cur->u.array.length)
            {
                cur = cur->u.array.values[segs[j].arrindex];
                found = 1;
            }
        }
        else if (cur && cur->type == json_object)
        {
            int i;
            for (i = 0; i < (int)cur->u.object.length; i++)
            {
                if (cur->u.object.values[i].name &&
                    strcmp(cur->u.object.values[i].name, segs[j].name) == 0)
                {
                    cur = cur->u.object.values[i].value;
                    found = 1;
                    break;
                }
            }
        }
        if (!found)
            return NULL;
    }
    return cur;
}

/* ---------- 对外 API ---------- */
json_value * json_parse(const json_char * json, size_t length)
{
    cJSON *root;
    char *buf;
    json_value *result;

    if (!json)
        return NULL;

    buf = (char *)malloc(length + 1);
    if (!buf)
        return NULL;
    memcpy(buf, json, length);
    buf[length] = 0;

    root = cJSON_Parse(buf);
    free(buf);
    if (!root)
        return NULL;

    result = convert_node(root, NULL);
    cJSON_Delete(root);
    return result;
}

json_value * json_parse_ex(json_settings * settings,
                           const json_char * json,
                           size_t length,
                           char * error)
{
    (void)settings; (void)error;
    return json_parse(json, length);
}

void json_value_free(json_value * v)
{
    free_value(v);
}

void json_value_free_ex(json_settings * settings, json_value * v)
{
    (void)settings;
    free_value(v);
}

int json_getobject(json_value* value, const char *name, json_value**ppobj)
{
    json_value *o = value ? find_value(value, name) : NULL;
    if (o == NULL)
        return -1;
    if (o->type != json_object && o->type != json_array)
        return -3;
    *ppobj = o;
    return 0;
}

int json_getstring(json_value* value, const char *name, char *val)
{
    json_value *o = value ? find_value(value, name) : NULL;
    if (o == NULL)
        return -1;
    if (o->type != json_string)
        return -3;
    strcpy(val, o->u.string.ptr);
    return 0;
}

int json_getstring_len(json_value* value, const char *name,
                       int len, int cmpmode, char *val)
{
    json_value *o = value ? find_value(value, name) : NULL;
    if (o == NULL)
        return -1;
    if (o->type != json_string)
        return -3;
    if (cmpmode == 0)
    {
        if (o->u.string.length != (unsigned int)len)
            return -2;
    }
    else if (cmpmode == 1)
    {
        if (o->u.string.length > (unsigned int)len)
            return -2;
    }
    else /* cmpmode == -1 或 2: 长度 >= len */
    {
        if (o->u.string.length < (unsigned int)len)
            return -2;
    }
    strcpy(val, o->u.string.ptr);
    return 0;
}

int json_getint(json_value* value, const char *name, int *val)
{
    json_value *o = value ? find_value(value, name) : NULL;
    if (o == NULL)
        return -1;
    if (o->type == json_integer)
    {
        *val = (int)o->u.integer;
        return 0;
    }
    if (o->type == json_double)
    {
        *val = (int)o->u.dbl;
        return 0;
    }
    if (o->type == json_string)
    {
        char *p = o->u.string.ptr;
        while (*p == ' ')
            p++;
        if (*p >= '0' && *p <= '9')
        {
            *val = atoi(p);
            return 0;
        }
        return -3;
    }
    return -3;
}

int json_getint64(json_value* value, const char *name, long long *val)
{
    json_value *o = value ? find_value(value, name) : NULL;
    if (o == NULL)
        return -1;
    if (o->type == json_integer)
    {
        *val = (long long)o->u.integer;
        return 0;
    }
    if (o->type == json_double)
    {
        *val = (long long)o->u.dbl;
        return 0;
    }
    if (o->type == json_string)
    {
        char *p = o->u.string.ptr;
        while (*p == ' ')
            p++;
        if (*p >= '0' && *p <= '9')
        {
            *val = atoll(p);
            return 0;
        }
        return -3;
    }
    return -3;
}

int json_getbool(json_value* value, const char *name, int *val)
{
    json_value *o = value ? find_value(value, name) : NULL;
    if (o == NULL)
        return -1;
    if (o->type != json_boolean)
        return -3;
    *val = o->u.boolean;
    return 0;
}
