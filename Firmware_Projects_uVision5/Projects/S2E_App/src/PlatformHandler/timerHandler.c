#include "W7500x_dualtimer.h"
#include "common.h"
#include "W7500x_board.h"
#include "timerHandler.h"
#include "seg.h"
#include "segcp.h"
#include "deviceHandler.h"
#include "gpioHandler.h"

#include "dhcp.h"
#include "dns.h"

static volatile uint16_t msec_cnt = 0;
static volatile uint16_t sec_cnt = 0;
static volatile uint32_t uptime_min = 0;

static uint8_t enable_phylink_check = 1;
static volatile uint32_t phylink_down_time_msec;

void Timer_Configuration(void)
{
	DUALTIMER_InitTypDef Dualtimer_InitStructure;
	
	NVIC_EnableIRQ(DUALTIMER0_IRQn);
	
	/* Dualtimer 0_0 clock enable */
	DUALTIMER_ClockEnable(DUALTIMER0_0);

	/* Dualtimer 0_0 configuration */
	Dualtimer_InitStructure.TimerLoad = 0x0000BB80;
	Dualtimer_InitStructure.TimerControl_Mode = DUALTIMER_TimerControl_Periodic;
	Dualtimer_InitStructure.TimerControl_OneShot = DUALTIMER_TimerControl_Wrapping;
	Dualtimer_InitStructure.TimerControl_Pre = DUALTIMER_TimerControl_Pre_1;
	Dualtimer_InitStructure.TimerControl_Size = DUALTIMER_TimerControl_Size_32;

	DUALTIMER_Init(DUALTIMER0_0, &Dualtimer_InitStructure);

	/* Dualtimer 0_0 Interrupt enable */
	DUALTIMER_IntConfig(DUALTIMER0_0, ENABLE);

	/* Dualtimer 0_0 start */
	DUALTIMER_Start(DUALTIMER0_0);
}

void Timer_IRQ_Handler(void)
{
	if(DUALTIMER_GetIntStatus(DUALTIMER0_0))
	{
		DUALTIMER_IntClear(DUALTIMER0_0);
		
		msec_cnt++; // millisecond counter
		
		seg_timer_msec();		// [msec] time counter for SEG (S2E)
		segcp_timer_msec();		// [msec] time counter for SEGCP (Config)
		device_timer_msec();	// [msec] time counter for DeviceHandler (fw update)
		
		if(enable_phylink_check) // will be modified
		{
			if(phylink_down_time_msec < 0xffffffff)	phylink_down_time_msec++;
			else									phylink_down_time_msec = 0;
		}
		
		if(flag_s2e_application_running)
		{
			gpio_handler_timer_msec();
		}
		
		/* Second Process */
		if((msec_cnt % 1000) == 0) 
		{
			msec_cnt = 0;
			sec_cnt++;
			
			seg_timer_sec(); // [sec] time counter for SEG
			
			DHCP_time_handler();	// Time counter for DHCP timeout
			DNS_time_handler();		// Time counter for DNS timeout
		}
		
		/* Minute Process */
		if((sec_cnt % 60) == 0) 
		{
			sec_cnt = 0;
			uptime_min++;
		}
	}

	if(DUALTIMER_GetIntStatus(DUALTIMER0_1))
	{
		DUALTIMER_IntClear(DUALTIMER0_1);
	}
}

uint32_t getDeviceUptime_min(void)
{
	return uptime_min;
}

uint16_t getDeviceUptime_sec(void)
{
	return sec_cnt;
}

uint16_t getDeviceUptime_msec(void)
{
	return msec_cnt;
}

void set_phylink_time_check(uint8_t enable)
{
	if(enable == 1) // start
	{
		phylink_down_time_msec = 0; // counter variable clear
	}
	else // stop
	{
		;
	}
	
	enable_phylink_check = enable;
}

uint32_t get_phylink_downtime(void)
{
	return phylink_down_time_msec;
}
