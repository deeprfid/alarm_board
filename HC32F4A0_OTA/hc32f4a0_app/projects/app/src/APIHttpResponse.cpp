#include <stdio.h>
#include <string.h>
#include "APIHttpResponse.h"
#include "Utility.h"
#include "hc32f46_driver.h"
#include "ff.h"          /* v9.81cl: GET /tagcsv 下载 */
#include "tag_csv.h"     /* v9.81cl: FAT 卷互斥 */
extern volatile int gIsUsbAvailable;   /* v9.81cl: USB 占用时禁读卷 */

#define SERVER_STRING "Server: BMAutomation UHF RFID/0.0.1\r\n"

#define EZRet(exp) \
	if (exp != 0) \
		return -1; \
        \


int send_v(int sock, char *buf, int len)
{
	volatile uint32_t time_cnt = 0x200;   /* v9.81ce: fast fail - 0xffff retries = 65s print spam on closed socket */
	do
	{
	//	time_cnt--;
		if(time_cnt-- == 0)
			return -1;
		else
		{
			if(write(sock, buf, len) == len)
				return 0;
		}
	}while(1);

//	if (write(sock, buf, len) == len)
//		return -1;
//	else
	
}

bool APIHttpResponse::m_IsReboot = false;
int APIHttpResponse::sendSameHeaders(int sock, bool iscache)
{
	char buf[250];
	strcpy(buf, SERVER_STRING);
	EZRet(send_v(sock, buf, strlen(buf)));
	sprintf(buf, "Connection: Keep-Alive\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));
	sprintf(buf, "Access-Control-Allow-Origin: *\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));//
	if (iscache)
		sprintf(buf, "Cache-Control: max-age=86400\r\n");
	else
		sprintf(buf, "Cache-Control: no-store\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));
	sprintf(buf, "\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));
	return 0;
}
int APIHttpResponse::SendChunkedTrailer(int sock)
{
	char buf[10];
	strcpy(buf, "0\r\n\r\n");
	EZRet(send_v(sock, buf, 5));
	if (m_IsReboot)
		Reboot();
	return 0;
}
int APIHttpResponse::SendChunkedSection(int sock, char *chunk, int len)
{
	char buf[50];
	sprintf(buf, "%x", len);
	strcat(buf, "\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));
//	strcat(chunk, "\r\n");
	EZRet(send_v(sock, chunk, len));
	sprintf(buf, "\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));
	return 0;
}
int APIHttpResponse::SendChunkedHeader(int sock)
{
	char buf[250];
	sprintf(buf, "HTTP/1.1 200 OK\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));
	sprintf(buf, "Content-Type: application/json; charset=utf-8\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));
	sprintf(buf, "Transfer-Encoding: chunked\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));
	EZRet(sendSameHeaders(sock, false));
	return 0;
}
int APIHttpResponse::SendJson(int sock, char *pjson)
{
	char buf[250];
	int jsonlen = strlen(pjson);
	sprintf(buf, "HTTP/1.1 200 OK\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));
	sprintf(buf, "Content-Type: application/json; charset=utf-8\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));
	sprintf(buf, "Content-Length:%d\r\n", jsonlen);
	EZRet(send_v(sock, buf, strlen(buf)));
	EZRet(sendSameHeaders(sock, false));
	EZRet(send_v(sock, pjson, jsonlen));
	return 0;
}

int APIHttpResponse::SendOptionsReqHeader(int sock)
{
	char buf[250];
	sprintf(buf, "HTTP/1.1 %d OK\r\n", 200);
	EZRet(send_v(sock, buf, strlen(buf)));

	strcpy(buf, "Content-Length:0\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));

	strcpy(buf, "Allow: GET, POST, OPTIONS\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));

	strcpy(buf, "Access-Control-Allow-Method: GET, POST, OPTIONS\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));

	strcpy(buf, "Access-Control-Allow-Headers: Origin, X-Requested-With, Content-Type, Accept\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));

	strcpy(buf, "Access-Control-Max-Age: 1728000\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));

	EZRet(sendSameHeaders(sock, false));
	return 0;
}

int APIHttpResponse::SendErrorHeader(int sock, int httpcode)
{
	char buf[250];
	if (httpcode == 204)
		sprintf(buf, "HTTP/1.1 %d OK\r\n", httpcode);
	else
		sprintf(buf, "HTTP/1.1 %d Error\r\n", httpcode);
	EZRet(send_v(sock, buf, strlen(buf)));
	strcpy(buf, "Content-Length:0\r\n");
	EZRet(send_v(sock, buf, strlen(buf)));
	EZRet(sendSameHeaders(sock, false));
	return 0;
}

#define EZCLOFDRET(exp) \
	do \
{ \
	if (exp != 0) \
{ \
	disconnect(curfd); \
	close(curfd); \
	return; \
} \
} while (false);

/* ---- v9.81cl: GET /tagcsv 网络下载 ----
 * 白名单 CSV 流式发送：挂载 FAT 卷 → 读 TAG.CSV → chunked 传输 → 卸载。
 * 去重在生成时（wl_dedup 位图）已完成；流式发送不占大内存（避免 readtag 全量 720KB 超堆）。
 * USB 占用 / CSV 生成中 → 503；文件不存在 → 404。 */
static int http_serve_tagcsv(int sock)
{
	static FATFS s_dl_fs;      /* 静态缓冲：不占用 8080 服务器任务栈 */
	static FIL   s_dl_fp;
	static char  s_dl_buf[1024];
	static char  s_dl_hdr[220];
	FRESULT fr;
	UINT br;

	/* USB 枚举激活：电脑可能并发写 FAT 卷，拒绝读取 */
	if (gIsUsbAvailable) {
		strcpy(s_dl_hdr, "HTTP/1.1 503 Busy\r\nContent-Type: text/plain\r\nContent-Length: 29\r\n\r\nUSB active, unplug USB first\r\n");
		return send_v(sock, s_dl_hdr, strlen(s_dl_hdr));
	}

	/* 与 tag_csv_task 生成互斥 */
	if (tag_csv_fat_trylock() != 0) {
		strcpy(s_dl_hdr, "HTTP/1.1 503 Busy\r\nContent-Type: text/plain\r\nContent-Length: 28\r\n\r\nCSV generating, retry later\r\n");
		return send_v(sock, s_dl_hdr, strlen(s_dl_hdr));
	}

	fr = f_mount(&s_dl_fs, "", 1);
	if (fr == FR_OK)
		fr = f_open(&s_dl_fp, "TAG.CSV", FA_READ);

	if (fr == FR_OK) {
		sprintf(s_dl_hdr, "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nTransfer-Encoding: chunked\r\nCache-Control: no-store\r\n\r\n");
		if (send_v(sock, s_dl_hdr, strlen(s_dl_hdr)) == 0) {
			while (f_read(&s_dl_fp, s_dl_buf, sizeof(s_dl_buf), &br) == FR_OK && br > 0) {
				if (APIHttpResponse::SendChunkedSection(sock, s_dl_buf, (int)br) != 0)
					break;
			}
			APIHttpResponse::SendChunkedTrailer(sock);
		}
		f_close(&s_dl_fp);
	} else {
		strcpy(s_dl_hdr, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 55\r\n\r\nTAG.CSV not found, push whitelist to generate first\r\n");
		send_v(sock, s_dl_hdr, strlen(s_dl_hdr));
	}
	f_mount(NULL, "", 0);
	tag_csv_fat_unlock();
	return 0;
}

void APIHttpResponse::httpRespose(HttpModuleAPI *pHMApi, APIHttpRequest *ApiReq, int curfd, char *threebytes)
{
	
	ApiReq->setSocket(curfd);

	int rcode = ApiReq->Parse(threebytes);
//	printf("rcode:%d\n", rcode);
		if (rcode == -1)
		{
			EZCLOFDRET(-1);
		}
		
			int httprcode = ApiReq->RespCode();
			if (httprcode == 200)
			{
				APIHttpRequest::HttpMethodType methodtype = ApiReq->Method();
				char *url = ApiReq->Url();
//				printf("httpRespose methodtype:%d, url:%s\n", methodtype, url);
				if (methodtype == APIHttpRequest::HttpMethod_POST)
				{
					char *apiurl = ApiReq->Url();
					if (apiurl[strlen(apiurl)-1] == '/')
						apiurl[strlen(apiurl)-1] = 0;
//					printf("httpRespose apiurl:%s\n", apiurl);
					
					httprcode = pHMApi->httpAPIDispatch(apiurl, 
						ApiReq->Body(), ApiReq->BodyLen());
//					printf("after pHMApi->httpAPIDispatch httprcode:%d\n", httprcode);
					if (httprcode == 200)
					{
						EZCLOFDRET(APIHttpResponse::SendChunkedHeader(curfd));
						
						char *pjrespbuf = pHMApi->GetRespBuf();
						
						int chunkret = pHMApi->httpGetChunkedHeader(curfd);
						if(chunkret>0x800)
						{
						int sendcnt=chunkret%0x600;
            int mode=chunkret/0x600;
						   for(int i=0;i<mode;i++)
						    {
								EZCLOFDRET(APIHttpResponse::SendChunkedSection(curfd, pjrespbuf+i*0x600, 0x600));
	
                }
                EZCLOFDRET(APIHttpResponse::SendChunkedSection(curfd, pjrespbuf+mode*0x600, sendcnt));
	
						}		
            else
						{
						     EZCLOFDRET(APIHttpResponse::SendChunkedSection(curfd, pjrespbuf, chunkret));

						}							
						while (true)
						{
							chunkret = pHMApi->httpGetNextChunked();
							if (chunkret > 0)
							{
								EZCLOFDRET(APIHttpResponse::SendChunkedSection(curfd, pjrespbuf, chunkret));
							}
							else
								break;
						}
						EZCLOFDRET(APIHttpResponse::SendChunkedTrailer(curfd));
					}
					else
					{
						EZCLOFDRET(APIHttpResponse::SendErrorHeader(curfd, httprcode));
					}
				}
				else if (methodtype == APIHttpRequest::HttpMethod_OPTIONS)
				{
					EZCLOFDRET(APIHttpResponse::SendOptionsReqHeader(curfd));
				}
				else if (methodtype == APIHttpRequest::HttpMethod_GET)
				{
					/* v9.81cl: GET /tagcsv - 流式下载白名单 CSV（去重已在生成时完成） */
					const char *gurl = ApiReq->Url();
					if (gurl != NULL && strstr(gurl, "tagcsv") != NULL) {
						EZCLOFDRET(http_serve_tagcsv(curfd));
					} else {
						EZCLOFDRET(APIHttpResponse::SendErrorHeader(curfd, 404));
					}
				}
				else
				{
					EZCLOFDRET(APIHttpResponse::SendErrorHeader(curfd, 400));
				}
			}
			else
			{
				EZCLOFDRET(APIHttpResponse::SendErrorHeader(curfd, httprcode));
			}
}

