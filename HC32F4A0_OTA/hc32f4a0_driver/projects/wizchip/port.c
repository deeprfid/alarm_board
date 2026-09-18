#include <stdlib.h>
#include <string.h>
#include "port.h"
#include "timer.h"
#include "wizchip_conf.h"
#include "hc32f46_driver.h"
#include "dhcp.h"
#include "driverconfig.h"


#define SPI_UNIT                        (CM_SPI1)
#define SPI_CLK                         (FCG1_PERIPH_SPI1)

/* RESET = PE10 */
#define SPI_RESET_PORT                     (GPIO_PORT_E)
#define SPI_RESET_PIN                      (GPIO_PIN_10)

/* W5100_INT = PE11 */
#define SPI_INT_PORT                     (GPIO_PORT_E)
#define SPI_INT_PIN                      (GPIO_PIN_11)

/* SS = PE12 */
#define SPI_SS_PORT                     (GPIO_PORT_E)
#define SPI_SS_PIN                      (GPIO_PIN_12)

/* SCK = PE13 */
#define SPI_SCK_PORT                    (GPIO_PORT_E)
#define SPI_SCK_PIN                     (GPIO_PIN_13)
#define SPI_SCK_FUNC                    (GPIO_FUNC_40)

/* MISO = PE14 */
#define SPI_MISO_PORT                   (GPIO_PORT_E)
#define SPI_MISO_PIN                    (GPIO_PIN_14)
#define SPI_MISO_FUNC                   (GPIO_FUNC_42)


/* MOSI = PE15 */
#define SPI_MOSI_PORT                   (GPIO_PORT_E)
#define SPI_MOSI_PIN                    (GPIO_PIN_15)
#define SPI_MOSI_FUNC                   (GPIO_FUNC_41)

void WIZ_SPI_Init(void)
{
    stc_spi_init_t stcSpiInit;
    stc_gpio_init_t stcGpioInit;
  //  stc_spi_delay_t stc_spi_delay;

    FCG_Fcg1PeriphClockCmd(SPI_CLK, ENABLE);
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDrv       = PIN_LOW_DRV;
    stcGpioInit.u16PinInputType = PIN_IN_TYPE_CMOS;
    (void)GPIO_Init(SPI_SCK_PORT,  SPI_SCK_PIN,  &stcGpioInit);
    (void)GPIO_Init(SPI_MOSI_PORT, SPI_MOSI_PIN, &stcGpioInit);
    (void)GPIO_Init(SPI_MISO_PORT, SPI_MISO_PIN, &stcGpioInit);

    stcGpioInit.u16PinState = PIN_STAT_SET;
    stcGpioInit.u16PinDir   = PIN_DIR_OUT;
	  stcGpioInit.u16PullUp   = PIN_PU_ON;
    (void)GPIO_Init(SPI_SS_PORT, SPI_SS_PIN, &stcGpioInit);
    (void)GPIO_Init(SPI_RESET_PORT, SPI_RESET_PIN, &stcGpioInit);

    stcGpioInit.u16PinDir   = PIN_DIR_IN;
	  stcGpioInit.u16PullUp   = PIN_PU_ON;
    (void)GPIO_Init(SPI_INT_PORT, SPI_INT_PIN, &stcGpioInit);
    /* Configure Port */

    GPIO_SetFunc(SPI_SCK_PORT,  SPI_SCK_PIN,  SPI_SCK_FUNC);
    GPIO_SetFunc(SPI_MOSI_PORT, SPI_MOSI_PIN, SPI_MOSI_FUNC);
    GPIO_SetFunc(SPI_MISO_PORT, SPI_MISO_PIN, SPI_MISO_FUNC);
    /* Configuration SPI structure */

    SPI_StructInit(&stcSpiInit);

    stcSpiInit.u32WireMode          = SPI_3_WIRE;
    stcSpiInit.u32TransMode         = SPI_FULL_DUPLEX;
    stcSpiInit.u32MasterSlave       = SPI_MASTER;
    stcSpiInit.u32Parity            = SPI_PARITY_INVD;
    stcSpiInit.u32SpiMode           = SPI_MD_0;
    stcSpiInit.u32BaudRatePrescaler = SPI_BR_CLK_DIV2;
    stcSpiInit.u32DataBits          = SPI_DATA_SIZE_8BIT;
    stcSpiInit.u32FirstBit          = SPI_FIRST_MSB;
    stcSpiInit.u32FrameLevel        = SPI_1_FRAME;
    (void)SPI_Init(SPI_UNIT, &stcSpiInit);

//    SPI_SetSckPolarity(SPI_UNIT,SPI_SCK_POLARITY_LOW);
//    SPI_SetSckPhase   (SPI_UNIT,SPI_SCK_PHASE_ODD_EDGE_SAMPLE);

//    stc_spi_delay.u32SetupDelay   = SPI_INTERVAL_TIME_1SCK;
//    stc_spi_delay.u32ReleaseDelay = SPI_RELEASE_TIME_1SCK;
//    stc_spi_delay.u32SetupDelay   = SPI_SETUP_TIME_1SCK;
//    SPI_DelayTimeConfig(SPI_UNIT, &stc_spi_delay);


    SPI_Cmd(SPI_UNIT, ENABLE);
}

void WIZ_CS(uint8_t val)
{
    if (val == LOW)
    {
        GPIO_ResetPins(SPI_SS_PORT, SPI_SS_PIN);
    }
    else if (val == HIGH)
    {
        GPIO_SetPins(SPI_SS_PORT, SPI_SS_PIN);
    }
}

void w5100s_cs_select(void)
{
    GPIO_ResetPins(SPI_SS_PORT, SPI_SS_PIN);
}

void w5100s_cs_deselect(void)
{
    GPIO_SetPins(SPI_SS_PORT, SPI_SS_PIN);
}

void Reset_W5100S(void) // 2018-07-17
{
    GPIO_ResetPins(SPI_RESET_PORT, SPI_RESET_PIN);
    DDL_DelayMS(200);
    GPIO_SetPins(SPI_RESET_PORT, SPI_RESET_PIN);
    DDL_DelayMS(200);
}

uint8_t SPI1_SendByte(uint8_t byte)
{
	 uint8_t pRxData;
   SPI_TransReceive(SPI_UNIT,&byte,&pRxData,1,0xFF);
	
	 return pRxData;
}

uint8_t w5100s_spi_readbyte(void)
{
    return SPI1_SendByte(0x00);
}

void w5100s_spi_writebyte(uint8_t wb)
{
    SPI1_SendByte(wb);

}

void SPI_WriteDatas(uint8_t * data, uint16_t len)
{
  SPI_Trans(SPI_UNIT,data,len,0xFF);
}
void SPI_ReadDatas(uint8_t * data, uint16_t len)
{
  SPI_Receive(SPI_UNIT,data,len,0xFF);
}

void rdr_ip_conflict(void)
{
    TRACE("CONFLICT IP from DHCP\r\n");
    //halt or reset or any...
    system_reset();
}

wiz_NetInfo gNetInfo;
void rdr_ip_assign(void)
{
    TRACE("rdr_ip_assign -------------------------------------------\n");
    getIPfromDHCP(gNetInfo.ip);
    getGWfromDHCP(gNetInfo.gw);
    getSNfromDHCP(gNetInfo.sn);
    getDNSfromDHCP(gNetInfo.dns);
    /* Network initialization */
    gNetInfo.dhcp = NETINFO_DHCP;
    ctlnetwork(CN_SET_NETINFO, (void*)&gNetInfo);
    w5100s_network_info_show();
    TRACE("DHCP LEASED TIME : %d Sec.\r\n", getDHCPLeasetime());
}


extern const uint8 DefIpAddr[];
extern const uint8 DefSubnetMask[];
extern const uint8 DefGateWay[];
extern const uint8 DefDnsServer[];
extern const uint8 DefMac[];

void set_default_ip(void)
{
    memcpy(gNetInfo.ip, DefIpAddr, 4);
    memcpy(gNetInfo.sn, DefSubnetMask, 4);
    memcpy(gNetInfo.gw, DefGateWay, 4);
    memcpy(gNetInfo.dns, DefDnsServer, 4);
    /*
    getIPfromDHCP(netInfo.ip);
    getGWfromDHCP(netInfo.gw);
    getSNfromDHCP(netInfo.sn);
    getDNSfromDHCP(netInfo.dns);
    */
    /* Network initialization */
    gNetInfo.dhcp = NETINFO_STATIC;
    ctlnetwork(CN_SET_NETINFO, (void*)&gNetInfo);
    w5100s_network_info_show();
}

int gIsConfDhcp = 0;
int gMaxAvailableSocket = _WIZCHIP_SOCK_NUM_;
networkParaConfig gNetConf;

/* v9.81ci: hw_api access */
networkParaConfig *hw_netconf(void) { return &gNetConf; }

int w5100s_network_info_init(int dhcpsn)
{
    uint8 *dhcpbuf;

    gIsConfDhcp = is_netconf_dhcp(&gNetConf);

    if (gIsConfDhcp < 0)
    {
        TRACE("get_network_config failed");
        return -1;
    }

    memcpy(gNetInfo.mac, gNetConf.mac, 6);
    memcpy(gNetInfo.dns, gNetConf.dnsServer, 4);
    memcpy(gNetInfo.ip, gNetConf.ip, 4);
    memcpy(gNetInfo.sn, gNetConf.subnetMask, 4);
    memcpy(gNetInfo.gw, gNetConf.gatewayIP, 4);

    if (gIsConfDhcp == 1)
    {
        gNetInfo.dhcp = NETINFO_DHCP;
        setSHAR(gNetInfo.mac);
        TRACE("dhcp mac:%02X-%02X-%02X-%02X-%02X-%02X\n", gNetInfo.mac[0],
              gNetInfo.mac[1], gNetInfo.mac[2], gNetInfo.mac[3], gNetInfo.mac[4],
              gNetInfo.mac[5]);
        dhcpbuf = malloc_hexp(1024);

        if (dhcpbuf == NULL)
        {
            TRACE("get_network_config dhcpbuf == NULL");
            return -1;
        }

        if (dhcpsn == -1)
        {
            dhcpsn = getMaxSocketId();
        }

        if (dhcpsn < 0)
        {
            TRACE("dhcp: no free socket, skip dhcp\n");
            free_hexp(dhcpbuf);
            return -1;
        }

        DHCP_init(dhcpsn, dhcpbuf);
        reg_dhcp_cbfunc(rdr_ip_assign, rdr_ip_conflict, rdr_ip_conflict);
    }
    else
    {
        gNetInfo.dhcp = NETINFO_STATIC;
        wizchip_setnetinfo(&gNetInfo);

    }

    return 0;
}

extern volatile int gIsDhcpIpSet;
void wait_fin_init(void)
{
    if (gIsConfDhcp == 1)
    {
        while(1)
        {
            sleep_ms(50);

            if (gIsDhcpIpSet == 1)
                break;
        }
    }
}



void w5100s_network_info_show(void)
{
    wiz_NetInfo info;

    wizchip_getnetinfo(&info);

    TRACE("w5500 network infomation:\r\n");
    TRACE("  -mac:%d:%d:%d:%d:%d:%d\r\n", info.mac[0], info.mac[1], info.mac[2],
          info.mac[3], info.mac[4], info.mac[5]);
    TRACE("  -ip:%d.%d.%d.%d\r\n", info.ip[0], info.ip[1], info.ip[2], info.ip[3]);
    TRACE("  -sn:%d.%d.%d.%d\r\n", info.sn[0], info.sn[1], info.sn[2], info.sn[3]);
    TRACE("  -gw:%d.%d.%d.%d\r\n", info.gw[0], info.gw[1], info.gw[2], info.gw[3]);
    TRACE("  -dns:%d.%d.%d.%d\r\n", info.dns[0], info.dns[1], info.dns[2], info.dns[3]);

    if (info.dhcp == NETINFO_DHCP)
    {
        TRACE("  -dhcp_mode: dhcp\r\n");
    }
    else
    {
        TRACE("  -dhcp_mode: static\r\n");
    }
}

#if IS_RTOS2_SUPPORT
osMutexId_t mutex_cris_id;
osRtxMutex_t gWizportmux_cb;
void cris_mutex_en(void)
{
    osStatus_t  status;
    status  = osMutexAcquire(mutex_cris_id, osWaitForever);

    if (status != osOK)
        TRACE("osMutexAcquire error:%d\n", status);
}

void cris_mutex_ex(void)
{
    osStatus_t  status;
    status  = osMutexRelease(mutex_cris_id);

    if (status != osOK)
        TRACE("osMutexRelease error:%d\n", status);
}

#endif

Spi_Ex_Code detect_spi_ex_dev(void)
{
    uint8_t mac[6] = {0};
    WIZ_SPI_Init();
    Reset_W5100S();
    reg_wizchip_cs_cbfunc(w5100s_cs_select, w5100s_cs_deselect);
    reg_wizchip_spi_cbfunc(w5100s_spi_readbyte, w5100s_spi_writebyte);
		reg_wizchip_spiburst_cbfunc(SPI_ReadDatas,SPI_WriteDatas);
    setSHAR((uint8_t *)DefMac);
    getSHAR(mac);

    if (memcmp(DefMac, mac, 6) == 0)
    {
        TRACE("find w5100s\n");
        return Spi_Ex_Ethernet;
    }
    else
    {
        TRACE("find no spi ex dev\n");
        return Spi_Ex_None;
    }
}

commonSocketPara gSocketParams[_WIZCHIP_SOCK_NUM_];

int network_init(int dhcpsn)
{
    int i;
    #if IS_RTOS2_SUPPORT
    osMutexAttr_t Mutex_attr =
    {
        "wizchip",
        osMutexRecursive | osMutexPrioInherit,
        &gWizportmux_cb,
        sizeof(gWizportmux_cb)
    };
    mutex_cris_id = osMutexNew(&Mutex_attr);

    if (mutex_cris_id == NULL)
    {
        TRACE("network_init osMutexNew failed");
        return -1;
    }

    #endif
    WIZ_SPI_Init();
    Reset_W5100S();
    reg_wizchip_cs_cbfunc(w5100s_cs_select, w5100s_cs_deselect);
    reg_wizchip_spi_cbfunc(w5100s_spi_readbyte, w5100s_spi_writebyte);
    reg_wizchip_spiburst_cbfunc(SPI_ReadDatas,SPI_WriteDatas);
    #if IS_RTOS2_SUPPORT
    reg_wizchip_cris_cbfunc(cris_mutex_en, cris_mutex_ex);
    #endif

    if (wizchip_init(NULL, NULL) != 0)
    {
        TRACE("network_init error\n");
        return -1;
    }
    else
        TRACE("network_init success !\n");

    w5100s_network_info_init(dhcpsn);
    w5100s_network_info_show();

    for (i = 0; i < _WIZCHIP_SOCK_NUM_; ++i)
    {
        gSocketParams[i].isBlock = O_BLOCK;
        gSocketParams[i].timeout = -1;
        close(i);
    }

    return 0;
}

extern volatile int gIsRunDnsTimeHandler;
void pre_DNS_init()
{
    gIsRunDnsTimeHandler = 1;
}

void aft_DNS_run()
{
    gIsRunDnsTimeHandler = 0;
}

void wiz_wait_link_on(void)
{
    uint8 tmp;
    int wcnt = 0;

    do
    {
        sleep_ms(100);

        if(ctlwizchip(CW_GET_PHYLINK, &tmp) == -1)
            printf("Unknown PHY Link stauts.\r\n");

        wcnt++;

        if (wcnt == 200)
            break;
    }
    while(tmp == PHY_LINK_OFF);

    sleep_ms(500);
}

int get_network_info(networkParaConfig *para)
{
    wiz_NetInfo info;
    wizchip_getnetinfo(&info);
    memcpy(para->ip, info.ip, 4);
    memcpy(para->subnetMask, info.sn, 4);
    memcpy(para->gatewayIP, info.gw, 4);
    memcpy(para->dnsServer, info.dns, 4);
    return 0;
}

