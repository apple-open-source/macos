/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
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
/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * 			INTEL CORPORATION PROPRIETARY INFORMATION
 *
 *	This software is supplied under the terms of a license  agreement or 
 *	nondisclosure agreement with Intel Corporation and may not be copied 
 *	nor disclosed except in accordance with the terms of that agreement.
 *
 *	Copyright 1988, 1989 Intel Corporation
 */

/*
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */

#include "libsaio.h"

/*
 * keyboard controller (8042) I/O port addresses
 */
#define PORT_A      0x60    /* port A */
#define PORT_B      0x64    /* port B */

/*
 * keyboard controller command
 */
#define CMD_WOUT    0xd1    /* write controller's output port */

/*
 * keyboard controller status flags
 */
#define KB_INFULL   0x2     /* input buffer full */
#define KB_OUTFULL  0x1     /* output buffer full */

#define KB_A20      0x9f    /* enable A20,
                               enable output buffer full interrupt
                               enable data line
                               disable clock line */

//==========================================================================
// Enable A20 gate to be able to access memory above 1MB

void enableA20()
{
    /* make sure that the input buffer is empty */
    while (inb(PORT_B) & KB_INFULL);

    /* make sure that the output buffer is empty */
    if (inb(PORT_B) & KB_OUTFULL)
        (void)inb(PORT_A);

    /* make sure that the input buffer is empty */
    while (inb(PORT_B) & KB_INFULL);

    /* write output port */
    outb(PORT_B, CMD_WOUT);
    delay(100);

    /* wait until command is accepted */
    while (inb(PORT_B) & KB_INFULL);

    outb(PORT_A, KB_A20);
    delay(100);

    while (inb(PORT_B) & KB_INFULL);   /* wait until done */
}

