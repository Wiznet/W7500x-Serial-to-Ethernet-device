
#include <string.h>
#include "common.h"
#include "ConfigData.h"
#include "wizchip_conf.h"
#include "W7500x_wztoe.h"
#include "socket.h"
#include "segcp.h"
#include "flashHandler.h"
#include "storageHandler.h"
#include "deviceHandler.h"

uint16_t get_firmware_from_network(uint8_t sock, uint8_t * buf);
void reset_fw_update_timer(void);

uint8_t enable_fw_update_timer = SEGCP_DISABLE;
volatile uint16_t fw_update_time = 0;
uint8_t flag_fw_update_timeout = SEGCP_DISABLE;
	
uint8_t enable_fw_from_network_timer = SEGCP_DISABLE;
volatile uint16_t fw_from_network_time = 0;
uint8_t flag_fw_from_network_timeout = SEGCP_DISABLE;

extern uint8_t g_recv_buf[DATA_BUF_SIZE];


void device_set_factory_default(void)
{
	//DevConfig *dev_config = get_DevConfig_pointer();
	
	set_DevConfig_to_factory_value();
	save_DevConfig_to_storage();
	
	//write_storage(STORAGE_CONFIG, &dev_config, sizeof(DevConfig));
}


void device_socket_termination(void)
{
	uint8_t sock_state = getSn_SR(SEGCP_TCP_SOCK);
	if((sock_state == SOCK_ESTABLISHED) || (sock_state == SOCK_CLOSE_WAIT)) disconnect(SEGCP_TCP_SOCK);
	else close(SEGCP_TCP_SOCK);
	
	close(SEGCP_UDP_SOCK);
}


void device_reboot(void)
{
	device_socket_termination();
	
	NVIC_SystemReset();
	while(1);
}


	// if(stype == STORAGE_APP_BACKUP)	>>> Config tool -> STORAGE_APP_BACKUP (App, network to flash update)
	// if(stype == STORAGE_APP_MAIN) 	>>> STORAGE_APP_BACKUP -> STORAGE_APP_MAIN (Boot, flash to flash update)

uint8_t device_firmware_update(teDATASTORAGE stype)
{
	struct __firmware_update *fwupdate = (struct __firmware_update *)&(get_DevConfig_pointer()->firmware_update);
	struct __serial_info *serial = (struct __serial_info *)&(get_DevConfig_pointer()->serial_info);
	
	uint8_t ret = DEVICE_FWUP_RET_PROGRESS; // No Meaning, [Firmware update process] have to work as blocking function.
	uint16_t len = 0;
	uint16_t recv_len = 0;
	uint16_t write_len = 0;
	static uint32_t write_fw_len;
	
	teDATASTORAGE src_storage;
	uint32_t src_storage_addr, target_storage_addr;
#ifdef _FWUP_DEBUG_
	uint8_t update_cnt = 0;
#endif
	if(fwupdate->fwup_flag == SEGCP_DISABLE)	return DEVICE_FWUP_RET_FAILED;
	if((fwupdate->fwup_size == 0) || (fwupdate->fwup_size > DEVICE_FWUP_SIZE))
	{
		if(serial->serial_debug_en == SEGCP_ENABLE)
		{
			printf(" > SEGCP:FW_UPDATE:FAILED - Invalid firmware size: %d bytes (Firmware size must be within %d bytes)\r\n", fwupdate->fwup_size, DEVICE_FWUP_SIZE);
		}

		return DEVICE_FWUP_RET_FAILED;
	}
	
	// App, FW update from Network (ethernet) to Flash memory (backup area)
	if(stype == NETWORK_APP_BACKUP)
	{
		if(serial->serial_debug_en == SEGCP_ENABLE)
		{
			printf(" > SEGCP:FW_UPDATE:NETWORK - Firmware size: [%d] bytes\r\n", fwupdate->fwup_size);
		}

		write_fw_len = 0;
		erase_storage(STORAGE_APP_BACKUP); // Erase application backup blocks
		
		// init firmware update timer
		enable_fw_update_timer = SEGCP_ENABLE;
		
		do 
		{
/////////////////////////////////////////////////////////////////////////////////////////////////////////
			recv_len = get_firmware_from_network(SOCK_FWUPDATE, g_recv_buf);
			
			if(recv_len > 0)
			{
				write_len = write_storage(STORAGE_APP_BACKUP, (DEVICE_APP_BACKUP_ADDR + write_fw_len), g_recv_buf, recv_len);
				write_fw_len += write_len;
				fw_update_time = 0; // Reset fw update timeout counter
			}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
			
			// Firmware update failed: Timeout occurred
			if(flag_fw_update_timeout == SEGCP_ENABLE)
			{
				if(serial->serial_debug_en == SEGCP_ENABLE) printf(" > SEGCP:FW_UPDATE:FAILED - Firmware update timeout\r\n");
				ret = DEVICE_FWUP_RET_FAILED;
				break;
			}
			
			// Firmware update failed: timeout occurred at get_firmware_from_network() function
			if(flag_fw_from_network_timeout == SEGCP_ENABLE)
			{
				if(serial->serial_debug_en == SEGCP_ENABLE) printf(" > SEGCP:FW_UPDATE:FAILED - Network download timeout\r\n");
				ret = DEVICE_FWUP_RET_FAILED;
				break;
			}
			
		} while(write_fw_len < fwupdate->fwup_size);
	}
	// Boot, FW update from Flash memory (backup area) to Flash memory (main application area)
	// Boot, FW update from Flash memory (main application area) to Flash memory (backup area)
	else if((stype == STORAGE_APP_MAIN) || (stype == STORAGE_APP_BACKUP)) // Run this code in the boot area only, stype means target storage
	{
		// init target / source flash area
		if(stype == STORAGE_APP_MAIN)
		{
			src_storage = STORAGE_APP_BACKUP;
			src_storage_addr = DEVICE_APP_BACKUP_ADDR;
			target_storage_addr = DEVICE_APP_MAIN_ADDR;
		}
		else
		{
			src_storage = STORAGE_APP_MAIN;
			src_storage_addr = DEVICE_APP_MAIN_ADDR;
			target_storage_addr = DEVICE_APP_BACKUP_ADDR;
		}
		
		if(serial->serial_debug_en == SEGCP_ENABLE)
		{
			printf(" > SEGCP:FW_UPDATE:FLASH-to-FLASH - Firmware size: [%d] bytes\r\n", fwupdate->fwup_size);
		}
		write_fw_len = 0; // Variable for calculate Full firmware size
		erase_storage(stype); // Erase targer storage blocks
		len = DATA_BUF_SIZE;
		
		// init firmware update timer
		enable_fw_update_timer = SEGCP_ENABLE;
		
		do 
		{
			// Timeout occurred
			if(flag_fw_update_timeout == SEGCP_ENABLE)
			{
				if(serial->serial_debug_en == SEGCP_ENABLE)
				{
					printf(" > SEGCP:FW_UPDATE:FAILED - Firmware update timeout\r\n");
				}
				ret = DEVICE_FWUP_RET_FAILED;
				break;
			}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
			if((write_fw_len + DATA_BUF_SIZE) > fwupdate->fwup_size) len = fwupdate->fwup_size - write_fw_len;
			
			recv_len = read_storage(src_storage, (src_storage_addr + write_fw_len), g_recv_buf, len);
#ifdef _FWUP_DEBUG_
			printf(" > SEGCP:FW_READ_LEN - %d bytes [start: 0x%x] [end: 0x%x]\r\n",recv_len, (src_storage_addr + write_fw_len), (src_storage_addr + write_fw_len + recv_len - 1));
#endif
			if(recv_len > 0)
			{
				write_len = write_storage(stype, (target_storage_addr + write_fw_len), g_recv_buf, recv_len);
#ifdef _FWUP_DEBUG_
				printf(" > SEGCP:FW_WRITE_LEN - %d bytes [start: 0x%x] [end: 0x%x]\r\n",write_len, (target_storage_addr + write_fw_len), (target_storage_addr + write_fw_len + write_len - 1));
#endif				
				write_fw_len += write_len;
				fw_update_time = 0; // Reset fw update timeout counter
			}
#ifdef _FWUP_DEBUG_
			printf(" > SEGCP:FW_UPDATE:UPDATE[%.2d] - %d / %d bytes\r\n", update_cnt++, write_fw_len, fwupdate->fwup_size);
#endif
/////////////////////////////////////////////////////////////////////////////////////////////////////////
		} while(write_fw_len < fwupdate->fwup_size);
#ifdef _FWUP_DEBUG_
		printf(" > SEGCP:FW_UPDATE:FLASH-to-FLASH - UPDATE END | [%d] bytes\r\n", write_fw_len);
#endif
	}
	else
	{
		ret = DEVICE_FWUP_RET_FAILED;
	}
	
	// shared code
	if(write_fw_len == fwupdate->fwup_size)
	{
		if(serial->serial_debug_en == SEGCP_ENABLE)
		{
			printf(" > SEGCP:FW_UPDATE:SUCCESS - %d / %d bytes\r\n", write_fw_len, fwupdate->fwup_size);
		}
		ret = DEVICE_FWUP_RET_SUCCESS;
	}
	
	reset_fw_update_timer();
	
	return ret;
}


uint16_t get_firmware_from_network(uint8_t sock, uint8_t * buf)
{
	struct __firmware_update *fwupdate = (struct __firmware_update *)&(get_DevConfig_pointer()->firmware_update);
	uint8_t len_buf[2] = {0, };
	uint16_t len = 0;
	uint8_t state = getSn_SR(sock);
	
	static uint32_t recv_fwsize;
	
	switch(state)
	{
		case SOCK_INIT:
			//listen(sock);
			break;
		
		case SOCK_LISTEN:
			break;
		
		case SOCK_ESTABLISHED:
			if(getSn_IR(sock) & Sn_IR_CON)
			{
				// init network firmware update timer
				enable_fw_from_network_timer = SEGCP_ENABLE;
				setSn_IR(sock, Sn_IR_CON);
			}
			
			// Timeout occurred
			if(flag_fw_from_network_timeout == SEGCP_ENABLE)
			{
#ifdef _FWUP_DEBUG_
				printf(" > SEGCP:FW_UPDATE:NET_TIMEOUT\r\n");
#endif
				disconnect(sock);
				return 0;
			}
			
			// DATA_BUF_SIZE
			if((len = getSn_RX_RSR(sock)) > 0)
			{
				if(len > DATA_BUF_SIZE) len = DATA_BUF_SIZE;
				if((recv_fwsize + len) > fwupdate->fwup_size) len = fwupdate->fwup_size - recv_fwsize; // remain
				
				len = recv(sock, buf, len);
				recv_fwsize += len;
#ifdef _FWUP_DEBUG_
				printf(" > SEGCP:FW_UPDATE:RECV_LEN - %d bytes | [%d] bytes\r\n", len, recv_fwsize);
#endif
				// Send ACK - receviced length - to configuration tool
				len_buf[0] = (uint8_t)((0xff00 & len) >> 8); // endian-independent code: Datatype translation, byte order regardless
				len_buf[1] = (uint8_t)(0x00ff & len);
				
				send(sock, len_buf, 2);
				
				fw_from_network_time = 0;
				
				if(recv_fwsize >= fwupdate->fwup_size)
				{
#ifdef _FWUP_DEBUG_
					printf(" > SEGCP:FW_UPDATE:NETWORK - UPDATE END | [%d] bytes\r\n", recv_fwsize);
#endif
					// timer disable: network timeout
					fw_update_time = 0;
					enable_fw_from_network_timer = 0;
					
					// socket close
					disconnect(sock);
				}
			}
			break;
			
		case SOCK_CLOSE_WAIT:
			disconnect(sock);
			break;
		
		case SOCK_FIN_WAIT:
		case SOCK_CLOSED:
			if(socket(sock, Sn_MR_TCP, DEVICE_FWUP_PORT, Sn_MR_ND) == sock)
			{
				recv_fwsize = 0;
				listen(sock);
				
#ifdef _FWUP_DEBUG_
				printf(" > SEGCP:FW_UPDATE:SOCKOPEN\r\n");
#endif
			}
			break;
			
		default:
			break;
	}
	
	return len;
}


// function for timer
void device_timer_msec(void)
{
	if(enable_fw_update_timer == SEGCP_ENABLE)
	{
		if(fw_update_time < 0xFFFF) 	fw_update_time++;
		else							fw_update_time = 0;
		
		if(fw_update_time >= DEVICE_FWUP_TIMEOUT)
		{
			flag_fw_update_timeout = SEGCP_ENABLE;
			fw_update_time = 0;
		}
	}
	
	if(enable_fw_from_network_timer == SEGCP_ENABLE)
	{
		if(fw_from_network_time < 0xFFFF)	fw_from_network_time++;
		else								fw_from_network_time = 0;
		
		if(fw_from_network_time >= DEVICE_FWUP_TIMEOUT)
		{
			flag_fw_from_network_timeout = SEGCP_ENABLE;
			fw_from_network_time = 0;
		}
	}
}

void reset_fw_update_timer(void)
{
	// Enables
	enable_fw_update_timer = SEGCP_DISABLE;
	enable_fw_from_network_timer = SEGCP_DISABLE;
	
	// Counters
	fw_update_time = 0;
	fw_from_network_time = 0;
	
	// Flags
	flag_fw_update_timeout = SEGCP_DISABLE;
	flag_fw_from_network_timeout = SEGCP_DISABLE;
}
