/**
  ******************************************************************************
  * @file    W7500x Serial to Ethernet Project - WIZ750SR App
  * @author  Eric Jung, Team Wiki
  * @version V0.8.0
  * @date    Mar-2016
  * @brief   Main program body
  ******************************************************************************
  * @attention
  * @par Revision history
  *    <2015/11/24> v0.0.1 Develop by Eric Jung
  *    <2016/03/02> v0.8.0 Develop by Eric Jung
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, WIZnet SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2016 WIZnet Co., Ltd.</center></h2>
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "W7500x_gpio.h"
#include "W7500x_uart.h"
#include "W7500x_crg.h"
#include "W7500x_wztoe.h"
#include "W7500x_miim.h"

#include "common.h"
#include "W7500x_board.h"

#include "dhcp.h"
#include "dhcp_cb.h"
#include "dns.h"

#include "seg.h"
#include "segcp.h"
#include "configData.h"

#include "timerHandler.h"
#include "deviceHandler.h"
#include "flashHandler.h"
#include "uartHandler.h"
#include "gpioHandler.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
//#define _MAIN_DEBUG_	// debugging message enable

/* Private function prototypes -----------------------------------------------*/
static void W7500x_Init(void);
static void W7500x_WZTOE_Init(void);
int8_t process_dhcp(void);
int8_t process_dns(void);

// Debug messages
void display_Dev_Info_header(void);
void display_Dev_Info_main(void);
void display_Dev_Info_dhcp(void);
void display_Dev_Info_dns(void);

void delay(__IO uint32_t milliseconds); //Notice: used ioLibray
void TimingDelay_Decrement(void);

/* Private variables ---------------------------------------------------------*/
static __IO uint32_t TimingDelay;

/* Public variables ---------------------------------------------------------*/
// Shared buffer declaration
uint8_t g_send_buf[DATA_BUF_SIZE];
uint8_t g_recv_buf[DATA_BUF_SIZE];

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
	DevConfig *dev_config = get_DevConfig_pointer();
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// W7500x Hardware Initialize
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	/* W7500x MCU Initialization */
	W7500x_Init(); // includes UART2 initialize code for print out debugging messages
	
	/* W7500x WZTOE (Hardwired TCP/IP stack) Initialization */
	W7500x_WZTOE_Init();
	
	/* W7500x Board Initialization */
	W7500x_Board_Init();
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// W7500x Application: Initialize
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	/* Load the Configuration data */
	load_DevConfig_from_storage();
	
	/* Set the MAC address to WIZCHIP */
	Mac_Conf();
	
	/* UART Initialization */
	S2E_UART_Configuration();
	
	/* GPIO Initialization*/
	IO_Configuration();
	
	if(dev_config->serial_info[0].serial_debug_en)
	{
		// Debug UART: Device information print out
		display_Dev_Info_header();
		display_Dev_Info_main();
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// W7500x Application: DHCP client / DNS client handler
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	/* Network Configuration - DHCP client */
	// Initialize Network Information: DHCP or Static IP allocation
	if(dev_config->options.dhcp_use)
	{
		if(process_dhcp() == DHCP_IP_LEASED) // DHCP success
		{
			flag_process_dhcp_success = ON;
		}
		else // DHCP failed
		{
			Net_Conf(); // Set default static IP settings
		}
	}
	else
	{
		Net_Conf(); // Set default static IP settings
	}
	
	// Debug UART: Network information print out (includes DHCP IP allocation result)
	if(dev_config->serial_info[0].serial_debug_en)
	{
		display_Net_Info();
		display_Dev_Info_dhcp();
	}
	
	/* DNS client */
	//if((value->network_info[0].working_mode == TCP_CLIENT_MODE) || (value->network_info[0].working_mode == TCP_MIXED_MODE))
	if(dev_config->network_info[0].working_mode != TCP_SERVER_MODE)
	{
		if(dev_config->options.dns_use) 
		{
			if(process_dns()) // DNS success
			{
				flag_process_dns_success = ON;
			}
		}
	}
	
	// Debug UART: DNS results print out
	if(dev_config->serial_info[0].serial_debug_en)
	{
		display_Dev_Info_dns();
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// W7500x Application: Main Routine
	////////////////////////////////////////////////////////////////////////////////////////////////////

	flag_s2e_application_running = ON;
	
	// HW_TRIG switch ON
	if(flag_hw_trig_enable)
	{
		init_trigger_modeswitch(DEVICE_AT_MODE);
		flag_hw_trig_enable = 0;
	}
	
	while(1) // main loop
	{
		do_segcp();
		do_seg(SOCK_DATA);
		
		if(dev_config->options.dhcp_use) DHCP_run(); // DHCP client handler for IP renewal
		
		// ## debugging: PHY link
		if(flag_check_phylink)
		{
			//printf("PHY Link status: %x\r\n", GPIO_ReadInputDataBit(PHYLINK_IN_PORT, PHYLINK_IN_PIN));
			flag_check_phylink = 0;	// flag clear
		}
		
		// ## debugging: Ring buffer full
		if(flag_ringbuf_full)
		{
			if(dev_config->serial_info[0].serial_debug_en) printf(" > UART Rx Ring buffer Full\r\n");
			flag_ringbuf_full = 0;
		}
	} // End of application main loop
} // End of main

/*****************************************************************************
 * Private functions
 ****************************************************************************/
static void W7500x_Init(void)
{
	////////////////////////////////////////////////////
	// W7500x MCU Initialize
	////////////////////////////////////////////////////
	
	/* External Clock */
	CRG_PLL_InputFrequencySelect(CRG_OCLK);
	
	/* Set system clock setting: 48MHz */
	*(volatile uint32_t *)(0x41001014) = 0x0060100; //clock setting 48MHz
	
	/* Set Systme init */
	SystemInit();
	
	/* DualTimer Initialization */
	Timer_Configuration();
	
	/* Simple UART init for Debugging */
	UART2_Configuration();
	
	/* SysTick_Config */
	SysTick_Config((GetSystemClock()/1000));
	
#ifdef _MAIN_DEBUG_
	printf("\r\n GetSystemClock : %d (Hz) \r\n", GetSystemClock());  
#endif
}

static void W7500x_WZTOE_Init(void)
{
	////////////////////////////////////////////////////
	// W7500x WZTOE (Hardwired TCP/IP core) Initialize
	////////////////////////////////////////////////////
	
	/* Set Network Configuration: HW Socket Tx/Rx buffer size */
	uint8_t tx_size[8] = { 4, 2, 2, 2, 2, 2, 2, 0 }; // default: { 2, 2, 2, 2, 2, 2, 2, 2 }
	uint8_t rx_size[8] = { 4, 2, 2, 2, 2, 2, 2, 0 }; // default: { 2, 2, 2, 2, 2, 2, 2, 2 }
	
	/* Structure for TCP timeout control: RTR, RCR */
	wiz_NetTimeout * net_timeout;
	
#ifdef _MAIN_DEBUG_
	uint8_t i;
#endif
	
	/* Set WZ_100US Register */
	setTIC100US((GetSystemClock()/10000));
#ifdef _MAIN_DEBUG_
	printf(" GetSystemClock: %X, getTIC100US: %X, (%X) \r\n", GetSystemClock(), getTIC100US(), *(uint32_t *)WZTOE_TIC100US); // for debugging
#endif
	/* Set TCP Timeout: retry count / timeout val */
	// Retry count default: [8], Timeout val default: [2000]
	net_timeout->retry_cnt = 8;
	net_timeout->time_100us = 2500;
	wizchip_settimeout(net_timeout);
	
#ifdef _MAIN_DEBUG_
	wizchip_gettimeout(net_timeout); // TCP timeout settings
	printf(" Network Timeout Settings - RCR: %d, RTR: %dms\r\n", net_timeout->retry_cnt, net_timeout->time_100us);
#endif
	
	/* Set Network Configuration */
	wizchip_init(tx_size, rx_size);
	
#ifdef _MAIN_DEBUG_
	printf(" WZTOE H/W Socket Buffer Settings (kB)\r\n");
	printf(" [Tx] ");
	for(i = 0; i < _WIZCHIP_SOCK_NUM_; i++) printf("%d ", getSn_TXBUF_SIZE(i));
	printf("\r\n");
	printf(" [Rx] ");
	for(i = 0; i < _WIZCHIP_SOCK_NUM_; i++) printf("%d ", getSn_RXBUF_SIZE(i));
	printf("\r\n");
#endif
}

int8_t process_dhcp(void)
{
	uint8_t ret = 0;
	uint8_t dhcp_retry = 0;

#ifdef _MAIN_DEBUG_
	printf(" - DHCP Client running\r\n");
#endif
	DHCP_init(SOCK_DHCP, g_send_buf);
	reg_dhcp_cbfunc(w7500x_dhcp_assign, w7500x_dhcp_assign, w7500x_dhcp_conflict);
	
	set_device_status(ST_UPGRADE);
	
	while(1)
	{
		ret = DHCP_run();
		
		if(ret == DHCP_IP_LEASED)
		{
#ifdef _MAIN_DEBUG_
			printf(" - DHCP Success\r\n");
#endif
			break;
		}
		else if(ret == DHCP_FAILED)
		{
			dhcp_retry++;
#ifdef _MAIN_DEBUG_
			if(dhcp_retry <= 3) printf(" - DHCP Timeout occurred and retry [%d]\r\n", dhcp_retry);
#endif
		}

		if(dhcp_retry > 3)
		{
#ifdef _MAIN_DEBUG_
			printf(" - DHCP Failed\r\n\r\n");
#endif
			DHCP_stop();
			break;
		}

		do_segcp(); // Process the requests of configuration tool during the DHCP client run.
	}
	
	set_device_status(ST_OPEN);
	
	return ret;
}


int8_t process_dns(void)
{
	DevConfig *dev_config = get_DevConfig_pointer();
	int8_t ret = 0;
	uint8_t dns_retry = 0;
	uint8_t dns_server_ip[4];
	
#ifdef _MAIN_DEBUG_
	printf(" - DNS Client running\r\n");
#endif
	
	DNS_init(SOCK_DNS, g_send_buf);
	
	dns_server_ip[0] = dev_config->options.dns_server_ip[0];
	dns_server_ip[1] = dev_config->options.dns_server_ip[1];
	dns_server_ip[2] = dev_config->options.dns_server_ip[2];
	dns_server_ip[3] = dev_config->options.dns_server_ip[3];
	
	set_device_status(ST_UPGRADE);
	
	while(1) 
	{
		if((ret = DNS_run(dns_server_ip, (uint8_t *)dev_config->options.dns_domain_name, dev_config->network_info[0].remote_ip)) == 1)
		{
#ifdef _MAIN_DEBUG_
			printf(" - DNS Success\r\n");
#endif
			break;
		}
		else
		{
			dns_retry++;
#ifdef _MAIN_DEBUG_
			if(dns_retry <= 2) printf(" - DNS Timeout occurred and retry [%d]\r\n", dns_retry);
#endif
		}

		if(dns_retry > 2) {
#ifdef _MAIN_DEBUG_
			printf(" - DNS Failed\r\n\r\n");
#endif
			break;
		}

		do_segcp(); // Process the requests of configuration tool during the DNS client run.

		if(dev_config->options.dhcp_use) DHCP_run();
	}
	
	set_device_status(ST_OPEN);
	return ret;
}

void display_Dev_Info_header(void)
{
	DevConfig *dev_config = get_DevConfig_pointer();

	printf("\r\n");
	printf("%s\r\n", STR_BAR);
#ifndef __W7500P__
	printf(" [W7500] Serial to Ethernet Device\r\n");
#else
	printf(" [W7500P] Serial to Ethernet Device\r\n");
#endif
	printf(" Firmware version: %d.%d.%d %s\r\n", dev_config->fw_ver[0], dev_config->fw_ver[1], dev_config->fw_ver[2], STR_VERSION_STATUS);
	printf("%s\r\n", STR_BAR);
}


void display_Dev_Info_main(void)
{
	DevConfig *dev_config = get_DevConfig_pointer();

	printf(" - Device name: %s\r\n", dev_config->module_name);
	printf(" - Device mode: %s\r\n", str_working[dev_config->network_info[0].working_mode]);
	
	printf(" - Network settings: \r\n");
	printf("\t- IP assign: %s\r\n", (dev_config->options.dhcp_use == 1)?"DHCP":"Static");
	
	printf(" - Search ID code: \r\n");
	printf("\t- %s: [%s]\r\n", (dev_config->options.pw_search[0] != 0)?"Enabled":"Disabled", (dev_config->options.pw_search[0] != 0)?dev_config->options.pw_search:"None");
	
	printf(" - Ethernet connection password: \r\n");
	printf("\t- %s %s\r\n", (dev_config->options.pw_connect_en == 1)?"Enabled":"Disabled", "(TCP server / mixed mode only)");
	
	printf(" - Serial settings: \r\n");
		printf("\t- %d-", baud_table[dev_config->serial_info[0].baud_rate]);
		printf("%d-", word_len_table[dev_config->serial_info[0].data_bits]);
		printf("%s-", parity_table[dev_config->serial_info[0].parity]);
		printf("%d / ", stop_bit_table[dev_config->serial_info[0].stop_bits]);
		printf("Flow Control: %s\r\n", flow_ctrl_table[dev_config->serial_info[0].flow_control]);

	printf(" - Serial data packing options:\r\n");
		printf("\t- Time: ");
			if(dev_config->network_info[0].packing_time) printf("%d (ms)\r\n", dev_config->network_info[0].packing_time);
			else printf("%s\r\n", STR_DISABLED);
		printf("\t- Size: ");
			if(dev_config->network_info[0].packing_size) printf("%d (bytes)\r\n", dev_config->network_info[0].packing_size);
			else printf("%s\r\n", STR_DISABLED);
		printf("\t- Char: ");
			if(dev_config->network_info[0].packing_delimiter_length == 1) printf("[%.2X] (Hex only)\r\n", dev_config->network_info[0].packing_delimiter[0]);
			else printf("%s\r\n", STR_DISABLED);
		
		printf(" - Serial command mode swtich code:\r\n");
		printf("\t- %s\r\n", (dev_config->options.serial_command == 1)?STR_ENABLED:STR_DISABLED);
		printf("\t- [%.2X][%.2X][%.2X] (Hex only)\r\n", dev_config->options.serial_trigger[0], dev_config->options.serial_trigger[1], dev_config->options.serial_trigger[2]);

	printf(" - Hardware informations:\r\n");
		printf("\t- %s-Data: %s%d\r\n", STR_UART, STR_UART, SEG_DATA_UART);
		printf("\t- %s-Debug: %s%d\r\n", STR_UART, STR_UART, SEG_DEBUG_UART);
		printf("\t- Status pin1: %s (PHY link)\r\n", "PA_10");
		printf("\t- Status pin2: %s (TCP connection)\r\n", "PA_01"); // shared pin; HW_TRIG (input) / TCP connection indicator (output)
	printf("%s\r\n", STR_BAR);
}


void display_Dev_Info_dhcp(void)
{
	DevConfig *dev_config = get_DevConfig_pointer();
	
	if(dev_config->options.dhcp_use) 
	{
		if(flag_process_dhcp_success == ON) printf(" # DHCP IP Leased time : %u seconds\r\n", getDHCPLeasetime());
		else printf(" # DHCP Failed\r\n");
	}
}


void display_Dev_Info_dns(void)
{
	DevConfig *dev_config = get_DevConfig_pointer();
	
	if(dev_config->options.dns_use) 
	{
		if(flag_process_dns_success == ON)
		{
			printf(" # DNS: %s => %d.%d.%d.%d : %d\r\n", dev_config->options.dns_domain_name, 
														dev_config->network_info[0].remote_ip[0],
														dev_config->network_info[0].remote_ip[1],
														dev_config->network_info[0].remote_ip[2],
														dev_config->network_info[0].remote_ip[3],
														dev_config->network_info[0].remote_port);
		}
		else printf(" # DNS Failed\r\n");
	}
	
	printf("\r\n");
}


/**
  * @brief  Inserts a delay time.
  * @param  nTime: specifies the delay time length, in milliseconds.
  * @retval None
  */
void delay(__IO uint32_t milliseconds)
{
	TimingDelay = milliseconds;
	while(TimingDelay != 0);
}


/**
  * @brief  Decrements the TimingDelay variable.
  * @param  None
  * @retval None
  */
void TimingDelay_Decrement(void)
{
	if(TimingDelay != 0x00)
	{
		TimingDelay--;
	}
}

