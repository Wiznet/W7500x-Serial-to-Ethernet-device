#include <string.h>
#include "common.h"
#include "W7500x_uart.h"
#include "W7500x_board.h"
#include "configdata.h"
#include "uartHandler.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private functions prototypes ----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
uint8_t * uart_if_table[] = {(uint8_t *)UART_IF_STR_RS232_TTL, (uint8_t *)UART_IF_STR_RS422_485};


void UART1_Configuration(void)
{
	/* UARTx configuration ------------------------------------------------------*/
	/* UARTx configured as follow: 115200-8-N-1
		BaudRate = 115200 baud
		Word Length = 8 Bits
		One Stop Bit
		No parity
		Hardware flow control disabled (RTS and CTS signals)
		Receive and transmit enabled
	*/
	
	UART_InitTypeDef UART_InitStructure;

	/* UART Initialization */
	UART_StructInit(&UART_InitStructure);
	UART_Init(UART1, &UART_InitStructure);
}

void UART2_Configuration(void)
{
	/* Configure UART2: Simple UART */
	S_UART_Init(115200);
}
