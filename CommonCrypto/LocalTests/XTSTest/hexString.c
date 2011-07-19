/*
 *  hexString.c
 *  byteutils
 *
 *  Created by Richard Murphy on 3/7/10.
 *  Copyright 2010 McKenzie-Murphy. All rights reserved.
 *
 */

#include "hexString.h"

/* utility function to convert hex character representation to their nibble (4 bit) values */
static uint8_t
nibbleFromChar(char c)
{
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	return 255;
}

/* Convert a string of characters representing a hex buffer into a series of bytes of that real value */
uint8_t
*hexStringToBytes(char *inhex)
{
	uint8_t *retval;
	uint8_t *p;
	int len, i;
	
    len = strlen(inhex) / 2;
	retval = malloc(len+1);
	for(i=0, p = (uint8_t *) inhex; i<len; i++) {
		retval[i] = (nibbleFromChar(*p) << 4) | nibbleFromChar(*(p+1));
		p += 2;
	}
    retval[len] = 0;
	return retval;
}

static char byteMap[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
static int byteMapLen = sizeof(byteMap);

/* Utility function to convert nibbles (4 bit values) into a hex character representation */
static char
nibbleToChar(uint8_t nibble)
{
	if(nibble < byteMapLen) return byteMap[nibble];
	return '*';
}

/* Convert a buffer of binary values into a hex string representation */
char
*bytesToHexString(uint8_t *bytes, size_t buflen)
{
	char *retval;
	int i;
	
	retval = malloc(buflen*2 + 1);
	for(i=0; i<buflen; i++) {
		retval[i*2] = nibbleToChar(bytes[i] >> 4);
		retval[i*2+1] = nibbleToChar(bytes[i] & 0x0f);
	}
    retval[i] = '\0';
	return retval;
}
