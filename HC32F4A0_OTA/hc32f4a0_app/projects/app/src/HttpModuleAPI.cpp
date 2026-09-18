#include <stdio.h>
#include <ctype.h>
#include "Utility.h"
#include "HttpModuleAPI.h"
#include "APIHttpResponse.h"
#include "hc32f46_driver.h"
#include "mp_pool.h"
#include "ipc.h"
//#include "List.h"
#include "cJSON.h"
#include "reader_init.h"   /* v9.81ch: rdr_* access */
#include "reader_msg.h"   /* v9.81ch: net/hb access */
#include "hw_api.h"   /* v9.81ci: driver config access */
#include "tag_csv.h"   /* v9.81cj-k */

/* 流式EPC处理 */
int g_stream_tags_added = 0;
int g_stream_tag_is_add = 1; /* 流式EPC是否ADD method: 1=ADD, 0=DEL */
extern "C" void LED_test(void);
extern "C" void set_eastag_to_flash(void);
extern "C" int get_tag_que(MsgQueObj_alarm_tag *mqObj);
extern "C" uint8_t get_tag_counter(void);
extern "C" uint8_t removeDuplicates(uint8_t *arr, uint8_t *epcid, uint8_t len);
extern "C" void tagtable_list_update(LTNode *phead, LTDataType epcID, uint8_t method);
extern "C" uint32_t tsdb_tarversal_total(fdb_tsdb_t tsdb);
extern "C" void whitelist_index_rebuild(fdb_tsdb_t tsdb);   /* v9.82i */
extern "C" LTDataType Getlist_tag(LTNode* phead, uint32_t index);
extern "C" uint32_t Get_whitetags_total_count(LTNode* phead);
extern "C" void seconds_to_date(time_t secs, char *date);
extern "C" void uploadlist_clear(void);

const char *readlogdata = "\"code\": 1,\"msg\": \"success\",\"count\": 2,\"data\": [{\"time\": \"2019-10-01 09:10:23\",\"deviceNo\": \"001\",\"epc\": [\"E01500223322445333\", \"E01500223322445334\", \"E0150022332244533A\",\"E01500223322445335\"],\"mem\": \"\"},{\"time\": \"2019-10-01 09:09:23\",\"deviceNo\": \"001\",\"epc\": [\"E01500223322445338\", \"E01500223322445337\",\"E0150022332244533B\", \"E01500223322445336\"],\"mem\": \"\"}]";
extern LTNode *taglist;
extern LTNode *ADDlist;   /* v9.82h: writetag 加到 ADDlist（原 taglist 不落盘） */
extern LTNode* staticlist;
static int validhexstring(char *hexstr)
{
	char *p = hexstr;
	while (*p)
	{
		if (!((*p >= '0' && *p <= '9') ||
			  (*p >= 'a' && *p <= 'f') ||
			  (*p >= 'A' && *p <= 'F')))
			return -1;
		p++;
	}
	return 0;
}

int HttpModuleAPI::validinvants(int *ants, int antcnt)
{
	for (int i = 0; i < antcnt; ++i)
	{
		if (ants[i] < 1 || ants[i] > m_phyantportnumber)
			return -1;
		for (int j = 0; j < antcnt; ++j)
		{
			if (i != j)
			{
				if (ants[i] == ants[j])
					return -1;
			}
		}
	}
	return 0;
}

void HttpModuleAPI::ResetTagBuffer()
{
	if (m_IsCreateBuffer)
	{
		tagClear();
		setRecHighestRssi(m_tbIsRecHighestRssi);
		setUniByAnt(m_tbIsUniByAnt);
		setUniByEmdData(m_tbIsUniByEmddata);
	}
}

static int validbinstring(char *str)
{
	char *p = str;
	while (*p)
	{
		if (!(*p == '0' || *p == '1'))
			return -1;
		p++;
	}
	return 0;
}
/* v9.81bz: json_parse leak fix - auto-recycle parse tree (mp_resetpool idea) */
static json_value *g_jvalueFree = NULL;
static json_value * json_parse_auto(const char *json, int len)
{
	json_value_free(g_jvalueFree); /* free last leftover tree */
	g_jvalueFree = json_parse(json, len);
	return g_jvalueFree;
}

#define CHK_RDR_STATUS                            \
	do                                            \
	{                                             \
		if (!m_IsConnectReader)                   \
		{                                         \
			if (m_hReader != -1)                  \
			{                                     \
				CloseReader(m_hReader);           \
				m_hReader = -1;                   \
			}                                     \
			m_OpError = Init();                   \
			if (HMApiErr_Ok == m_OpError)         \
				break;                            \
			else                                  \
			{                                     \
				m_OpError = HMApiErr_Init_Rdr;    \
				return;                           \
			}                                     \
		}                                         \
		m_IsAddOtherJson = false;                 \
		if (!m_IsAsyncOp)                         \
		{                                         \
			if (m_IsAsyncRead)                    \
			{                                     \
				m_OpError = HMApiErr_Reader_Busy; \
				return;                           \
			}                                     \
		}                                         \
	} while (false);

#define CHK_JPAS_RET(exp, pname)                    \
	do                                              \
	{                                               \
		int expret = exp;                           \
		if (expret != 0)                            \
		{                                           \
			if (expret == -1)                       \
				m_OpError = HMApiErr_Param_Missing; \
			else if (expret == -2 || expret == -3)  \
				m_OpError = HMApiErr_Param_Err;     \
			strcpy(m_ErrParamName, pname);          \
			m_IsCheckError = true;                  \
			return;                                 \
		}                                           \
	} while (false);

#define CHK_PARA_RET(exp, pname)            \
	do                                      \
	{                                       \
		if (exp)                            \
		{                                   \
			m_OpError = HMApiErr_Param_Err; \
			strcpy(m_ErrParamName, pname);  \
			m_IsCheckError = true;          \
			return;                         \
		}                                   \
	} while (false);

#define CHK_MAPI_ERR(exp)                              \
	do                                                 \
	{                                                  \
		READER_ERR mapicallerr;                        \
		m_OpError = HMApiErr_Ok;                       \
		mapicallerr = exp;                             \
		if (mapicallerr == MT_IO_ERR)                  \
		{                                              \
			CloseReader(m_hReader);                    \
			m_hReader = -1;                            \
			if ((m_OpError = Init()) == HMApiErr_Ok)   \
				m_OpError = HMApiErr_Rdr_IOErr;        \
			return;                                    \
		}                                              \
		else if (mapicallerr != MT_OK_ERR)             \
		{                                              \
			m_OpError = MTECode2HMAEcode(mapicallerr); \
			if (m_IsInvTags)                           \
			{                                          \
				m_IsInvTags = false;                   \
				ResetTagBuffer();                      \
			}                                          \
			return;                                    \
		}                                              \
	} while (false);

HttpModuleAPI::HttpModuleAPI()
{
	osSemaphoreAttr_t sem_attr = {
		NULL,
		NULL,
		&m_AsyncReadSem_cb,
		sizeof(m_AsyncReadSem_cb)};
	osThreadAttr_t thAttr_t;
	osMutexAttr_t mux_attr = {
		NULL,
		osMutexRecursive | osMutexPrioInherit,
		&m_TagbufMux_cb,
		sizeof(m_TagbufMux_cb)};

	m_IsAsyncRead = false;
	m_IsConnectReader = false;
	setIsUseMutex(0);
	//	m_TagBuffer.initBuffer(500);
	//	m_TagBuffer.tagClear();
	m_IsUploadTags = false;
	m_IsInvTags = false;
	m_IsAddOtherJson = false;
	m_ErrParamName[0] = 0;
	m_AddOtherJsonBuf = m_RespJsonBuffer + OtherAddJsonBufPos;
	m_IsCheckError = false;
	m_phyantportnumber = -1;
	m_hReader = -1;
	m_HWdetails.board = MAINBOARD_NONE;
	m_HWdetails.module = MODOULE_NONE;

	m_AsyncReadSem = osSemaphoreNew(1, 0, &sem_attr);
	m_TagbufMux = osMutexNew(&mux_attr);
	//	printf("m_TagbufMux_cb:%p, m_TagbufMux:%p\n", &m_TagbufMux_cb, m_TagbufMux);
	init_osThreadAttr_t(&thAttr_t, 1536, osPriorityNormal);
	m_ThIdAsRd = osThreadNew(HttpModuleAPI::AsyncRead_Thread, this, &thAttr_t);
	m_IsCreateBuffer = false;
}

int HttpModuleAPI::create_buffer(int itcnt, int msize)
{
	m_tbMaxRecLength = itcnt;
	initTbBuffer(itcnt, msize);
	m_IsCreateBuffer = true;
	ResetTagBuffer();
	return 0;
}

#define CHK_INIT_RDR(exp)                     \
	do                                        \
	{                                         \
		READER_ERR merr;                      \
		merr = exp;                           \
		if (merr != MT_OK_ERR)                \
		{                                     \
			TRACE("%s err:%d\n", #exp, merr); \
			return MTECode2HMAEcode(merr);    \
		}                                     \
		else                                  \
			TRACE("%s ok\n", #exp);           \
	} while (false);

HttpMApiErrCode HttpModuleAPI::Init()
{
	m_IsConnectReader = false;

	CHK_INIT_RDR(OpenReader());
	m_hReader = rdr_get_handle();
	m_phyantportnumber = rdr_get_ant_number();
	m_HWdetails.module = (Module_Type)hb_data()->rfid_mod;
	m_HWdetails.board = MAINBOARD_ARM7;

	CHK_INIT_RDR(ParamGet(m_hReader, MTR_PARAM_RF_MINPOWER, &m_MinPwr));
	CHK_INIT_RDR(ParamGet(m_hReader, MTR_PARAM_RF_MAXPOWER, &m_MaxPwr));

	CHK_INIT_RDR(ParamGet(m_hReader, MTR_PARAM_TAGDATA_UNIQUEBYANT, &m_tbIsUniByAnt));
	CHK_INIT_RDR(ParamGet(m_hReader, MTR_PARAM_TAGDATA_UNIQUEBYEMDDATA, &m_tbIsUniByEmddata));
	CHK_INIT_RDR(ParamGet(m_hReader, MTR_PARAM_TAGDATA_RECORDHIGHESTRSSI, &m_tbIsRecHighestRssi));

	ResetTagBuffer();
	/*
	m_TagBuffer.tagClear();
	m_TagBuffer.setUniByAnt(m_tbIsUniByAnt == 1);
	m_TagBuffer.setUniByEmdData(m_tbIsUniByEmddata == 1);
	m_TagBuffer.setRecHighestRssi(m_tbIsRecHighestRssi == 1);
	m_TagBuffer.setUniByTimeStamp(false);
	m_TagBuffer.setUniByCrc(false);
	*/
	m_IsConnectReader = true;
	return HMApiErr_Ok;
}

#define HttpAPIUrl_paramset "/moduleapi/paramset"
#define HttpAPIUrl_paramget "/moduleapi/paramget"
#define HttpAPIUrl_startasyncinventory "/moduleapi/startasyncinventory"
#define HttpAPIUrl_stopasyncinventory "/moduleapi/stopasyncinventory"
#define HttpAPIUrl_getasynctags "/moduleapi/getasynctags"
#define HttpAPIUrl_syncinventory "/moduleapi/syncinventory"
#define HttpAPIUrl_readtagbank "/moduleapi/readtagbank"
#define HttpAPIUrl_writetagbank "/moduleapi/writetagbank"
#define HttpAPIUrl_writetagepc "/moduleapi/writetagepc"
#define HttpAPIUrl_locktag "/moduleapi/locktag"
#define HttpAPIUrl_killtag "/moduleapi/killtag"
#define HttpAPIUrl_getgpi "/moduleapi/getgpi"
#define HttpAPIUrl_setgpo "/moduleapi/setgpo"
#define HttpAPIUrl_psamtransceiver "/moduleapi/psamtransceiver"
#define HttpAPIUrl_reboot "/moduleapi/reboot"
#define HttpAPIUrl_resetrfidmodule "/moduleapi/resetrfidmodule"
// #define HttpAPIUrl_method "/moduleapi/tagadd"
#define HttpAPIUrl_method ""
#define HttpAPIUrl_tagdata "/tagdata"
#define HttpAPIUrl_eascfg "/moduleapi/eascfg"
int HttpModuleAPI::httpAPIDispatch(char *url, char *json, int len)
{
	m_IsWhitelist = false;   /* v9.81ce: reset per request */
	m_IsCheckError = false;
	m_IsInvTags = false;
	m_IsAsyncOp = false;

	/* v9.81cj-i: body 动态申请保护——POST 需 body，NULL 且 len>0 说明分配失败 */
	if (json == NULL && len > 0)
		return HMApiErr_Param_Err;


	//	TRACE("httpAPIDispatch url:%s, json:%s\n", url, json);
	if (strcmp(url, HttpAPIUrl_paramset) == 0)
		httpParamSet(json, len);
	else if (strcmp(url, HttpAPIUrl_syncinventory) == 0)
		httpSyncInventory(json, len);

	else if ((strcmp(url, HttpAPIUrl_method) == 0) || (strcmp(url, HttpAPIUrl_tagdata) == 0))
	{
		httpSaveTagMethod(json, len);
		strcpy(url + 11, "EAS_Tag");
	}
	else if (strcmp(url, HttpAPIUrl_eascfg) == 0)
		httpEascfg(json, len);

	else if (strcmp(url, HttpAPIUrl_paramget) == 0)
		httpParamGet(json, len);
	else if (strcmp(url, HttpAPIUrl_startasyncinventory) == 0)
		httpStartAsyncInventory(json, len);
	else if (strcmp(url, HttpAPIUrl_stopasyncinventory) == 0)
		httpStopAsyncInventory(json, len);
	else if (strcmp(url, HttpAPIUrl_getasynctags) == 0)
		httpGetAsynctags(json, len);
	else if (strcmp(url, HttpAPIUrl_readtagbank) == 0)
		httpReadTagBank(json, len);
	else if (strcmp(url, HttpAPIUrl_writetagbank) == 0)
		httpWriteTagBank(json, len);
	else if (strcmp(url, HttpAPIUrl_writetagepc) == 0)
		httpWriteTagEpc(json, len);
	else if (strcmp(url, HttpAPIUrl_locktag) == 0)
		httpLockTag(json, len);
	else if (strcmp(url, HttpAPIUrl_killtag) == 0)
		httpKillTag(json, len);
	else if (strcmp(url, HttpAPIUrl_getgpi) == 0)
		httpGetGPI(json, len);
	else if (strcmp(url, HttpAPIUrl_setgpo) == 0)
		httpSetGPO(json, len);
	else if (strcmp(url, HttpAPIUrl_reboot) == 0)
		httpReboot(json, len);
	else if (strcmp(url, HttpAPIUrl_resetrfidmodule) == 0)
		httpResetRfidModule(json, len);
	else
		return 404;

	strcpy(m_ReqType, url + 11);
	return 200;
}

int HttpModuleAPI::httpGetChunkedHeader(int fd)
{
	m_IsFinChunked = false;
	const char *errdes;
	if (m_OpError == HMApiErr_Param_Err ||
		m_OpError == HMApiErr_Param_Missing ||
		m_OpError == HMApiErr_Param_NotSupported)
		errdes = ParamErr2String(m_OpError);
	else
		errdes = HMECode2String(m_OpError, m_hReader);

	GetRdrIdentifier(m_RdrAddr, fd);

	if (m_IsInvTags)
	{
		sprintf(m_RespJsonBuffer, "{\"reader_name\":\"%s\",\"op_type\":\"%s\",\"err_code\":%d,\"err_string\":\"%s\",\"result\":[",
				m_RdrAddr, m_ReqType, m_OpError, errdes);
		m_IsFirstTag = true;
	}
	else if (m_IsAddOtherJson && m_OpError == HMApiErr_Ok)
	{
		sprintf(m_RespJsonBuffer, "{\"reader_name\":\"%s\",\"op_type\":\"%s\",\"err_code\":%d,\"err_string\":\"%s\",%s",
				m_RdrAddr, m_ReqType, m_OpError, errdes, m_AddOtherJsonBuf);
	}
	/*
	else if (m_IsUploadTags)
	{
		sprintf(m_RespJsonBuffer, "{\"reader_name\":\"%s\",\"op_type\":\"%s\",\"err_code\":%d,\"err_string\":\"%s\",%s",
			m_RdrAddr, m_ReqType, m_OpError, errdes, m_AddOtherJsonBuf);
	}
	*/
	else
		sprintf(m_RespJsonBuffer, "{\"reader_name\":\"%s\",\"op_type\":\"%s\",\"err_code\":%d,\"err_string\":\"%s\"",
				m_RdrAddr, m_ReqType, m_OpError, errdes);

	return strlen(m_RespJsonBuffer);
}
int HttpModuleAPI::httpGetNextChunked()
{
	if (m_IsFinChunked)
		return 0;

	if (m_IsWhitelist)   /* v9.81ce: deduped EPC string array chunked */
	{
		int stringlen = 0;
		m_RespJsonBuffer[0] = 0;
		while (true)
		{
			if (m_WlIdx >= m_WlTotal)
			{
				strcat(m_RespJsonBuffer, "]}");
				m_IsFinChunked = true;
				m_IsWhitelist = false;
				stringlen += 2;
				LTClear(staticlist);   /* v9.81ce: free readtag-rebuilt staticlist (~79KB) - else heap exhausts on next add/del */
				break;
			}
			LTDataType epctag = Getlist_tag(staticlist, m_WlIdx + 1);
			char epcstr[64];
			memset(epcstr, 0, sizeof(epcstr));
			uint8_t eplen = (epctag.Epclen > 16) ? 16 : epctag.Epclen;   /* v9.81ce: Epclen cap 16 - prevent overread */
			Hex2Str(epctag.epc, eplen, epcstr);
			if (m_WlIdx == 0)
				strcat(m_RespJsonBuffer, "\"");
			else
				strcat(m_RespJsonBuffer, ",\"");
			strcat(m_RespJsonBuffer, epcstr);
			strcat(m_RespJsonBuffer, "\"");
			m_WlIdx++;
			stringlen = (int)strlen(m_RespJsonBuffer);   /* v9.81ce: actual len - was len+4/EPC overcount -> send overread garbage */
			if (stringlen > 1000)   /* chunk < W5100S TX buf (2KB default) */
				break;
		}
		return stringlen;
	}

	if (m_IsInvTags)
	{
		TAGINFO tag;
		int stringlen = 0;
		m_RespJsonBuffer[0] = 0;
		osMutexAcquire(m_TagbufMux, osWaitForever);
		while (true)
		{
			if (tagGetNext(&tag) < 0)
			{
				strcat(m_RespJsonBuffer, "]}");
				m_IsFinChunked = true;
				stringlen += 2;
				m_IsInvTags = false;
				break;
			}
			if (m_IsFirstTag)
			{
				strcat(m_RespJsonBuffer, "{\"epc\":\"");
				stringlen += 8;
				m_IsFirstTag = false;
			}
			else
			{
				strcat(m_RespJsonBuffer, ",{\"epc\":\"");
				stringlen += 9;
			}
			Hex2Str(tag.EpcId, tag.Epclen, m_StrConvertBuffer);
			strcat(m_RespJsonBuffer, m_StrConvertBuffer);
			stringlen += tag.Epclen * 2;
			strcat(m_RespJsonBuffer, "\",\"bank_data\":\"");
			stringlen += 15;
			Hex2Str(tag.EmbededData, tag.EmbededDatalen, m_StrConvertBuffer);
			strcat(m_RespJsonBuffer, m_StrConvertBuffer);
			stringlen += tag.EmbededDatalen * 2;
			strcat(m_RespJsonBuffer, "\",\"antenna\":");
			stringlen += 12;
			sprintf(m_StrConvertBuffer, "%d", tag.AntennaID);
			strcat(m_RespJsonBuffer, m_StrConvertBuffer);
			if (tag.AntennaID < 10)
				stringlen += 1;
			else
				stringlen += 2;
			strcat(m_RespJsonBuffer, ",\"read_count\":");
			stringlen += 14;
			sprintf(m_StrConvertBuffer, "%d", tag.ReadCnt);
			strcat(m_RespJsonBuffer, m_StrConvertBuffer);
			stringlen += strlen(m_StrConvertBuffer);
			strcat(m_RespJsonBuffer, ",\"protocol\":");
			stringlen += 12;
			sprintf(m_StrConvertBuffer, "%d", tag.protocol);
			strcat(m_RespJsonBuffer, m_StrConvertBuffer);
			stringlen += 1;
			strcat(m_RespJsonBuffer, ",\"rssi\":");
			stringlen += 8;
			sprintf(m_StrConvertBuffer, "%d", (signed char)((unsigned char)(tag.RSSI)));
			strcat(m_RespJsonBuffer, m_StrConvertBuffer);
			stringlen += strlen(m_StrConvertBuffer);
			strcat(m_RespJsonBuffer, ",\"firstseen_timestamp\":0,\"lastseen_timestamp\":0}");
			stringlen += 48;
			if (stringlen > 800)
				break;
		}
		osMutexRelease(m_TagbufMux);
		return stringlen;
	}
	else
	{
		strcpy(m_RespJsonBuffer, "}");
		m_IsFinChunked = true;
		return 1;
	}
}
char *HttpModuleAPI::ParamErr2String(HttpMApiErrCode err)
{
	char tmpbuf[MaxErrPnameBufLen - 20];
	switch (err)
	{
	case HMApiErr_Param_Err:
		if (strncmp(m_ErrParamName, "json", 4) == 0)
			return m_ErrParamName;
		strcpy(tmpbuf, m_ErrParamName);
		sprintf(m_ErrParamName, "the parameter %s error", tmpbuf);
		return m_ErrParamName;
	case HMApiErr_Param_Missing:
		strcpy(tmpbuf, m_ErrParamName);
		sprintf(m_ErrParamName, "missing parameter %s", tmpbuf);
		return m_ErrParamName;
	case HMApiErr_Param_NotSupported:
		strcpy(tmpbuf, m_ErrParamName);
		sprintf(m_ErrParamName, "parameter %s is not supported", tmpbuf);
		return m_ErrParamName;
	default:
		return (char *)"common error";
	}
}

//{"param_name":"protocol/gen2/session","param_value":1}
//{"param_name":"protocol/gen2/q","param_value":-1}
//{"param_name":"protocol/gen2/write_mode","param_value":0}
//{"param_name":"protocol/gen2/target","param_value":0}
//{"param_name":"rf/tx_powers","param_value":[{"antenna":4,"read_power":1200,"write_power":2200}]}
//{"param_name":"rf/region","param_value":1}
//{"param_name":"rf/hop_table","param_value":[902625,925175]}
//{"param_name":"reader/work_mode", "param_value":"passive_mode"}
//{"param_name":"tag_data/unique_by_antenna", "param_value":true}
//{"param_name":"tag_data/unique_by_bank_data", "param_value":true}
//{"param_name":"tag_data/record_highest_rssi", "param_value":true}
//{"param_name":"reader/work_mode", "param_value":"active"}
void HttpModuleAPI::httpParamSet(char *json, int len, bool issavefile)
{
	json_value *jvalue;
	char pname[50];
	int pvalint;

	jvalue = json_parse_auto(json, len);
	CHK_PARA_RET(jvalue == NULL, "json format error");
	CHK_JPAS_RET(json_getstring(jvalue, "param_name", pname), "param_name");

	m_IsAddOtherJson = false;
	m_OpError = HMApiErr_Ok;

	CHK_RDR_STATUS;
	if (strcmp(pname, "tag_data/unique_by_antenna") == 0)
	{
		CHK_JPAS_RET(json_getbool(jvalue, "param_value", &pvalint), "param_value");
		CHK_MAPI_ERR(ParamSet(m_hReader, MTR_PARAM_TAGDATA_UNIQUEBYANT, &pvalint));
		m_tbIsUniByAnt = pvalint;
	}
	else if (strcmp(pname, "tag_data/unique_by_bank_data") == 0)
	{
		CHK_JPAS_RET(json_getbool(jvalue, "param_value", &pvalint), "param_value");
		CHK_MAPI_ERR(ParamSet(m_hReader, MTR_PARAM_TAGDATA_UNIQUEBYEMDDATA, &pvalint));
		m_tbIsUniByEmddata = pvalint;
	}
	else if (strcmp(pname, "tag_data/record_highest_rssi") == 0)
	{
		CHK_JPAS_RET(json_getbool(jvalue, "param_value", &pvalint), "param_value");
		CHK_MAPI_ERR(ParamSet(m_hReader, MTR_PARAM_TAGDATA_RECORDHIGHESTRSSI, &pvalint));
		m_tbIsRecHighestRssi = pvalint;
	}
	else if (strcmp(pname, "protocol/gen2/session") == 0)
	{
		CHK_JPAS_RET(json_getint(jvalue, "param_value", &pvalint), "param_value");
		CHK_PARA_RET(pvalint < 0 || pvalint > 4, "param_value");
		CHK_MAPI_ERR(ParamSet(m_hReader, MTR_PARAM_POTL_GEN2_SESSION, &pvalint));
	}
	else if (strcmp(pname, "protocol/gen2/write_mode") == 0)
	{
		CHK_JPAS_RET(json_getint(jvalue, "param_value", &pvalint), "param_value");
		CHK_PARA_RET(pvalint < 0 || pvalint > 1, "param_value");
		CHK_MAPI_ERR(ParamSet(m_hReader, MTR_PARAM_POTL_GEN2_WRITEMODE, &pvalint));
	}
	else if (strcmp(pname, "protocol/gen2/q") == 0)
	{
		CHK_JPAS_RET(json_getint(jvalue, "param_value", &pvalint), "param_value");
		CHK_PARA_RET(pvalint < -1 || pvalint > 15, "param_value");
		CHK_MAPI_ERR(ParamSet(m_hReader, MTR_PARAM_POTL_GEN2_Q, &pvalint));
	}
	else if (strcmp(pname, "protocol/gen2/target") == 0)
	{
		CHK_JPAS_RET(json_getint(jvalue, "param_value", &pvalint), "param_value");
		CHK_PARA_RET(pvalint < 0 || pvalint > 3, "param_value");
		CHK_MAPI_ERR(ParamSet(m_hReader, MTR_PARAM_POTL_GEN2_TARGET, &pvalint));
	}
	else if (strcmp(pname, "rf/tx_powers") == 0)
	{
		AntPowerConf pwrs;
		pwrs.antcnt = 0;
		char tmpstrbuf[40];
		json_value *pobj;
		CHK_JPAS_RET(json_getobject(jvalue, "param_value", &pobj), "param_value");
		CHK_PARA_RET(pobj->type != json_array, "param_value");
		pwrs.antcnt = pobj->u.array.length;
		CHK_PARA_RET(pwrs.antcnt == 0 || pwrs.antcnt > m_phyantportnumber, "param_value");
		for (int i = 0; i < pwrs.antcnt; ++i)
		{
			sprintf(tmpstrbuf, "param_value[%d].antenna", i);
			CHK_JPAS_RET(json_getint(pobj->u.array.values[i], "antenna", &pwrs.Powers[i].antid), tmpstrbuf);
			CHK_PARA_RET(pwrs.Powers[0].antid < 1 || pwrs.Powers[0].antid > m_phyantportnumber, tmpstrbuf);
			sprintf(tmpstrbuf, "param_value[%d].read_power", i);
			CHK_JPAS_RET(json_getint(pobj->u.array.values[i], "read_power", &pvalint), tmpstrbuf);
			CHK_PARA_RET(pvalint < m_MinPwr || pvalint > m_MaxPwr, tmpstrbuf);
			pwrs.Powers[i].readPower = (unsigned short)pvalint;
			sprintf(tmpstrbuf, "param_value[%d].write_power", i);
			CHK_JPAS_RET(json_getint(pobj->u.array.values[i], "write_power", &pvalint), tmpstrbuf);
			CHK_PARA_RET(pvalint < m_MinPwr || pvalint > m_MaxPwr, tmpstrbuf);
			pwrs.Powers[i].writePower = (unsigned short)pvalint;
		}

		int antids[MaxPhyAntPortCount];
		for (int i = 0; i < pwrs.antcnt; ++i)
			antids[i] = pwrs.Powers[i].antid;
		CHK_PARA_RET(validinvants(antids, pwrs.antcnt) != 0, "param_value");
		CHK_MAPI_ERR(ParamSet(m_hReader, MTR_PARAM_RF_ANTPOWER, &pwrs));
	}
	else if (strcmp(pname, "rf/region") == 0)
	{
		Region_Conf stmprg;
		CHK_JPAS_RET(json_getint(jvalue, "param_value", &pvalint), "param_value");
		CHK_PARA_RET(!(pvalint == 0x01 || pvalint == 0x02 || pvalint == 0x03 ||
					   pvalint == 0x06 || pvalint == 0x07 || pvalint == 0x08 ||
					   pvalint == 0x0A || pvalint == 0xFF),
					 "param_value");
		stmprg = (Region_Conf)pvalint;
		CHK_MAPI_ERR(ParamSet(m_hReader, MTR_PARAM_FREQUENCY_REGION, &stmprg));
	}
	else if (strcmp(pname, "rf/hop_table") == 0)
	{
		json_value *pobj;
		CHK_JPAS_RET(json_getobject(jvalue, "param_value", &pobj), "param_value");
		CHK_PARA_RET(pobj->type != json_array, "param_value");
		HoptableData_ST hop_table;
		hop_table.lenhtb = pobj->u.array.length;
		CHK_PARA_RET(hop_table.lenhtb == 0 || hop_table.lenhtb > 70, "param_value");
		char tmpselbuf[10];
		for (int i = 0; i < hop_table.lenhtb; ++i)
		{
			sprintf(tmpselbuf, "[%d]", i);
			CHK_JPAS_RET(json_getint(pobj, tmpselbuf, (int *)&hop_table.htb[i]), "param_value");
		}

		CHK_MAPI_ERR(ParamSet(m_hReader, MTR_PARAM_FREQUENCY_HOPTABLE, &hop_table));
	} //{"param_name":"reader/configuration"}
	/*
	{"param_name":"reader/active_mode","param_value":{"antennas":[1],"tag_filter":{"bank":1,"start_bit":32,"mask":"0011","match":true},
	"bank_data_option":{"bank":2,"start_block":0,"block_count":2,"access_password":"00000000"},
	//"tag_aggregation":{"mode":1, "duration":5000},"gpi_triggers":{"mode":1,"trigger_1":[{"gpi":1,"state":1}],"trigger_2":[{"gpi":2,"state":1}],"timeout":10},
	//"upload_settings":{"mode":1,"socket_options":{"connection":"short","port":12345},"http_options":{"url":"http://192.168.0.123:5000/tags"}}
	//,"event_notification":{"events":["heart_beat","tag_coming","reader_exception","gpi"],"heart_beat_cycle":10},
	//"advance_settings":{"inventory_cycle":200,"inventory_interval":300,"custom_parameter":"3sd8fus33"}}}
	*/

	//{"param_name":"reader/network_settings","param_value":
	//{"ip_settings":{"ip":"192.168.1.101","mask":"255.255.255.0","gateway":"192.168.1.1"},
	//"wireless_settings":{"ssid":"zsoffice","auth_mode":"wpa2-psk","password":"55555"},}}
	else if (strcmp(pname, "reader/network_settings") == 0)
	{
		json_value *pobj;
		int ret;
		char iptmpbuf[20];

		CHK_JPAS_RET(json_getobject(jvalue, "param_value", &pobj), "param_value");
		CHK_JPAS_RET(json_getstring_len(pobj, "ip_settings.ip", 19, 1, iptmpbuf), "param_value.ip_settings.ip");
		ret = addr_str2bin(iptmpbuf, hw_netconf()->ip);
		CHK_PARA_RET(ret != 0, "ip_settings.ip");

		CHK_JPAS_RET(json_getstring_len(pobj, "ip_settings.mask", 19, 1, iptmpbuf), "param_value.ip_settings.mask");
		ret = addr_str2bin(iptmpbuf, hw_netconf()->subnetMask);
		CHK_PARA_RET(ret != 0, "ip_settings.mask");

		CHK_JPAS_RET(json_getstring_len(pobj, "ip_settings.gateway", 19, 1, iptmpbuf), "param_value.ip_settings.gateway");
		ret = addr_str2bin(iptmpbuf, hw_netconf()->gatewayIP);
		CHK_PARA_RET(ret != 0, "ip_settings.gateway");

		set_network_config(hw_netconf());
		APIHttpResponse::m_IsReboot = true;
	}
	else
	{
		m_OpError = HMApiErr_Param_Err;
		strcpy(m_ErrParamName, "param_name");
		return;
	}
}

//{"hardware_version":"1.0.0.3","software_version":"1.0.0.53","ethmac":"1EFFAA22",
//"wifimac":"none","main_board":"ARM9_WIFI_V2","rfid_module":"slr1100","antennaportnumber":4,
//"gpi_number":2,"gpo_number":4,"cpu":"ARM926EJ-S rev 5 (v5l)","os":"linux 2.6.35.3"}

//"result":{"param_name":"protocol/gen2/session", "param_value":1}
//"result":{"param_name":"protocol/gen2/q", "param_value":-1}
//"result":{"param_name":"protocol/gen2/write_mode", "param_value":0}
//"result":{"param_name":"protocol/gen2/target", "param_value":"A"}
//"result":{"param_name":"rf/tx_powers", "param_value":[{"antenna":1,"read_power":3000,"write_power":3000}]}
//"result":{"param_name":"rf/region", "param_value":1}
//"result":{"param_name":"rf/hop_table", "param_value":[902625,925175]}
//{"param_name":"rf/max_tx_power"}
//{"param_name":"rf/min_tx_power"}
//{"param_name":"reader/hardware_info"}
//{"param_name":"reader/work_mode"}
//{"param_name":"tag_data/unique_by_antenna"}
//{"param_name":"tag_data/unique_by_bank_data"}
//{"param_name":"tag_data/record_highest_rssi"}
void firmware_version(unsigned char *version);
void HttpModuleAPI::httpParamGet(char *json, int len)
{
	json_value *jvalue;
	char pname[50];
	int pvalint;
	jvalue = json_parse_auto(json, len);
	CHK_PARA_RET(jvalue == NULL, "json format error");
	CHK_JPAS_RET(json_getstring(jvalue, "param_name", pname), "param_name");
	m_IsAddOtherJson = true;
	m_OpError = HMApiErr_Ok;

	if (strcmp(pname, "reader/work_mode") == 0)
	{
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":\"passive_mode\"}", pname);
		return;
	}
	else if (strcmp(pname, "reader/device_info") == 0)
	{
		char mboard[20];
		char rfidmod[20];
		char hwver[10];
		char swver[10];
		unsigned char swbinver[4];
		firmware_version(swbinver);
		strcpy(hwver, "2.0.0.0");
		sprintf(swver, "%d.%d.%d.%d", swver[0], swver[1], swver[2], swver[3]);
		GetHardwareString(m_HWdetails, mboard, rfidmod);

		char ethmac[20];
		char wifimac[20];
		wifimac[0] = 0;
		int gpicnt = 4;

		sprintf(ethmac, "%02X:%02X:%02X:%02X:%02X:%02X", hw_netconf()->mac[0],
				hw_netconf()->mac[1], hw_netconf()->mac[2], hw_netconf()->mac[3],
				hw_netconf()->mac[4], hw_netconf()->mac[5]);

		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":{\"hardware_version\":\"%s\",\"software_version\":\"%s\",\"eth_mac_address\":\"%s\",\"wifi_mac_address\":\"%s\",\"main_board\":\"%s\",\"rfid_module\":\"%s\",\"antenna_ports_number\":%d,\"gpi_number\":%d,\"gpo_number\":4}}",
				pname, hwver, swver, ethmac, wifimac, mboard, rfidmod, rdr_get_ant_number(), gpicnt);
		return;
	}
	else if (strcmp(pname, "reader/hardware_info") == 0)
	{
		char mboard[20];
		char rfidmod[20];
		GetHardwareString(m_HWdetails, mboard, rfidmod);
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":{\"main_board\":\"%s\",\"rfid_module\":\"%s\"}}",
				pname, mboard, rfidmod);
		return;
	}
	else if (strcmp(pname, "reader/antenna_ports_number") == 0)
	{
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":%d}", pname, rdr_get_ant_number());
		return;
	}
	else if (strcmp(pname, "reader/network_settings") == 0)
	{
		char ipstr_[20];
		char maskstr_[20];
		char gwstr_[20];
		sprintf(ipstr_, "%d.%d.%d.%d", hw_netconf()->ip[0],
				hw_netconf()->ip[1], hw_netconf()->ip[2], hw_netconf()->ip[3]);
		sprintf(maskstr_, "%d.%d.%d.%d", hw_netconf()->subnetMask[0],
				hw_netconf()->subnetMask[1], hw_netconf()->subnetMask[2],
				hw_netconf()->subnetMask[3]);
		sprintf(gwstr_, "%d.%d.%d.%d", hw_netconf()->gatewayIP[0],
				hw_netconf()->gatewayIP[1], hw_netconf()->gatewayIP[2],
				hw_netconf()->gatewayIP[3]);
		//{"ip_settings":{"ip":"192.168.1.101","mask":"255.255.255.0","gateway":"192.168.1.1"}}
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":{\"ip_settings\":{\"ip\":\"%s\",\"mask\":\"%s\",\"gateway\":\"%s\"}}}",
				pname, ipstr_, maskstr_, gwstr_);
		return;
		;
	}

	CHK_RDR_STATUS;
	m_IsAddOtherJson = true;
	if (strcmp(pname, "tag_data/unique_by_antenna") == 0)
	{
		CHK_MAPI_ERR(ParamGet(m_hReader, MTR_PARAM_TAGDATA_UNIQUEBYANT, &pvalint));
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":", pname);
		if (pvalint == 1)
			strcat(m_AddOtherJsonBuf, "true}");
		else
			strcat(m_AddOtherJsonBuf, "false}");
	}
	else if (strcmp(pname, "tag_data/unique_by_bank_data") == 0)
	{
		CHK_MAPI_ERR(ParamGet(m_hReader, MTR_PARAM_TAGDATA_UNIQUEBYEMDDATA, &pvalint));
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":", pname);
		if (pvalint == 1)
			strcat(m_AddOtherJsonBuf, "true}");
		else
			strcat(m_AddOtherJsonBuf, "false}");
	}
	else if (strcmp(pname, "tag_data/record_highest_rssi") == 0)
	{
		CHK_MAPI_ERR(ParamGet(m_hReader, MTR_PARAM_TAGDATA_RECORDHIGHESTRSSI, &pvalint));
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":", pname);
		if (pvalint == 1)
			strcat(m_AddOtherJsonBuf, "true}");
		else
			strcat(m_AddOtherJsonBuf, "false}");
	}
	else if (strcmp(pname, "protocol/gen2/session") == 0)
	{
		CHK_MAPI_ERR(ParamGet(m_hReader, MTR_PARAM_POTL_GEN2_SESSION, &pvalint));
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":%d}", pname, pvalint);
	}
	else if (strcmp(pname, "protocol/gen2/write_mode") == 0)
	{
		CHK_MAPI_ERR(ParamGet(m_hReader, MTR_PARAM_POTL_GEN2_WRITEMODE, &pvalint));
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":%d}", pname, pvalint);
	}
	else if (strcmp(pname, "protocol/gen2/q") == 0)
	{
		CHK_MAPI_ERR(ParamGet(m_hReader, MTR_PARAM_POTL_GEN2_Q, &pvalint));
		signed char qchar = (signed char)((unsigned char)pvalint);
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":%d}", pname, qchar);
	}
	else if (strcmp(pname, "protocol/gen2/target") == 0)
	{
		CHK_MAPI_ERR(ParamGet(m_hReader, MTR_PARAM_POTL_GEN2_TARGET, &pvalint));
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":%d}", pname, pvalint);
	}
	else if (strcmp(pname, "rf/tx_powers") == 0)
	{
		AntPowerConf pwrs;
		char tmpstrbuf[80];
		CHK_MAPI_ERR(ParamGet(m_hReader, MTR_PARAM_RF_ANTPOWER, &pwrs));
		strcpy(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"rf/tx_powers\",\"param_value\":[");
		for (int i = 0; i < pwrs.antcnt; ++i)
		{
			if (i == 0)
				sprintf(tmpstrbuf, "{\"antenna\":%d,\"read_power\":%d,\"write_power\":%d}",
						pwrs.Powers[i].antid, pwrs.Powers[i].readPower, pwrs.Powers[i].writePower);
			else
				sprintf(tmpstrbuf, ",{\"antenna\":%d,\"read_power\":%d,\"write_power\":%d}",
						pwrs.Powers[i].antid, pwrs.Powers[i].readPower, pwrs.Powers[i].writePower);
			strcat(m_AddOtherJsonBuf, tmpstrbuf);
		}
		strcat(m_AddOtherJsonBuf, "]}");
	}
	else if (strcmp(pname, "rf/region") == 0)
	{
		Region_Conf rg;
		CHK_MAPI_ERR(ParamGet(m_hReader, MTR_PARAM_FREQUENCY_REGION, &rg));
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":%d}", pname, rg);
	}
	else if (strcmp(pname, "rf/hop_table") == 0)
	{
		HoptableData_ST hop_table;
		char tmpstrbuf[10];
		CHK_MAPI_ERR(ParamGet(m_hReader, MTR_PARAM_FREQUENCY_HOPTABLE, &hop_table));
		strcpy(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"rf/hop_table\",\"param_value\":[");
		for (int i = 0; i < hop_table.lenhtb; ++i)
		{
			if (i == 0)
				sprintf(tmpstrbuf, "%d", hop_table.htb[i]);
			else
				sprintf(tmpstrbuf, ",%d", hop_table.htb[i]);
			strcat(m_AddOtherJsonBuf, tmpstrbuf);
		}
		strcat(m_AddOtherJsonBuf, "]}");
	}
	else if (strcmp(pname, "rf/max_tx_power") == 0)
	{
		unsigned short pvalushort;
		CHK_MAPI_ERR(ParamGet(m_hReader, MTR_PARAM_RF_MAXPOWER, &pvalushort));
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":%d}", pname, pvalushort);
	}
	else if (strcmp(pname, "rf/min_tx_power") == 0)
	{
		unsigned short pvalushort;
		CHK_MAPI_ERR(ParamGet(m_hReader, MTR_PARAM_RF_MINPOWER, &pvalushort));
		sprintf(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"%s\",\"param_value\":%d}", pname, pvalushort);
	}
	else if (strcmp(pname, "reader/connected_antennas") == 0)
	{
		char tmpstrbuf[10];
		ConnAnts_ST connants;
		CHK_MAPI_ERR(ParamGet(m_hReader, MTR_PARAM_READER_CONN_ANTS, &connants));
		strcpy(m_AddOtherJsonBuf, "\"result\":{\"param_name\":\"reader/connected_antennas\",\"param_value\":[");
		for (int i = 0; i < connants.antcnt; ++i)
		{
			if (i == 0)
				sprintf(tmpstrbuf, "%d", connants.connectedants[i]);
			else
				sprintf(tmpstrbuf, ",%d", connants.connectedants[i]);
			strcat(m_AddOtherJsonBuf, tmpstrbuf);
		}
		strcat(m_AddOtherJsonBuf, "]}");
	}
	else
	{
		m_IsAddOtherJson = false;
		m_OpError = HMApiErr_Param_Err;
		strcpy(m_ErrParamName, "param_name");
		return;
	}
}

void HttpModuleAPI::setEmbData(json_value *jvalue, bool isset)
{
	json_value *pobj;
	EmbededData_ST emddata;
	unsigned char acspwd[4];

	if (isset)
		ParamSet(m_hReader, MTR_PARAM_TAG_EMBEDEDDATA, NULL);
	if (json_getobject(jvalue, "bank_data_option", &pobj) == 0)
	{
		if (pobj->type != json_null)
		{
			if (!(m_HWdetails.module == MODOULE_M6E ||
				  m_HWdetails.module == MODOULE_M6E_PRC ||
				  m_HWdetails.module == MODOULE_M6E_MICRO ||
				  m_HWdetails.module == MODOULE_M5E ||
				  m_HWdetails.module == MODOULE_M5E_C ||
				  m_HWdetails.module == MODOULE_SLR1100 ||
				  m_HWdetails.module == MODOULE_SLR1200 ||
				  m_HWdetails.module == MODOULE_R902_M1S ||
				  m_HWdetails.module == MODOULE_SIM3100 ||
				  m_HWdetails.module == MODOULE_SIM3200 ||
				  m_HWdetails.module == MODOULE_SIM3300 ||
				  m_HWdetails.module == MODOULE_SIM3400 ||
				  m_HWdetails.module == MODOULE_SIM5100 ||
				  m_HWdetails.module == MODOULE_SIM5200 ||
				  m_HWdetails.module == MODOULE_SIM5300 ||
				  m_HWdetails.module == MODOULE_SIM5400 ||
				  m_HWdetails.module == MODOULE_SIM7100 ||
				  m_HWdetails.module == MODOULE_SIM7200 ||
				  m_HWdetails.module == MODOULE_SIM7300 ||
				  m_HWdetails.module == MODOULE_SIM7400))
			{
				m_OpError = HMApiErr_Param_NotSupported;
				strcpy(m_ErrParamName, "bank_data_option");
				m_IsCheckError = true;
				return;
			}
			CHK_JPAS_RET(json_getint(jvalue, "bank_data_option.bank", &emddata.bank), "bank_data_option.bank");
			CHK_PARA_RET(emddata.bank < 0 || emddata.bank > 3, "bank_data_option.bank");
			CHK_JPAS_RET(json_getint(jvalue, "bank_data_option.start_block", &emddata.startaddr), "bank_data_option.start_block");
			CHK_PARA_RET(emddata.startaddr < 0, "bank_data_option.start_block");
			CHK_JPAS_RET(json_getint(jvalue, "bank_data_option.block_count", &emddata.bytecnt), "bank_data_option.block_count");
			CHK_PARA_RET(emddata.bytecnt <= 0, "bank_data_option.block_count");
			emddata.bytecnt *= 2;
			char ascpwdstr[9];
			int tmpret = json_getstring_len(jvalue, "bank_data_option.access_password", 8, 0, ascpwdstr);
			CHK_PARA_RET(tmpret == -2, "bank_data_option.access_password");
			if (tmpret == 0)
			{
				CHK_PARA_RET(validhexstring(ascpwdstr) != 0, "bank_data_option.access_password");
				Str2Hex(ascpwdstr, 8, acspwd);
				if (acspwd[0] == 0 && acspwd[1] == 0 && acspwd[2] == 0 && acspwd[3] == 0)
					emddata.accesspwd = NULL;
				else
					emddata.accesspwd = acspwd;
			}
			else
				emddata.accesspwd = NULL;
			if (isset)
				ParamSet(m_hReader, MTR_PARAM_TAG_EMBEDEDDATA, &emddata);
		}
	}
}

void HttpModuleAPI::setTagFilter(json_value *jvalue, bool isset)
{
	TagFilter_ST filter;
	char mask[512];
	unsigned char fdata[64];

	if (isset)
		ParamSet(m_hReader, MTR_PARAM_TAG_FILTER, NULL);
	json_value *pobj;
	if (json_getobject(jvalue, "tag_filter", &pobj) == 0)
	{
		if (pobj->type != json_null)
		{
			CHK_JPAS_RET(json_getint(jvalue, "tag_filter.bank", &filter.bank), "tag_filter.bank");
			CHK_PARA_RET(filter.bank < 1 || filter.bank > 3, "tag_filter.bank");
			CHK_JPAS_RET(json_getint(jvalue, "tag_filter.start_bit", (int *)&filter.startaddr), "tag_filter.start_bit");
			CHK_PARA_RET(filter.startaddr > 511, "tag_filter.start_bit");
			CHK_JPAS_RET(json_getstring_len(jvalue, "tag_filter.mask", 512, 1, mask), "tag_filter.mask");
			CHK_PARA_RET(validbinstring(mask) != 0, "tag_filter.mask");
			Str2Binary(mask, strlen(mask), fdata);
			filter.fdata = fdata;
			filter.flen = strlen(mask);
			CHK_PARA_RET(json_getbool(jvalue, "tag_filter.match", &filter.isInvert) != 0, "tag_filter.match");
			filter.isInvert = 1 - filter.isInvert;
			if (isset)
				ParamSet(m_hReader, MTR_PARAM_TAG_FILTER, &filter);
		}
	}
}

void HttpModuleAPI::preInventory(json_value *jvalue,
								 int *antennas, int &antcount)
{
	json_value *pobj;

	setTagFilter(jvalue);
	if (m_IsCheckError)
		return;
	setEmbData(jvalue);
	if (m_IsCheckError)
		return;

	CHK_JPAS_RET(json_getobject(jvalue, "antennas", &pobj), "antennas");
	CHK_PARA_RET(pobj->type != json_array, "antennas");
	CHK_PARA_RET(pobj->u.array.length == 0, "antennas");
	antcount = pobj->u.array.length;
	char tmpselbuf[10];
	for (int i = 0; i < antcount; ++i)
	{
		sprintf(tmpselbuf, "[%d]", i);
		CHK_JPAS_RET(json_getint(pobj, tmpselbuf, &antennas[i]), "antennas");
	}
	CHK_PARA_RET(validinvants(antennas, antcount) != 0, "antennas");
}

//{"antennas":[1],"timeout":1500,"tag_filter":{"bank":1,"start_bit":32,"mask":"0011","match":true},"bank_data_option":{"bank":2,"start_block":0,"block_count":2,"access_password":"00000000"}}
void HttpModuleAPI::httpSyncInventory(char *json, int len)
{
	CHK_RDR_STATUS;
	int timeout;
	int antennas[MaxPhyAntPortCount];
	int antcount = 0;
	//	printf("start httpSyncInventory:%d\n", GetTickCount());
	json_value *jvalue;
	jvalue = json_parse_auto(json, len);
	CHK_PARA_RET(jvalue == NULL, "json format error");
	preInventory(jvalue, antennas, antcount);
	if (m_IsCheckError)
		return;
	CHK_JPAS_RET(json_getint(jvalue, "timeout", &timeout), "timeout");
	CHK_PARA_RET(timeout < 50 || timeout > 65535, "timeout");
	int tagcnt;
	m_IsInvTags = true;

	ResetTagBuffer();
	/*
	m_TagBuffer.tagClear();
	m_TagBuffer.setUniByAnt(m_tbIsUniByAnt == 1);
	m_TagBuffer.setUniByEmdData(m_tbIsUniByEmddata == 1);
	m_TagBuffer.setRecHighestRssi(m_tbIsRecHighestRssi == 1);
	*/
	CHK_MAPI_ERR(TagInventory_Raw(m_hReader, antennas, antcount, timeout, &tagcnt));
	//	printf("after TagInventory_Raw:%d\n", GetTickCount());
	if (tagcnt > 0)
	{
		//		printf("tagcnt:%d\n", tagcnt);
		TAGINFO tag;
		//		printf("before GetNextTag:%d\n", GetTickCount());
		for (int i = 0; i < tagcnt; ++i)
		{
			CHK_MAPI_ERR(GetNextTag(m_hReader, &tag));
			tagInsert(&tag);
		}
		//		printf("after GetNextTag:%d\n", GetTickCount());
	}
}

void HttpModuleAPI::AsyncRead_Thread(void *arg)
{
	TAGINFO tag;
	bool isstop = false;
	HttpModuleAPI *phttpModAPI = (HttpModuleAPI *)arg;
	READER_ERR err;
	while (true)
	{
		//		printf("before osSemaphoreAcquire\n");
		osSemaphoreAcquire(phttpModAPI->m_AsyncReadSem, osWaitForever);
		//		printf("after osSemaphoreAcquire\n");
		int fastoption = (((0x1 << 1) | (0x1 << 2) | (0x1 << 3) | (0x1 << 7)) << 8) | 0x80 | (0x01 << 24);
		err = SyncStartFastReading(phttpModAPI->m_hReader,
								   phttpModAPI->m_AsyncInvAnts, phttpModAPI->m_AsyncInvAntsCnt, fastoption);
		phttpModAPI->m_AsyncReadStartErr = err;
		if (err != MT_OK_ERR)
		{
			//			printf("m_AsyncReadStartErr:%d\n", phttpModAPI->m_AsyncReadStartErr);
			phttpModAPI->m_IsAsyncRead = false;
			continue;
		}
		isstop = false;
		//		printf("before  while(phttpModAPI->m_IsAsyncRead)\n");
		phttpModAPI->m_AsyncReadGetErr = MT_OK_ERR;

		while (phttpModAPI->m_IsAsyncRead)
		{
			err = SyncGetNextTag(phttpModAPI->m_hReader, &tag);
			//			printf("m_AsyncReadGetErr:%d, err:%d\n", phttpModAPI->m_AsyncReadGetErr, err);
			if (err == MT_OK_ERR)
			{
				osMutexAcquire(phttpModAPI->m_TagbufMux, osWaitForever);
				if (tag.Epclen + tag.EmbededDatalen <= phttpModAPI->m_tbMaxRecLength)
					tagInsert(&tag);
				osMutexRelease(phttpModAPI->m_TagbufMux);
			}
			else if (err == MT_CMD_NO_TAG_ERR)
				continue;
			else
			{
				phttpModAPI->m_AsyncReadGetErr = err;
				//				SyncStopFastReading(phttpModAPI->m_hReader);
				isstop = true;
				break;
			}
		}
		if (!isstop)
			phttpModAPI->m_AsyncReadStopErr = SyncStopFastReading(phttpModAPI->m_hReader);
		else
		{
			osMutexAcquire(phttpModAPI->m_TagbufMux, osWaitForever);
			phttpModAPI->m_IsAsyncRead = false;
			phttpModAPI->m_AsyncReadStopErr = MT_OK_ERR;
			osMutexRelease(phttpModAPI->m_TagbufMux);
		}
		//		printf("m_AsyncReadStopErr:%d\n", phttpModAPI->m_AsyncReadStopErr);
	}
}
//{"antennas":[1],"tag_filter":{"bank":1,"start_bit":32,"mask":"0011","match":true},
//"bank_data_option":{"bank":2,"start_block":0,"block_count":2,"access_password":"00000000"}}
void HttpModuleAPI::httpStartAsyncInventory(char *json, int len)
{
	m_IsAsyncOp = true;
	CHK_RDR_STATUS;
	json_value *jvalue;
	jvalue = json_parse_auto(json, len);
	CHK_PARA_RET(jvalue == NULL, "json format error");
	preInventory(jvalue, m_AsyncInvAnts, m_AsyncInvAntsCnt);
	if (m_IsCheckError)
		return;
	if (m_IsAsyncRead)
	{
		m_OpError = HMApiErr_Reader_Busy;
		return;
	}

	m_AsyncReadStartErr = (READER_ERR)-1;

	ResetTagBuffer();
	/*
	m_TagBuffer.tagClear();
	m_TagBuffer.setUniByAnt(m_tbIsUniByAnt == 1);
	m_TagBuffer.setUniByEmdData(m_tbIsUniByEmddata == 1);
	m_TagBuffer.setRecHighestRssi(m_tbIsRecHighestRssi == 1);
	*/
	m_IsAsyncRead = true;
	osSemaphoreRelease(m_AsyncReadSem);
	//	printf("httpStartAsyncInventory 00000000000000000\n");
	while (1)
	{
		if (m_AsyncReadStartErr != (READER_ERR)-1)
			break;
		sleep_ms(5);
	}
	//	printf("httpStartAsyncInventory 1111111111111111 m_AsyncReadStartErr:%d\n", m_AsyncReadStartErr);
	CHK_MAPI_ERR(m_AsyncReadStartErr);
}
void HttpModuleAPI::httpStopAsyncInventory(char *json, int len)
{
	m_IsAsyncOp = true;
	CHK_RDR_STATUS;
	//	printf("httpStopAsyncInventory aaaaaaaaaaaaaaaaaaa\n");
	osMutexAcquire(m_TagbufMux, osWaitForever);
	if (!m_IsAsyncRead)
	{
		m_OpError = HMApiErr_Ok;
		osMutexRelease(m_TagbufMux);
		return;
	}
	m_AsyncReadStopErr = (READER_ERR)-1;
	m_IsAsyncRead = false;
	osMutexRelease(m_TagbufMux);
	//	printf("httpStopAsyncInventory 00000000000000000\n");
	while (1)
	{
		if (m_AsyncReadStopErr != (READER_ERR)-1)
			break;
		sleep_ms(5);
	}
	//	printf("httpStopAsyncInventory 11111111111111111111\n");
	CHK_MAPI_ERR(m_AsyncReadStopErr);
}

void HttpModuleAPI::httpGetAsynctags(char *json, int len)
{
	m_IsAsyncOp = true;
	CHK_RDR_STATUS;
	if (m_AsyncReadGetErr != MT_OK_ERR)
	{
		m_OpError = MTECode2HMAEcode(m_AsyncReadGetErr);
		m_AsyncReadGetErr = MT_OK_ERR;
		m_IsInvTags = false;
		return;
	}
	if (!m_IsAsyncRead)
	{
		m_OpError = HMApiErr_Invalid_Op;
		return;
	}
	m_OpError = HMApiErr_Ok;
	m_IsInvTags = true;
}

void HttpModuleAPI::preTagOperation(json_value *jvalue,
									int *antenna, unsigned char *acspwd,
									unsigned char **ppwd, int *timeout)
{
	char ascpwdstr[9];
	setTagFilter(jvalue);
	if (m_IsCheckError)
		return;

	int tmpret = json_getstring_len(jvalue, "access_password", 8, 0, ascpwdstr);
	CHK_PARA_RET(tmpret == -2, "access_password");
	if (tmpret == 0)
	{
		CHK_PARA_RET(validhexstring(ascpwdstr) != 0, "access_password");
		Str2Hex(ascpwdstr, 8, acspwd);
		if (acspwd[0] == 0 && acspwd[1] == 0 && acspwd[2] == 0 && acspwd[3] == 0)
			*ppwd = NULL;
		else
			*ppwd = acspwd;
	}
	else
		*ppwd = NULL;

	CHK_JPAS_RET(json_getint(jvalue, "antenna", antenna), "antenna");
	CHK_PARA_RET(*antenna < 1 || *antenna > m_phyantportnumber, "antenna");
	if (json_getint(jvalue, "timeout", timeout) != 0)
		*timeout = 1000;
	else
	{
		CHK_PARA_RET(*timeout < 50 || *timeout > 65535, "timeout");
	}
}
//{"antenna":1,"bank":1,"start_block":2,"block_count":6,"access_password":"00000000", "timeout":1000}
void HttpModuleAPI::httpReadTagBank(char *json, int len)
{
	CHK_RDR_STATUS;
	json_value *jvalue;
	unsigned char *ppwd;
	unsigned char acspwd[4];
	int bank;
	int start_block;
	int block_count;
	int antenna;
	int timeout;
	unsigned char bankbuffer[MaxTagOpBankBufLen];
	m_IsAddOtherJson = true;
	jvalue = json_parse_auto(json, len);
	CHK_PARA_RET(jvalue == NULL, "json format error");
	preTagOperation(jvalue, &antenna, acspwd, &ppwd, &timeout);
	if (m_IsCheckError)
		return;

	CHK_JPAS_RET(json_getint(jvalue, "bank", &bank), "bank");
	CHK_PARA_RET(bank < 0 || bank > 3, "bank");
	CHK_JPAS_RET(json_getint(jvalue, "start_block", &start_block), "start_block");
	CHK_PARA_RET(start_block < 0, "start_block");
	CHK_JPAS_RET(json_getint(jvalue, "block_count", &block_count), "block_count");
	CHK_PARA_RET(block_count < 0 || block_count > MaxTagOpBankBufLen, "block_count");

	CHK_MAPI_ERR(GetTagData(m_hReader, antenna, (unsigned char)bank, start_block, block_count, bankbuffer, ppwd, timeout));
	Hex2Str(bankbuffer, block_count * 2, m_AddOtherJsonBuf + 50);
	sprintf(m_AddOtherJsonBuf, "\"result\":\"%s\"", m_AddOtherJsonBuf + 50);
}

//{"antenna":1,"bank":1,"start_block":2,"bank_data":\"1234567887654321\","access_password":"00000000", "timeout":1000}
void HttpModuleAPI::httpWriteTagBank(char *json, int len)
{
	CHK_RDR_STATUS;
	json_value *jvalue;
	unsigned char *ppwd;
	unsigned char acspwd[4];
	int bank;
	int start_block;
	int antenna;
	int timeout;
	char bankbuffer_str[MaxTagOpBankBufLen * 2];
	unsigned char bankbuffer[MaxTagOpBankBufLen];

	jvalue = json_parse_auto(json, len);
	CHK_PARA_RET(jvalue == NULL, "json format error");
	preTagOperation(jvalue, &antenna, acspwd, &ppwd, &timeout);
	if (m_IsCheckError)
		return;

	CHK_JPAS_RET(json_getint(jvalue, "bank", &bank), "bank");
	CHK_PARA_RET(bank < 0 || bank > 3, "bank");
	CHK_JPAS_RET(json_getint(jvalue, "start_block", &start_block), "start_block");
	CHK_PARA_RET(start_block < 0, "start_block");
	CHK_JPAS_RET(json_getstring_len(jvalue, "bank_data", MaxTagOpBankBufLen * 2, 1, bankbuffer_str), "bank_data");
	int bankstrlen = strlen(bankbuffer_str);
	CHK_PARA_RET(bankstrlen % 4 != 0, "bank_data");
	CHK_PARA_RET(validhexstring(bankbuffer_str) != 0, "bank_data");
	Str2Hex(bankbuffer_str, bankstrlen, bankbuffer);
	CHK_MAPI_ERR(WriteTagData(m_hReader, antenna, (unsigned char)bank, start_block, bankbuffer, bankstrlen / 2, ppwd, timeout));
}

//{"antenna":1,"epc":"1234567887654321","access_password":"00000000", "timeout":1000}
void HttpModuleAPI::httpWriteTagEpc(char *json, int len)
{
	CHK_RDR_STATUS;
	json_value *jvalue;
	unsigned char *ppwd;
	unsigned char acspwd[4];
	int antenna;
	int timeout;
	char bankbuffer_str[124];
	unsigned char bankbuffer[62];

	jvalue = json_parse_auto(json, len);
	CHK_PARA_RET(jvalue == NULL, "json format error");
	preTagOperation(jvalue, &antenna, acspwd, &ppwd, &timeout);
	if (m_IsCheckError)
		return;

	CHK_JPAS_RET(json_getstring_len(jvalue, "epc", 124, 1, bankbuffer_str), "epc");
	int bankstrlen = strlen(bankbuffer_str);
	CHK_PARA_RET(bankstrlen % 4 != 0, "epc");
	CHK_PARA_RET(validhexstring(bankbuffer_str) != 0, "epc");
	Str2Hex(bankbuffer_str, bankstrlen, bankbuffer);
	CHK_MAPI_ERR(WriteTagEpcEx(m_hReader, antenna, bankbuffer, bankstrlen / 2, ppwd, timeout));
}

static int checkLockObjName(char *name, unsigned char &lock_objects,
							unsigned short &locktypes, int ltypeval)
{
	int isfind = -1;
	int bitbase;
	unsigned short lockbit = 0;
	if (strcmp(name, "kill_password") == 0)
	{
		lock_objects |= LOCK_OBJECT_KILL_PASSWORD;
		bitbase = 8;
		isfind = 0;
	}
	else if (strcmp(name, "access_password") == 0)
	{
		lock_objects |= LOCK_OBJECT_ACCESS_PASSWD;
		bitbase = 6;
		isfind = 0;
	}
	else if (strcmp(name, "bank1") == 0)
	{
		lock_objects |= LOCK_OBJECT_BANK1;
		bitbase = 4;
		isfind = 0;
	}
	else if (strcmp(name, "bank2") == 0)
	{
		lock_objects |= LOCK_OBJECT_BANK2;
		bitbase = 2;
		isfind = 0;
	}
	else if (strcmp(name, "bank3") == 0)
	{
		lock_objects |= LOCK_OBJECT_BANK3;
		bitbase = 0;
		isfind = 0;
	}

	for (int i = 0; i < ltypeval; ++i)
		lockbit |= 1 << (1 - i);
	locktypes |= lockbit << bitbase;

	return isfind;
}

//{"antenna":1,"tag_lock_option":[{"lock_object":"access_password","lock_status":1},{"lock_object":"bank1","lock_status":1}],"access_password":"00000000", "timeout":1000}
void HttpModuleAPI::httpLockTag(char *json, int len)
{
	CHK_RDR_STATUS;
	json_value *jvalue;
	unsigned char *ppwd;
	unsigned char acspwd[4];
	int antenna;
	int timeout;
	unsigned short locktypes = 0;
	unsigned char lock_objects = 0;

	jvalue = json_parse_auto(json, len);
	CHK_PARA_RET(jvalue == NULL, "json format error");
	preTagOperation(jvalue, &antenna, acspwd, &ppwd, &timeout);
	if (m_IsCheckError)
		return;

	json_value *pobj;
	char tmplobjbuf[30];
	char tmpstrbuf[50];
	int tmplcokstatus;
	CHK_JPAS_RET(json_getobject(jvalue, "tag_lock_option", &pobj), "tag_lock_option");
	CHK_PARA_RET(pobj->type != json_array, "tag_lock_option");
	CHK_PARA_RET(pobj->u.array.length > 5 || pobj->u.array.length == 0, "tag_lock_option");
	for (unsigned int i = 0; i < pobj->u.array.length; ++i)
	{
		sprintf(tmpstrbuf, "tag_lock_option[%d].lock_status", i);
		CHK_JPAS_RET(json_getint(pobj->u.array.values[i], "lock_status", &tmplcokstatus), tmpstrbuf);
		CHK_PARA_RET(tmplcokstatus < 0 || tmplcokstatus > 2, tmpstrbuf);
		sprintf(tmpstrbuf, "tag_lock_option[%d].lock_object", i);
		CHK_JPAS_RET(json_getstring_len(pobj->u.array.values[i], "lock_object", 30, 1, tmplobjbuf), tmpstrbuf);
		CHK_PARA_RET(checkLockObjName(tmplobjbuf, lock_objects, locktypes, tmplcokstatus) != 0, tmpstrbuf);
	}
	CHK_MAPI_ERR(LockTag(m_hReader, antenna, lock_objects, locktypes, ppwd, timeout));
}

void HttpModuleAPI::httpKillTag(char *json, int len)
{
	CHK_RDR_STATUS;
	json_value *jvalue;
	unsigned char *ppwd;
	unsigned char kiilpwd[4];
	int antenna;
	int timeout;
	jvalue = json_parse_auto(json, len);
	CHK_PARA_RET(jvalue == NULL, "json format error");
	preTagOperation(jvalue, &antenna, kiilpwd, &ppwd, &timeout);
	if (m_IsCheckError)
		return;
	char killpwdstr[9];
	CHK_JPAS_RET(json_getstring_len(jvalue, "kill_password", 8, 0, killpwdstr), "kill_password");
	CHK_PARA_RET(validhexstring(killpwdstr) != 0, "kill_password");
	Str2Hex(killpwdstr, 8, kiilpwd);
	int killpwdint = (kiilpwd[0] << 24) | (kiilpwd[1] << 16) |
					 (kiilpwd[2] << 8) | kiilpwd[3];
	CHK_PARA_RET(killpwdint == 0, "kill_password");
	CHK_MAPI_ERR(KillTag(m_hReader, antenna, kiilpwd, timeout));
}
// 返回示例:result:[{"gpi":1,"state":0},{"gpi":2,"state":1}]
void HttpModuleAPI::httpGetGPI(char *json, int len)
{
	CHK_RDR_STATUS;
	m_IsAddOtherJson = true;
	GpiInfo_ST ginfo;
	char tmpbuf[30];

	m_OpError = HMApiErr_Ok;
	ginfo.gpiCount = 4;
	for (int i = 0; i < 4; ++i)
	{
		ginfo.gpiStats[i].GpiId = i + 1;
		ginfo.gpiStats[i].State = gpi_get(i + 1);
	}

	strcpy(m_AddOtherJsonBuf, "\"result\":[");
	for (int i = 0; i < ginfo.gpiCount; ++i)
	{
		sprintf(tmpbuf, "{\"gpi\":%d,\"state\":%d}", ginfo.gpiStats[i].GpiId, ginfo.gpiStats[i].State);
		strcat(m_AddOtherJsonBuf, tmpbuf);
		if (i == ginfo.gpiCount - 1)
			strcat(m_AddOtherJsonBuf, "]");
		else
			strcat(m_AddOtherJsonBuf, ",");
	}
}
/***********************************************************
 *
 * http_post : tag to rfid reader
 * method    : add, del
 * return    : status ture/false
 *
 ************************************************************/

//{"gpo_states":[{"gpo":1, "state":1},{"gpo":2, "state":1}]}
uint8_t tag_method;
extern uint32_t FlashDB_Sync_flag;
extern    rfidcfg  mycfgdata;
extern    mqttcfg  mymqttcfg;
extern  struct fdb_tsdb whitelistDB;
extern LTNode* g_Uploadtag;

void HttpModuleAPI::httpResetRfidModule(char *json, int len)
{
	CHK_RDR_STATUS;
	CloseReader(m_hReader);
	CHK_MAPI_ERR(OpenReader());
	m_hReader = rdr_get_handle();
}

//{"gpo_states":[{"gpo":1, "state":1},{"gpo":2, "state":1}]}

extern    rfidcfg mycfgdata;
extern "C" size_t fdb_tsl_query_count(fdb_tsdb_t db, fdb_time_t from, fdb_time_t to, fdb_tsl_status_t status);   /* v9.81ce: fast count */
extern "C" bool del_all_cb(fdb_tsl_t tsl, void *arg);   /* v9.81ce: cleartag soft-delete (no flash erase) */
void HttpModuleAPI::httpEascfg(char *json, int len)
{
   
    m_IsAddOtherJson = true;
	  json_value* jvalue;
    int method_type=0,tempint=0;
   // char system_time[30];
    char tmpbuf[256];

    {   /* v9.82j: EPCs already streamed into ADDlist/DELlist by on_stream_epc (stream scanner).
         * Body may be unbuffered (json==0 when request lacks Content-Length) so json_parse below
         * cannot run; if tags were streamed, set the sync flag directly and let Check_Buffer_Diff
         * flush ADDlist into TSDB and rebuild the 64-bit hash index. */
        cJSON *STCP;
        char *stag;
        if (g_stream_tags_added > 0)
        {
            tag_method = g_stream_tag_is_add ? OPTION_ADD : OPTION_DEL;
            FlashDB_Sync_flag = 11;
            tag_csv_mark_dirty();
            g_stream_tags_added = 0;
            STCP = cJSON_CreateObject();
            if (STCP != NULL)
            {
                cJSON_AddNumberToObject(STCP, "code", 1);
                cJSON_AddStringToObject(STCP, "msg", "success");
                cJSON_AddStringToObject(STCP, "result", "succ");
                stag = cJSON_Print(STCP);
                if (stag != NULL)
                {
                    int selen = (int)strlen(stag);
                    strcpy(m_AddOtherJsonBuf, stag);
                    if (selen >= 2) { m_AddOtherJsonBuf[0] = ' '; m_AddOtherJsonBuf[selen - 1] = ' '; }
                    cJSON_free(stag);
                }
                cJSON_Delete(STCP);
            }
            m_OpError = HMApiErr_Ok;
            return;
        }
    }

    jvalue = json_parse_auto(json, len);
     CHK_PARA_RET(jvalue == NULL, "json format error");
     memset(tmpbuf,0,sizeof(tmpbuf));

     if(json_getint(jvalue, "get", &method_type)==0)
     {    
      if(method_type==1)
      { 

      // CHK_JPAS_RET(json_getstring(jvalue, "system_time", system_time), "system_time");          
       mycfgdata.totaltagcnt = (uint32_t)fdb_tsl_query_count(&whitelistDB, 0, 0x7FFFFFFF, FDB_TSL_WRITE);   /* v9.81ce: fast count - tsdb_tarversal_total O(n^2) too slow */
       /* v9.82g: cJSON 构建打包（替代手写 sprintf/strcat，数字字段规范化 number 非字符串） */
       {
           cJSON *root = cJSON_CreateObject();
           cJSON_AddStringToObject(root, "result", "get");
           cJSON_AddStringToObject(root, "tagstoragedays", mycfgdata.tagstoragedays);
           cJSON_AddNumberToObject(root, "totaltags", (double)mycfgdata.totaltagcnt);
           cJSON_AddNumberToObject(root, "totalalarmcnt", (double)mycfgdata.totalalarmcnt);
           cJSON_AddStringToObject(root, "deviceID", mycfgdata.deviceID);
           cJSON_AddNumberToObject(root, "easflag", (double)mycfgdata.easflag);
           cJSON_AddNumberToObject(root, "radar_range", (double)mycfgdata.radar_range);
           cJSON_AddNumberToObject(root, "peoplecount", (double)mycfgdata.peoplecount);
           cJSON_AddNumberToObject(root, "alarm_duration", (double)mycfgdata.alarm_duration);
           cJSON_AddNumberToObject(root, "alarm_volume", (double)mycfgdata.alarm_volume);
           cJSON_AddNumberToObject(root, "alarm_switch", (double)mycfgdata.alarm_switch);
           cJSON_AddNumberToObject(root, "tag_read_cnt", (double)mycfgdata.tag_read_cnt);
           cJSON *arr = cJSON_AddArrayToObject(root, "filter_rule");
           for (int i = 0; i < 10; i++)
           {
               cJSON *o = cJSON_CreateObject();
               cJSON_AddNumberToObject(o, "ch_num", (double)mycfgdata.tagfilter_rule[i].chnum);
               cJSON_AddNumberToObject(o, "ch_status", (double)mycfgdata.tagfilter_rule[i].chstate);
               cJSON_AddNumberToObject(o, "start_addr", (double)mycfgdata.tagfilter_rule[i].start_addr);
               cJSON_AddNumberToObject(o, "match_len", (double)mycfgdata.tagfilter_rule[i].match_len);
               cJSON_AddStringToObject(o, "mask_code", mycfgdata.tagfilter_rule[i].maskcode);
               cJSON_AddItemToArray(arr, o);
           }
           char *out = cJSON_PrintUnformatted(root);
           if (out != NULL)
           {
               int n = (int)strlen(out);
               if (n >= 2 && out[0] == 123 && out[n - 1] == 125)  //2026-09-09
               {
                   strncpy(m_AddOtherJsonBuf, out + 1, (size_t)(n - 2));
                   m_AddOtherJsonBuf[n - 2] = 0;
               }
               else
                   strcpy(m_AddOtherJsonBuf, out);
               cJSON_free(out);
           }
           cJSON_Delete(root);
       }
       m_OpError = HMApiErr_Ok; 
       return;
      }
    } 
	 else if(json_getint(jvalue, "set", &method_type)==0)
  {
     if(method_type==1)
      {
         CHK_JPAS_RET(json_getstring(jvalue, "tagstoragedays", tmpbuf),"tagstoragedays");
         strcpy(mycfgdata.tagstoragedays,tmpbuf);
          
         CHK_JPAS_RET(json_getint(jvalue, "easflag", &method_type),"easflag");
         mycfgdata.easflag= method_type; 
				
				 CHK_JPAS_RET(json_getint(jvalue, "radar_range", &method_type),"radar_range");
         if(method_type>10)
         {
				  method_type=1; 
				 } 					 
				mycfgdata.radar_range= method_type; 
				
				 CHK_JPAS_RET(json_getint(jvalue, "alarm_volume", &method_type),"alarm_volume");
          if(method_type>10)
         {
				  method_type=1; 
				 }         
				 mycfgdata.alarm_volume= method_type; 
				 
				  CHK_JPAS_RET(json_getint(jvalue, "alarm_duration", &method_type),"alarm_duration");
          if(method_type>10)
         {
				  method_type=1; 
				 }         
				 mycfgdata.alarm_duration= method_type; 
				 
				  CHK_JPAS_RET(json_getint(jvalue, "tag_read_cnt", &method_type),"tag_read_cnt");
          if(method_type>10)
         {
				  method_type=1; 
					 
				 }         
				 mycfgdata.tag_read_cnt= method_type;  
				 
//         CHK_JPAS_RET(json_getstring(jvalue, "system_time", system_time), "system_time");
//         strcpy(mycfgdata.system_time,system_time); 
         
		json_value* pobj = NULL;
		CHK_JPAS_RET(json_getobject(jvalue, "filter_rule", &pobj), "filter_rule");
		for (int i = 0; i < 10; ++i)
		{
			CHK_JPAS_RET(json_getint(pobj->u.array.values[i], "ch_num", &tempint), "ch_num");
			CHK_PARA_RET(tempint < 1 || tempint > 10, "ch_num");
			mycfgdata.tagfilter_rule[i].chnum = tempint;
			CHK_JPAS_RET(json_getint(pobj->u.array.values[i], "ch_status", &tempint), "ch_status");
			mycfgdata.tagfilter_rule[i].chstate = tempint;
			CHK_JPAS_RET(json_getint(pobj->u.array.values[i], "start_addr", &tempint), "start_addr");
			mycfgdata.tagfilter_rule[i].start_addr = tempint;
			CHK_JPAS_RET(json_getint(pobj->u.array.values[i], "match_len", &tempint), "match_len");
			mycfgdata.tagfilter_rule[i].match_len = tempint;
			CHK_JPAS_RET(json_getstring(pobj->u.array.values[i], "mask_code", mycfgdata.tagfilter_rule[i].maskcode), "mask_code");
		}
          set_eastag_to_flash();
          
          strcpy(m_AddOtherJsonBuf,"\"set\":\"success\","); 
          m_OpError = HMApiErr_Ok;
          return;
      } 
  }
  else 
	{
	  httptagprocess(json,len);
	
	}		
}		 
void HttpModuleAPI::httptagprocess(char *json, int len)
{

    m_IsAddOtherJson = true;
  //  char system_time[30];
    cJSON  *CFGROOT = cJSON_Parse(json);

    if(!CFGROOT)
    {
        cJSON  *CFGRESPONSE = cJSON_CreateObject();
        cJSON_AddNumberToObject(CFGRESPONSE, "code", -1); // 参数错误
        cJSON_AddStringToObject(CFGRESPONSE, "msg", "json format error");		// 错误信息
        char *res = cJSON_Print(CFGRESPONSE);
        strcpy(m_AddOtherJsonBuf, res);
        m_OpError = HMApiErr_Param_Err;
        cJSON_Delete(CFGRESPONSE);
        cJSON_Delete(CFGROOT);
        cJSON_free(res);
        return;
    }

    else if(strcmp(cJSON_GetObjectItem(CFGROOT, "method")->valuestring, "upload") == 0)
    {
        uint32_t tagcnt = 0;
        LTDataType  epctag;
        char tagtmpbuf[32];
        char systime[32];

        if(strcmp(cJSON_GetObjectItem(CFGROOT, "option")->valuestring, "clear") == 0)
        {
            m_IsAddOtherJson = false;
            uploadlist_clear();
					  cJSON_Delete(CFGROOT);
            m_OpError = HMApiErr_Ok;
            return;
        }

        if(strcmp(cJSON_GetObjectItem(CFGROOT, "option")->valuestring, "get") == 0)
        {
            cJSON  *tagroot = cJSON_CreateObject();
            cJSON_AddStringToObject(tagroot, "method", "upload"); //uploadtag
            tagcnt = Get_whitetags_total_count(g_Uploadtag);
            cJSON_AddNumberToObject(tagroot, "tagcount", tagcnt);
            cJSON  *DATA  = cJSON_CreateArray();

            for(uint32_t i = 0; i < tagcnt; ++i)
            {
                epctag = Getlist_tag(g_Uploadtag, i + 1);
                memset(tagtmpbuf, 0, sizeof(tagtmpbuf));
                Hex2Str(epctag.epc, epctag.Epclen, tagtmpbuf);
                sprintf(systime, "%s-%d", tagtmpbuf, epctag.crc);
                cJSON_AddStringToObject(DATA, "", systime);

            }

            cJSON_AddItemToObject(tagroot, "epc", DATA);
            rtc_time_update(systime);
            cJSON_AddStringToObject(tagroot, "createtime", systime);
            char *taglistout = cJSON_Print(tagroot);
            int epclenth = strlen(taglistout);
            strcpy(m_AddOtherJsonBuf, taglistout);

            m_AddOtherJsonBuf[0] = ' ';
            m_AddOtherJsonBuf[epclenth - 1] = ' ';
            m_AddOtherJsonBuf[epclenth] = 0;
            cJSON_Delete(CFGROOT);
            cJSON_Delete(tagroot);
            cJSON_free(taglistout);
            m_OpError = HMApiErr_Ok;
            return;
        }
    }
    else if(strcmp(cJSON_GetObjectItem(CFGROOT, "method")->valuestring, "readlog") == 0)
    {
        char logtime[32];
        LTDataType  epctag;
        char tagtmpbuf[32];
        uint32_t tagcnt = Get_whitetags_total_count(g_Uploadtag);
        cJSON  *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "code", 1);
        cJSON_AddStringToObject(root, "msg", "success");
        cJSON_AddNumberToObject(root, "count", tagcnt);

        cJSON  *DATA  = cJSON_CreateArray();

        for(uint32_t i = 0; i < tagcnt; ++i)
        {
            cJSON  *Item = cJSON_CreateObject();
            epctag = Getlist_tag(g_Uploadtag, i + 1);
            seconds_to_date(epctag.TimeStamp, logtime);
            cJSON_AddStringToObject(Item, "time", logtime);
            cJSON_AddStringToObject(Item, "deviceNo", mycfgdata.deviceID);
            cJSON  *EPC  = cJSON_CreateArray();

            memset(tagtmpbuf, 0, sizeof(tagtmpbuf));
            Hex2Str(epctag.epc, epctag.Epclen, tagtmpbuf);
            cJSON_AddStringToObject(EPC, "", tagtmpbuf);

            cJSON_AddItemToObject(Item, "epc", EPC);
            cJSON_AddItemToArray(DATA, Item);
        }

        cJSON_AddItemToObject(root, "data", DATA);
        char *output = cJSON_Print(root);

        strcpy(m_AddOtherJsonBuf, output);

        int epclenth = strlen(m_AddOtherJsonBuf);
        m_AddOtherJsonBuf[0] = ' ';
        m_AddOtherJsonBuf[epclenth - 1] = ' ';
        m_AddOtherJsonBuf[epclenth] = 0;
        cJSON_Delete(CFGROOT);
        cJSON_Delete(root);
        cJSON_free(output);
        m_OpError = HMApiErr_Ok;
        return;

    }
    else if(strcmp(cJSON_GetObjectItem(CFGROOT, "method")->valuestring, "readtag") == 0)
    {
        uint32_t tagcnt = 0;
        /* v9.81ce: chunked deduped EPC string array - cJSON full 128KB -> 4KB m_AddOtherJsonBuf overflow */
        tsdb_tarversal_total(&whitelistDB);
        tagcnt = Get_whitetags_total_count(staticlist);
        mycfgdata.totaltagcnt = tagcnt;
        sprintf(m_AddOtherJsonBuf, " \"method\":\"readtag\",\"tagcount\":%u,\"epc\":[", (unsigned)tagcnt);
        m_IsWhitelist = true;
        m_WlIdx = 0;
        m_WlTotal = (int)tagcnt;
        m_IsAddOtherJson = true;
        cJSON_Delete(CFGROOT);
        m_OpError = HMApiErr_Ok;
        return;
    }

    else if(strcmp(cJSON_GetObjectItem(CFGROOT, "method")->valuestring, "cleartag") == 0)
    {
        /* v9.81ce: clear whitelist TSDB (remove stale/old-version records that cause garbage in readtag) */
        fdb_tsl_iter(&whitelistDB, del_all_cb, &whitelistDB);   /* v9.81ce: soft-delete all WRITE - preserves FlashDB structure (no partition erase) */
        whitelist_index_rebuild(&whitelistDB);   /* v9.82i: cleartag 软删除后重建索引 */
        m_IsAddOtherJson = true;
        sprintf(m_AddOtherJsonBuf, " \"result\":\"cleartag\",\"msg\":\"ok\"");
        cJSON_Delete(CFGROOT);
        m_OpError = HMApiErr_Ok;
        return;
    }

    else if(strcmp(cJSON_GetObjectItem(CFGROOT, "method")->valuestring, "writetag") == 0)
    {
        uint8_t  uflag = 2, wepclenth = 0, wtmpbuf[50];
        LTDataType  epcid;
        cJSON  *Wroot = cJSON_Parse(json);
        cJSON  *WTCP = cJSON_CreateObject();
        cJSON *WTIM = cJSON_GetObjectItem(Wroot, "epc");
        uint32_t array_size = cJSON_GetArraySize(WTIM);// 获取数组大小

        for(uint32_t i = 0; i < array_size; i++)
        {
            memset(&epcid, 0, sizeof(epcid));
            wepclenth = 0xFF & (strlen(cJSON_GetArrayItem(WTIM, i)->valuestring));
            Str2Hex(cJSON_GetArrayItem(WTIM, i)->valuestring, wepclenth, (unsigned char *)wtmpbuf);
            wepclenth = wepclenth / 2;
            memcpy(epcid.epc, wtmpbuf, wepclenth);
            epcid.Epclen = wepclenth;

            tagtable_list_update(ADDlist, epcid, uflag);
        }

        FlashDB_Sync_flag = 11;
        tag_method = OPTION_ADD;
        cJSON_AddNumberToObject(WTCP, "code", 1);              // 成功返回
        cJSON_AddStringToObject(WTCP, "msg", "success");		   // 成功信息
        cJSON_AddStringToObject(WTCP, "whitelist", "writetag");	 // 白名单写标签
        cJSON_AddNumberToObject(WTCP, "total tags", array_size); // 标签总数
        char *woutput = cJSON_Print(WTCP);

        strcpy(m_AddOtherJsonBuf, woutput);

        int epclenth = strlen(m_AddOtherJsonBuf);
        m_AddOtherJsonBuf[0] = ' ';
        m_AddOtherJsonBuf[epclenth - 1] = ' ';
        cJSON_Delete(CFGROOT);
        cJSON_Delete(WTCP);
        cJSON_Delete(Wroot);
        cJSON_free(woutput);
        m_OpError = HMApiErr_Ok;
        return;
    }

    else if(strcmp(cJSON_GetObjectItem(CFGROOT, "method")->valuestring, "mqtt") == 0)
    {

        if(strcmp(cJSON_GetObjectItem(CFGROOT, "option")->valuestring, "get") == 0)
        {
            cJSON  *WTCP = cJSON_CreateObject();
            cJSON_AddNumberToObject(WTCP, "code", 1);            // 成功返回
            cJSON_AddStringToObject(WTCP, "msg", "success");		  // 成功信息

            cJSON_AddStringToObject(WTCP, "host", mymqttcfg.host);
            cJSON_AddStringToObject(WTCP, "port", mymqttcfg.port);
            cJSON_AddStringToObject(WTCP, "user_name", mymqttcfg.user_name);
            cJSON_AddStringToObject(WTCP, "user_pwd", mymqttcfg.user_pwd);

            cJSON_AddStringToObject(WTCP, "warehouseCode", mymqttcfg.system_mqtt_heartbeat.warehouseCode);
            cJSON_AddStringToObject(WTCP, "deviceCode", mymqttcfg.system_mqtt_heartbeat.deviceCode);
            cJSON_AddStringToObject(WTCP, "warehouseType", mymqttcfg.system_mqtt_heartbeat.warehouseType);
            cJSON_AddStringToObject(WTCP, "deviceType", mymqttcfg.system_mqtt_heartbeat.deviceType);
            cJSON_AddStringToObject(WTCP, "deviceSn", mymqttcfg.system_mqtt_heartbeat.deviceSn);
            cJSON_AddStringToObject(WTCP, "deviceModel", mymqttcfg.system_mqtt_heartbeat.deviceModel);
            cJSON_AddStringToObject(WTCP, "deviceBrand", mymqttcfg.system_mqtt_heartbeat.deviceBrand);
            cJSON_AddStringToObject(WTCP, "softVersion", mymqttcfg.system_mqtt_heartbeat.softVersion);
            cJSON_AddStringToObject(WTCP, "empCode", mymqttcfg.system_mqtt_heartbeat.empCode);
            cJSON_AddStringToObject(WTCP, "softName", mymqttcfg.system_mqtt_heartbeat.softName);
            cJSON_AddStringToObject(WTCP, "remark", mymqttcfg.system_mqtt_heartbeat.remark);

            char *woutput = cJSON_Print(WTCP);
            strcpy(m_AddOtherJsonBuf, woutput);

            int epclenth = strlen(m_AddOtherJsonBuf);
            m_AddOtherJsonBuf[0] = ' ';
            m_AddOtherJsonBuf[epclenth - 1] = ' ';
            cJSON_Delete(CFGROOT);
            cJSON_Delete(WTCP);
            cJSON_free(woutput);
            m_OpError = HMApiErr_Ok;
            return;

        }

        if(strcmp(cJSON_GetObjectItem(CFGROOT, "option")->valuestring, "set") == 0)
        {

            cJSON *tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "host");

            if(tmpbuf != NULL)
            {
                strcpy(  mymqttcfg.host, tmpbuf->valuestring);
            }

            tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "port");

            if(tmpbuf != NULL)
            {
                strcpy(  mymqttcfg.port, tmpbuf->valuestring);
            }

            tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "user_name");

            if(tmpbuf != NULL)
            {
                strcpy(  mymqttcfg.user_name, tmpbuf->valuestring);
            }

            tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "user_pwd");

            if(tmpbuf != NULL)
            {
                strcpy(  mymqttcfg.user_pwd, tmpbuf->valuestring);
            }

            tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "warehouseCode");

            if(tmpbuf != NULL)
            {
                strcpy(  mymqttcfg.system_mqtt_heartbeat.warehouseCode, tmpbuf->valuestring);
            }

            tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "warehouseType");

            if(tmpbuf != NULL)
            {
                strcpy(  mymqttcfg.system_mqtt_heartbeat.warehouseType, tmpbuf->valuestring);
            }

            tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "deviceType");

            if(tmpbuf != NULL)
            {
                strcpy(  mymqttcfg.system_mqtt_heartbeat.deviceType, tmpbuf->valuestring);
            }

//				   tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT,"deviceSn");
//					 if(tmpbuf!=NULL)
//					 {
//					 strcpy(  mymqttcfg.system_mqtt_heartbeat.deviceSn ,tmpbuf->valuestring);
//					 }
            tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "deviceModel");

            if(tmpbuf != NULL)
            {
                strcpy(  mymqttcfg.system_mqtt_heartbeat.deviceModel, tmpbuf->valuestring);
            }

            tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "deviceBrand");

            if(tmpbuf != NULL)
            {
                strcpy(  mymqttcfg.system_mqtt_heartbeat.deviceBrand, tmpbuf->valuestring);
            }

            tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "deviceCode");

            if(tmpbuf != NULL)
            {
                strcpy(mymqttcfg.system_mqtt_heartbeat.deviceCode, tmpbuf->valuestring);
            }

            tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "softVersion");

            if(tmpbuf != NULL)
            {
                strcpy(  mymqttcfg.system_mqtt_heartbeat.softVersion, tmpbuf->valuestring);
            }

            tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "empCode");

            if(tmpbuf != NULL)
            {
                strcpy(  mymqttcfg.system_mqtt_heartbeat.empCode, tmpbuf->valuestring);
            }

            tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "softName");

            if(tmpbuf != NULL)
            {
                strcpy(  mymqttcfg.system_mqtt_heartbeat.softName, tmpbuf->valuestring);
            }

            tmpbuf   = cJSON_GetObjectItemCaseSensitive(CFGROOT, "remark");

            if(tmpbuf != NULL)
            {
                strcpy(  mymqttcfg.system_mqtt_heartbeat.remark, tmpbuf->valuestring);
            }

            cJSON  *WTCP = cJSON_CreateObject();
            cJSON_AddNumberToObject(WTCP, "code", 1);            // 成功返回
            cJSON_AddStringToObject(WTCP, "msg", "success");		  // 成功信息
            char *woutput = cJSON_Print(WTCP);
            strcpy(m_AddOtherJsonBuf, woutput);

            int epclenth = strlen(m_AddOtherJsonBuf);
            m_AddOtherJsonBuf[0] = ' ';
            m_AddOtherJsonBuf[epclenth - 1] = ' ';
            cJSON_Delete(CFGROOT);
            cJSON_Delete(WTCP);
            cJSON_free(woutput);
        //    set_mqttcfg_to_flash();
            m_OpError = HMApiErr_Ok;
            return;
        }
    }

    else if(strcmp(cJSON_GetObjectItem(CFGROOT, "method")->valuestring, "gpiotest") == 0)
    {
			  LED_test();
        cJSON  *WTCP = cJSON_CreateObject();
        cJSON_AddNumberToObject(WTCP, "code", 1);              // 成功返回
        cJSON_AddStringToObject(WTCP, "msg", "success");		   // 成功信息
        char *woutput = cJSON_Print(WTCP);
        strcpy(m_AddOtherJsonBuf, woutput);
        int epclenth = strlen(m_AddOtherJsonBuf);
        m_AddOtherJsonBuf[0] = ' ';
        m_AddOtherJsonBuf[epclenth - 1] = ' ';
        cJSON_Delete(CFGROOT);
        cJSON_Delete(WTCP);
        cJSON_free(woutput);
        m_OpError = HMApiErr_Ok;
			  return;
    }
}

extern LTNode *ADDlist;
extern LTNode *DELlist;
void HttpModuleAPI::httpSaveTagMethod(char *json, int len)
{
	LTClear(staticlist);   /* v9.81ce: ensure readtag-rebuilt staticlist freed before add/del (socket-abort path may skip chunk-end clear) */
    /* 在OnBodyCallback中解析流式EPC并转cJSON */
    if (g_stream_tags_added > 0) {
        tag_method = g_stream_tag_is_add ? OPTION_ADD : OPTION_DEL;
        FlashDB_Sync_flag = 11; /* F3(2026-08-14): 流式分支漏置同步标志，白名单不落盘 Flash（重启丢失） */
                /* v9.81cj-k: ADD 和 DEL 后都重新生成 TAG.CSV（保持文件与 TSDB 同步） */
        tag_csv_mark_dirty();
        g_stream_tags_added = 0;
        m_IsAddOtherJson = true;
        cJSON *TCP = cJSON_CreateObject();
        cJSON_AddNumberToObject(TCP, "code", 1);
        cJSON_AddStringToObject(TCP, "msg", "success");
        cJSON_AddStringToObject(TCP, "result", "succ");
        char *tagstring = cJSON_Print(TCP);
        strcpy(m_AddOtherJsonBuf, tagstring);
        int epclenth = strlen(m_AddOtherJsonBuf);
        m_AddOtherJsonBuf[0] = ' ';
        m_AddOtherJsonBuf[epclenth - 1] = ' ';
        cJSON_Delete(TCP);
        cJSON_free(tagstring);
        m_OpError = HMApiErr_Ok;
        return;
    }

	uint8_t  wepclenth = 0, tmpbuf[50];
	LTDataType epcid;
	m_IsAddOtherJson = true;
	cJSON *root = cJSON_Parse(json);
	cJSON *TCP = cJSON_CreateObject();
	LTNode * plist=NULL;
	if (!root)
	{
		cJSON_AddNumberToObject(TCP, "code", -1);				  // 参数错误
		cJSON_AddStringToObject(TCP, "msg", "json format error"); // 错误信息
		char *res = cJSON_Print(TCP);
		strcpy(m_AddOtherJsonBuf, res);
		m_OpError = HMApiErr_Param_Err;
		cJSON_Delete(TCP);
		cJSON_Delete(root);
		return;
	}
	cJSON *item = cJSON_GetObjectItem(root, "method");
	char *name = cJSON_GetStringValue(item);
	if (name == NULL)
	{
		/* F4(2026-08-14): method 字段缺失，strcmp(NULL) 会崩溃，先判空 */
		cJSON_AddNumberToObject(TCP, "code", -1);
		cJSON_AddStringToObject(TCP, "msg", "method missing");
		char *res = cJSON_Print(TCP);
		strcpy(m_AddOtherJsonBuf, res);
		m_OpError = HMApiErr_Param_Missing;
		cJSON_Delete(TCP);
		cJSON_Delete(root);
		return;
	}
	if (strcmp(name, "add") == 0)
	{

		tag_method= OPTION_ADD;
	  plist=ADDlist;
	}
	else if (strcmp(name, "del") == 0)
	{
		tag_method= OPTION_DEL;
		plist=DELlist;
	}
	else
	{
		/* F4(2026-08-14): method 非 add/del，plist=NULL 会崩溃，先判空 */
		cJSON_AddNumberToObject(TCP, "code", -1);
		cJSON_AddStringToObject(TCP, "msg", "method not supported");
		char *res = cJSON_Print(TCP);
		strcpy(m_AddOtherJsonBuf, res);
		m_OpError = HMApiErr_Param_Err;
		cJSON_Delete(TCP);
		cJSON_Delete(root);
		return;
	}
	cJSON *TIM = cJSON_GetObjectItem(root, "epc"); // 获取epc数组

	uint32_t TIM_size = cJSON_GetArraySize(TIM); // 获取数组大小
	for (uint32_t i = 0; i < TIM_size; i++)
	{
		memset(&epcid, 0, sizeof(epcid));
		cJSON *epc_item = cJSON_GetArrayItem(TIM, i);
		const char *epc_str = (epc_item != NULL) ? epc_item->valuestring : NULL;
		if (epc_str == NULL)
		{
			/* F4b(2026-08-14): EPC 元素为空字符串，strlen(NULL) 会崩溃，跳过 */
			continue;
		}
		wepclenth = 0xFF & (strlen(epc_str));
		Str2Hex(epc_str, wepclenth, (unsigned char *)tmpbuf);
		wepclenth = wepclenth / 2;
		if (wepclenth == 0 || wepclenth > EPCIDMAXLEN)
		{
			/* F4b: EPC hex 长度非法，跳过 */
			continue;
		}
		memcpy(epcid.epc, tmpbuf, wepclenth);
		epcid.Epclen = wepclenth;
		// crc = ipcCrc((uint8_t *)tmpbuf, epclenth);
		// epcid.crc =crc;
		 tagtable_list_update(plist,epcid,OPTION_ADD);

	}
  FlashDB_Sync_flag=11;
	cJSON_AddNumberToObject(TCP, "code", 1);			  // 成功返回
	cJSON_AddStringToObject(TCP, "msg", "success");		  // 成功信息
	cJSON_AddStringToObject(TCP, "result", "succ");	  // 成功结果
	cJSON_AddStringToObject(TCP, "whitelist", name);	  // 白名单名
	cJSON_AddNumberToObject(TCP, "total tags", TIM_size); // 标签总数
	char *tagstring = cJSON_Print(TCP);
	strcpy(m_AddOtherJsonBuf, tagstring);
	int epclenth = strlen(m_AddOtherJsonBuf);
	m_AddOtherJsonBuf[0] = ' ';
	m_AddOtherJsonBuf[epclenth - 1] = ' ';
	cJSON_Delete(TCP);
	cJSON_Delete(root);
	cJSON_free(tagstring);
	m_OpError = HMApiErr_Ok;
}
void HttpModuleAPI::httpSetGPO(char *json, int len)
{
	CHK_RDR_STATUS;
	json_value *jvalue;
	json_value *pobj;
	int gpocnt;
	int gpoid[4];
	int gpostate[4];
	char tmpbuf[30];

	jvalue = json_parse_auto(json, len);
	CHK_PARA_RET(jvalue == NULL, "json format error");
	CHK_JPAS_RET(json_getobject(jvalue, "gpo_states", &pobj), "gpo_states");
	CHK_PARA_RET(pobj->type != json_array, "gpo_states");
	gpocnt = pobj->u.array.length;
	CHK_PARA_RET(gpocnt == 0, "gpo_states");
	CHK_PARA_RET(gpocnt < 1 || gpocnt > 4, "gpo_states");

	for (int i = 0; i < gpocnt; ++i)
	{
		sprintf(tmpbuf, "gpo_states[%d].gpo", i);
		CHK_JPAS_RET(json_getint(jvalue, tmpbuf, &gpoid[i]), tmpbuf);
		CHK_PARA_RET(gpoid[i] < 1 || gpoid[i] > 4, tmpbuf);
		sprintf(tmpbuf, "gpo_states[%d].state", i);
		CHK_JPAS_RET(json_getint(jvalue, tmpbuf, &gpostate[i]), tmpbuf);
		CHK_PARA_RET(gpostate[i] < 0 || gpostate[i] > 1, tmpbuf);
	}

	m_OpError = HMApiErr_Ok;
	for (int i = 0; i < gpocnt; ++i)
		gpo_set(gpoid[i], gpostate[i]);
}

void HttpModuleAPI::httpReboot(char *json, int len)
{
	m_OpError = HMApiErr_Ok;
	m_IsAddOtherJson = false;
	APIHttpResponse::m_IsReboot = true;
}
