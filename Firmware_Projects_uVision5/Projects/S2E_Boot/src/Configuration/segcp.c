#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "common.h"
#include "W7500x_board.h"
#include "W7500x_wztoe.h"
#include "socket.h"
#include "ConfigData.h"
#include "storageHandler.h"
#include "deviceHandler.h"

#include "segcp.h"
#include "util.h"
#include "uartHandler.h"


/* Private define ------------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
uint16_t uart_get_commandline(uint8_t uartNum, uint8_t* buf, uint16_t maxSize);

/* Private variables ---------------------------------------------------------*/

static uint8_t gSEGCPREQ[CONFIG_BUF_SIZE];
static uint8_t gSEGCPREP[CONFIG_BUF_SIZE];

uint8_t * strDEVSTATUS[]  = {"BOOT", "OPEN", "CONNECT", "UPGARDE", "ATMODE", "UDP", 0};

// V4.4 Add Command EE : EEPROM ERASE
uint8_t * tbSEGCPCMD[] = {"MC", "VR", "MN", "IM", "OP", "DD", "CP", "PO", "DG", "KA", 
							"KI", "KE", "RI", "LI", "SM", "GW", "DS", "PI", "PP", "DX",
							"DP", "DI", "DW", "DH", "LP", "RP", "RH", "BR", "DB", "PR",
							"SB", "FL", "IT", "PT", "PS", "PD", "TE", "SS", "NP", "SP",
							"LG", "ER", "FW", "MA", "PW", "SV", "EX", "RT", "UN", "ST",
							"FR", "EC", "K!", "UE", 0};

uint8_t * tbSEGCPERR[] = {"ERNULL", "ERNOTAVAIL", "ERNOPARAM", "ERIGNORED", "ERNOCOMMAND", "ERINVALIDPARAM", "ERNOPRIVILEGE"};

uint8_t gSEGCPPRIVILEGE = SEGCP_PRIVILEGE_CLR;

// Keep-alive timer values for TCP unicast search function
uint8_t enable_configtool_keepalive_timer = SEGCP_DISABLE;
volatile uint16_t configtool_keepalive_time = 0;
uint8_t flag_send_configtool_keepalive = SEGCP_DISABLE;


void do_segcp(void)
{
	DevConfig *dev_config = get_DevConfig_pointer();

	uint16_t segcp_ret = 0;
	
	segcp_ret  = proc_SEGCP_udp(gSEGCPREQ, gSEGCPREP);
	segcp_ret |= proc_SEGCP_tcp(gSEGCPREQ, gSEGCPREP);

	if(segcp_ret & SEGCP_RET_ERR)
	{
		if(dev_config->serial_info[0].serial_debug_en == SEGCP_ENABLE) printf(" > SEGCP:ERROR:%04X\r\n", segcp_ret);
	}
	else if(segcp_ret) // Command parsing success
	{
		if(segcp_ret & SEGCP_RET_FACTORY)
		{
			device_set_factory_default();
		}
		else if(segcp_ret & SEGCP_RET_SAVE)
		{
			save_DevConfig_to_storage();
		}
		else if(segcp_ret & SEGCP_RET_ERASE_EEPROM)
		{
			//uart_gets(SEG_DATA_UART, ConfigErasePW, sizeof(ConfigErasePW));
			//uart_get_pass_blk(ConfigErasePW, sizeof(PW_ERASE_CONFIG_DATA));
			
			//if(strcmp(ConfigErasePW, PW_ERASE_CONFIG_DATA) == 0)
			//{
#ifdef _SEGCP_DEBUG_
				printf(">> Erase success\r\n");
#endif
				printf("\r\n");
				
				erase_storage(STORAGE_MAC);
				erase_storage(STORAGE_CONFIG);
			//}
//#ifdef _SEGCP_DEBUG_
			//else
			//{
				//printf(">> Erase failed\r\n");
			//}
//#endif
		}
	
		if (segcp_ret & SEGCP_RET_FWUP)
		{
			if(device_firmware_update(NETWORK_APP_BACKUP) == DEVICE_FWUP_RET_SUCCESS)
			{
				save_DevConfig_to_storage();
				device_reboot();
			}
			//else
			//{
			//	;
			//}
		}
		else if (segcp_ret & SEGCP_RET_REBOOT)
		{
			device_reboot();
		}
	}
}


uint8_t parse_SEGCP(uint8_t * pmsg, uint8_t * param)
{
	uint8_t** pcmd;
	uint8_t cmdnum = 0;
	uint8_t i;

	*param = 0;

	for(pcmd = tbSEGCPCMD; *pcmd != 0; pcmd++)
	{
		if(!strncmp(pmsg, *pcmd, strlen(*pcmd))) break;
	}
	
	if(*pcmd == 0) 
	{
#ifdef _SEGCP_DEBUG_   
		printf("SEGCP[UNKNOWN]:%s\r\n", pmsg);
#endif
		return SEGCP_UNKNOWN;
	}
	
	cmdnum = (uint8_t)(pcmd - tbSEGCPCMD);
	
	if(cmdnum == (uint8_t)SEGCP_MA) 
	{
		if((pmsg[8] == '\r') && (pmsg[9] == '\n'))
		{
			memcpy(param, (uint8_t*)&pmsg[2], 6);
		}
		else
		{
			return SEGCP_UNKNOWN;
		}
	}
	else if(cmdnum == (uint8_t)SEGCP_PW)
	{
		for(i = 0; pmsg[2+i] != '\r'; i++)
		{
			param[i] = pmsg[2+i];
		}
		
		if(pmsg[2+i+1] == '\n')
		{
			param[i] = 0; param[i+1] = 0;
		}
		else
		{
			return SEGCP_UNKNOWN;
		}
	}
	else
	{
		strcpy(param, (uint8_t*)&pmsg[2]);
	}

#ifdef _SEGCP_DEBUG_   
	printf("SEGCP[%d:%s:", cmdnum, *pcmd);
	if(cmdnum == SEGCP_MA)
	{
		for(i = 0; i < 6; i++) printf("%.2x", param[i]);
	}
	else
	{
		printf("%s",param);
	}
	printf("]\r\n");
#endif
	
	return cmdnum;
}

uint16_t proc_SEGCP(uint8_t* segcp_req, uint8_t* segcp_rep)
{
	DevConfig *dev_config = get_DevConfig_pointer();
	
	uint8_t  i = 0;
	uint16_t ret = 0;
	uint8_t  cmdnum = 0;
	uint8_t* treq;
	uint8_t* trep = segcp_rep;
	uint16_t param_len = 0;
	
	uint8_t  tmp_byte = 0;
	uint16_t tmp_int = 0;
	uint32_t tmp_long = 0;
	
	uint8_t tmp_ip[4];
	uint8_t tmp_ip_cnt = 0;

	uint8_t param[SEGCP_PARAM_MAX*2];
	
#ifdef _SEGCP_DEBUG_   
	printf("SEGCP_REQ : %s\r\n",segcp_req);
#endif
	memset(trep, 0, sizeof(trep));
	treq = strtok(segcp_req, SEGCP_DELIMETER);
	
	while(treq)
	{
#ifdef _SEGCP_DEBUG_
		printf("SEGCP_REQ_TOK : %s\r\n",treq);
#endif
		if((cmdnum = parse_SEGCP(treq, param)) != SEGCP_UNKNOWN)
		{
			param_len = strlen(param);
			
			if(*param == 0)
			{
				memcpy(trep, tbSEGCPCMD[cmdnum], SEGCP_CMD_MAX);
				trep+=SEGCP_CMD_MAX;
				
				switch((teSEGCPCMDNUM)cmdnum)
				{
					case SEGCP_MC: sprintf(trep,"%02X:%02X:%02X:%02X:%02X:%02X", 
											dev_config->network_info_common.mac[0], dev_config->network_info_common.mac[1], dev_config->network_info_common.mac[2],
											dev_config->network_info_common.mac[3], dev_config->network_info_common.mac[4], dev_config->network_info_common.mac[5]);
						break;
					case SEGCP_VR: sprintf(trep,"%x.%x.%x", dev_config->fw_ver[0], dev_config->fw_ver[1], dev_config->fw_ver[2]);
						break;
					case SEGCP_MN: sprintf(trep,"%s", dev_config->module_name);
						break;
					case SEGCP_IM: sprintf(trep,"%d", dev_config->options.dhcp_use);	// 0:STATIC, 1:DHCP (PPPoE X)
						break;
					case SEGCP_OP: sprintf(trep,"%d", dev_config->network_info[0].working_mode); // opmode
						break;
					//case SEGCP_DD: sprintf(trep,"%d", tsvDEVCONFnew.ddns_en);
					case SEGCP_DD: sprintf(trep,"%d", 0);
						break;
					//case SEGCP_PO: sprintf(trep,"%d", tsvDEVCONFnew.telnet_en[0]);
					case SEGCP_PO: sprintf(trep,"%d", 0);
						break;
					case SEGCP_CP: sprintf(trep,"%d", dev_config->options.pw_connect_en);
						break;
					case SEGCP_DG: sprintf(trep,"%d", dev_config->serial_info[0].serial_debug_en);
						break;
					case SEGCP_KA: sprintf(trep,"%d", dev_config->network_info[0].keepalive_en);
						break;
					case SEGCP_KI: sprintf(trep,"%d", dev_config->network_info[0].keepalive_wait_time);
						break;
					case SEGCP_KE: sprintf(trep,"%d", dev_config->network_info[0].keepalive_retry_time);
						break;
					case SEGCP_RI: sprintf(trep,"%d", dev_config->network_info[0].reconnection);
						break;
					case SEGCP_LI:
						sprintf(trep,"%d.%d.%d.%d", dev_config->network_info_common.local_ip[0], dev_config->network_info_common.local_ip[1],
													dev_config->network_info_common.local_ip[2], dev_config->network_info_common.local_ip[3]);
						break;
					case SEGCP_SM: 
						sprintf(trep,"%d.%d.%d.%d", dev_config->network_info_common.subnet[0], dev_config->network_info_common.subnet[1],
													dev_config->network_info_common.subnet[2], dev_config->network_info_common.subnet[3]);
						break;
					case SEGCP_GW: 
						sprintf(trep,"%d.%d.%d.%d", dev_config->network_info_common.gateway[0], dev_config->network_info_common.gateway[1],
													dev_config->network_info_common.gateway[2], dev_config->network_info_common.gateway[3]);
						break;
					case SEGCP_DS:
						sprintf(trep,"%d.%d.%d.%d", dev_config->options.dns_server_ip[0], dev_config->options.dns_server_ip[1],
													dev_config->options.dns_server_ip[2], dev_config->options.dns_server_ip[3]);
						break;
					case SEGCP_PI:
						//if(tsvDEVCONFnew.pppoe_id[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
						//else sprintf(trep,"%s",tsvDEVCONFnew.pppoe_id);
						sprintf(trep,"%c", SEGCP_NULL);
						break;
					case SEGCP_PP: 
						//if(tsvDEVCONFnew.pppoe_pass[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
						//sprintf(trep,"%s",tsvDEVCONFnew.pppoe_pass);
						sprintf(trep,"%c",SEGCP_NULL);
						break;
					//case SEGCP_DX: sprintf(trep,"%d",tsvDEVCONFnew.ddns_index);
					case SEGCP_DX: sprintf(trep,"%d", 0);
						break;
					//case SEGCP_DP: sprintf(trep,"%d",tsvDEVCONFnew.ddns_port);
					case SEGCP_DP: sprintf(trep,"%d", DEVICE_DDNS_PORT);
						break;
					//case SEGCP_DI: sprintf(trep,"%s",tsvDEVCONFnew.ddns_id);
					case SEGCP_DI: sprintf(trep,"%c", SEGCP_NULL);
						break;
					case SEGCP_DW: 
						//if(tsvDEVCONFnew.ddns_pass[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
						//else sprintf(trep,"%s",tsvDEVCONFnew.ddns_pass);
						sprintf(trep, "%c", SEGCP_NULL);
						break;
					case SEGCP_DH:
						if(dev_config->module_name[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
						else sprintf(trep, "%s-%02x%02x%02x", dev_config->module_name, dev_config->network_info_common.mac[3], dev_config->network_info_common.mac[4], dev_config->network_info_common.mac[5]);
						break;
					case SEGCP_LP: sprintf(trep, "%d", dev_config->network_info[0].local_port);
						break;
					case SEGCP_RP: sprintf(trep, "%d", dev_config->network_info[0].remote_port);
						break;
					case SEGCP_RH: 
						if(dev_config->options.dns_use == SEGCP_DISABLE)
						{
							sprintf(trep, "%d.%d.%d.%d", dev_config->network_info[0].remote_ip[0], dev_config->network_info[0].remote_ip[1],
														dev_config->network_info[0].remote_ip[2], dev_config->network_info[0].remote_ip[3]);
						}
						else
						{
							if(dev_config->options.dns_domain_name[0] == 0) sprintf(trep, "%c", SEGCP_NULL);
							else sprintf(trep, "%s", dev_config->options.dns_domain_name);
						}
						break;
					case SEGCP_BR: sprintf(trep, "%d", dev_config->serial_info[0].baud_rate);
						break;
					case SEGCP_DB: sprintf(trep, "%d", dev_config->serial_info[0].data_bits);
						break;
					case SEGCP_PR: sprintf(trep, "%d", dev_config->serial_info[0].parity);
						break;
					case SEGCP_SB: sprintf(trep, "%d", dev_config->serial_info[0].stop_bits);
						break;
					case SEGCP_FL: sprintf(trep, "%d", dev_config->serial_info[0].flow_control);
						break;
					case SEGCP_IT: sprintf(trep, "%d", dev_config->network_info[0].inactivity);
						break;
					case SEGCP_PT: sprintf(trep, "%d", dev_config->network_info[0].packing_time);
						break;
					case SEGCP_PS: sprintf(trep, "%d", dev_config->network_info[0].packing_size);
						break;
					case SEGCP_PD: sprintf(trep, "%02X", dev_config->network_info[0].packing_delimiter[0]);
						break;
					case SEGCP_TE: sprintf(trep, "%d", dev_config->options.serial_command);
						break;
					case SEGCP_SS: sprintf(trep, "%02X%02X%02X", dev_config->options.serial_trigger[0], dev_config->options.serial_trigger[1], dev_config->options.serial_trigger[2]);
						break;
					case SEGCP_NP: 
						if(dev_config->options.pw_connect[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
						else sprintf(trep, "%s", dev_config->options.pw_connect);
						break;
					case SEGCP_SP:
						if(dev_config->options.pw_search[0] == 0) sprintf(trep,"%c",SEGCP_NULL);
						else sprintf(trep, "%s", dev_config->options.pw_search);
						break;
					case SEGCP_LG: 
					case SEGCP_ER: 
					case SEGCP_MA:
					case SEGCP_PW: ret |= SEGCP_RET_ERR_NOTAVAIL;
						break;
					case SEGCP_FW: ret |= SEGCP_RET_ERR_NOPARAM;
						break;
					case SEGCP_SV:
						if(gSEGCPPRIVILEGE & (SEGCP_PRIVILEGE_SET|SEGCP_PRIVILEGE_WRITE)) ret |= SEGCP_RET_SAVE;
						else ret |= SEGCP_RET_ERR_NOPRIVILEGE;
						break;
					case SEGCP_EX:
						if(gSEGCPPRIVILEGE & (SEGCP_PRIVILEGE_SET|SEGCP_PRIVILEGE_WRITE)) ret |= SEGCP_RET_SWITCH;
						else ret |= SEGCP_RET_ERR_NOPRIVILEGE;
						break;
					case SEGCP_RT:
						if(gSEGCPPRIVILEGE & (SEGCP_PRIVILEGE_SET|SEGCP_PRIVILEGE_WRITE)) ret |= SEGCP_RET_REBOOT;
						else ret |= SEGCP_RET_ERR_NOPRIVILEGE;
						break;
					case SEGCP_UN: sprintf(trep, "%d", DEVICE_UART_CNT);
						break;
					case SEGCP_ST: sprintf(trep, "%s", strDEVSTATUS[dev_config->network_info[0].state]);
						break;
					case SEGCP_FR: 
						if(gSEGCPPRIVILEGE & (SEGCP_PRIVILEGE_SET|SEGCP_PRIVILEGE_WRITE)) ret |= SEGCP_RET_FACTORY | SEGCP_RET_REBOOT;
						else ret |= SEGCP_RET_ERR_NOPRIVILEGE;
						break;
					case SEGCP_EC:
						sprintf(trep,"%d",dev_config->options.serial_command_echo);
						break;
					case SEGCP_K1:
						ret |= SEGCP_RET_ERASE_EEPROM | SEGCP_RET_REBOOT;
						break;
					case SEGCP_UE:
						sprintf(trep, "%d", 0);
						break;
					default:
						ret |= SEGCP_RET_ERR_NOCOMMAND;
						sprintf(trep,"%s", strDEVSTATUS[dev_config->network_info[0].state]);
						break;
				}
				
				if(ret & (SEGCP_RET_ERR | SEGCP_RET_REBOOT | SEGCP_RET_SWITCH | SEGCP_RET_SAVE))
				{
					trep -= SEGCP_CMD_MAX;
					*trep = 0;
				}
				else
				{
					strcat(trep, SEGCP_DELIMETER);
					trep += strlen(trep);
				}
			}
			else if(gSEGCPPRIVILEGE & (SEGCP_PRIVILEGE_SET|SEGCP_PRIVILEGE_WRITE))
			{
				switch((teSEGCPCMDNUM)cmdnum)
				{
					case SEGCP_MC:
						if((dev_config->network_info_common.mac[0] == 0x00) && (dev_config->network_info_common.mac[1] == 0x08) && (dev_config->network_info_common.mac[2] == 0xDC)) ret |= SEGCP_RET_ERR_IGNORED;
						else if(!is_macaddr(param, ".:-", dev_config->network_info_common.mac)) ret |= SEGCP_RET_ERR_INVALIDPARAM;			
						break;
					case SEGCP_VR: 
					case SEGCP_MN:
						ret |= SEGCP_RET_ERR_IGNORED;
						break;
					case SEGCP_IM:
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > SEGCP_DHCP) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->options.dhcp_use = tmp_byte;
						break;
					case SEGCP_OP: 
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > UDP_MODE)
						{
							ret |= SEGCP_RET_ERR_INVALIDPARAM;
						}
						else
						{
							close(SEG_SOCK);
							dev_config->network_info[0].working_mode = tmp_byte;
						}
						break;
				   case SEGCP_DD: // ## Does nothing
						//if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						//else tsvDEVCONFnew.ddns_en = tmp_byte;
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						break;               
					case SEGCP_PO: // ## Does nothing
						//if(param_len != 1 || tmp_byte > SEGCP_TELNET) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						//else tsvDEVCONFnew.telnet_en[0] = tmp_byte;
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > SEGCP_TELNET) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						break;
					case SEGCP_CP:
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->options.pw_connect_en = tmp_byte;
						break;
					case SEGCP_DG:
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->serial_info[0].serial_debug_en = tmp_byte;
						break;
					case SEGCP_KA:
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->network_info[0].keepalive_en = tmp_byte;
						break;
					case SEGCP_KI:
						sscanf(param,"%ld", &tmp_long);
						if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->network_info[0].keepalive_wait_time = (uint16_t) tmp_long;
						break;
					case SEGCP_KE:
						sscanf(param,"%ld", &tmp_long);
						if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->network_info[0].keepalive_retry_time = (uint16_t) tmp_long;
						break;
					case SEGCP_RI:
						sscanf(param,"%ld", &tmp_long);
						if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->network_info[0].reconnection = (uint16_t) tmp_long;
						break;
					case SEGCP_LI:
						if(is_ipaddr(param, tmp_ip))
						{
							dev_config->network_info_common.local_ip[0] = tmp_ip[0];
							dev_config->network_info_common.local_ip[1] = tmp_ip[1];
							dev_config->network_info_common.local_ip[2] = tmp_ip[2];
							dev_config->network_info_common.local_ip[3] = tmp_ip[3];
						}
						else
						{
							ret |= SEGCP_RET_ERR_INVALIDPARAM;
						}
						break;
					case SEGCP_SM:
						if(is_ipaddr(param, tmp_ip))
						{
							dev_config->network_info_common.subnet[0] = tmp_ip[0];
							dev_config->network_info_common.subnet[1] = tmp_ip[1];
							dev_config->network_info_common.subnet[2] = tmp_ip[2];
							dev_config->network_info_common.subnet[3] = tmp_ip[3];
						}
						else ret |= SEGCP_RET_ERR_INVALIDPARAM;
						break;
					case SEGCP_GW:
						if(is_ipaddr(param, tmp_ip))
						{
							dev_config->network_info_common.gateway[0] = tmp_ip[0];
							dev_config->network_info_common.gateway[1] = tmp_ip[1];
							dev_config->network_info_common.gateway[2] = tmp_ip[2];
							dev_config->network_info_common.gateway[3] = tmp_ip[3];
						}
						else ret |= SEGCP_RET_ERR_INVALIDPARAM;
						break;
					case SEGCP_DS:
						if(is_ipaddr(param, tmp_ip))
						{
							dev_config->options.dns_server_ip[0] = tmp_ip[0];
							dev_config->options.dns_server_ip[1] = tmp_ip[1];
							dev_config->options.dns_server_ip[2] = tmp_ip[2];
							dev_config->options.dns_server_ip[3] = tmp_ip[3];
						}
						else ret |= SEGCP_RET_ERR_INVALIDPARAM;
						break;
					case SEGCP_PI: // ## Does nothing
						//if(param_len > sizeof(tsvDEVCONFnew.pppoe_id)-1) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						//else
						//{
							//if(param[0] == SEGCP_NULL) tsvDEVCONFnew.pppoe_id[0]= 0;
							//else  sscanf(param,"%s",&tsvDEVCONFnew.pppoe_id);
						//}
						;
						break;
					case SEGCP_PP: // ## Does nothing
						//if(param_len > sizeof(tsvDEVCONFnew.pppoe_pass)-1) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						//else 
						//{
							//if(param[0] == SEGCP_NULL) tsvDEVCONFnew.pppoe_pass[0]= 0;
							//else sscanf(param,"%s",&tsvDEVCONFnew.pppoe_pass);
						//}
						;
						break;
					case SEGCP_DX: // ## Does nothing
						//if(param_len != 1 || tmp_byte > (DEVICE_DDNS_CNT-1)) ret |= SEGCP_RET_ERR_INVALIDPARAM;                   
						//else tsvDEVCONFnew.ddns_index = tmp_byte;
						;
						break;
					case SEGCP_DP: // ## Does nothing
						//if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						//else tsvDEVCONFnew.ddns_port = (uint16_t)tmp_long;
						;
						break;
					case SEGCP_DI: // ## Does nothing
						//if(param_len > sizeof(tsvDEVCONFnew.ddns_id)-1) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						//else 
						//{
							//if(param[0] == SEGCP_NULL) tsvDEVCONFnew.ddns_id[0] = 0;
							//else sscanf(param,"%s",&tsvDEVCONFnew.ddns_id);
						//}
						;
						break;	 
					case SEGCP_DW: // ## Does nothing
						//if(param_len > sizeof(tsvDEVCONFnew.ddns_pass)-1) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						//else 
						//{
							//if(param[0] == SEGCP_NULL) tsvDEVCONFnew.ddns_pass[0] = 0;
							//else sscanf(param,"%s",&tsvDEVCONFnew.ddns_pass);
						//}
						;
						break;
					case SEGCP_DH:
						if(param_len > sizeof(dev_config->module_name)-1) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else 
						{
							if(param[0] == SEGCP_NULL)
							{
								dev_config->module_name[0] = 0;
							}
							else
							{
								sscanf(param, "%s", &dev_config->module_name);
							}
						}
						break;
					case SEGCP_LP:
						sscanf(param,"%ld", &tmp_long);
						if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->network_info[0].local_port = (uint16_t)tmp_long;
						break;
					case SEGCP_RP:
						sscanf(param,"%ld", &tmp_long);
						if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;                  
						else dev_config->network_info[0].remote_port = (uint16_t)tmp_long;
						break;
					case SEGCP_RH:
						if(is_ipaddr(param, tmp_ip))
						{
							dev_config->options.dns_use = SEGCP_DISABLE;
							dev_config->network_info[0].remote_ip[0] = tmp_ip[0];
							dev_config->network_info[0].remote_ip[1] = tmp_ip[1];
							dev_config->network_info[0].remote_ip[2] = tmp_ip[2];
							dev_config->network_info[0].remote_ip[3] = tmp_ip[3];
						}
						else
						{
							dev_config->options.dns_use = SEGCP_ENABLE;
							if(param[0] == SEGCP_NULL) dev_config->options.dns_domain_name[0] = 0;
							else strcpy(dev_config->options.dns_domain_name, param);
						}
						
						break;
					case SEGCP_BR:
						sscanf(param, "%d", &tmp_int);
						if(param_len > 2 || tmp_int > baud_230400) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->serial_info[0].baud_rate = (uint8_t)tmp_int;
						break;
					case SEGCP_DB:
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > word_len8) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->serial_info[0].data_bits = tmp_byte;
						break;
					case SEGCP_PR:
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > parity_even) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->serial_info[0].parity = tmp_byte;
						break;
					case SEGCP_SB:
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > stop_bit2) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->serial_info[0].stop_bits = tmp_byte;
						break;
					case SEGCP_FL:
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > flow_rts_cts) ret |= SEGCP_RET_ERR_INVALIDPARAM; // ## RS422/485 will be added
						else dev_config->serial_info[0].flow_control = tmp_byte;
						break;
					case SEGCP_IT:
						sscanf(param, "%ld", &tmp_long);
						if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->network_info[0].inactivity = (uint16_t)tmp_long;
						break;
					case SEGCP_PT:
						sscanf(param, "%ld", &tmp_long);
						if(tmp_long > 0xFFFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->network_info[0].packing_time = (uint16_t)tmp_long;
						break;
					case SEGCP_PS:
						sscanf(param, "%d", &tmp_int);
						if(param_len > 3 || tmp_int > 0xFF) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->network_info[0].packing_size = (uint8_t)tmp_int;
						break;
					case SEGCP_PD:
						if(param_len != 2 || !is_hexstr(param))
						{
							ret |= SEGCP_RET_ERR_INVALIDPARAM;
						}
						else
						{
							sscanf(param,"%x", &dev_config->network_info[0].packing_delimiter[0]);
							if(dev_config->network_info[0].packing_delimiter[0] == 0x00) 
								dev_config->network_info[0].packing_delimiter_length = 0;
							else 
								dev_config->network_info[0].packing_delimiter_length = 1;
						}
						break;
					case SEGCP_TE:
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->options.serial_command = tmp_byte;
						break;
					case SEGCP_SS:
						if(param_len != 6 || !is_hexstr(param) || !str_to_hex(param, dev_config->options.serial_trigger))
						{
							printf(">> SEGCP_SS = %.2X %.2X %.2X", dev_config->options.serial_trigger[0], dev_config->options.serial_trigger[1], dev_config->options.serial_trigger[2]);
							ret |= SEGCP_RET_ERR_INVALIDPARAM;
						}
						break;
					case SEGCP_NP:
						if(param_len > sizeof(dev_config->options.pw_connect)-1)
						{
							ret |= SEGCP_RET_ERR_INVALIDPARAM;
						}
						else
						{
							if(param[0] == SEGCP_NULL) dev_config->options.pw_connect[0] = 0;
							else sscanf(param,"%s", &dev_config->options.pw_connect[0]);
						}
						break;
					case SEGCP_SP:
						if(param_len > sizeof(dev_config->options.pw_search)-1)
						{
							ret |= SEGCP_RET_ERR_INVALIDPARAM;
						}
						else
						{
							if(param[0] == SEGCP_NULL) dev_config->options.pw_search[0] = 0;
							else sscanf(param,"%s", &dev_config->options.pw_search);
						}
						break;
					case SEGCP_FW:
						sscanf(param, "%ld", &tmp_long);
						if(tmp_long > (((uint32_t)DEVICE_FWUP_SIZE) & 0x0FFFF))
						{
							dev_config->firmware_update.fwup_size = 0;
							ret |= SEGCP_RET_ERR_INVALIDPARAM;
#ifdef _SEGCP_DEBUG_
							printf("SEGCP_FW:ERROR:TOOBIG\r\n");
#endif
						}
						else
						{
							dev_config->firmware_update.fwup_size = (uint16_t)tmp_long;
							dev_config->firmware_update.fwup_flag = SEGCP_ENABLE;
							ret |= SEGCP_RET_FWUP;
							sprintf(trep,"FW%d.%d.%d.%d:%d:%d\r\n", dev_config->network_info_common.local_ip[0], dev_config->network_info_common.local_ip[1]
							,dev_config->network_info_common.local_ip[2] , dev_config->network_info_common.local_ip[3], (uint16_t)DEVICE_FWUP_PORT);
							
							close(SEG_SOCK);
#ifdef _SEGCP_DEBUG_
							printf("SEGCP_FW:OK\r\n");
#endif
						}
						break;
					case SEGCP_EC:
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else dev_config->options.serial_command_echo = tmp_byte;
						break;
					case SEGCP_UE: // User echo, Not used
						tmp_byte = is_hex(*param);
						if(param_len != 1 || tmp_byte > SEGCP_ENABLE) ret |= SEGCP_RET_ERR_INVALIDPARAM;
						else ;
						break;

					case SEGCP_UN:
					case SEGCP_ST:
					case SEGCP_LG:
					case SEGCP_ER: 
					case SEGCP_MA:
					case SEGCP_EX: 
					case SEGCP_SV:
					case SEGCP_RT:
					case SEGCP_FR:
					case SEGCP_PW:
						ret |= SEGCP_RET_ERR_NOTAVAIL;
						break;
					default:
						ret |= SEGCP_RET_ERR_NOCOMMAND;
						break;
				}
			}
			else
			{
				ret |= SEGCP_RET_ERR_NOPRIVILEGE;
			}
		}
		else
		{
			ret |= SEGCP_RET_ERR_NOCOMMAND;
		}

		if(ret & SEGCP_RET_ERR)
		{
			treq[2] = 0;
			sprintf(trep,"%s:%s\r\n",tbSEGCPERR[((ret-SEGCP_RET_ERR) >> 8)],(cmdnum!=SEGCP_UNKNOWN)? tbSEGCPCMD[cmdnum] : treq);
#ifdef _SEGCP_DEBUG_
			printf("ERROR : %s\r\n",trep);
#endif
			return ret;
		}
		
		treq = strtok(NULL, SEGCP_DELIMETER);
#ifdef _SEGCP_DEBUG_
		//printf(">> strtok: %s\r\n", treq);
#endif
	}
	
#ifdef _SEGCP_DEBUG_
	printf("\r\nEND of [proc_SEGCP] function - RET[0x%.4x]\r\n\r\n", ret);
#endif
	
	return ret;
}

uint16_t proc_SEGCP_udp(uint8_t* segcp_req, uint8_t* segcp_rep)
{
	DevConfig *dev_config = get_DevConfig_pointer();
	
	uint8_t ret = 0;   
	uint16_t len = 0;
	uint16_t i = 0;
	
	uint8_t destip[4];
	uint16_t destport;
	
	uint8_t tpar[SEGCP_PARAM_MAX*2];
	uint8_t* treq;
	uint8_t* trep;
	
	gSEGCPPRIVILEGE = SEGCP_PRIVILEGE_CLR;
	switch(getSn_SR(SEGCP_UDP_SOCK))
	{
		case SOCK_UDP:
			if((len = getSn_RX_RSR(SEGCP_UDP_SOCK)) > 0)
			{
				treq = segcp_req;
				trep = segcp_rep;
				len = recvfrom(SEGCP_UDP_SOCK, treq, len, destip, &destport);
				treq[len-1] = 0;

				if(SEGCP_MA == parse_SEGCP(treq, tpar))
				{
					if(!memcmp(tpar,"\xFF\xFF\xFF\xFF\xFF\xFF", 6)) gSEGCPPRIVILEGE |= (SEGCP_PRIVILEGE_SET | SEGCP_PRIVILEGE_READ);
					else if(!memcmp(tpar, dev_config->network_info_common.mac, sizeof(dev_config->network_info_common.mac))) gSEGCPPRIVILEGE |= (SEGCP_PRIVILEGE_SET | SEGCP_PRIVILEGE_WRITE);
					else break;

					if(gSEGCPPRIVILEGE & SEGCP_PRIVILEGE_SET)
					{
						sprintf(trep,"%s%c%c%c%c%c%c\r\n",tbSEGCPCMD[SEGCP_MA],
							dev_config->network_info_common.mac[0], dev_config->network_info_common.mac[1], dev_config->network_info_common.mac[2],
							dev_config->network_info_common.mac[3], dev_config->network_info_common.mac[4], dev_config->network_info_common.mac[5]);
						
						treq += 10;
						trep += 10;
						
						if(SEGCP_PW == parse_SEGCP(treq, tpar))
						{
							if((tpar[0] == SEGCP_NULL && dev_config->options.pw_search[0] == 0) || !strcmp(tpar, dev_config->options.pw_search))
							{
								memcpy(trep,treq, strlen(tpar)+4);  // "PWxxxx\r\n"
								treq += (strlen(tpar) + 4);
								trep += (strlen(tpar) + 4);
								ret = proc_SEGCP(treq,trep);
								sendto(SEGCP_UDP_SOCK, segcp_rep, 14+strlen(tpar)+strlen(trep), "\xFF\xFF\xFF\xFF", destport);
							}
						}
					}
					else
					{
						return 0;
					}
				}
				else
				{
					return 0;
				}
			}
			break;
		case SOCK_CLOSED:
			if(socket(SEGCP_UDP_SOCK, Sn_MR_UDP, DEVICE_SEGCP_PORT, 0x00) == SEGCP_UDP_SOCK)
			{
				;//if(dev_config->serial_info[0].serial_debug_en == SEGCP_ENABLE) printf(" > SEGCP:UDP:STARTED\r\n");
			}
			break;
	}
	return ret;
}


uint16_t proc_SEGCP_tcp(uint8_t* segcp_req, uint8_t* segcp_rep)
{
	DevConfig *dev_config = get_DevConfig_pointer();
	
	uint16_t ret = 0;
	uint16_t len = 0;
	uint16_t i = 0;
	uint8_t tpar[SEGCP_PARAM_MAX+1];
	uint8_t * treq;
	uint8_t * trep;
	
	uint8_t state = getSn_SR(SEGCP_TCP_SOCK);
	gSEGCPPRIVILEGE = SEGCP_PRIVILEGE_CLR;
	
	switch(state)
	{
		case SOCK_INIT:
			//listen(SEGCP_TCP_SOCK); Function call Immediately after socket open operation
			break;
		
		case SOCK_LISTEN:
			break;
		
		case SOCK_ESTABLISHED:
			if(getSn_IR(SEGCP_TCP_SOCK) & Sn_IR_CON)
			{
				// TCP unicast search: Keep-alive timer enable 
				enable_configtool_keepalive_timer = ENABLE;
				configtool_keepalive_time = 0;
				
				setSn_IR(SEGCP_TCP_SOCK, Sn_IR_CON); // TCP connection interrupt clear
			}
			
			if(flag_send_configtool_keepalive == SEGCP_ENABLE) // default: 15sec
			{
				send_keepalive_packet_configtool(SEGCP_TCP_SOCK);
				flag_send_configtool_keepalive = SEGCP_DISABLE; // flag clear
			}

			if((len = getSn_RX_RSR(SEGCP_TCP_SOCK)) > 0)
			{
				treq = segcp_req;
				trep = segcp_rep;
				len = recv(SEGCP_TCP_SOCK,treq,len);
				treq[len-1] = 0x00;

				if(SEGCP_MA == parse_SEGCP(treq,tpar))
				{
					if(!memcmp(tpar, "\xFF\xFF\xFF\xFF\xFF\xFF", 6)) gSEGCPPRIVILEGE |= (SEGCP_PRIVILEGE_SET | SEGCP_PRIVILEGE_READ);
					else if(!memcmp(tpar, dev_config->network_info_common.mac, sizeof(dev_config->network_info_common.mac))) gSEGCPPRIVILEGE |= (SEGCP_PRIVILEGE_SET | SEGCP_PRIVILEGE_WRITE);
					else break;
					
					if(gSEGCPPRIVILEGE & SEGCP_PRIVILEGE_SET)
					{
						sprintf(trep,"%s%c%c%c%c%c%c\r\n",tbSEGCPCMD[SEGCP_MA],
							dev_config->network_info_common.mac[0], dev_config->network_info_common.mac[1], dev_config->network_info_common.mac[2],
							dev_config->network_info_common.mac[3], dev_config->network_info_common.mac[4], dev_config->network_info_common.mac[5]);
						
						treq += 10;
						trep += 10;
						
						if(SEGCP_PW == parse_SEGCP(treq,tpar))
						{
							if((tpar[0] == SEGCP_NULL && dev_config->options.pw_search[0] == 0) || !strcmp(tpar, dev_config->options.pw_search))
							{
								memcpy(trep,treq, strlen(tpar)+4);  // "PWxxxx\r\n"
								treq += (strlen(tpar) + 4);
								trep += (strlen(tpar) + 4);
								ret = proc_SEGCP(treq,trep);
								send(SEGCP_TCP_SOCK, segcp_rep, 14+strlen(tpar)+strlen(trep));
							}
						}
					}
					else
					{
						return 0;
					}
				}
				else
				{
					return 0;
				}
			}
			break;
			
		case SOCK_CLOSE_WAIT:
			disconnect(SEGCP_TCP_SOCK);
		
		case SOCK_CLOSED:
		case SOCK_FIN_WAIT:
			close(SEGCP_TCP_SOCK);
			
			if(socket(SEGCP_TCP_SOCK, Sn_MR_TCP, DEVICE_SEGCP_PORT, Sn_MR_ND) == SEGCP_TCP_SOCK)
			{
				;//if(dev_config->serial_info[0].serial_debug_en == SEGCP_ENABLE) printf(" > SEGCP:TCP:STARTED\r\n");
				
				//Keep-alive timer keep disabled until TCP connection established.
				enable_configtool_keepalive_timer = DISABLE;
				
				listen(SEGCP_TCP_SOCK);
			}
			break;
	}
	return ret;
}

void send_keepalive_packet_configtool(uint8_t sock)
{
	setsockopt(sock, SO_KEEPALIVESEND, 0);
#ifdef _SEGCP_DEBUG_
	printf("SOCKET[%x]: SEND KEEP-ALIVE PACKET\r\n", sock);
#endif 
}

void segcp_timer_msec(void)
{
	if(enable_configtool_keepalive_timer)
	{
		if(configtool_keepalive_time < 0xFFFF) 	configtool_keepalive_time++;
		else									configtool_keepalive_time = 0;
		
		if(configtool_keepalive_time >= CONFIGTOOL_KEEPALIVE_TIME_MS)
		{
			flag_send_configtool_keepalive = SEGCP_ENABLE;
			configtool_keepalive_time = 0;
		}
	}
}

