/**
  ******************************************************************************
  * @file    W7500x Serial to Ethernet Project - WIZ750SR Boot
  * @author  Eric Jung, Team Wiki
  * @version V0.8.0
  * @date    Mar-2016
  * @brief   Boot program body
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
#include <string.h>
#include "W7500x_gpio.h"
#include "W7500x_uart.h"
#include "W7500x_crg.h"
#include "W7500x_wztoe.h"
#include "W7500x_miim.h"

#include "common.h"
#include "W7500x_board.h"

#include "dhcp.h"
#include "dhcp_cb.h"

#include "segcp.h"
#include "configData.h"

#include "timerHandler.h"
#include "uartHandler.h"
#include "flashHandler.h"
#include "deviceHandler.h"


/* Private typedef -----------------------------------------------------------*/
typedef void (*pFunction)(void);

/* Private define ------------------------------------------------------------*/
//#define _MAIN_DEBUG_	// debugging message enable

// Define for Interrupt Vector Table Remap
#define BOOT_VEC_BACK_ADDR 		(DEVICE_APP_MAIN_ADDR - SECT_SIZE)

// Define for MAC address settings
#define MACSTR_SIZE		22

/* Private function prototypes -----------------------------------------------*/
void application_jump(uint32_t AppAddress);
uint8_t check_mac_address(void);

static void W7500x_Init(void);
static void W7500x_WZTOE_Init(void);
int8_t process_dhcp(void);

// Debug messages
void display_Dev_Info_header(void);
void display_Dev_Info_dhcp(void);

// Functions for Interrupt Vector Table Remap
void Copy_Interrupt_VectorTable(uint32_t start_addr);
void Backup_Boot_Interrupt_VectorTable(void);

// Delay
void delay(__IO uint32_t milliseconds); //Notice: used ioLibray
void TimingDelay_Decrement(void);

/* Private variables ---------------------------------------------------------*/
static __IO uint32_t TimingDelay;

/* Public variables ---------------------------------------------------------*/
// Shared buffer declaration
uint8_t g_send_buf[DATA_BUF_SIZE];
uint8_t g_recv_buf[DATA_BUF_SIZE];

uint8_t flag_s2e_application_running = OFF;
uint8_t flag_process_dhcp_success = OFF;

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
	DevConfig *dev_config = get_DevConfig_pointer();
	uint8_t appjump_enable = OFF;
	uint8_t ret = 0;
	//uint16_t i;
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// W7500x Hardware Initialize
	////////////////////////////////////////////////////////////////////////////////////////////////////	
	
	/* W7500x MCU Initialization */
	W7500x_Init(); // includes UART2 init code for debugging
	
	/* W7500x WZTOE (Hardwired TCP/IP stack) Initialization */
	W7500x_WZTOE_Init();
	
	/* W7500x Board Initialization */
	W7500x_Board_Init();
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// W7500x Application: Initialize part1
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	/* Load the Configuration data */
	load_DevConfig_from_storage();
	
	/* Set the device working mode: BOOT */
	dev_config->network_info[0].state = ST_BOOT;
	
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// W7500x Application: Check the MAC address and Firmware update
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	// 1. MAC address가 없는 경우				-> MAC 주소 입력 및 save
	
	if(check_mac_address()) appjump_enable = ON;
	
	// 2. Firmware update flag가 1인 경우 		-> 전체 영역 erase, Fwup_size만큼 fw update 수행, app backup (flash) >> app main (flash)
	
	if(dev_config->firmware_update.fwup_flag == 1)
	{
		// Firmware download has already been done at application routine.
		ret = device_firmware_update(STORAGE_APP_MAIN); // Firmware copy: [App backup] -> [App main]
		if(ret == DEVICE_FWUP_RET_SUCCESS)
		{
			dev_config->firmware_update.fwup_flag = SEGCP_DISABLE;
			dev_config->firmware_update.fwup_size = 0;
			save_DevConfig_to_storage();
			
			appjump_enable = ON;
		}
	}
	
	
	if ((*(uint32_t*)DEVICE_APP_MAIN_ADDR != 0xFFFFFFFF) && (*(uint32_t*)DEVICE_APP_BACKUP_ADDR != 0xFFFFFFFF))
	{
		appjump_enable = ON;
	}
	/*
	else if (*(uint32_t*)DEVICE_APP_BACKUP_ADDR == 0xFFFFFFFF) // genarate application backup storage for device restore
	{
		device_firmware_update(STORAGE_APP_BACKUP);
		appjump_enable = ON;
	}
	else if (*(uint32_t*)DEVICE_APP_MAIN_ADDR == 0xFFFFFFFF) // copy the backup to application main storage
	{
		device_firmware_update(STORAGE_APP_MAIN);
		appjump_enable = ON;
	}
	*/
	
//#ifdef _MAIN_DEBUG_
	if (*(uint32_t*)DEVICE_APP_MAIN_ADDR == 0xFFFFFFFF) 
	{
#ifdef _MAIN_DEBUG_
		printf("\r\n>> Application Main: Empty [0x%.8x], Jump Failed\r\n", DEVICE_APP_MAIN_ADDR);
#endif
		appjump_enable = OFF;
	}
	else
	{
#ifdef _MAIN_DEBUG_
		printf("\r\n>> Application Main: Detected [0x%.8x], Jump Start\r\n", DEVICE_APP_MAIN_ADDR);
#endif
		appjump_enable = ON;
	}
//#endif
	
	if(appjump_enable == ON)
	{
		// Copy the application code interrupt vector to 0x00000000
		//printf("\r\n copy the interrupt vector, app area [0x%.8x] ==> boot", DEVICE_APP_MAIN_ADDR);
		Copy_Interrupt_VectorTable(DEVICE_APP_MAIN_ADDR);
		
		application_jump(DEVICE_APP_MAIN_ADDR);
		
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// W7500x Application: Initialize part2
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	if(dev_config->serial_info[0].serial_debug_en)
	{
		// Debug UART: Device information print out
		display_Dev_Info_header();
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
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// W7500x Boot: Main Routine
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	flag_s2e_application_running = ON;
	LED_On(LED1);
	
	while(1) // main loop
	{
		do_segcp();
		
		if(dev_config->options.dhcp_use) DHCP_run(); // DHCP client handler for IP renewal
		
		if(flag_check_main_routine)
		{
			// Device working indicator: Boot
			// LEDs blink rapidly (100ms)
			LED_Toggle(LED1);
			LED_Toggle(LED2);
			flag_check_main_routine = 0;
		}
	} // End of application main loop
} // End of main

/*****************************************************************************
 * Private functions
 ****************************************************************************/
void application_jump(uint32_t AppAddress)
{
	pFunction Jump_To_Application;
	uint32_t JumpAddress;
	
	JumpAddress = *( uint32_t*)(AppAddress + 0x04);
	Jump_To_Application = (pFunction)JumpAddress;
	
	// Initialize user application's stack pointer
	__set_MSP(*(__IO uint32_t *)AppAddress);
	
	// Jump to application
	Jump_To_Application();
}


static void W7500x_Init(void)
{
	////////////////////////////////////////////////////
	// W7500x MCU Initialize
	////////////////////////////////////////////////////
	
	/* External Clock */
	CRG_PLL_InputFrequencySelect(CRG_OCLK);
	
	/* Set system clock setting: 48MHz */
	*(volatile uint32_t *)(0x41001014) = 0x0060100; //clock setting 48MHz
	//*(volatile uint32_t *)(0x41001014) = 0x000C0200; // 48MHz
	
	/* Set Systme init */
	SystemInit();
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// W7500x ISR: Interrupt Vector Table Remap (Custom)
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	if (*(uint32_t*)BOOT_VEC_BACK_ADDR == 0xFFFFFFFF) // after boot code first write
	{
		Backup_Boot_Interrupt_VectorTable();
	}
	else
	{
		Copy_Interrupt_VectorTable(BOOT_VEC_BACK_ADDR);
	}
	
	/* DualTimer Initialization */
	Timer_Configuration();
	
	/* UART Initialization */
	UART2_Configuration(); // Simple UART (UART2) for Debugging
	
	/* SysTick_Config */
	SysTick_Config((GetSystemClock()/1000));
	
#ifdef _MAIN_DEBUG_
	printf(" GetSystemClock : %d (Hz) \r\n", GetSystemClock());  
#endif
}

static void W7500x_WZTOE_Init(void)
{
	////////////////////////////////////////////////////
	// W7500x WZTOE (Hardwired TCP/IP core) Initialize
	////////////////////////////////////////////////////
	
	/* Set Network Configuration: HW Socket Tx/Rx buffer size */
	//uint8_t tx_size[8] = { 2, 2, 2, 2, 2, 2, 2, 2 };
	//uint8_t rx_size[8] = { 2, 2, 2, 2, 2, 2, 2, 2 };
		
	/* Structure for timeout control: RTR, RCR */
	wiz_NetTimeout * net_timeout;
	
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
	
	//wizchip_gettimeout(net_timeout); // ## for debugging - check the TCP timeout settings
#ifdef _MAIN_DEBUG_
	printf(" Network Timeout Settings - RTR: %d, RCR: %d\r\n", net_timeout->retry_cnt, net_timeout->time_100us);
#endif
	
	/* Set Network Configuration */
	//wizchip_init(tx_size, rx_size);
	
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
	printf(" Firmware version: Boot %x.%x.%x %s\r\n", dev_config->fw_ver[0], dev_config->fw_ver[1], dev_config->fw_ver[2], STR_VERSION_STATUS);
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


//////////////////////////////////////////////////////////////////////////////////
// Functions for MAC address 
//////////////////////////////////////////////////////////////////////////////////

uint8_t check_mac_address(void)
{
	DevConfig *dev_config = get_DevConfig_pointer();
	
	uint8_t i;
	uint8_t mac_buf[6] = {0, };
	uint8_t mac_str[MACSTR_SIZE] = {0, };
	uint8_t trep[MACSTR_SIZE] = {0, };
	uint8_t ret = 0;
	
	if(((dev_config->network_info_common.mac[0] != 0x00) || (dev_config->network_info_common.mac[1] != 0x08) || (dev_config->network_info_common.mac[2] != 0xDC))  ||
		((dev_config->network_info_common.mac[0] == 0xff) && (dev_config->network_info_common.mac[1] == 0xff) && (dev_config->network_info_common.mac[2] == 0xff)))
	{
		read_storage(STORAGE_MAC, 0, mac_buf, 0);
		
		//printf("Storage MAC: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\r\n", mac_buf[0], mac_buf[1], mac_buf[2], mac_buf[3], mac_buf[4], mac_buf[5]);
		
		// Initial stage: Input the MAC address
		if(((mac_buf[0] != 0x00) || (mac_buf[1] != 0x08) || (mac_buf[2] != 0xDC)) ||
			((mac_buf[0] == 0xff) && (mac_buf[1] == 0xff) && (mac_buf[2] == 0xff)))
		{
			gSEGCPPRIVILEGE = SEGCP_PRIVILEGE_CLR;
			gSEGCPPRIVILEGE = (SEGCP_PRIVILEGE_SET|SEGCP_PRIVILEGE_WRITE);
			
			while(1)
			{
				printf("INPUT FIRST MAC?\r\n");
				for(i = 0; i < MACSTR_SIZE-1; i++)
				{
					mac_str[i] = S_UartGetc();
					//S_UartPutc(mac_str[i]);
				}
				
				if(!proc_SEGCP(mac_str, trep))
				{
					// Save the MAC address to PRIVATE SPACE for MAC address only
					erase_storage(STORAGE_MAC);
					write_storage(STORAGE_MAC, 0, dev_config->network_info_common.mac, 0);
					
					// Set factory default
					set_DevConfig_to_factory_value();
					save_DevConfig_to_storage();
					ret = 1;
					break;
				}
				else
				{
					if(dev_config->serial_info[0].serial_debug_en) printf("%s\r\n", trep);
					memset(mac_str, 0x00, MACSTR_SIZE);
				}
			}
		}
		else // Lost the MAC address, MAC address restore
		{
			memcpy(dev_config->network_info_common.mac, mac_buf, 6);
			save_DevConfig_to_storage();
		}
		
	}
	else
	{
		ret = 1;
	}
	
	return ret;
}


//////////////////////////////////////////////////////////////////////////////////
// Functions for Interrupt Vector Table Remap
//////////////////////////////////////////////////////////////////////////////////
void Copy_Interrupt_VectorTable(uint32_t start_addr)
{
	uint32_t i;
	uint8_t flash_vector_area[SECT_SIZE];
	
	for (i = 0x00; i < 0x08; i++)			flash_vector_area[i] = *(volatile uint8_t *)(0x00000000+i);
	for (i = 0x08; i < 0xA8; i++) 			flash_vector_area[i] = *(volatile uint8_t *)(start_addr+i); // Actual address range; Interrupt vector table is located here
	for (i = 0xA8; i < SECT_SIZE; i++)	flash_vector_area[i] = *(volatile uint8_t *)(0x00000000+i);
	
	__disable_irq();
	
	DO_IAP(IAP_ERAS_SECT, 0x00000000, 0, 0); 						// Erase the interrupt vector table area : Sector 0
	DO_IAP(IAP_PROG, 0x00000000, flash_vector_area , SECT_SIZE);	// Write the applicaion vector table to 0x00000000
	
	__enable_irq();
}

void Backup_Boot_Interrupt_VectorTable(void)
{
	uint32_t i;
	uint8_t flash_vector_area[SECT_SIZE];
	for (i = 0; i < SECT_SIZE; i++)
	{
		flash_vector_area[i] = *(volatile uint8_t *)(0x00000000+i);
	}
	
	__disable_irq();
	
	DO_IAP(IAP_PROG, BOOT_VEC_BACK_ADDR, flash_vector_area , SECT_SIZE);
	
	__enable_irq();
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

