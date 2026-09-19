#ifndef EAS_CONFIG_HANDLER_H
#define EAS_CONFIG_HANDLER_H

#include "hc32f46_driver.h"
#include "mp_pool.h"
#include "cJSON.h"
#include "HttpModuleAPI.h"

// 定义最大缓冲区大小
#define EAS_CONFIG_MAX_BUF_SIZE 1024
#define MAX_FILTER_RULES 10
#define MAX_TAGS_IN_RESPONSE 1000

// 响应状态码
typedef enum
{
    EAS_SUCCESS = 0,
    EAS_ERR_JSON_PARSE = -1,
    EAS_ERR_MISSING_PARAM = -2,
    EAS_ERR_INVALID_PARAM = -3,
    EAS_ERR_MEMORY = -4,
    EAS_ERR_OPERATION = -5
} EASErrorCode;

// 处理函数指针类型
typedef EASErrorCode (*EASHandlerFunc)(cJSON *root, char *outBuf, int bufSize);

// EAS配置处理器
class EASConfigHandler
{
public:
    static EASErrorCode processRequest(const char *jsonStr, int len,
                                       HttpModuleAPI *api,
                                       char *outBuf, int bufSize);

private:
    // 各功能处理函数
    static EASErrorCode handleGetConfig(cJSON *root, char *outBuf, int bufSize);
    static EASErrorCode handleSetConfig(cJSON *root, char *outBuf, int bufSize);
    static EASErrorCode handleUploadTags(cJSON *root, char *outBuf, int bufSize);
    static EASErrorCode handleReadLog(cJSON *root, char *outBuf, int bufSize);
    static EASErrorCode handleReadTags(cJSON *root, char *outBuf, int bufSize);
    static EASErrorCode handleWriteTags(cJSON *root, char *outBuf, int bufSize);
    static EASErrorCode handleMqttConfig(cJSON *root, char *outBuf, int bufSize);
    static EASErrorCode handleGpioTest(cJSON *root, char *outBuf, int bufSize);

    // 工具函数
    static bool parseFilterRule(cJSON *ruleObj, tagfilter *filter);
    static bool parseMqttConfig(cJSON *mqttObj);
    static void buildFilterRulesJson(cJSON *parent);
    static void buildMqttConfigJson(cJSON *parent);
    static void buildTagListJson(cJSON *parent, LTNode *tagList,
                                 uint32_t maxCount, bool includeCrc = false);
    static void buildTagLogJson(cJSON *parent, LTNode *tagList);

    // 字符串处理（嵌入式环境专用）
    static int safeStrCopy(char *dest, const char *src, int maxLen);
    static bool isHexString(const char *str);
    static bool hexStringToBytes(const char *hexStr, uint8_t *bytes, int maxLen);
    static void bytesToHexString(const uint8_t *bytes, int len, char *outStr);

    // 配置验证
    static bool validateConfigValues(rfidcfg *cfg);
    static bool validateSystemTime(const char *timeStr);

private:
    // 全局配置指针（通过processRequest传入）
    static rfidcfg *s_rfidConfig;
    static mqttcfg *s_mqttConfig;
    static fdb_tsdb *s_whitelistDB;
    static LTNode *s_uploadTagList;
    static LTNode *s_staticTagList;

    // 临时缓冲区
    static char s_tempBuf[256];
};

#endif // EAS_CONFIG_HANDLER_H