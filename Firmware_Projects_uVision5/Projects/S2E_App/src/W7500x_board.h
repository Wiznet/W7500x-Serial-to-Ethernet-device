/*
*
@file		W7500x_board.h
@brief
*/

#ifndef __W7500X_BOARD_H__ 
#define __W7500X_BOARD_H__ 

#include <stdint.h>
#include "common.h"

////////////////////////////////
// Product Configurations	  //
////////////////////////////////
/* Board Name */
#define DEVICE_BOARD_NAME	WIZwiki_W7500ECO
//#define DEVICE_BOARD_NAME	WIZ750SR

#ifdef DEVICE_BOARD_NAME
	#if (DEVICE_BOARD_NAME == WIZ750SR)
		#define __W7500P__
		#define DEVICE_ID_DEFAULT	"WIZ750SR" // W7500P Devices
	#else 
		#define DEVICE_ID_DEFAULT	"W7500-S2E" // WIZwiki_W7500 or WIZwiki_W7500ECO Board
	#endif
#else
	#define DEVICE_BOARD_NAME	UNKNOWN_DEVICE
	#define DEVICE_ID_DEFAULT	"UNKNOWN"
#endif

#define DEVICE_UART_CNT		1

/* Configuration Storage Selector */
// => Defalut: Internal Flash (DAT1)
//#define USE_EXT_EEPROM

// PHY init defines for WIZwiki-W7500 board series
#define __DEF_USED_MDIO__ 
#ifndef __W7500P__
	#define __DEF_USED_IC101AG__ // PHY initialize for WIZwiki-W7500 Board
#endif

////////////////////////////////
// Pin definitions			  //
////////////////////////////////
// PHY link status pin: Input (PHYLINK_IN_PIN -> STATUS_PHYLINK_PIN)
#define PHYLINK_IN_PIN				GPIO_Pin_9
#define PHYLINK_IN_PORT				GPIOA
#define PHYLINK_IN_PAD_AF			PAD_AF1 // PAD Config - LED used 2nd Function

// Connection status indicator pins
// Direction: Output
#define STATUS_PHYLINK_PIN			GPIO_Pin_10
#define STATUS_PHYLINK_PORT			GPIOA
#define STATUS_PHYLINK_PAD_AF		PAD_AF1

#define STATUS_TCPCONNECT_PIN		GPIO_Pin_1
#define STATUS_TCPCONNECT_PORT		STATUS_PHYLINK_PORT
#define STATUS_TCPCONNECT_PAD_AF	STATUS_PHYLINK_PAD_AF

// HW_TRIG - Command mode switch enable pin
// Direction: Input (Shared pin with TCP connection status pin)
#define HW_TRIG_PIN					STATUS_TCPCONNECT_PIN
#define HW_TRIG_PORT				STATUS_TCPCONNECT_PORT
#define HW_TRIG_PAD_AF				STATUS_TCPCONNECT_PAD_AF


/* To be developed: DTR / DSR */

// DTR / DSR - Handshaking signals, Shared with PHYLINK_PIN and TCPCONNECT_PIN (selectable)
// > DTR - Data Terminal Ready, Direction: Output (= PHYLINK_PIN)
// 		>> This signal pin asserted when the device could be possible to transmit the UART inputs
// 		>> [O], After boot and initialize
// 		>> [X], nope (E.g., TCP connected (Server / client mode) or TCP mixed mode or UDP mode)
// > DSR - Data Set Ready, Direction: Input (= TCPCONNECT_PIN)
// 		>> [O] Ethet_to_UART() function control

#define DTR_PIN						STATUS_PHYLINK_PIN
#define DTR_PORT					STATUS_PHYLINK_PORT
#define DTR_PAD_AF					STATUS_PHYLINK_PAD_AF

#define DSR_PIN						STATUS_TCPCONNECT_PIN
#define DSR_PORT					STATUS_TCPCONNECT_PORT
#define DSR_PAD_AF					STATUS_TCPCONNECT_PAD_AF

// Expansion GPIOs (4-Pins, GPIO A / B / C / D)
//#define MAX_USER_IOn    16
#define USER_IOn       4
#define USER_IO_A      (uint16_t)(0x01 <<  0)     // USER's I/O A
#define USER_IO_B      (uint16_t)(0x01 <<  1)     // USER's I/O B
#define USER_IO_C      (uint16_t)(0x01 <<  2)     // USER's I/O C
#define USER_IO_D      (uint16_t)(0x01 <<  3)     // USER's I/O D

#define USER_IO_DEFAULT_PAD_AF		PAD_AF1 // [2nd] GPIO
#define USER_IO_AIN_PAD_AF			PAD_AF3 // [4th] AIN

#define USER_IO_NO_ADC				0xff

/*
// USER IO pins for WIZ750SR, 
#define USER_IO_A_PIN				GPIO_Pin_13 // ECO: P28, AIN2
#define USER_IO_A_PORT				GPIOC
#define USER_IO_A_ADC_CH			ADC_CH2;

#define USER_IO_B_PIN				GPIO_Pin_12 // ECO: P27, AIN3
#define USER_IO_B_PORT				GPIOC
#define USER_IO_B_ADC_CH			ADC_CH3;

#define USER_IO_C_PIN				GPIO_Pin_9 // ECO: P26, AIN6
#define USER_IO_C_PORT				GPIOC
#define USER_IO_C_ADC_CH			ADC_CH6;

#define USER_IO_D_PIN				GPIO_Pin_8 // ECO: P25, AIN7
#define USER_IO_D_PORT				GPIOC
#define USER_IO_D_ADC_CH			ADC_CH7;
*/

// Pins for Test (sample board)
#define USER_IO_A_PIN				GPIO_Pin_5
#define USER_IO_A_PORT				GPIOA
#define USER_IO_A_ADC_CH			USER_IO_NO_ADC

#define USER_IO_B_PIN				GPIO_Pin_6
#define USER_IO_B_PORT				GPIOA
#define USER_IO_B_ADC_CH			USER_IO_NO_ADC

#define USER_IO_C_PIN				GPIO_Pin_7
#define USER_IO_C_PORT				GPIOA
#define USER_IO_C_ADC_CH			USER_IO_NO_ADC

#define USER_IO_D_PIN				GPIO_Pin_8
#define USER_IO_D_PORT				GPIOA
#define USER_IO_D_ADC_CH			USER_IO_NO_ADC

// Status LEDs define
#if (DEVICE_BOARD_NAME == WIZ750SR)

	#define LED1_PIN			GPIO_Pin_2
	#define LED1_GPIO_PORT		GPIOB
	#define LED1_GPIO_PAD		PAD_PB
	#define LED1_GPIO_PAD_AF	PAD_AF1		// PAD Config - LED used 2nd Function

	#define LED2_PIN			GPIO_Pin_3
	#define LED2_GPIO_PORT		GPIOB
	#define LED2_GPIO_PAD		PAD_PB
	#define LED2_GPIO_PAD_AF	PAD_AF1

#elif (DEVICE_BOARD_NAME == WIZwiki_W7500ECO)

	#define LED1_PIN			GPIO_Pin_1
	#define LED1_GPIO_PORT		GPIOA
	#define LED1_GPIO_PAD		PAD_PA
	#define LED1_GPIO_PAD_AF	PAD_AF1		// PAD Config - LED used 2nd Function

	#define LED2_PIN			GPIO_Pin_2
	#define LED2_GPIO_PORT		GPIOA
	#define LED2_GPIO_PAD		PAD_PA
	#define LED2_GPIO_PAD_AF	PAD_AF1

#else // WIZwiki-W7500 board

		// [RGB LED] R: PC_08, G: PC_09, B: PC_05
	#define LED1_PIN			GPIO_Pin_8 // RGB LED: RED
	#define LED1_GPIO_PORT		GPIOC
	#define LED1_GPIO_PAD		PAD_PC
	#define LED1_GPIO_PAD_AF	PAD_AF1

	#define LED2_PIN			GPIO_Pin_9 // RGB LED: GREEN
	#define LED2_GPIO_PORT		GPIOC
	#define LED2_GPIO_PAD		PAD_PC
	#define LED2_GPIO_PAD_AF	PAD_AF1
	
/*	
	#define LED_R_PIN			GPIO_Pin_8
	#define LED_R_GPIO_PORT		GPIOC
	#define LED_R_GPIO_PAD		PAD_PC
	#define LED_R_GPIO_PAD_AF	PAD_AF1

	#define LED_G_PIN			GPIO_Pin_9
	#define LED_G_GPIO_PORT		GPIOC
	#define LED_G_GPIO_PAD		PAD_PC
	#define LED_G_GPIO_PAD_AF	PAD_AF1

	#define LED_B_PIN			GPIO_Pin_5
	#define LED_B_GPIO_PORT		GPIOC
	#define LED_B_GPIO_PAD		PAD_PC
	#define LED_B_GPIO_PAD_AF	PAD_AF1

	// LED
	#define LEDn		3
	typedef enum
	{
	  LED_R = 0,
	  LED_G = 1,
	  LED_B = 2  
	} Led_TypeDef;
*/
#endif
	
	
	/* PHY Link check  */
	#define PHYLINK_CHECK_CYCLE_MSEC			1000

	// LED
	#define LEDn		2
	typedef enum
	{
	  LED1 = 0,	// PHY link status
	  LED2 = 1	// TCP connection status
	} Led_TypeDef;

	extern volatile uint16_t phylink_check_time_msec;
	extern uint8_t flag_check_phylink;
	extern uint8_t flag_hw_trig_enable;
	
	void W7500x_Board_Init(void);
	
	void init_hw_trig_pin(void);
	uint8_t get_hw_trig_pin(void);
	
	void init_phylink_in_pin(void);
	uint8_t get_phylink_in_pin(void);
	
	void LED_Init(Led_TypeDef Led);
	void LED_On(Led_TypeDef Led);
	void LED_Off(Led_TypeDef Led);
	void LED_Toggle(Led_TypeDef Led);
	uint8_t get_LED_Status(Led_TypeDef Led);
	
#endif
