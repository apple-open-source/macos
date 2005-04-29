/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1994-1996 Apple Computer, Inc..  All rights reserved.
 *
 *  HardwareDefs.h - This file contains the hardware definition for the
 *		     AppleSCCSerial driver, which supports IBM Asynchronous
 *		     Communications Adapters, and compatible hardware.
 *
 * HISTORY
 * 23-June-94	Dean Reece at NeXT
 *      Created.
 */
#ifndef Hardware_defs_h
#define Hardware_defs_h
#include "Z85C30.h"

#if 0
#define	UNKNOWN		0	// No UART, or unknown type 
#define	I8250		1	// Original UART, No FIFO 
#define	NS16450		2	// Improved UART, No FIFO 
#define	NS16C1450	3	// UART with Powerdown Mode, No FIFO 
#define	NS16550		4	// UART with Broken FIFO (unusable) 
#define	NS16550C	5	// UART with working 16 byte FIFO 
#define	NS16C1550	6	// UART with Powerdown Mode & FIFO 
#define	ST16C650	7	// UART with 32 byte FIFO 
#define	I82510		8	// unusual UART with 4 byte FIFO 

/* The following define all register locations that this hardware
 * presents.  The values are relative to the base address which will
 * usually be 0x3F8(COM1), 0x2F8(COM2), 0x3E8(COM3), or 0x2E8(COM4).
 * 
 * Many ports have multiple functions, and not all meanings apply to every
 * UART we support.
 */
#define	DATA_BUFFER  ((port->Base)+0)	// R/W data buffer 
#define	RX_BUFFER    ((port->Base)+0)	//  R  of data buffer/FIFO 
#define	TX_BUFFER    ((port->Base)+0)	//  W  to data buffer/FIFO 
#define	BRG_LSB      ((port->Base)+0)	//  W  to LSB of 16 bit BRG divisor 
#define	IRQ_ENABLE   ((port->Base)+1)	// R/W IRQ control register 
#define	BRG_MSB      ((port->Base)+1)	//  W  to MSB of 16 bit BRG divisor 
#define	IRQ_ID       ((port->Base)+2)	//  R  identifies cause of interrupt 
#define	FIFO_CTRL    ((port->Base)+2)	//  W  FIFO enable/reset controls 
#define	LINE_CTRL    ((port->Base)+3)	// R/W frame parameters 
#define	MODEM_CTRL   ((port->Base)+4)	// R/W DTR, RTS, etc 
#define	LINE_STAT    ((port->Base)+5)	//  R  event reporting 
#define	MODEM_STAT   ((port->Base)+6)	// R/W DSR, CTS, DCD, etc 
#define	SCRATCH      ((port->Base)+7)	// R/W unused register 
#define	SPECIAL_CTRL ((port->Base)+7)	// R/W special control of 82510 
#define	XON1_WORD    ((port->Base)+4)	// R/W of Xon-1 of ST16C650 
#define	XON2_WORD    ((port->Base)+5)	// R/W of Xon-2 of ST16C650 
#define	XOFF1_WORD   ((port->Base)+6)	// R/W of Xoff-1 of ST16C650 
#define	XOFF2_WORD   ((port->Base)+7)	// R/W of Xoff-2 of ST16C650 


// bit masks for the IRQ_ENABLE	register:
#define	RX_DATA_AVAIL_IRQ_EN	0x01
#define	TX_DATA_EMPTY_IRQ_EN	0x02
#define	LINE_STAT_IRQ_EN	0x04
#define	MODEM_STAT_IRQ_EN	0x08


// bit masks for the IRQ_ID register:
#define	IRQ_ID_FIELD	0x0F
#define	BANK_PTR_FIELD	0x60	// 82510 only 
#define	FIFO_FIELD	0xC0	// 1655x only 

// field values for IRQ_ID_FIELD in the IRQ_ID register:
#define	MODEM_STAT_CH	0x00
#define	NO_IRQ		0x01
#define	TX_DATA_EMPTY	0x02
#define	RX_DATA_AVAIL	0x04
#define	LINE_STAT_CH	0x06
#define	TX_MACHINE	0x08	// 82510 only 
#define	TIMER_IRQ	0x0A	// 82510 only 
#define	CHR_TIMEOUT	0x0C	// 1655x only 

// field values for BANK_PTR_FIELD (82510 only) in the IRQ_ID register:
#define	COMPAT_BANK	0x00
#define	WORK_BANK	0x20
#define	CONFIG_BANK	0x40
#define	MODEM_BANK	0x60

// field values for FIFO_FIELD (1655x only) in the IRQ_ID register:
#define	FIFO_DISABLED	0x00
#define	FIFO_ENABLED	0xC0


// bit masks for the FIFO_CTRL (1655x only) register:
#define	FIFO_ENABLE	0x01
#define	RX_FIFO_RESET	0x02
#define	TX_FIFO_RESET	0x04
#define	DMA_MODE_SEL	0x08
#define	FIFO_TRIG_FIELD	0x0C

// field values for FIFO_TRIG_FIELD (1655x only) in the FIFO_CTRL register:
#define	LEVEL_01_16	0x00
#define	LEVEL_04_16	0x40
#define	LEVEL_08_16	0x80
#define	LEVEL_14_16	0xC0

// field values for FIFO_TRIG_FIELD (16650 only) in the FIFO_CTRL register:
#define	LEVEL_08_32	0x00
#define	LEVEL_16_32	0x40
#define	LEVEL_24_32	0x80
#define	LEVEL_28_32	0xC0


// bit masks for the LINE_CTRL register:
#define	WORD_LEN_FIELD	0x03
#define	STOP_LEN_FIELD	0x04
#define	PARITY_FIELD	0x38
#define	SET_BREAK	0x40
#define	DIVISOR_ACCESS	0x80

// field values for WORD_LEN_FIELD in the LINE_CTRL register:
#define	WORD_LEN_5	0x00	// do not use, broken on some chips
#define	WORD_LEN_6	0x01
#define	WORD_LEN_7	0x02
#define	WORD_LEN_8	0x03

// field values for STOP_LEN_FIELD in the LINE_CTRL register:
#define	STOP_LEN_1	0x00
#define	STOP_LEN_2	0x04

// field values for PARITY_FIELD in the LINE_CTRL register:
#define	NO_PARITY	0x00
#define	ODD_PARITY	0x08
#define	EVEN_PARITY	0x18
#define	MARK_PARITY	0x28
#define	SPACE_PARITY	0x38


// bit masks for the MODEM_CTRL register:
#define	DTR		0x01
#define	RTS		0x02
#define	OUT1		0x04	// Unused
#define	OUT2		0x08	// Gates IRQ; set to 1 to enable interrupts
#define	LOOP		0x10
#define	POWER_DOWN	0x80
#define	CLOCK_SELECT	0x80	// ST16C650 only


// bit masks for the LINE_STAT register:
#define	DATA_READY	0x01
#define	OVERRUN_ERR	0x02
#define	PARITY_ERR	0x04
#define	FRAMING_ERR	0x08
#define	GOT_BREAK	0x10
#define	TX_EMPTY	0x20
#define	TX_IDLE		0x40
#define	RX_FIFO_ERR	0x80	// 1655x only


// bit masks for the MODEM_STAT register:
#define	D_CTS		0x01
#define	D_DSR		0x02
#define	TE_RI		0x04
#define	D_DCD		0x08
#define	CTS		0x10
#define	DSR		0x20
#define	RI		0x40
#define	DCD		0x80
#endif

#endif
