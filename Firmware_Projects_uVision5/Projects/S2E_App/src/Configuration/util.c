#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "util.h"
/*
uint8_t str_to_ipaddr(uint8_t * ipaddr_str, uint8_t * ip)
{
	int8_t * ptr;
	uint8_t ip_cnt = 0;
	uint16_t digit, sumchk = 0;
	uint8_t tmp[16] = {0, };
	
	if(is_ipaddr(ipaddr_str))
	{
		strcpy((char*)tmp, (char*)ipaddr_str);
		ptr = (int8_t*)strtok((char*)tmp, ".");
		
		while(ptr != NULL)
		{
			digit = atoi((char *)ptr);
			if(digit > 255 || digit < 0) return 0;
			
			if(ip) ip[ip_cnt] = digit;
			sumchk += digit;
			ip_cnt++;
			ptr = (int8_t*)strtok(NULL, ".");
		}
		
		if(ip_cnt != 4 || sumchk == 0) return 0;
	}
	else
	{
		return 0;
	}
	
	return 1;
}
*/


uint8_t is_ipaddr(uint8_t * ipaddr, uint8_t * ret_ip)
{
	uint8_t i = 0;
	uint8_t dotcnt = 0;
	uint16_t tval = 0;
	uint8_t len = strlen((char *)ipaddr);
	
	uint8_t tmp[3] = {0, };
	uint8_t tmpcnt = 0;
	
	if(len > 15 || len < 7) return 0;
	
	for(i = 0; i < len ; i++)
	{
		if(isdigit(ipaddr[i]))
		{
			tval = (tval * 10) + ipaddr[i] - '0';
			if(tval > 255) return 0;
			
			// added for ret_ip arrary
			tmp[tmpcnt++] = ipaddr[i];
			if(tmpcnt > sizeof(tmp)) return 0;
		}
		else if(ipaddr[i] == '.')
		{
			if(tval > 255) return 0;
			if(++dotcnt > 4) return 0;
			tval = 0;
			
			// added for ret_ip arrary
			ret_ip[dotcnt-1] = atoi((char *)tmp);
			memset(tmp, 0x00, sizeof(tmp));
			tmpcnt = 0;
		}
		else return 0;
	}
	// added for ret_ip arrary
	ret_ip[dotcnt] = atoi((char *)tmp);
	
	return 1;
}
/*
uint8_t is_ipaddr(uint8_t * ipaddr)
{
	uint8_t dotcnt = 0;
	uint8_t i = 0;
	uint16_t tval = 0;
	uint8_t len = strlen((char *)ipaddr);
	
	if(len > 15 || len < 7) return 0;
	
	for(i = 0; i < len ; i++)
	{
		if(isdigit(ipaddr[i]))
		{
			tval = (tval * 10) + ipaddr[i] - '0';
			if(tval > 255) return 0;
		}
		else if(ipaddr[i] == '.')
		{
			if(tval > 255) return 0;
			if(++dotcnt > 4) return 0;
			tval = 0;
		}
		else return 0;
	}
	
	return 1;
}
*/

uint8_t is_hexstr(uint8_t * hexstr)
{
	uint8_t i = 0;
	
	for(i=0; i < strlen((char *)hexstr); i++)
	{
		if(!isxdigit(hexstr[i])) return 0;
	}
	return 1; 
}

uint8_t is_hex(uint8_t hex)
{
	uint8_t ret = hex;
	
	if(hex < 0x11) return 0xFF;

	if(isdigit(hex)) ret = hex - '0';
	else if(hex > '\'' && hex < 'g') ret = hex - 'a' + 0x10;
	else if(hex > '@'  && hex > 'G') ret = hex - 'A' + 0x10;
	
	return ret;
}

uint8_t is_macaddr(uint8_t * macstr, uint8_t * digitstr, uint8_t * mac)
{
	uint8_t tmp_mac[6];
	uint8_t tmp_hexstr[4];
	uint8_t i = 0;
	uint8_t len = strlen((char *)macstr);

	if(macstr[0] == 0 || len != 17) return 0;
	
	for( i = 0; i < 6; i++)
	{
		memcpy(tmp_hexstr,macstr+i*3,3);
		if(tmp_hexstr[2] == 0 || strchr((char *)digitstr, tmp_hexstr[2]))
		{
			tmp_hexstr[2] = 0;
			if(is_hexstr(tmp_hexstr))
			{
				//sscanf(tmp_hexstr,"%x", &tmp_mac[i]);
				str_to_hex(tmp_hexstr, &tmp_mac[i]);
			}
			else
				return 0;
		}
		else
			return 0;
	}
	memcpy(mac, tmp_mac, sizeof(tmp_mac));
	return 1;
}


uint8_t str_to_hex(uint8_t * str, uint8_t * hex)
{
	uint8_t i;
	uint8_t hexcnt = 0;
	uint8_t hn, ln;
	uint8_t len = strlen((char *)str);
	
	if((len & 0x01)  || (len > 16) || (*str == 0)) return 0;
	
	for(i = 0; i < len; i+=2)
	{
		hn = str[i] > '9' ? (str[i] - 'A' + 10) : (str[i] - '0');
		ln = str[i+1] > '9' ? (str[i+1] - 'A' + 10) : (str[i+1] - '0');
		
		hex[hexcnt++] = (hn << 4 ) | ln;
	}
	
	return 1;
}

/*
hex_decode(const char *in, size_t len,uint8_t *out)
{
	unsigned int i, t, hn, ln;

	for (t = 0,i = 0; i < len; i+=2,++t) {

	hn = in[i] > '9' ? in[i] - 'A' + 10 : in[i] - '0';
	ln = in[i+1] > '9' ? in[i+1] - 'A' + 10 : in[i+1] - '0';

	out[t] = (hn << 4 ) | ln;
	}

	return out;
}
*/

// Does not use
uint8_t conv_hexstr(uint8_t * hexstr, uint8_t * hexarray)
{
	//uint8_t i = 0;
	char tmp_hexstr[3];
	uint8_t len = strlen((char *)hexstr);
	
	if((len & 0x01)  || (len > 16) || (*hexstr == 0)) return 0;
	
	while(*hexstr)
	{
		memcpy(tmp_hexstr,hexstr,2);
		tmp_hexstr[2] = 0;
		sscanf(tmp_hexstr, "%x", hexarray++);
		hexstr+=2;
	}
	return 1;
}

/**
 * @brief Check strings and then execute callback function by each string.
 * @param src The information of URI
 * @param s1 The start string to be researched
 * @param s2 The end string to be researched
 * @param sub The string between s1 and s2
 * @return The length value atfer working
 */
void mid(char* src, char* s1, char* s2, char* sub)
{
	char* sub1;
	char* sub2;
	uint16_t n;

	sub1=strstr((char*)src,(char*)s1);
	sub1+=strlen((char*)s1);
	sub2=strstr((char*)sub1,(char*)s2);

	n=sub2-sub1;
	strncpy((char*)sub,(char*)sub1,n);
	sub[n]='\0';
}
