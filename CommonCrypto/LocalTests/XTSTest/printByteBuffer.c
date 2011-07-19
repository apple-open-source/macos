/*
 *  printByteBuffer.c
 *  byteutils
 *
 *  Created by Richard Murphy on 3/7/10.
 *  Copyright 2010 McKenzie-Murphy. All rights reserved.
 *
 */

#include "printByteBuffer.h"

void printByteBuffer(uint8_t *buff, size_t len, char *name)
{
	int i;
	printf("Dumping %d bytes from %s\n", len, name);
	for(i=0; i<len; i++) {
		if(i > 0 && !(i%8)) putchar(' ');
		if(i > 0 && !(i%64)) putchar('\n');
		printf("%02x", buff[i]);
	}
	putchar('\n');
}
