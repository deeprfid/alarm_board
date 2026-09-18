#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hc32f46_driver.h"
#include "APIHttpRequest.h"
#include "Utility.h"
#include "stream_epc_scanner.h"
#include "ipc.h"

extern LTNode *ADDlist;
extern LTNode *DELlist;
extern LTNode *staticlist;
extern void LTClear(LTNode *phead);   /* v9.81ce: clear readtag staticlist before body parse */
extern struct fdb_tsdb whitelistDB;
extern "C" void Transfer_EPC_from_list_to_TSDB(fdb_tsdb_t tsDB, LTNode *plist, uint8_t method);
extern "C" void Init_list_clear(LTNode *phead);
extern int g_stream_tags_added;
extern int g_stream_tag_is_add;
extern "C" void tagtable_list_update(LTNode *phead, LTDataType epcID, uint8_t method);

/* ---- Á÷Ê½½âÎö body ÖÐµÄ EPC£¬¹¹½¨ ADDlist/DELlist ---- */
static void on_stream_epc(const uint8_t *epc_bin, uint8_t epc_len,
                           int is_add, void *user)
{
    (void)user;
    if (epc_len == 0 || epc_len > 16) return;   /* v9.81cd: epc[16] (EPCIDMAXLEN) - was 32, memcpy would overflow */
    LTDataType epcid;
    memset(&epcid, 0, sizeof(epcid));
    memcpy(epcid.epc, epc_bin, epc_len);
    epcid.Epclen = epc_len;
        tagtable_list_update(is_add ? ADDlist : DELlist, epcid, OPTION_ADD);
    if (g_stream_tags_added == 0) g_stream_tag_is_add = is_add;
    g_stream_tags_added++;
    /* v9.81cj-i fix: add/del ¶¼Ã¿ 500 ÕÅ·ÖÅú flush¡ª¡ªADDlist 4001x36B=144KB + m_postjson 130KB
     * Í¬Ê±×¤Áô³¬¶ÑÉÏÏÞ(287KB)ÖÂ malloc Ê§°Ü¶ª EPC£»·ÖÅúºó·åÖµ ~38KB */
    if ((g_stream_tags_added % 500) == 0)
    {
        Transfer_EPC_from_list_to_TSDB(&whitelistDB, is_add ? ADDlist : DELlist,
                                  is_add ? OPTION_ADD : OPTION_DEL);
        Init_list_clear(is_add ? ADDlist : DELlist);
    }
}


APIHttpRequest::APIHttpRequest()
{
	http_parser_settings_init(&m_hpsettings);
	m_hpsettings.on_header_value = OnHeaderValueCallback;
	m_hpsettings.on_url = OnUrlCallback;
	m_hpsettings.on_message_complete = OnMessageCompleteCallback;
	m_hpsettings.on_body = OnBodyCallback;
	m_hpsettings.on_header_field = OnHeaderFieldCallback;
	m_hpsettings.on_headers_complete = OnHeadersCompleteCallback;

	m_scanner = stream_epc_scanner_new(on_stream_epc, NULL);
	m_postjson = NULL;   /* v9.81cj-i: ¶¯Ì¬ÉêÇë£¬Æ½Ê±²»Õ¼ */
}

APIHttpRequest::~APIHttpRequest()
{
	stream_epc_scanner_free(m_scanner);
	m_scanner = NULL;
}

int APIHttpRequest::Parse(char *threebytes)
{
	LTClear(staticlist);   /* v9.81ce: free readtag-rebuilt staticlist BEFORE body parse - on_stream_epc (DELlist add) needs heap; message-complete clear was too late */
	if (m_postjson != NULL) { free_hexp(m_postjson); m_postjson = NULL; }   /* v9.81cj-i: ÊÍ·ÅÉÏ¸öÇëÇóµÄ body »º³å */
	int pos = 0;
	g_stream_tags_added = 0;   /* v9.82j: per-request reset (stream scanner re-counts from 0) */
	m_iscontentlen = false;
	m_isfinpars = false;
	m_lasthttpheaderstate = HttpParseHeader_None;
	m_contentlen = 0;
	m_url[0] = 0;
	bool IsFirstRecv = true;
	http_parser_init(&m_htParser, HTTP_REQUEST);
	m_htParser.data = this;

	/* Reset stream scanner for this request */
	stream_epc_scanner_reset(m_scanner);

	if (threebytes != NULL && IsFirstRecv)
	{
		memcpy(m_httpmsgbuffer, threebytes, 3);
		pos += 3;
	}
	while (true)
	{
		int nrecv;
		if (IsFirstRecv)
		{
			nrecv = read_n(m_sock, m_httpmsgbuffer+pos, HTTP_URLEN);			
		}
		else
			nrecv = read(m_sock, m_httpmsgbuffer, MAXHTTPMSGBUFLEN);
					
		if (nrecv <= 0)
		{
//			TRACE("0000000 nrecv:%d\n", nrecv);
			return -1;
		}
		
		if (IsFirstRecv)
		{
			if (threebytes != NULL)
				nrecv += 3;
			IsFirstRecv = false;
		}

		m_httpmsgbuffer[nrecv] = 0;
//		TRACE("nrecv:%d, recvstr:%s\n", nrecv, m_httpmsgbuffer);
		
		int nparsed = http_parser_execute(&m_htParser, &m_hpsettings, 
			m_httpmsgbuffer, nrecv);
		if (nparsed != nrecv)
		{
//			TRACE("11111111 nparsed != nrecv err:%s\n", http_errno_description(HTTP_PARSER_ERRNO(&m_htParser)));
			return -1;
		}
		if (m_htParser.http_errno != HPE_OK)
		{
//			TRACE("22222222 htParser.http_errno != HPE_OK\n");
			return 400;
		}

		if (m_isfinpars)
			break;
	}
	return 200;
}

int APIHttpRequest::OnUrlCallback(http_parser *parser, 
								  const char *at, size_t length)
{
	APIHttpRequest *pReq = (APIHttpRequest*)parser->data;
	if (length > HTTP_URLEN - 1)
	{
		pReq->m_respcode = 400;
		return -1;
	}

	memcpy(pReq->m_url, at, length);
	pReq->m_url[length] = 0;
	strcpy(pReq->m_method, http_method_str((http_method)parser->method));
//	printf("method:%s\n", pReq->m_method);
//	printf("url:%s\n", pReq->m_url);
	return 0;
}

int APIHttpRequest::OnHeaderFieldCallback(http_parser *parser, 
										  const char *at, size_t length)
{
	APIHttpRequest *pReq = (APIHttpRequest*)parser->data;
	if (pReq->m_lasthttpheaderstate == HttpParseHeader_None || 
		pReq->m_lasthttpheaderstate == HttpParseHeader_Value)
	{
		if (pReq->m_lasthttpheaderstate == HttpParseHeader_Value)
		{
			pReq->m_headervalue[pReq->m_headervaluepos] = 0;
//			printf("val:%s\n", pReq->m_headervalue);
		}
		if (length <= HTTP_HEADERFIELDLEN)
		{
			memcpy(pReq->m_headerfield, at, length);
			pReq->m_headerfieldpos = length;
		}
		else
			pReq->m_headerfieldpos = 0;
	}
	else
	{
		if (length + pReq->m_headerfieldpos <= HTTP_HEADERFIELDLEN)
		{
			memcpy(pReq->m_headerfield+pReq->m_headerfieldpos, at, length);
			pReq->m_headerfieldpos += length;
		}
	}
	pReq->m_lasthttpheaderstate = HttpParseHeader_Field;
	return 0;
}

int APIHttpRequest::OnHeaderValueCallback(http_parser *parser, const char *at, size_t length)
{	
	APIHttpRequest *pReq = (APIHttpRequest*)parser->data;

	if (pReq->m_lasthttpheaderstate == HttpParseHeader_Field)
	{
		pReq->m_headerfield[pReq->m_headerfieldpos] = 0;
		if (strcmp("Content-Length", pReq->m_headerfield) == 0 ||
		    strcmp("content-length", pReq->m_headerfield) == 0)
			pReq->m_iscontentlen = true;
		if (length <= HTTP_HEADERVALUELEN)
		{
			memcpy(pReq->m_headervalue, at, length);
			pReq->m_headervaluepos = length;
		}
		else
			pReq->m_headervaluepos = 0;
	}
	else
	{
		if (length + pReq->m_headervaluepos <= HTTP_HEADERVALUELEN)
		{
			memcpy(pReq->m_headervalue+pReq->m_headervaluepos, at, length);
			pReq->m_headervaluepos += length;
		}
	}
	pReq->m_lasthttpheaderstate = HttpParseHeader_Value;
	return 0;
}

int APIHttpRequest::OnBodyCallback(http_parser *parser, 
								   const char *at, size_t length)
{
	APIHttpRequest *pReq = (APIHttpRequest*)parser->data;

	/* ---- Á÷Ê½´¦Àí body chunk ÖÐµÄ EPC Êý¾Ý ---- */
	if (pReq->m_scanner)
		stream_epc_scanner_feed(pReq->m_scanner, at, length);

	/* ---- Á÷Ê½ÀÛ»ý buffer ²¢½âÎö EPC ---- */
	if (pReq->m_postjson == NULL)   /* v9.81cj-i fix: Î´·ÖÅäÊ±²»ÖÐ¶ÏÁ÷Ê½ */
		return 0;
	if (pReq->m_contentlen + (int)length > HTTP_POSTJSONLEN - 1   /* v9.82j: cap=alloc, no overflow */)
	{
		/* v9.81bz: stream scanner already handled EPCs - skip buffering, no 400 */
		if (pReq->m_scanner)
			return 0;
		pReq->m_respcode = 400;
		return -1;
	}
	memcpy(pReq->m_postjson + pReq->m_contentlen, at, length);
	pReq->m_contentlen += length;

	return 0;
}
int APIHttpRequest::OnMessageCompleteCallback(http_parser *parser)
{
	APIHttpRequest *pReq = (APIHttpRequest*)parser->data;
	if (pReq->m_postjson != NULL)   /* v9.81cj-i fix: NULL ±£»¤ */
		pReq->m_postjson[pReq->m_contentlen] = 0;
//	printf("m_contentlen:%d\n", pReq->m_contentlen);
//	printf("MessageComplete\n");
	pReq->m_respcode = 200;
	pReq->m_isfinpars = true;
	return 0;
}
int APIHttpRequest::OnHeadersCompleteCallback(http_parser *parser)
{
	APIHttpRequest *pReq = (APIHttpRequest*)parser->data;
	pReq->m_contentlen = 0;

	/* v9.81cj-i: Content-Length known - allocate body by actual size (supports >128KB) */
	if (pReq->m_postjson == NULL && pReq->m_iscontentlen)
	{
		int bodylen = atoi(pReq->m_headervalue);
		if (bodylen <= 0) bodylen = HTTP_POSTJSONLEN;
		/* v9.82j: æ¢å¤è®¾è®¡â€”â€”body æŒ‰ Content-Length åŠ¨æ€ç”³è¯·ã€ä¸Šé™ 128KB(v9.81cj-i-b)ï¼›>128KB æˆªæ–­ç¼“å†²ï¼ŒEPC èµ°æµå¼è±å… */
		if (bodylen > HTTP_POSTJSONLEN)
			bodylen = 0;   /* v9.82j: >HTTP_POSTJSONLEN = stream-only (scanner captures EPCs; no buffer overflow possible) */
		else
		{
			pReq->m_postjson = (char*)malloc_hexp(bodylen + 1);
			if (pReq->m_postjson == NULL)
				pReq->m_respcode = 500;
		}
	}

	pReq->m_headervalue[pReq->m_headervaluepos] = 0;
//	printf("val:%s\n", pReq->m_headervalue);

	return 0;
}

APIHttpRequest::HttpMethodType APIHttpRequest::Method()
{
	if (strcmp("GET", m_method) == 0)
	return HttpMethod_GET;
if (strcmp("POST", m_method) == 0)
		return HttpMethod_POST;
	else if (strcmp("OPTIONS", m_method) == 0)
		return HttpMethod_OPTIONS;
	return HttpMethod_None;
}
int APIHttpRequest::RespCode()
{
	return m_respcode;
}
