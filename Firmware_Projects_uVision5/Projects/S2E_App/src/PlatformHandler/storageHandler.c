#include <stdio.h>
#include <string.h>
#include "common.h"
#include "flashHandler.h"
#include "deviceHandler.h"
#include "storageHandler.h"

#ifdef _STORAGE_DEBUG_
	#include <stdio.h>
#endif

#ifdef USE_EXT_EEPROM
	#include "eepromHandler.h"
#endif

uint32_t read_storage(teDATASTORAGE stype, uint32_t addr, void *data, uint16_t size)
{
	uint32_t ret_len;
	
	switch(stype)
	{
		case STORAGE_MAC:
#ifndef USE_EXT_EEPROM
			ret_len = read_flash(DEVICE_MAC_ADDR, data, 6); // flash
#else
			ret_len = read_eeprom(DEVICE_MAC_ADDR, data, 6); // external eeprom for configuration data
#endif
			break;
				
		case STORAGE_CONFIG:
#ifndef USE_EXT_EEPROM
			ret_len = read_flash(DEVICE_CONFIG_ADDR, data, size); // flash
#else
			ret_len = read_eeprom(DEVICE_CONFIG_ADDR, data, size); // external eeprom for configuration data
#endif
			break;
		
		case STORAGE_APP_MAIN:
			ret_len = read_flash(addr, data, size);
			break;
		
		case STORAGE_APP_BACKUP:
			ret_len = read_flash(addr, data, size);
			break;
		default:
			break;
	}
	
	return ret_len;
}


uint32_t write_storage(teDATASTORAGE stype, uint32_t addr, void *data, uint16_t size)
{
	uint32_t ret_len;
	
	switch(stype)
	{
		case STORAGE_MAC:
#ifndef USE_EXT_EEPROM	// flash
			erase_storage(STORAGE_MAC);
			ret_len = write_flash(DEVICE_MAC_ADDR, data, size);
#else
			ret_len = write_eeprom(DEVICE_MAC_ADDR, data, size); // eeprom for configuration data
#endif
			break;
		
		case STORAGE_CONFIG:
#ifndef USE_EXT_EEPROM	// flash
			erase_storage(STORAGE_CONFIG);
			ret_len = write_flash(DEVICE_CONFIG_ADDR, data, size);
#else
			ret_len = write_eeprom(DEVICE_CONFIG_ADDR, data, size); // eeprom for configuration data
#endif
			break;
		
		case STORAGE_APP_MAIN:
			ret_len = write_flash(addr, data, size);
			break;
		
		case STORAGE_APP_BACKUP:
			ret_len = write_flash(addr, data, size);
			break;
		default:
			break;
	}
	
	return ret_len;
}

void erase_storage(teDATASTORAGE stype)
{
	uint16_t i;
	uint32_t address, working_address;
	
	uint8_t blocks = 0;
	uint16_t sectors = 0, remainder = 0;
	
	switch(stype)
	{
		case STORAGE_MAC:
#ifndef USE_EXT_EEPROM	// flash
			erase_flash_sector(DEVICE_MAC_ADDR);
#else
			erase_eeprom(DEVICE_MAC_ADDR);		// eeprom for configuration data
#endif
			break;
		
		case STORAGE_CONFIG:
#ifndef USE_EXT_EEPROM	// flash
			erase_flash_sector(DEVICE_CONFIG_ADDR);
#else
			erase_eeprom(DEVICE_CONFIG_ADDR);		// eeprom for configuration data
#endif
			break;
		
		case STORAGE_APP_MAIN:
			address = DEVICE_APP_MAIN_ADDR;
			break;
		
		case STORAGE_APP_BACKUP:
			address = DEVICE_APP_BACKUP_ADDR;
			break;
		default:
			break;
	}

	if((stype == STORAGE_APP_MAIN) || (stype == STORAGE_APP_BACKUP))
	{
		blocks = DEVICE_APP_SIZE / BLOCK_SIZE;
		
		if(DEVICE_APP_SIZE > (blocks * BLOCK_SIZE))
		{
			remainder = DEVICE_APP_SIZE - (blocks * BLOCK_SIZE);
			sectors = (remainder / SECT_SIZE);
			if(remainder > (sectors * SECT_SIZE)) sectors++;
		}
		
		working_address = address;
#ifdef _STORAGE_DEBUG_
		printf(" > STORAGE:ERASE_START:ADDR - 0x%x\r\n", working_address);
#endif
		// Flash block erase operation
		for(i = 0; i < blocks; i++)
		{
			erase_flash_block(working_address + (BLOCK_SIZE * i));
#ifdef _STORAGE_DEBUG_
			printf(" > STORAGE:BLOCK_ERASE:ADDR - 0x%x\r\n", working_address + (BLOCK_SIZE * i));
#endif
		}
		
		working_address += (blocks * BLOCK_SIZE);
		
		// Flash sector erase operation
		if(sectors > 0)
		{
			for(i = 0; i < sectors; i++)
			{
				erase_flash_sector(working_address + (SECT_SIZE * i));
#ifdef _STORAGE_DEBUG_
				printf(" > STORAGE:SECTOR_ERASE:ADDR - 0x%x\r\n", working_address + (SECT_SIZE * i));
#endif
			}
			working_address += (sectors * SECT_SIZE);
		}
#ifdef _STORAGE_DEBUG_
			printf(" > STORAGE:ERASE_END:ADDR_RANGE - [0x%x ~ 0x%x]\r\n", address, working_address-1);
#endif
	}
}

