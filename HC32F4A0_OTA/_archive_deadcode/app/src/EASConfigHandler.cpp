#include "EASConfigHandler.h"
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"
// 静态成员初始化
rfidcfg *EASConfigHandler::s_rfidConfig   = nullptr;
mqttcfg *EASConfigHandler::s_mqttConfig   = nullptr;
fdb_tsdb *EASConfigHandler::s_whitelistDB = nullptr;
LTNode *EASConfigHandler::s_uploadTagList = nullptr;
LTNode *EASConfigHandler::s_staticTagList = nullptr;
char EASConfigHandler::s_tempBuf[256];

// 主处理函数
EASErrorCode EASConfigHandler::processRequest(const char *jsonStr, int len,
                                              HttpModuleAPI *api,
                                              char *outBuf, int bufSize)
{
    if (!jsonStr || len <= 0 || !api || !outBuf || bufSize < 128)
    {
        return EAS_ERR_INVALID_PARAM;
    }

    // 解析JSON
    cJSON *root = cJSON_ParseWithLength(jsonStr, len);
    if (!root)
    {
        return EAS_ERR_JSON_PARSE;
    }

    // 设置全局配置指针（从api传入）
    // 注意：这里需要根据您的实际情况获取这些指针
    extern rfidcfg mycfgdata;
    extern mqttcfg mymqttcfg;
    extern struct fdb_tsdb whitelistDB;
    extern LTNode *g_Uploadtag;
    extern LTNode *staticlist;

    s_rfidConfig = &mycfgdata;
    s_mqttConfig = &mymqttcfg;
    s_whitelistDB = &whitelistDB;
    s_uploadTagList = g_Uploadtag;
    s_staticTagList = staticlist;

    EASErrorCode result = EAS_ERR_OPERATION;
    bool hasMethod = false;

    // 检查是否有method字段
    cJSON *methodItem = cJSON_GetObjectItem(root, "method");
    if (methodItem && cJSON_IsString(methodItem))
    {
        hasMethod = true;
        const char *method = methodItem->valuestring;

        // 根据method调用相应的处理函数
        if (strcmp(method, "upload") == 0)
        {
            result = handleUploadTags(root, outBuf, bufSize);
        }
        else if (strcmp(method, "readlog") == 0)
        {
            result = handleReadLog(root, outBuf, bufSize);
        }
        else if (strcmp(method, "readtag") == 0)
        {
            result = handleReadTags(root, outBuf, bufSize);
        }
        else if (strcmp(method, "writetag") == 0)
        {
            result = handleWriteTags(root, outBuf, bufSize);
        }
        else if (strcmp(method, "mqtt") == 0)
        {
            result = handleMqttConfig(root, outBuf, bufSize);
        }
        else if (strcmp(method, "gpiotest") == 0)
        {
            result = handleGpioTest(root, outBuf, bufSize);
        }
    }

    // 如果没有method字段，检查get/set配置
    if (!hasMethod)
    {
        cJSON *getItem = cJSON_GetObjectItem(root, "get");
        cJSON *setItem = cJSON_GetObjectItem(root, "set");

        if (getItem && cJSON_IsNumber(getItem) && getItem->valueint == 1)
        {
            result = handleGetConfig(root, outBuf, bufSize);
        }
        else if (setItem && cJSON_IsNumber(setItem) && setItem->valueint == 1)
        {
            result = handleSetConfig(root, outBuf, bufSize);
        }
    }

    cJSON_Delete(root);
    return result;
}

// 处理获取配置
EASErrorCode EASConfigHandler::handleGetConfig(cJSON *root, char *outBuf, int bufSize)
{
    if (!s_rfidConfig || !outBuf)
        return EAS_ERR_INVALID_PARAM;

    cJSON *response = cJSON_CreateObject();
    if (!response)
        return EAS_ERR_MEMORY;

    cJSON_AddStringToObject(response, "result", "get");
    cJSON_AddStringToObject(response, "tagstoragedays", s_rfidConfig->tagstoragedays);
    cJSON_AddStringToObject(response, "remark", s_rfidConfig->remark);
    cJSON_AddNumberToObject(response, "totaltags", s_rfidConfig->totaltagcnt);
    cJSON_AddNumberToObject(response, "totalalarmcnt", s_rfidConfig->totalalarmcnt);
    cJSON_AddStringToObject(response, "deviceID", s_rfidConfig->deviceID);
    cJSON_AddNumberToObject(response, "easflag", s_rfidConfig->easflag);
    cJSON_AddStringToObject(response, "system_time", s_rfidConfig->system_time);
    cJSON_AddNumberToObject(response, "radar_range", s_rfidConfig->radar_range);
    cJSON_AddNumberToObject(response, "alarm_volume", s_rfidConfig->alarm_volume);
    cJSON_AddNumberToObject(response, "peoplecount", s_rfidConfig->peoplecount);
    cJSON_AddNumberToObject(response, "alarm_duration", s_rfidConfig->alarm_duration);
    cJSON_AddNumberToObject(response, "tag_read_cnt", s_rfidConfig->tag_read_cnt);
    cJSON_AddNumberToObject(response, "alarm_switch", s_rfidConfig->alarm_switch);
    cJSON_AddNumberToObject(response, "accumulated_time", s_rfidConfig->accumulated_time);
    cJSON_AddNumberToObject(response, "accumulated_count", s_rfidConfig->accumulated_count);
    cJSON_AddNumberToObject(response, "opening_time", s_rfidConfig->opening_time);
    cJSON_AddNumberToObject(response, "closing_time", s_rfidConfig->closing_time);

    // 添加filter_rule数组
    buildFilterRulesJson(response);

    char *jsonStr = cJSON_PrintUnformatted(response);
    if (jsonStr)
    {
        safeStrCopy(outBuf, jsonStr, bufSize);
        cJSON_free(jsonStr);
    }

    cJSON_Delete(response);
    return EAS_SUCCESS;
}

// 处理设置配置
EASErrorCode EASConfigHandler::handleSetConfig(cJSON *root, char *outBuf, int bufSize)
{
    if (!s_rfidConfig)
        return EAS_ERR_INVALID_PARAM;

    // 逐个解析配置项
    cJSON *item = nullptr;

    if ((item = cJSON_GetObjectItem(root, "tagstoragedays")) && cJSON_IsString(item))
    {
        safeStrCopy(s_rfidConfig->tagstoragedays, item->valuestring,
                    sizeof(s_rfidConfig->tagstoragedays));
    }

// 解析其他配置项
#define SET_CONFIG_INT(field)                                               \
    if ((item = cJSON_GetObjectItem(root, #field)) && cJSON_IsNumber(item)) \
    s_rfidConfig->field = item->valueint

    SET_CONFIG_INT(easflag);
    SET_CONFIG_INT(radar_range);
    SET_CONFIG_INT(alarm_volume);
    SET_CONFIG_INT(alarm_duration);
    SET_CONFIG_INT(tag_read_cnt);
    SET_CONFIG_INT(accumulated_time);
    SET_CONFIG_INT(accumulated_count);
    SET_CONFIG_INT(opening_time);
    SET_CONFIG_INT(closing_time);
    SET_CONFIG_INT(alarm_switch);

#undef SET_CONFIG_INT

    // 边界检查
    if (s_rfidConfig->alarm_volume > 10)
        s_rfidConfig->alarm_volume = 10;
    if (s_rfidConfig->radar_range > 10)
        s_rfidConfig->radar_range = 10;
    if (s_rfidConfig->alarm_duration > 10)
        s_rfidConfig->alarm_duration = 10;
    if (s_rfidConfig->tag_read_cnt > 10)
        s_rfidConfig->tag_read_cnt = 10;

    // 系统时间
    if ((item = cJSON_GetObjectItem(root, "system_time")) && cJSON_IsString(item))
    {
        safeStrCopy(s_rfidConfig->system_time, item->valuestring,
                    sizeof(s_rfidConfig->system_time));
        // isValidDateTime(s_rfidConfig->system_time); // 如果需要验证
    }

    // 解析filter_rule
    if ((item = cJSON_GetObjectItem(root, "filter_rule")) && cJSON_IsArray(item))
    {
        int count = cJSON_GetArraySize(item);
        if (count > MAX_FILTER_RULES)
            count = MAX_FILTER_RULES;

        for (int i = 0; i < count; i++)
        {
            cJSON *rule = cJSON_GetArrayItem(item, i);
            parseFilterRule(rule, &s_rfidConfig->tagfilter_rule[i]);
        }
    }

    // 保存配置到Flash
    // set_eastag_to_flash();

    // 构建成功响应
    const char *successMsg = "{\"set\":\"success\"}";
    safeStrCopy(outBuf, successMsg, bufSize);

    return EAS_SUCCESS;
}

// 处理上传标签
EASErrorCode EASConfigHandler::handleUploadTags(cJSON *root, char *outBuf, int bufSize)
{
    if (!s_uploadTagList)
        return EAS_ERR_INVALID_PARAM;

    cJSON *optionItem = cJSON_GetObjectItem(root, "option");
    if (!optionItem || !cJSON_IsString(optionItem))
    {
        return EAS_ERR_MISSING_PARAM;
    }

    const char *option = optionItem->valuestring;
    cJSON *response = cJSON_CreateObject();
    if (!response)
        return EAS_ERR_MEMORY;

    if (strcmp(option, "clear") == 0)
    {
        // uploadlist_clear();
        cJSON_AddNumberToObject(response, "code", 1);
        cJSON_AddStringToObject(response, "msg", "cleared");
    }
    else if (strcmp(option, "get") == 0)
    {
        cJSON_AddStringToObject(response, "method", "upload");

        // 获取标签总数
        uint32_t tagCount = Get_whitetags_total_count(s_uploadTagList);
        cJSON_AddNumberToObject(response, "tagcount", tagCount);

        // 构建标签列表
        buildTagListJson(response, s_uploadTagList, tagCount, true);

        // 添加创建时间
        char currentTime[32];
        rtc_time_update(currentTime);
        cJSON_AddStringToObject(response, "createtime", currentTime);

        cJSON_AddNumberToObject(response, "code", 1);
        cJSON_AddStringToObject(response, "msg", "success");
    }
    else
    {
        cJSON_AddNumberToObject(response, "code", -1);
        cJSON_AddStringToObject(response, "msg", "Invalid option");
    }

    char *jsonStr = cJSON_PrintUnformatted(response);
    if (jsonStr)
    {
        safeStrCopy(outBuf, jsonStr, bufSize);
        cJSON_free(jsonStr);
    }

    cJSON_Delete(response);
    return EAS_SUCCESS;
}

// 处理读取日志
EASErrorCode EASConfigHandler::handleReadLog(cJSON *root, char *outBuf, int bufSize)
{
    if (!s_uploadTagList || !s_rfidConfig)
        return EAS_ERR_INVALID_PARAM;

    cJSON *response = cJSON_CreateObject();
    if (!response)
        return EAS_ERR_MEMORY;

    // 获取标签总数
    uint32_t tagCount = Get_whitetags_total_count(s_uploadTagList);

    cJSON_AddNumberToObject(response, "code", 1);
    cJSON_AddStringToObject(response, "msg", "success");
    cJSON_AddNumberToObject(response, "count", tagCount);

    // 构建数据数组
    buildTagLogJson(response, s_uploadTagList);

    char *jsonStr = cJSON_PrintUnformatted(response);
    if (jsonStr)
    {
        safeStrCopy(outBuf, jsonStr, bufSize);
        cJSON_free(jsonStr);
    }

    cJSON_Delete(response);
    return EAS_SUCCESS;
}

// 处理读取标签
EASErrorCode EASConfigHandler::handleReadTags(cJSON *root, char *outBuf, int bufSize)
{
    if (!s_staticTagList)
        return EAS_ERR_INVALID_PARAM;

    cJSON *response = cJSON_CreateObject();
    if (!response)
        return EAS_ERR_MEMORY;

    cJSON_AddStringToObject(response, "method", "readtag");

    // 获取标签数量
    tsdb_tarversal_total(s_whitelistDB);
    uint32_t tagCount = Get_whitetags_total_count(s_staticTagList);
    s_rfidConfig->totaltagcnt = tagCount;

    // 限制最大返回数量
    if (tagCount > MAX_TAGS_IN_RESPONSE)
    {
        tagCount = MAX_TAGS_IN_RESPONSE;
    }

    cJSON_AddNumberToObject(response, "tagcount", tagCount);

    // 构建标签列表
    buildTagListJson(response, s_staticTagList, tagCount, false);

    // 添加创建时间
    char currentTime[32];
    rtc_time_update(currentTime);
    cJSON_AddStringToObject(response, "createtime", currentTime);

    char *jsonStr = cJSON_PrintUnformatted(response);
    if (jsonStr)
    {
        safeStrCopy(outBuf, jsonStr, bufSize);
        cJSON_free(jsonStr);
    }

    cJSON_Delete(response);
    return EAS_SUCCESS;
}

// 处理写入标签
EASErrorCode EASConfigHandler::handleWriteTags(cJSON *root, char *outBuf, int bufSize)
{
    cJSON *epcArray = cJSON_GetObjectItem(root, "epc");
    if (!epcArray || !cJSON_IsArray(epcArray))
    {
        return EAS_ERR_MISSING_PARAM;
    }

    int arraySize = cJSON_GetArraySize(epcArray);
    uint8_t successCount = 0;

    // 逐个处理EPC
    for (int i = 0; i < arraySize; i++)
    {
        cJSON *epcItem = cJSON_GetArrayItem(epcArray, i);
        if (!cJSON_IsString(epcItem))
            continue;

        const char *epcStr = epcItem->valuestring;
        int strLen = strlen(epcStr);

        if (strLen % 4 != 0 || !isHexString(epcStr))
        {
            continue; // 跳过无效的EPC
        }

        // 转换EPC
        LTDataType epcData = {0};
        epcData.Epclen = strLen / 2;

        if (!hexStringToBytes(epcStr, epcData.epc, sizeof(epcData.epc)))
        {
            continue;
        }

        // 更新标签列表
        extern LTNode *taglist;
        uint8_t updateFlag = 2; // 写标签标志
        tagtable_list_update(taglist, epcData, updateFlag);

        successCount++;
    }

    // 标记需要同步到Flash
    extern uint32_t FlashDB_Sync_flag;
    FlashDB_Sync_flag = 11;

    extern uint8_t tag_method;
    tag_method = OPTION_ADD;

    // 构建响应
    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "code", 1);
    cJSON_AddStringToObject(response, "msg", "success");
    cJSON_AddStringToObject(response, "whitelist", "writetag");
    cJSON_AddNumberToObject(response, "total tags", arraySize);
    cJSON_AddNumberToObject(response, "success count", successCount);

    char *jsonStr = cJSON_PrintUnformatted(response);
    if (jsonStr)
    {
        safeStrCopy(outBuf, jsonStr, bufSize);
        cJSON_free(jsonStr);
    }

    cJSON_Delete(response);
    return EAS_SUCCESS;
}

// 处理MQTT配置
EASErrorCode EASConfigHandler::handleMqttConfig(cJSON *root, char *outBuf, int bufSize)
{
    if (!s_mqttConfig)
        return EAS_ERR_INVALID_PARAM;

    cJSON *optionItem = cJSON_GetObjectItem(root, "option");
    if (!optionItem || !cJSON_IsString(optionItem))
    {
        return EAS_ERR_MISSING_PARAM;
    }

    const char *option = optionItem->valuestring;
    cJSON *response = cJSON_CreateObject();
    if (!response)
        return EAS_ERR_MEMORY;

    if (strcmp(option, "get") == 0)
    {
        cJSON_AddNumberToObject(response, "code", 1);
        cJSON_AddStringToObject(response, "msg", "success");
        buildMqttConfigJson(response);
    }
    else if (strcmp(option, "set") == 0)
    {
        // 解析并更新MQTT配置
        parseMqttConfig(root);

        // 保存配置
        // set_mqttcfg_to_flash();

        cJSON_AddNumberToObject(response, "code", 1);
        cJSON_AddStringToObject(response, "msg", "success");
    }

    char *jsonStr = cJSON_PrintUnformatted(response);
    if (jsonStr)
    {
        safeStrCopy(outBuf, jsonStr, bufSize);
        cJSON_free(jsonStr);
    }

    cJSON_Delete(response);
    return EAS_SUCCESS;
}

// 处理GPIO测试
EASErrorCode EASConfigHandler::handleGpioTest(cJSON *root, char *outBuf, int bufSize)
{
    // 执行GPIO测试
    extern void LED_test();
    LED_test();

    const char *successMsg = "{\"code\":1,\"msg\":\"success\"}";
    safeStrCopy(outBuf, successMsg, bufSize);

    return EAS_SUCCESS;
}

// ============ 工具函数实现 ============

// 安全的字符串拷贝
int EASConfigHandler::safeStrCopy(char *dest, const char *src, int maxLen)
{
    if (!dest || !src || maxLen <= 0)
        return 0;

    int i;
    for (i = 0; i < maxLen - 1 && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return i;
}

// 检查是否为有效的十六进制字符串
bool EASConfigHandler::isHexString(const char *str)
{
    if (!str)
        return false;

    while (*str)
    {
        if (!((*str >= '0' && *str <= '9') ||
              (*str >= 'a' && *str <= 'f') ||
              (*str >= 'A' && *str <= 'F')))
        {
            return false;
        }
        str++;
    }
    return true;
}

// 十六进制字符串转字节数组
bool EASConfigHandler::hexStringToBytes(const char *hexStr, uint8_t *bytes, int maxLen)
{
    if (!hexStr || !bytes)
        return false;

    int len = strlen(hexStr);
    if (len % 2 != 0 || len / 2 > maxLen)
        return false;

    for (int i = 0; i < len / 2; i++)
    {
        char hex[3] = {hexStr[i * 2], hexStr[i * 2 + 1], '\0'};
        bytes[i] = (uint8_t)strtoul(hex, nullptr, 16);
    }

    return true;
}

// 字节数组转十六进制字符串
void EASConfigHandler::bytesToHexString(const uint8_t *bytes, int len, char *outStr)
{
    if (!bytes || !outStr || len <= 0)
    {
        outStr[0] = '\0';
        return;
    }

    for (int i = 0; i < len; i++)
    {
        sprintf(&outStr[i * 2], "%02X", bytes[i]);
    }
    outStr[len * 2] = '\0';
}

// 解析过滤器规则
bool EASConfigHandler::parseFilterRule(cJSON *ruleObj, tagfilter *filter)
{
    if (!ruleObj || !filter)
        return false;

    cJSON *item;

    if ((item = cJSON_GetObjectItem(ruleObj, "ch_num")) && cJSON_IsNumber(item))
    {
        filter->chnum = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(ruleObj, "ch_status")) && cJSON_IsNumber(item))
    {
        filter->chstate = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(ruleObj, "start_addr")) && cJSON_IsNumber(item))
    {
        filter->start_addr = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(ruleObj, "match_len")) && cJSON_IsNumber(item))
    {
        filter->match_len = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(ruleObj, "mask_code")) && cJSON_IsString(item))
    {
        safeStrCopy(filter->maskcode, item->valuestring, sizeof(filter->maskcode));
    }

    return true;
}

// 解析MQTT配置
bool EASConfigHandler::parseMqttConfig(cJSON *mqttObj)
{
    if (!s_mqttConfig)
        return false;

    cJSON *item;

#define COPY_MQTT_STRING(field)                                                \
    if ((item = cJSON_GetObjectItem(mqttObj, #field)) && cJSON_IsString(item)) \
    safeStrCopy(s_mqttConfig->field, item->valuestring, sizeof(s_mqttConfig->field))

#define COPY_MQTT_HEARTBEAT(field)                                             \
    if ((item = cJSON_GetObjectItem(mqttObj, #field)) && cJSON_IsString(item)) \
    safeStrCopy(s_mqttConfig->system_mqtt_heartbeat.field, item->valuestring,  \
                sizeof(s_mqttConfig->system_mqtt_heartbeat.field))

    COPY_MQTT_STRING(host);
    COPY_MQTT_STRING(port);
    COPY_MQTT_STRING(user_name);
    COPY_MQTT_STRING(user_pwd);

    COPY_MQTT_HEARTBEAT(warehouseCode);
    COPY_MQTT_HEARTBEAT(warehouseType);
    COPY_MQTT_HEARTBEAT(deviceType);
    COPY_MQTT_HEARTBEAT(deviceModel);
    COPY_MQTT_HEARTBEAT(deviceBrand);
    COPY_MQTT_HEARTBEAT(deviceCode);
    COPY_MQTT_HEARTBEAT(softVersion);
    COPY_MQTT_HEARTBEAT(empCode);
    COPY_MQTT_HEARTBEAT(softName);
    COPY_MQTT_HEARTBEAT(remark);

#undef COPY_MQTT_STRING
#undef COPY_MQTT_HEARTBEAT

    return true;
}

// 构建过滤器规则JSON
void EASConfigHandler::buildFilterRulesJson(cJSON *parent)
{
    if (!parent || !s_rfidConfig)
        return;

    cJSON *filterArray = cJSON_CreateArray();
    if (!filterArray)
        return;

    for (int i = 0; i < MAX_FILTER_RULES; i++)
    {
        cJSON *item = cJSON_CreateObject();
        if (!item)
            continue;

        cJSON_AddNumberToObject(item, "ch_num", s_rfidConfig->tagfilter_rule[i].chnum);
        cJSON_AddNumberToObject(item, "ch_status", s_rfidConfig->tagfilter_rule[i].chstate);
        cJSON_AddNumberToObject(item, "start_addr", s_rfidConfig->tagfilter_rule[i].start_addr);
        cJSON_AddNumberToObject(item, "match_len", s_rfidConfig->tagfilter_rule[i].match_len);
        cJSON_AddStringToObject(item, "mask_code", s_rfidConfig->tagfilter_rule[i].maskcode);

        cJSON_AddItemToArray(filterArray, item);
    }

    cJSON_AddItemToObject(parent, "filter_rule", filterArray);
}

// 构建MQTT配置JSON
void EASConfigHandler::buildMqttConfigJson(cJSON *parent)
{
    if (!parent || !s_mqttConfig)
        return;

    cJSON_AddStringToObject(parent, "host", s_mqttConfig->host);
    cJSON_AddStringToObject(parent, "port", s_mqttConfig->port);
    cJSON_AddStringToObject(parent, "user_name", s_mqttConfig->user_name);
    cJSON_AddStringToObject(parent, "user_pwd", s_mqttConfig->user_pwd);

    cJSON_AddStringToObject(parent, "warehouseCode", s_mqttConfig->system_mqtt_heartbeat.warehouseCode);
    cJSON_AddStringToObject(parent, "deviceCode", s_mqttConfig->system_mqtt_heartbeat.deviceCode);
    cJSON_AddStringToObject(parent, "warehouseType", s_mqttConfig->system_mqtt_heartbeat.warehouseType);
    cJSON_AddStringToObject(parent, "deviceType", s_mqttConfig->system_mqtt_heartbeat.deviceType);
    cJSON_AddStringToObject(parent, "deviceSn", s_mqttConfig->system_mqtt_heartbeat.deviceSn);
    cJSON_AddStringToObject(parent, "deviceModel", s_mqttConfig->system_mqtt_heartbeat.deviceModel);
    cJSON_AddStringToObject(parent, "deviceBrand", s_mqttConfig->system_mqtt_heartbeat.deviceBrand);
    cJSON_AddStringToObject(parent, "softVersion", s_mqttConfig->system_mqtt_heartbeat.softVersion);
    cJSON_AddStringToObject(parent, "empCode", s_mqttConfig->system_mqtt_heartbeat.empCode);
    cJSON_AddStringToObject(parent, "softName", s_mqttConfig->system_mqtt_heartbeat.softName);
    cJSON_AddStringToObject(parent, "remark", s_mqttConfig->system_mqtt_heartbeat.remark);
}

// 构建标签列表JSON
void EASConfigHandler::buildTagListJson(cJSON *parent, LTNode *tagList,
                                        uint32_t maxCount, bool includeCrc)
{
    if (!parent || !tagList)
        return;

    cJSON *epcArray = cJSON_CreateArray();
    if (!epcArray)
        return;

    char hexBuf[32];
    char itemBuf[40];

    for (uint32_t i = 0; i < maxCount; i++)
    {
        LTDataType epcTag = Getlist_tag(tagList, i + 1);
        if (epcTag.Epclen == 0)
            break;

        // 转换EPC为十六进制字符串
        bytesToHexString(epcTag.epc, epcTag.Epclen, hexBuf);

        if (includeCrc)
        {
            sprintf(itemBuf, "%s-%d", hexBuf, epcTag.crc);
            cJSON_AddStringToObject(epcArray, "", itemBuf);
        }
        else
        {
            cJSON_AddStringToObject(epcArray, "", hexBuf);
        }
    }

    cJSON_AddItemToObject(parent, "epc", epcArray);
}

// 构建标签日志JSON
void EASConfigHandler::buildTagLogJson(cJSON *parent, LTNode *tagList)
{
    if (!parent || !tagList || !s_rfidConfig)
        return;

    cJSON *dataArray = cJSON_CreateArray();
    if (!dataArray)
        return;

    char hexBuf[32];
    char logTime[32];

    // 获取标签总数
    uint32_t tagCount = Get_whitetags_total_count(tagList);

    for (uint32_t i = 0; i < tagCount; i++)
    {
        LTDataType epcTag = Getlist_tag(tagList, i + 1);
        if (epcTag.Epclen == 0)
            break;

        cJSON *item = cJSON_CreateObject();
        if (!item)
            continue;

        // 转换时间戳
        seconds_to_date(epcTag.TimeStamp, logTime);
        cJSON_AddStringToObject(item, "time", logTime);
        cJSON_AddStringToObject(item, "deviceNo", s_rfidConfig->deviceID);

        // 创建EPC数组
        cJSON *epcArray = cJSON_CreateArray();
        if (epcArray)
        {
            bytesToHexString(epcTag.epc, epcTag.Epclen, hexBuf);
            cJSON_AddStringToObject(epcArray, "", hexBuf);
            cJSON_AddItemToObject(item, "epc", epcArray);
        }

        cJSON_AddItemToArray(dataArray, item);
    }

    cJSON_AddItemToObject(parent, "data", dataArray);
}
