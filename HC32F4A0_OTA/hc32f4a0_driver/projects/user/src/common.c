#include "irq.h"
#include "hc32f46_driver.h"
#include "readercfg.h"
#include <stdlib.h>
#include "driverconfig.h"
#include "common.h"
#include "bsp.h"


unsigned short GetNumU16(uint8 *p)
{
    return (p[0] << 8) | p[1];
}

unsigned int GetNumU32(uint8 *p)
{
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

void SetNumU16(uint8 *p, uint16 num)
{
    p[0] = (num >> 8) & 0xff;
    p[1] = (num >> 0) & 0xff;
}

void SetNumU32(uint8 *p, uint32 num)
{
    p[0] = (num >> 24) & 0xff;
    p[1] = (num >> 16) & 0xff;
    p[2] = (num >> 8) & 0xff;
    p[3] = (num >> 0) & 0xff;
}

void *align8byte(void *addr, int size, int *newsize)
{
    int p = (int)addr;
    int l = p % 8;
    *newsize = size;

    if (l != 0)
    {
        p += (8 - l);
        *newsize = size - 8;
    }

    return (void *)p;
}

int IsIpv4address(const char *addr)
{
    int i;
    int isIp = 1;

    for (i = 0; i < (int)strlen(addr); ++i)
    {
        if (addr[i] != '.')
        {
            if (addr[i] < '0' || addr[i] > '9')
                isIp = 0;
        }
    }

    return isIp;
}

void system_reset(void)
{
    TRACE("system reboot......\n");
    sleep_ms(300);
    __NVIC_SystemReset();
}


int isspace_(int x)
{
    if(x == ' ' || x == '\t' || x == '\n' || x == '\f' || x == '\b' || x == '\r')
        return 1;
    else
        return 0;
}

int isdigit_(int x)
{
    if(x <= '9' && x >= '0')
        return 1;
    else
        return 0;
}

int atoi_Arm(const char *nptr)
{
    int c;              /* current char */
    int total;         /* current total */
    int sign;           /* if '-', then negative, otherwise positive */

    /* skip whitespace */
    while ( isspace_((int)(unsigned char) * nptr) )
        ++nptr;

    c = (int)(unsigned char) * nptr++;
    sign = c;           /* save sign indication */

    if (c == '-' || c == '+')
        c = (int)(unsigned char) * nptr++;  /* skip sign */

    total = 0;

    while (isdigit_(c))
    {
        total = 10 * total + (c - '0');     /* accumulate digit */
        c = (int)(unsigned char) * nptr++;  /* get next char */
    }

    if (sign == '-')
        return -total;
    else
        return total;   /* return result, negated if necessary */
}

int addr_str2bin(char *ipstr, uint8 *ipbins)
{
    char *tmp1;

    if (IsIpv4address(ipstr) != 1)
        return -1;

    ipbins[0] = atoi_Arm(ipstr);

    tmp1 = strstr(ipstr, ".");

    if (tmp1 == NULL)
        return -1;

    ipbins[1] = atoi_Arm(tmp1 + 1);

    tmp1 = strstr(tmp1 + 1, ".");

    if (tmp1 == NULL)
        return -1;

    ipbins[2] = atoi_Arm(tmp1 + 1);

    tmp1 = strstr(tmp1 + 1, ".");

    if (tmp1 == NULL)
        return -1;

    ipbins[3] = atoi_Arm(tmp1 + 1);
    return 0;
}

void  GPIO_Configuration(void)
{
    Board_GPIO_Init();

}
/*
int gPrintfInterface = 0;
int gPrintfFd;

int stdout_putchar (int ch)
{
    if (gPrintfInterface != 0)
    {
        uint8 datas[2];
        uint8_t u8Data = (uint8_t)ch;
        int pos = 0;

        if (u8Data == '\n')
            datas[pos++] = '\r';

        datas[pos++] = u8Data;
        write(gPrintfFd, &datas, pos);
    }

    return ch;
}

int InitDegutPrintf(int type, int sn, char *hostip, unsigned short port)
{
    uint8 ipbins[4];

    if (type == 2)
    {
        if (socket(sn, Sn_MR_TCP, 0, SF_TCP_NODELAY) != sn)
            return -1;

        addr_str2bin(hostip, ipbins);

        if (connect(sn, ipbins, port) != SOCK_OK)
            return -2;

        gPrintfFd = sn;
    }
    else if (type == 1)
    {
        commonUartPara uart2Para;
        memset(&uart2Para, 0, sizeof(commonUartPara));
        uart2Para.isBlock	= O_BLOCK;
        uart2Para.isPrintf	= 1;
        uart2Para.baudrate	= 115200;
        uart2Para.timeout	= 20;
        uart2Para.isRdam = 0;

        uart_open(COMMON_INTERFACE_UART1, &uart2Para);
        gPrintfFd = COMMON_INTERFACE_UART1;
    }

    gPrintfInterface = type;
    return 0;
}

void DisableDegutPrintf()
{
    if (gPrintfInterface == 2)
    {
        disconnect(gPrintfFd);
        close(gPrintfFd);
    }
    else if (gPrintfInterface == 1)
        uart_close(COMMON_INTERFACE_UART2);

    gPrintfInterface = 0;

}
*/
int SafeSend(int fd, unsigned char *data, int len)
{
    int sret;
    int scnt = 0;

    while (1)
    {
        sret = write(fd, data, len);

        if (sret < 0)
            return -1;
        else if (sret == 0)
        {
            scnt++;

            if (scnt > 6)
                return -1;
            else
            {
                os_dly_wait(10);
                continue;
            }
        }
        else
            return 0;
    }
}

extern int gIsBrdHnadlerRun;

#define MaxPhy06ReadCnt 52
#define MaxPhy00ReadCnt 45

int gSt06Cnt = 0;
int gSt00Cnt = 0;
uint8 gLastPhySt = 0;
uint64 gLastLinkCheckTime = 0;
void auto_set_eth_link(void)
{
    wiz_PhyConf phyconf;
    uint8 phystate;
    uint64 now;

    now = getSysTick();

    if (now - gLastLinkCheckTime < 92)
        return;
    else
        gLastLinkCheckTime = now;

    phystate = getPHYSR();

    if (phystate == 0x86 && gLastPhySt != 0x86)
    {
        phyconf.by = PHY_CONFBY_SW;
        phyconf.mode = PHY_MODE_AUTONEGO;
        phyconf.duplex = PHY_DUPLEX_FULL;
        phyconf.speed = PHY_SPEED_100;

        wizphy_setphyconf(&phyconf);
//		printf("wizphy_setphyconf to 100M\n");
//		printf("wizphy_setphyconf to PHY_MODE_AUTONEGO\n");
    }

    if (phystate == 0x06)
    {
        if (gLastPhySt == 0x06)
            gSt06Cnt++;
        else
            gSt06Cnt = 0;
    }
    else if (phystate == 0x00)
    {
        if (gLastPhySt == 0x00)
            gSt00Cnt++;
        else
            gSt00Cnt = 0;
    }
    else
    {
        gSt06Cnt = 0;
        gSt00Cnt = 0;
    }

    gLastPhySt = phystate;

    if (gSt06Cnt >= MaxPhy06ReadCnt)
    {
        phyconf.by = PHY_CONFBY_SW;
        phyconf.mode = PHY_MODE_MANUAL;
        phyconf.duplex = PHY_DUPLEX_FULL;
        phyconf.speed = PHY_SPEED_10;

        wizphy_setphyconf(&phyconf);
        gSt06Cnt = 0;
//		printf("wizphy_setphyconf to 10M\n");
    }

    if (gSt00Cnt >= MaxPhy00ReadCnt)
    {
        TRACE("if (gSt00Cnt >= MaxPhy00ReadCnt).....reboot\n");
        system_reset();
    }

//	printf("tm:%lld, phystate:%02X, st06cnt:%d\n", getSysTick(), phystate, gSt06Cnt);
}

cycle_task g_cy_tasks[3];
void *g_cy_task_datas[3];
uint8 g_cy_task_cnt = 0;
int add_cycle_task(cycle_task task, void *data)
{
    if (g_cy_task_cnt >= 3)
        return -1;

    g_cy_tasks[g_cy_task_cnt] = task;
    g_cy_task_datas[g_cy_task_cnt] = data;
    g_cy_task_cnt++;

    return 0;
}

typedef struct
{
    void *addr;
    int size;
    uint8 use;
} MemItem_ST;

#define MaxMemItemCnt 80
MemItem_ST gMItems[MaxMemItemCnt];

int gNewMemSize = 0;
void init_mem_sta(void)
{
    int i;

    for (i = 0; i < MaxMemItemCnt; ++i)
        gMItems[i].use = 0;
}

void add_new_mem_sta(int size)
{
    gNewMemSize += size;
}

extern unsigned char Image$$RW_IRAM1$$ZI$$Limit;
extern unsigned char  __initial_sp;

int get_left_heap_size(char *prefix)
{
	int i;
	int totmem = 0;
	int totblk = 0;
	/* v9.81cj-g: 堆基址用真实值（静态区末尾）——原硬编码 0x20000000 偏低，
	 * 与 main.c:192 的 _init_alloc(heap_base, heap_top) 保持一致 */
	int heap_base_address = (int)(&Image$$RW_IRAM1$$ZI$$Limit);
	int heap_top_address  = (int)0x20060000;
	int left;
	
	heap_base_address += 64 - heap_base_address %64;
	
	for (i = 0; i < MaxMemItemCnt; ++i)
	{
		if (gMItems[i].use == 1)
		{
			totblk++;
			totmem += gMItems[i].size;
		}
	}
	
	left = heap_top_address - heap_base_address - totmem - gNewMemSize;
	TRACE("%s top:%p, base:%p, totmem:%d, gNewMemSize:%d, totblk:%d, left:%d\n", 
		prefix, (void*)heap_top_address, (void*)heap_base_address, 
		totmem, gNewMemSize, totblk, left);
	return left;
}

void *malloc_hexp(unsigned int size)
{
    int i;
    void *p = malloc(size);

//	printf("malloc_hexp size:%d\n", size);
    for (i = 0; i < MaxMemItemCnt; ++i)
    {
        if (gMItems[i].use == 0)
            break;
    }

    gMItems[i].addr = p;
    gMItems[i].size = size;
    gMItems[i].use = 1;
    return p;
}

void *calloc_hexp(unsigned int num, unsigned int size)
{
    int i;
    void *p = calloc(num, size);

    for (i = 0; i < MaxMemItemCnt; ++i)
    {
        if (gMItems[i].use == 0)
            break;
    }

    gMItems[i].addr = p;
    gMItems[i].size = num * size;
    gMItems[i].use = 1;
    return p;
}

void free_hexp(void *p)
{
    int i;

    for (i = 0; i < MaxMemItemCnt; ++i)
    {
        if (gMItems[i].use == 1)
        {
            if (gMItems[i].addr == p)
            {
//				printf("free_hexp size:%d\n", gMItems[i].size);
                gMItems[i].use = 0;
                break;
            }
        }
    }

    free(p);
}

#if IS_RTOS2_SUPPORT
extern uint8_t udp_tag_update ;
void broadcast_process(void* arg)
{
    int i;
    /*
    	wiz_PhyConf phyconf;
    	sleep_ms(3000);
    	wizphy_getphyconf(&phyconf);
    	printf("by:%d, duplex:%d, mode:%d, speed:%d\n", phyconf.by,
    		phyconf.duplex, phyconf.mode, phyconf.speed);
    */

    while(1)
    {
        sleep_ms(100);

        for (i = 0; i < 3; ++i)
        {
            if (g_cy_tasks[i] != NULL)
                g_cy_tasks[i](g_cy_task_datas[i]);
        }

        if (get_spi_ex_dev() == Spi_Ex_Ethernet)
        {
            if (gIsBrdHnadlerRun == 1)
            {
                brdcst_conf_handler();
            }

            if(udp_tag_update == 0)
            {
                auto_set_eth_link();
            }
        }

        if (gEEcmdGpoSet.isFire == 1)
        {
            int allfin = 1;

            for (i = 0; i < gEEcmdGpoSet.idcnt; ++i)
            {
                if (gEEcmdGpoSet.finflags[i] == 0)
                {
                    if (gEEcmdGpoSet.durs[i] <= 0)
                    {
                        gpo_set(gEEcmdGpoSet.ids[i], 1 - gEEcmdGpoSet.states[i]);
                        gEEcmdGpoSet.finflags[i] = 1;
                    }
                    else
                        gEEcmdGpoSet.durs[i] -= 100;
                }
            }

            for (i = 0; i < gEEcmdGpoSet.idcnt; ++i)
            {
                if (gEEcmdGpoSet.finflags[i] == 0)
                    allfin = 0;
            }

            if (allfin == 1)
                gEEcmdGpoSet.isFire = 0;
        }
    }

}

extern const char *ACPMagicStr;

int ValidFlashConfig(unsigned char *pData, int datalen)
{
    int pos = 18;
    int temp;
    int temp2;
    int temp3;
    int i;

    if (pData[pos++] != 0xFF)
        return -1;

    if (pData[pos++] != 0x00)
        return -1;

    temp = pData[pos++];

    if (temp > 128 || temp < 1)
        return -1;

    pos += temp;
    temp = pData[pos++];

    if (!(temp == 4 || temp == 0 || temp == 1))
        return -1;

    for (i = 0; i < temp; ++i)
    {
        temp2 = (pData[pos] << 8) | pData[pos + 1];

        if (temp2 > 3300 || temp2 < 500)
            return -1;

        pos += 2;
    }

    pos++;
    temp = pData[pos++];

    if (temp > 50)
        return -1;

    pos += temp * 4;
    temp = pData[pos++];

    if (temp > 3)
        return -1;

    temp = pData[pos++];

    if (temp != 255)
    {
        if (temp > 15)
            return -1;

    }

    if (pData[pos++] > 1)
        return -1;

    if (pData[pos++] > 1)
        return -1;

    if (pData[pos++] > 1)
        return -1;


    if (pData[pos++] != 0)
        return -1;

    if (pData[pos++] != 0)
        return -1;

    if (pData[pos++] != 0)
        return -1;


    temp = pData[pos++];

    if (temp > 4)
        return -1;

    for (i = 0; i < temp; ++i)
    {
        temp2 = pData[pos++];

        if (temp2 < 1 || temp2 > 4)
            return -1;

    }

    temp = (pData[pos] << 8) | pData[pos + 1];

    if (temp < 5 || temp > 7200)
        return -1;

    pos += 2;

    temp = pData[pos++];

    if (temp == 0 || temp == 1)
    {
        if (temp == 1)
        {
            temp = pData[pos++];

            if (temp > 3 || temp < 1)
                return -1;

            temp = (pData[pos] << 8) | pData[pos + 1];
            pos += 2;

            if (temp > 2048)
                return -1;


            temp = (pData[pos] << 8) | pData[pos + 1];
            pos += 2;

            if (temp > 512)
                return -1;

            pos += (temp % 8 == 0) ? (temp / 8) : (temp / 8 + 1);
            temp = pData[pos++];

            if (temp > 1)
                return -1;

        }
    }
    else
        return -1;


    temp = pData[pos++];

    if (temp == 0 || temp == 1)
    {
        if (temp == 1)
        {
            if (pData[pos++] > 3)
                return -1;

            pos++;

            if (pData[pos++] > 32)
                return -1;

            pos += 4;
        }
    }
    else
        return -1;


    if (pData[pos++] == 0)
    {
        temp = (pData[pos] << 16) | (pData[pos + 1] << 8) | pData[pos + 2];

        if (!(temp == 9600 || temp == 19200 || temp == 38400 ||
                temp == 57600 || temp == 115200))
            return -1;

        pos += 3;
        temp = (pData[pos] << 8) | pData[pos + 1];

        if (temp > 3)
            return -1;

        pos += 2;
    }
    else
        pos += 5;

    pos++;
    temp = pData[pos++];

    if (!(temp == 0 || temp == 1))
        return -1;


    temp = pData[pos++];

    if (!(temp == 2 || temp == 1))
        return -1;


    if (temp == 1)
    {
        temp2 = (pData[pos] << 24) | (pData[pos + 1] << 16) | (pData[pos + 2] << 8) | pData[pos + 3];
        pos += 4;

        if (temp2 < 50 || temp2 > 86400000)
            return -1;

    }


    temp = pData[pos++];

    if (!(temp == 0 || temp == 1))
        return -1;

    if (temp == 1)
    {
        int unigpicnt[4];
        temp2 = pData[pos++];

        if (temp2 < 1 || temp2 > 4)
            return -1;

        //cond 1
        temp3 = pData[pos++];

        if (temp3 < 1 || temp3 > 4)
            return -1;

        for (i = 0; i < 4; ++i)
            unigpicnt[i] = 0;

        for (i = 0; i < temp3; ++i)
        {
            if (pData[pos] < 1 || pData[pos] > 4)
                return -1;
            else
                unigpicnt[pData[pos] - 1]++;

            pos++;

            if (!(pData[pos] == 0 || pData[pos] == 1))
                return -1;

            pos++;
        }

        for (i = 0; i < 4; ++i)
        {
            if (unigpicnt[i] > 1)
                return -1;

        }

        if (temp2 == 1 || temp2 == 3 || temp2 == 4)
        {
            //cond 2
            temp3 = pData[pos++];

            if (temp3 < 1 || temp3 > 4)
                return -1;

            for (i = 0; i < 4; ++i)
                unigpicnt[i] = 0;

            for (i = 0; i < temp3; ++i)
            {
                if (pData[pos] < 1 || pData[pos] > 4)
                    return -1;

                else
                    unigpicnt[pData[pos] - 1]++;

                pos++;

                if (!(pData[pos] == 0 || pData[pos] == 1))
                    return -1;

                pos++;
            }

            for (i = 0; i < 4; ++i)
            {
                if (unigpicnt[i] > 1)
                    return -1;

            }
        }

        temp3 = GetNumU32(pData + pos);
        pos += 4;

        if (temp2 == 2 || temp2 == 3)
        {
            if (temp3 < 500)
                return -1;
        }
    }

    pos++;
    temp = pData[pos++];

    if (temp > 4)
        return -1;

    for (i = 0; i < temp; ++i)
    {
        temp2 = pData[pos++];

        if (temp2 < 1 || temp2 > 4)
            return -1;

        if (pData[pos++] > 1)
            return -1;

        temp2 = GetNumU32(pData + pos);

        if (temp2 < 50)
            return -1;

        pos += 4;
    }

    temp = pData[pos++];

    if (temp > 10)
        return -1;

    for (i = 0; i < temp; ++i)
    {
        if (pData[pos] > 10 || pData[pos] < 1)
            return -1;

        pos++;
    }

    pos += 4;

    temp2 = pData[pos++];

    if (temp2 > 126 || temp2 < 2)
        return -1;

    temp = pData[pos++];

    if (temp > 64)
        return -1;

    pos += temp;

    if (datalen != pos)
        return -1;


    return 0;
}

int GetFlashConfig(unsigned char *pData, int *datalen)
{
    unsigned char magicstr[20];
    int pos = 0;
    int cnflen;
    int roundcnflen;

    flash_bytes_read(ActiveModeConfig_Addr, magicstr, 20);

    if (memcmp(magicstr, ACPMagicStr, 15) != 0)
    {
        return -1;
    }

    memcpy(pData + pos, gNetConf.ip, 4);
    pos += 4;
    memcpy(pData + pos, gNetConf.subnetMask, 4);
    pos += 4;
    memcpy(pData + pos, gNetConf.gatewayIP, 4);
    pos += 4;
    memcpy(pData + pos, gNetConf.mac, 6);
    pos += 6;
    memcpy(pData + pos, magicstr + 17, 3);
    pos += 3;
    cnflen = GetNumU16(magicstr + 15);
    roundcnflen = (4 - cnflen % 4) + cnflen;
    flash_bytes_read(ActiveModeConfig_Addr + 20, pData + pos, roundcnflen);
    pos += cnflen;
    *datalen = pos;
    return 0;
}


int SetFlashConfig(unsigned char *pData, int datalen, networkParaConfig *netconfig)
{
    int issetnet = 1;
    int pos;
    uint8 netcfgbuf[MaxNetConfigLen];

    if (ValidFlashConfig(pData, datalen) != 0)
        return -1;

//	printf("SetFlashConfig\n");
    if (!(pData[0] == 0xff && pData[1] == 0xff &&
            pData[2] == 0xff && pData[3] == 0xff))
    {
        memcpy(netconfig->ip, pData, 4);
        memcpy(netconfig->subnetMask, pData + 4, 4);
        memcpy(netconfig->gatewayIP, pData + 8, 4);

        if (!(netconfig->mac[0] == SilionMACBase1 &&
                netconfig->mac[1] == SilionMACBase2 &&
                netconfig->mac[2] == SilionMACBase3 &&
                (netconfig->mac[3] & 0xF0) == SilionMACBase4))
        {
            if (!(pData[12] == 0xff && pData[13] == 0xff))
                memcpy(netconfig->mac, pData + 12, 6);
        }

        strcpy((char *)netcfgbuf, NetConfig_Marker);
        pos = strlen(NetConfig_Marker);
        memcpy(netcfgbuf + pos, netconfig->ip, 4);
        pos += 4;
        memcpy(netcfgbuf + pos, netconfig->subnetMask, 4);
        pos += 4;
        memcpy(netcfgbuf + pos,  netconfig->gatewayIP, 4);
        pos += 4;

        memcpy(netcfgbuf + pos, netconfig->mac, 6);
        pos += 6;

        SetNumU16(netcfgbuf + pos, netconfig->listenPort);
        pos += 2;
        memcpy(netcfgbuf + pos, netconfig->dnsServer, 4);
        pos += 4;
    }
    else
        issetnet = 0;

    memcpy(pData + 1, ACPMagicStr, 15);
    SetNumU16(pData + 16, datalen - 18);

//	printf("11111111111111111111\n");
    if (issetnet == 1)
        set_multi_config(netcfgbuf, pos, NULL, 0, pData + 1, datalen - 1);
    else
        set_multi_config(NULL, 0, NULL, 0, pData + 1, datalen - 1);

    return 0;
}

int HandleFwAndPCPCmdEx(int fd, unsigned char dlenfitbyte)
{
    unsigned char tmpbuf[3];
    unsigned char SendBuffer[32];
    int datalen;
//	baseParaConfig IPconfig;
    networkParaConfig netconfig;
    unsigned char *pData;

    if (read_n(fd, tmpbuf, 3) != 3)
        return -1;

    get_network_config(&netconfig);

    if (tmpbuf[1] == 20)
    {
        unsigned char getresperr[] = {0xff, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x12};
        int datalen;
        int lastnbytes;
        int i;
        int pos = 0;
        uint8 buf3bytes[3];

        flash_bytes_read(ActiveModeConfig_Addr, SendBuffer, 20);

        if (memcmp(SendBuffer, ACPMagicStr, 15) != 0)
        {
            SafeSend(fd, getresperr, sizeof(getresperr));
            return 0;
        }

        memcpy(buf3bytes, SendBuffer + 17, 3);

        SendBuffer[pos++] = 0xff;
        SendBuffer[pos++] = 0x00;
        datalen = GetNumU16(SendBuffer + 15);
        SetNumU16(SendBuffer + pos, datalen + 18);
//			printf("datalen:%d\n", datalen);
        pos += 2;
        SendBuffer[pos++] = 20;
        SendBuffer[pos++] = 0x00;
        SendBuffer[pos++] = 0x00;
        SendBuffer[pos++] = 0x00;
        SendBuffer[pos++] = 0x00;
        SendBuffer[pos++] = 0x00;

        memcpy(SendBuffer + pos, netconfig.ip, 4);
        pos += 4;
        memcpy(SendBuffer + pos, netconfig.subnetMask, 4);
        pos += 4;
        memcpy(SendBuffer + pos, netconfig.gatewayIP, 4);
        pos += 4;
        memcpy(SendBuffer + pos, netconfig.mac, 6);
        pos += 6;
        memcpy(SendBuffer + pos, buf3bytes, 3);
        pos += 3;

        if (SafeSend(fd, SendBuffer, pos) != 0)
            return -1;

        for (i = 0; i < datalen / 32; ++i)
        {
            flash_bytes_read(ActiveModeConfig_Addr + 20 + i * 32, SendBuffer, 32);

            if (SafeSend(fd, SendBuffer, 32) != 0)
                return -1;
        }

        lastnbytes = datalen % 32;

        if (lastnbytes != 0)
        {
            int realnum = lastnbytes;

            if (lastnbytes % 4 != 0)
                realnum = lastnbytes + (4 - lastnbytes % 4);

            flash_bytes_read(ActiveModeConfig_Addr + 20 + i * 32, SendBuffer, realnum);

            if (SafeSend(fd, SendBuffer, lastnbytes) != 0)
                return -1;
        }
    }
    else if (tmpbuf[1] == 21)
    {
        unsigned char setrespok[] = {0xff, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00};
        unsigned char setresperr[] = {0xff, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x07};
        datalen = (dlenfitbyte << 8) | tmpbuf[0];

//			printf("HandleFwAndPCPCmdEx datalen:%d\n", datalen);
        if (datalen == 0)
        {
            uint16 israsnet = 0;

//				printf("if (datalen == 0) ------------------------\n");
            if (netconfig.ip[0] == 0 && netconfig.ip[1] == 0 &&
                    netconfig.ip[2] == 0 && netconfig.ip[3] == 0)
                israsnet = 1;

            erase_multi_config(israsnet | ERASE_FLS_CFG_BIT_ACTMODE);

            write(fd, setrespok, sizeof(setrespok));
            os_dly_wait(100);
            TRACE("	if (datalen == 0)......\n");
            system_reset();
            return 0;
        }

        pData = malloc_hexp(datalen);

        if (read_n(fd, pData, datalen) != datalen)
        {
            free_hexp(pData);
            return -1;
        }

        if (SetFlashConfig(pData, datalen, &netconfig) < 0)
        {
            SafeSend(fd, setresperr, sizeof(setresperr));
            free_hexp(pData);
            return 0;
        }

        free_hexp(pData);
        SafeSend(fd, setrespok, sizeof(setrespok));
        os_dly_wait(100);
        TRACE("else if (tmpbuf[1] == 21).....reboot\n");
        system_reset();
    }

    return 0;
}

extern void httpapi_hander(int fd, char *threebytes);
void firmware_upgrade_process(void *arg)
{
    int m_curfd, nread;
    int m_rtm = 300;
    uint8 recvbuf[100];
    uint64 lastacttm;
    int lstsock = COMMON_INTERFACE_SOCKET1;   /* v9.81t-b(方案B): 上位机 TCP 8080 固定 SOCKET1（主动模式空闲口），不再递减争抢 */
    wait_fin_init();
    /***********************************************************************/
    active_http_post(); //20250315
    /***********************************************************************/
    ioctl(lstsock, COMMON_INTERFACE_SET_TIMEOUT, &m_rtm);

    while(1)
    {
        m_curfd = apt_single_select(lstsock, 8080, &lastacttm, 3000);
        nread = read_n(m_curfd, recvbuf, 3);

        if (nread != 3)
        {
            disconnect(lstsock);
            close(lstsock);
            continue;
        }

        if ((recvbuf[0] == 0x56) && (recvbuf[1] == 0x45) && (recvbuf[2] == 0x52))
        {
            uint8 sendver[4];
            firmware_version(sendver);
            write(m_curfd, sendver, 4);
        }
        else if ((recvbuf[0] == 0xee) && (recvbuf[1] == 0x00))
        {
            nread = read_n(m_curfd, recvbuf + 3, 3);

            if (nread != 3)
                continue;

            custom_ee_commond(m_curfd, recvbuf, 2, NULL, SafeSend, NULL, NULL);
        }
        else if ((recvbuf[0] == 'P') && (recvbuf[1] == 'O') && (recvbuf[2] == 'S'))
        {
           httpapi_hander(m_curfd, (char *)recvbuf);//20250315
        }
        else if ((recvbuf[0] == 'G') && (recvbuf[1] == 'E') && (recvbuf[2] == 'T'))
        {
           httpapi_hander(m_curfd, (char *)recvbuf);   /* v9.81cl: GET /tagcsv */
        }
        else
        {
            nread = read_n(m_curfd, recvbuf + 3, 2);

            if (nread != 2)
            {
                disconnect(lstsock);
                close(lstsock);
                continue;
            }

            //'IOGET'
            if((recvbuf[0] == 0x49) && (recvbuf[1] == 0x4F) &&
                    (recvbuf[2] == 0x47) && (recvbuf[3] == 0x45) && (recvbuf[4] == 0x54))
            {
//				HandleFwAndPCPCmd(m_curfd);
            }
            else
            {
                nread = read_n(m_curfd, recvbuf + 5, 3);

                if (nread != 3)
                {
                    close(m_curfd);
                    continue;
                }

                if((recvbuf[0] == 0x49) && (recvbuf[1] == 0x4E) && (recvbuf[2] == 0x49) && (recvbuf[3] == 0x54) &&
                        (recvbuf[4] == 0x55) && (recvbuf[5] == 0x50) && (recvbuf[6] == 0x46))
                {
                    firmware_upgrade(recvbuf[7]);
                }
            }
        }

        disconnect(lstsock);
        close(m_curfd);
    }
}

void init_osThreadAttr_t(osThreadAttr_t *attr, int stacksize, osPriority_t prio)
{
    void *realstacktop;
    void *stacktop;
    int newstksize;
    osRtxThread_t *rtxTh_t;

    memset(attr, 0, sizeof(osThreadAttr_t));
    realstacktop = malloc_hexp(stacksize);
    stacktop = align8byte(realstacktop, stacksize, &newstksize);
    TRACE("realstacktop:%p, stacktop:%p, newstksize:%d\n", realstacktop, stacktop, newstksize);
    attr->name = NULL;
    attr->attr_bits = osThreadDetached;
    rtxTh_t = malloc_hexp(sizeof(osRtxThread_t));
    attr->stack_mem = stacktop;
    attr->stack_size = newstksize;
    attr->cb_mem = rtxTh_t;
    attr->cb_size = sizeof(osRtxThread_t);
    attr->priority = prio;
}

#endif

extern int gMaxAvailableSocket;
int getMaxSocketId()
{
    /* v9.81t-b(方案B): 遍历 0-3 找 SOCK_CLOSED 的空闲 socket。
     * 原实现 gMaxAvailableSocket-- 只减不增且无下限：第 5 次调用返回 -1；
     * 且与固定监听（广播3/OTA2/8080-1）递减相撞，socket() 无条件 close 强抢导致
     * 上位机 8080/广播被 OTA 抢占（主动模式无法通信根因）。无空闲返回 -1。 */
    int s;
    for (s = 0; s < _WIZCHIP_SOCK_NUM_; ++s)
    {
        if (getSn_SR((uint8_t)s) == SOCK_CLOSED)
            return s;
    }
    return -1;
}

void led_toggle(int dur, int cycle, void (*cb)(void))
{
    int cnt = 0;
    uint64 start = getSysTick();
    uint64 now;

    while (1)
    {
        sleep_ms(cycle);

        if (cnt % 2 == 0)
            led_on();
        else
            led_off();

        cnt++;

        if (cb != NULL)
            cb();

        if (dur != -1)
        {
            now = getSysTick();

            if (now - start > dur)
                break;
        }
    }

    led_off();
}

void rgb_led_toggle(uint8_t gpoid, int dur, int cycle, void (*cb)(void))
{
    int cnt = 0;
    uint64 start = getSysTick();
    uint64 now;

    while (1)
    {
        sleep_ms(cycle);

        if (cnt % 2 == 0)
            gpo_set(gpoid,1);
        else
            gpo_set(gpoid,0);

        cnt++;

        if (cb != NULL)
            cb();

        if (dur != -1)
        {
            now = getSysTick();

            if (now - start > dur)
                break;
        }
    }

    gpo_set(gpoid,0);
}

void hexTostr(const uint8 *buf, int len, char *strbuf)
{
    int i;

    strbuf[0] = 0;

    for (i = 0; i < len; ++i)
        sprintf(strbuf + i * 2, "%02X", buf[i]);
}

int strTohex(const char *buf, int len, unsigned char *hexbuf)
{
    unsigned char hex;
    int i;

    for (i = 0; i < len / 2; ++i)
        hexbuf[i] = 0;

    for (i = 0; i < len; ++i)
    {
        if ((buf[i] >= '0') && (buf[i] <= '9'))
            hex = buf[i] - '0';
        else if ((buf[i] >= 'A') && (buf[i] <= 'F'))
            hex = buf[i] - 'A' + 10;
        else if ((buf[i] >= 'a') && (buf[i] <= 'f'))
            hex = buf[i] - 'a' + 10;
        else
            return -1;

        hexbuf[i / 2] |= (hex & 0xf) << (((i + 1) % 2) * 4);
    }

    return 0;
}

void toupper_arm(char *str)
{
    int len = strlen(str);
    int i;

    for (i = 0; i < len; ++i)
    {
        if (str[i] >= 'a' && str[i] <= 'z')
            str[i] -= 32;
    }
}

int nonrep_int_array(int *arr, int cnt)
{
    int i;
    int j;

    for (i = 0; i < cnt; ++i)
    {
        for (j = 0; j < cnt; ++j)
        {
            if (i != j)
            {
                if (arr[i] == arr[j])
                    return 0;
            }
        }
    }

    return 1;
}

int nonrep_uint8_array(uint8 *arr, int cnt)
{
    int i;
    int j;

    for (i = 0; i < cnt; ++i)
    {
        for (j = 0; j < cnt; ++j)
        {
            if (i != j)
            {
                if (arr[i] == arr[j])
                    return 0;
            }
        }
    }

    return 1;
}

void memcpy_byb(void *dst, const void *src, size_t len)
{
    uint8 *dst_ = (uint8 *)dst;
    uint8 *src_ = (uint8 *)src;
    int i;

    for (i = 0; i < len; ++i)
        dst_[i] = src_[i];
}
