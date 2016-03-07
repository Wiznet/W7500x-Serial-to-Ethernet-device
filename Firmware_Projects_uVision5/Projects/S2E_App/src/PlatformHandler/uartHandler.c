#include <string.h>
#include "W7500x_uart.h"
#include "common.h"
#include "W7500x_board.h"
#include "configdata.h"
#include "uartHandler.h"
#include "seg.h"

#include <stdio.h> // for debugging

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private functions prototypes ----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
int32_t uart_putc(uint8_t uartNum, uint8_t ch);
int32_t uart_puts(uint8_t uartNum, uint8_t* buf, uint16_t reqSize);
int32_t uart_getc(uint8_t uartNum);
int32_t uart_getc_nonblk(uint8_t uartNum);
int32_t uart_gets(uint8_t uartNum, uint8_t* buf, uint16_t reqSize);

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
uint8_t flag_ringbuf_full = 0;

// UART Ring buffer declaration
BUFFER_DEFINITION(data_rx, SEG_DATA_BUF_SIZE);

// UART structure declaration for switching between UART0 and UART1 
// UART selector [SEG_DATA_UART] and [SEG_DEBUG_UART] Defines are located at common.h file.

#if (SEG_DATA_UART == 0)
	UART_TypeDef * 		UART_data = UART0;
	IRQn_Type 			UART_data_irq = UART0_IRQn;
#else
	UART_TypeDef * 		UART_data = UART1;
	IRQn_Type 			UART_data_irq = UART1_IRQn;
#endif

uint32_t baud_table[14] = {
	300, 600, 1200, 1800, 2400, 4800, 9600,
	14400, 19200, 28800, 38400, 57600, 115200, 230400
};
uint8_t word_len_table[] = {7, 8, 9};
uint8_t * parity_table[] = {"N", "ODD", "EVEN"};
uint8_t stop_bit_table[] = {1, 2};
uint8_t * flow_ctrl_table[] = {"NONE", "XON/XOFF", "RTS/CTS", "RS422", "RS485"};

// 1-byte character variable for UART Interrupt request handler
static uint8_t ch;

// XON/XOFF Status; 
static uint8_t xonoff_status = UART_XON;

/* Public functions ----------------------------------------------------------*/

////////////////////////////////////////////////////////////////////////////////
// UART Configuration & IRQ handler
// 		SEG_DATA_UART: 	UART0 or UART1
// 		SEG_DEBUG_UART:	UART2
////////////////////////////////////////////////////////////////////////////////

void S2E_UART_IRQ_Handler(UART_TypeDef * s2e_uart)
{
	if(UART_GetITStatus(s2e_uart,  UART_IT_FLAG_RXI))
	{
		if(IS_BUFFER_FULL(data_rx))
		{
			UartGetc(s2e_uart);
			flag_ringbuf_full = 1;
			//BUFFER_CLEAR(data_rx); // buffer full (or 3/4) 일 때 1. flush / 2. wait - 어떤 방법이 가장 좋은가
		}
		else
		{
			ch = UartGetc(s2e_uart); // ch = UART_ReceiveData(s2e_uart);
			
	#ifdef _SEG_DEBUG_
			UART_SendData(s2e_uart, ch);	// ## UART echo; for debugging
	#endif
			if(!(check_modeswitch_trigger(ch))) // ret: [0] data / [!0] trigger code
			{
				if(check_serial_store_permitted(ch)) // ret: [0] not permitted / [1] permitted
				{
					BUFFER_IN(data_rx) = ch;
					BUFFER_IN_MOVE(data_rx, 1);
				}
			}
		}
		
		init_time_delimiter_timer();
		UART_ClearITPendingBit(s2e_uart, UART_IT_FLAG_RXI);
	}

	if(UART_GetITStatus(s2e_uart, UART_IT_FLAG_TXI)) 
	{
		UART_ClearITPendingBit(s2e_uart, UART_IT_FLAG_TXI);
	}
}

void S2E_UART_Configuration(void)
{
	DevConfig *value = get_DevConfig_pointer();
	
	/* Configure the UARTx */
	serial_info_init(UART_data, &(value->serial_info[0])); // Load the UART_data Settings from Flash
	
	/* Configure UARTx Interrupt Enable */
	//UART_ITConfig(UART_data, (UART_IT_FLAG_TXI | UART_IT_FLAG_RXI), ENABLE);
	UART_ITConfig(UART_data, UART_IT_FLAG_RXI, ENABLE);
	
	/* NVIC configuration */
	NVIC_ClearPendingIRQ(UART_data_irq);
	NVIC_SetPriority(UART_data_irq, 1);
	NVIC_EnableIRQ(UART_data_irq);
}

/*
void UART1_Configuration(void)
{
	UART_InitTypeDef UART_InitStructure;

	// UART Initialization
	UART_StructInit(&UART_InitStructure);
	UART_Init(UART1, &UART_InitStructure);
}
*/

void UART2_Configuration(void)
{
	/* Configure UART2: Simple UART */
	/* UART2 configured as follow: 115200-8-N-1 */
	
	S_UART_Init(115200);
}

void serial_info_init(UART_TypeDef *pUART, struct __serial_info *serial)
{
	UART_InitTypeDef UART_InitStructure;
	uint32_t valid_arg = 0;

	/* Set Baud Rate */
	if(serial->baud_rate < (sizeof(baud_table) / sizeof(baud_table[0])))
	{
		UART_InitStructure.UART_BaudRate = baud_table[serial->baud_rate];
		valid_arg = 1;
	}
	
	if(!valid_arg)
		UART_InitStructure.UART_BaudRate = baud_table[baud_115200];

	/* Set Data Bits */
	switch(serial->data_bits) {
		case word_len7:
			UART_InitStructure.UART_WordLength = UART_WordLength_7b;
			break;
		case word_len8:
			UART_InitStructure.UART_WordLength = UART_WordLength_8b;
			break;
		default:
			UART_InitStructure.UART_WordLength = UART_WordLength_8b;
			serial->data_bits = word_len8;
			break;
	}

	/* Set Stop Bits */
	switch(serial->stop_bits) {
		case stop_bit1:
			UART_InitStructure.UART_StopBits = UART_StopBits_1;
			break;
		case stop_bit2:
			UART_InitStructure.UART_StopBits = UART_StopBits_2;
			break;
		default:
			UART_InitStructure.UART_StopBits = UART_StopBits_1;
			serial->stop_bits = stop_bit1;
			break;
	}

	/* Set Parity Bits */
	switch(serial->parity) {
		case parity_none:
			UART_InitStructure.UART_Parity = UART_Parity_No;
			break;
		case parity_odd:
			UART_InitStructure.UART_Parity = UART_Parity_Odd;
			break;
		case parity_even:
			UART_InitStructure.UART_Parity = UART_Parity_Even;
			break;
		default:
			UART_InitStructure.UART_Parity = UART_Parity_No;
			serial->parity = parity_none;
			break;
	}
	
	/* Flow Control */
	switch(serial->flow_control) {
	case flow_none:
		UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_None;
		break;
	case flow_rts_cts:
		UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_RTS_CTS;
		break;
	case flow_xon_xoff:
		UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_None;
		break;
	case flow_rs422:
		UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_None;
		break;
	case flow_rs485:
		UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_None;
		break;
	default:
		UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_None;
		break;
	}

	// RS232 Hardware Flow Control
	//7		RTS		Request To Send		Output
	//8		CTS		Clear To Send		Input
	
	/* Configure the UARTx */
	UART_InitStructure.UART_Mode = UART_Mode_Rx | UART_Mode_Tx;
	UART_Init(pUART, &UART_InitStructure);
}

void check_uart_flow_control(uint8_t flow_ctrl)
{
	if(flow_ctrl == flow_xon_xoff)
	{
		if((xonoff_status == UART_XON) && (BUFFER_USED_SIZE(data_rx) > UART_XOFF_THRESHOLD)) // Send the transmit stop command to peer - go XOFF
		{
			UartPutc(UART_data, UART_XOFF);
			xonoff_status = UART_XOFF;
#ifdef _UART_DEBUG_
			printf(" >> SEND XOFF [%d]\r\n", BUFFER_USED_SIZE(data_rx));
#endif
		}
		else if((xonoff_status == UART_XOFF) && (BUFFER_USED_SIZE(data_rx) < UART_XON_THRESHOLD)) // Send the transmit start command to peer. -go XON
		{
			UartPutc(UART_data, UART_XON);
			xonoff_status = UART_XON;
#ifdef _UART_DEBUG_
			printf(" >> SEND XON [%d]\r\n", BUFFER_USED_SIZE(data_rx));
#endif
		}
	}
	//else if(flow_ctrl == flow_dtr_dsr) // ...
	//{
	//	;
	//}
}

int32_t uart_putc(uint8_t uartNum, uint8_t ch)
{
	if(uartNum == SEG_DATA_UART)
	{
		UartPutc(UART_data, ch); // 원 함수 / 지연의 원인. UART busy - 해결 방안 고민해보기. tx도 ringbuf 처리?
	}
	else if(uartNum == SEG_DEBUG_UART)
	{
		S_UartPutc(ch);
	}
	return RET_OK;
}

int32_t uart_puts(uint8_t uartNum, uint8_t* buf, uint16_t reqSize)
{
	uint16_t lentot = 0;

	while(*buf != '\0' && lentot < reqSize)
	{
		if(uartNum == SEG_DATA_UART)
		{
			uart_putc(uartNum, *buf);
		}
		else if(uartNum == SEG_DEBUG_UART)
		{
			S_UartPutc(*buf);
		}
		else
			return 0;
		
		buf++;
		lentot++;
	}

	return lentot;
}

int32_t uart_getc(uint8_t uartNum)
{
	int32_t ch;

	if(uartNum == SEG_DATA_UART)
	{
		while(IS_BUFFER_EMPTY(data_rx));
		ch = (int32_t)BUFFER_OUT(data_rx);
		BUFFER_OUT_MOVE(data_rx, 1);
	}
	else if(uartNum == SEG_DEBUG_UART)
	{
		;//ch = (uint8_t)S_UartGetc();
	}
	else
		return RET_NOK;

	return ch;
}

int32_t uart_getc_nonblk(uint8_t uartNum)
{
	int32_t ch;

	if(uartNum == SEG_DATA_UART)
	{
		if(IS_BUFFER_EMPTY(data_rx)) return RET_NOK;
		ch = (int32_t)BUFFER_OUT(data_rx);
		BUFFER_OUT_MOVE(data_rx,1);
	}
	else if(uartNum == SEG_DEBUG_UART)
	{
		;
	}
	else
		return RET_NOK;

	return ch;
}

int32_t uart_gets(uint8_t uartNum, uint8_t* buf, uint16_t reqSize)
{
	uint16_t lentot = 0, len1st = 0;

	if(uartNum == SEG_DATA_UART)
	{
		lentot = reqSize = MIN(BUFFER_USED_SIZE(data_rx), reqSize);
		if(IS_BUFFER_OUT_SEPARATED(data_rx) && (len1st = BUFFER_OUT_1ST_SIZE(data_rx)) < reqSize) {
			memcpy(buf, &BUFFER_OUT(data_rx), len1st);
			BUFFER_OUT_MOVE(data_rx, len1st);
			reqSize -= len1st;
		}
		memcpy(buf+len1st, &BUFFER_OUT(data_rx), reqSize);
		BUFFER_OUT_MOVE(data_rx,reqSize);
	}
	else if(uartNum == SEG_DEBUG_UART)
	{
		;
	}
	else
		return RET_NOK;

	return lentot;
}

void uart_rx_flush(uint8_t uartNum)
{
	if(uartNum == SEG_DATA_UART)
	{
		BUFFER_CLEAR(data_rx);
	}
}
