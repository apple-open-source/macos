/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <bios.h>
#include <dos.h>
#include <stdio.h>

char bufold[512];

int main(int argc, char *argv[])
{
	void far (*reset)();
	#define CMD 2
	#define DRIVE 0x80
	#define HEAD 0
	#define TRACK 0
	#define SECT 1
	#define NSECT 1
	int result, i;
	int found;
	unsigned char *p1, *p2, key;
	int softReboot=0;
	int dontAsk=0;

	for (i=1; i<argc; i++)
	{
		if (!strcmp(argv[i],"-dontask")) dontAsk=1;
		else if (!strcmp(argv[i],"-soft")) softReboot=1;
	}

	if (!dontAsk)
	{
		printf("Do you want to reboot into NEXTSTEP?\n");
		printf("    (This will immediately terminate all processes!)\n");
		key = bioskey(0);
		if (!(key=='y' || key=='Y'))
		{
			printf("Cancelled\n");
			exit(0);
		}
	}

	result = biosdisk(CMD,DRIVE,HEAD,TRACK,SECT,NSECT,bufold);
	if (result != 0)
	{	printf("Couldn't read bootsectorread\n");
		exit(0);
	}

	p1 = bufold + 445;

	if (*p1 != 0xa7)
	{
		printf("GONEXT can only work if NeXT's BOOT0 bootsector"
			" is installed.\n");
		exit(0);
	}

	found = 0;
	p1=bufold + 446;
	for (i=0; i<4; i++)
	{ 	p2 = p1 + (i*16) + 4;
		if (*p2 == 0xa7)
		{	found=1;
			break;
		}
	}

	if (!found)
	{	printf("No NEXTSTEP partition installed\n");
		exit(0);
	}

	/* now do the proper CMOS write! */


	outportb(0x70,6);	/* select */
	key = inportb(0x71);
	key |= 0x10;
	outportb(0x70,6);	/* select */
	outportb(0x71,key);

	if (softReboot) geninterrupt(0x19);
	else
	{
		printf("'Scuse me while I kiss the sky!\n");
		reset = MK_FP(0xf000,0xfff0);
		reset();
		geninterrupt(0x19);	/* try again - shouldn't get here */
	}

	printf("Reboot failed\n");
	return -1;	/* you never get here */
}