#ifndef APIHttpRequest_H
#define APIHttpRequest_H
#include "http_parser.h"
#include "stream_epc_scanner.h"


class APIHttpRequest
{
public:
	typedef enum
	{
		HttpMethod_None    = 0,
		HttpMethod_GET     = 1,
		HttpMethod_POST    = 2,
		HttpMethod_OPTIONS = 3,
	} HttpMethodType;

	APIHttpRequest();
	~APIHttpRequest();

	int Parse(char *threebytes);
	HttpMethodType Method();
	int RespCode();
	void setSocket(int sock)
	{
		m_sock = sock;
	}
	int getSocket()
	{
		return m_sock;
	}
	char *Url()
	{
		return m_url;
	}
	char *Body()
	{
		return m_postjson;
	}
	int BodyLen()
	{
		return m_contentlen;
	}
	StreamEpcScanner *getScanner()
	{
		return m_scanner;
	}

private:
	typedef enum
	{
		HttpParseHeader_None = 0,
		HttpParseHeader_Field = 1,
		HttpParseHeader_Value = 2,
	} HttpParseHeaderState;


#define HTTP_URLEN 50
#define HTTP_POSTJSONLEN 1024*40   /* v9.81cc: 128KB->64KB->32KB: heap_left 24KB -> 57KB, whitelist ~1000 nodes; >32KB push handled by stream-exempt (OnBodyCallback) */
/* v9.81bz: 100KB->128KB - 4000 EPC (~112KB) fits; beyond -> stream-exempt */
#define MAXHTTPMSGBUFLEN 1024*5
#define HTTP_HEADERFIELDLEN 30
#define HTTP_HEADERVALUELEN 80
#define HTTP_METHODLEN 20

	http_parser_settings m_hpsettings;
	static int OnUrlCallback(http_parser *parser, const char *at, size_t length);
	static int OnHeaderFieldCallback(http_parser *parser, const char *at, size_t length);
	static int OnHeaderValueCallback(http_parser *parser, const char *at, size_t length);
	static int OnBodyCallback(http_parser *parser, const char *at, size_t length);
	static int OnMessageCompleteCallback(http_parser *parser);
	static int OnHeadersCompleteCallback(http_parser *parser);

	char m_url[HTTP_URLEN];
	int m_contentlen;
	/* v9.81cj-i: m_postjson 动态按 Content-Length 申请（支持 >128KB 大推送）——
	 * 平时 0 占用，收到 HTTP POST 才按实际大小 malloc，请求结束释放 */
	char *m_postjson;
	char m_httpmsgbuffer[MAXHTTPMSGBUFLEN];
	bool m_isfinpars;
	HttpParseHeaderState m_lasthttpheaderstate;
	int m_headerfieldpos;
	int m_headervaluepos;

	char m_headerfield[HTTP_HEADERFIELDLEN];
	char m_headervalue[HTTP_HEADERVALUELEN];
	char m_method[HTTP_METHODLEN];
	int m_respcode;
	int m_sock;
	http_parser m_htParser;
	bool m_iscontentlen;

	StreamEpcScanner *m_scanner;
};


#endif
