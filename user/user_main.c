/*
	Find public IP address of module and publish it on Thingspeak.
*/

#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <esp8266.h>
#include "user_interface.h"
#include "user_config.h"
#include "espconn.h"
#include "mem.h"
#include "uart_hw.h"
#include "stdout.h"
#include "wifi.h"
#include "debug.h"


extern int ets_uart_printf(const char *fmt, ...);
LOCAL struct espconn *pCon = NULL;
LOCAL struct espconn *tCon = NULL;
LOCAL char ext_ip_address[16];
LOCAL os_timer_t findip_timer;


static void ICACHE_FLASH_ATTR dyndns_findip_connect_cb(void *arg)
{
  espconn_connect(pCon);
  struct espconn *pespconn = (struct espconn *)arg;
  char payload[128]="";
  os_sprintf(payload, " \n"); // don't need to add "GET"!
  espconn_sent(pespconn,(uint8_t *)payload, strlen(payload));
}

static void ICACHE_FLASH_ATTR publish_thingspeak(void)
{
  espconn_connect(tCon);
}

static void ICACHE_FLASH_ATTR tConnectCb(void *arg)
{
	if (DEBUG) INFO("Connect callback on tconn %p\n", arg);
	struct espconn *tespconn = (struct espconn *)arg;
	char payload[138]="";
	char field1[16];
	os_sprintf(field1, "%s", ext_ip_address);
	if (DEBUG) INFO("field1=%s\n", field1);
	os_sprintf(payload, "GET /update?api_key=%s&field1=%s\n", THINGSPK_WKEY, field1);
	if (DEBUG) INFO("payload=%s\n", payload);
	espconn_sent(tespconn,(uint8_t *)payload, strlen(payload));
	if (arg==NULL) return;
}

static void ICACHE_FLASH_ATTR SentCb(void *arg)
{
	if (DEBUG) INFO("Sent callback on conn %p\n", arg);
	if (arg==NULL) return;
}

static void ICACHE_FLASH_ATTR RecvCb(void *arg, char *data, unsigned short len) {
	if (DEBUG) INFO("Recvd callback on conn %p\n", arg);
	if (arg==NULL) return;
	// Extract IP address from received data.
	int i,j,k=0;
	// skip headers
	for (i=150; i<len; i++) {
		if (data[i]=='s' && data[i+1]==':' && data[i+2]==' ') {
			for (j=i+3; j<len; j++) {
			  ext_ip_address[k]=data[j];
		      if (DEBUG) os_printf("%c", ext_ip_address[k]);
			  k++;
		      if (data[j+1] == '<')
		      break;
			}
		}
	}
	publish_thingspeak();
}

static void ICACHE_FLASH_ATTR tRecvCb(void *arg, char *data, unsigned short len) {
	if (DEBUG) INFO("Recvd callback on tconn %p\n", arg);
	if (arg==NULL) return;
}

static void ICACHE_FLASH_ATTR DisconCb(void *arg)
{
	if (DEBUG) {INFO("Disconnect callback on conn %p\n", arg);
				INFO("Free heap size:%d\n",system_get_free_heap_size());
	}
	if (arg==NULL) return;
}

static void ICACHE_FLASH_ATTR ReconCb(void *arg, sint8 err) {
	if (DEBUG) INFO("Reconnect callback on conn %p\n", arg);
	if (arg==NULL) return;
}

void ICACHE_FLASH_ATTR wifiConnectCb(uint8_t status)
{
	if(status == STATION_GOT_IP){
		char fail[10];
		int ret = 0;
		if (DEBUG) INFO("connecting...\r\n");

		pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
		if (pCon == NULL){
			INFO("pCon ALLOCATION FAIL\r\n");
			return;
		}
		else {if (DEBUG) INFO("pCon alloc OK\r\n");}
		pCon->type = ESPCONN_TCP;
		pCon->state = ESPCONN_NONE;
		pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
		pCon->proto.tcp->local_port = espconn_port();
		//set up the server remote port
		pCon->proto.tcp->remote_port = 80;
		//set up the remote IP
		uint32_t ip = ipaddr_addr("216.146.38.70"); //IP address for checkip.dyndns.com
		os_memcpy(pCon->proto.tcp->remote_ip, &ip, 4);
		//set up the local IP
		struct ip_info ipconfig;
		wifi_get_ip_info(STATION_IF, &ipconfig);
		os_memcpy(pCon->proto.tcp->local_ip, &ipconfig.ip, 4);
		espconn_regist_connectcb(pCon, dyndns_findip_connect_cb);
		espconn_regist_reconcb(pCon, ReconCb);
		espconn_regist_disconcb(pCon, DisconCb);
		espconn_regist_sentcb(pCon, SentCb);
		espconn_regist_recvcb(pCon, RecvCb);

		tCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
		if (tCon == NULL){
			INFO("tCon ALLOCATION FAIL\r\n");
			return;
		}
		else {if (DEBUG) INFO("tCon alloc OK\r\n");}
		tCon->type = ESPCONN_TCP;
		tCon->state = ESPCONN_NONE;
		tCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
		tCon->proto.tcp->local_port = espconn_port();
		//set up the server remote port
		tCon->proto.tcp->remote_port = 80;
		//set up the remote IP
		uint32_t t_ip = ipaddr_addr("184.106.153.149"); //IP address for thingspeak.com
		os_memcpy(tCon->proto.tcp->remote_ip, &t_ip, 4);
		espconn_regist_connectcb(tCon, tConnectCb);
		espconn_regist_reconcb(tCon, ReconCb);
		espconn_regist_disconcb(tCon, DisconCb);
		espconn_regist_sentcb(tCon, SentCb);
		espconn_regist_recvcb(tCon, tRecvCb);

		ret = espconn_connect(pCon);
		if(ret == 0){
			if (DEBUG) INFO("espconn_connect OK!\r\n");
		}
		else {
		  INFO("espconn_connect FAILED!\r\n");
		  os_sprintf(fail, "%d \r\n", ret);
		  INFO(fail);
		  //clean up allocated memory
		  if(pCon->proto.tcp)
			  os_free(pCon->proto.tcp);
		  os_free(pCon);
		  pCon = NULL;
		}
	}
}

void user_rf_pre_init(void)
{
}

void user_init(void)
{
	// Configure the UART
	stdoutInit();
	os_delay_us(100000);
	//Disarm, Setup function, Arm timer...
	os_timer_disarm(&findip_timer);
	os_timer_setfn(&findip_timer, (os_timer_func_t *)dyndns_findip_connect_cb, (void *)0);
	os_timer_arm(&findip_timer, 600000, 1);  //Every 10 minutes says Dyndns.
	WIFI_Connect(wifiConnectCb);
}



