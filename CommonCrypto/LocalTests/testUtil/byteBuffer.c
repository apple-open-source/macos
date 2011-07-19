/*
 *  printByteBuffer.c
 *  byteutils
 *
 *  Created by Richard Murphy on 3/7/10.
 *  Copyright 2010 McKenzie-Murphy. All rights reserved.
 *
 */

#include "byteBuffer.h"

void printBytes(uint8_t *buff, size_t len, char *name)
{
	int i;
	printf("Dumping %d bytes from %s\n", (int) len, name);
	for(i=0; i<len; i++) {
		if(i > 0 && !(i%8)) putchar(' ');
		if(i > 0 && !(i%64)) putchar('\n');
		printf("%02x", buff[i]);
	}
	putchar('\n');
}

void printByteBuffer(byteBuffer bb, char *name)
{
    printBytes(bb->bytes, bb->len, name);
}


byteBuffer
mallocByteBuffer(size_t len)
{
	byteBuffer retval;
	if((retval = (byteBuffer) malloc(sizeof(byteBufferStruct) + len + 1)) == NULL) return NULL;
    retval->len = len;
    retval->bytes = (uint8_t *) (retval + 1) ; /* just past the byteBuffer in malloc'ed space */
    return retval;
}

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
byteBuffer
hexStringToBytes(char *inhex)
{
	byteBuffer retval;
	uint8_t *p;
	int len, i;
	
	len = strlen(inhex) / 2;
	if((retval = mallocByteBuffer(len)) == NULL) return NULL;
    
	for(i=0, p = (uint8_t *) inhex; i<len; i++) {
		retval->bytes[i] = (nibbleFromChar(*p) << 4) | nibbleFromChar(*(p+1));
		p += 2;
	}
    retval->bytes[len] = 0;
	return retval;
}

byteBuffer
bytesToBytes(void *bytes, size_t len)
{
    byteBuffer retval = mallocByteBuffer(len);
    memcpy(retval->bytes, bytes, len);
    return retval;
}

int
bytesAreEqual(byteBuffer b1, byteBuffer b2)
{
    if(b1->len != b2->len) return 0;
    return (memcmp(b1->bytes, b2->bytes, b1->len) == 0);
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
*bytesToHexString(byteBuffer bb)
{
	char *retval;
	int i;
	
	retval = malloc(bb->len*2 + 1);
	for(i=0; i<bb->len; i++) {
		retval[i*2] = nibbleToChar(bb->bytes[i] >> 4);
		retval[i*2+1] = nibbleToChar(bb->bytes[i] & 0x0f);
	}
    retval[bb->len*2] = 0;
	return retval;
}

