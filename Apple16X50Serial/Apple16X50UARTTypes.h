/*
 Copyright (c) 1997-2002 Apple Computer, Inc. All rights reserved.
 Copyright (c) 1994-1996 NeXT Software, Inc.  All rights reserved.

IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (ÒAppleÓ) in consideration of your agreement to the following terms, and your use, installation, modification or redistribution of this Apple software constitutes acceptance of these terms.  If you do not agree with these terms, please do not use, install, modify or redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject to these terms, Apple grants you a personal, non-exclusive license, under AppleÕs copyrights in this original Apple software (the ÒApple SoftwareÓ), to use, reproduce, modify and redistribute the Apple Software, with or without modifications, in source and/or binary forms; provided that if you redistribute the Apple Software in its entirety and without modifications, you must retain this notice and the following text and disclaimers in all such redistributions of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used to endorse or promote products derived from the Apple Software without specific prior written permission from Apple.  Except as expressly stated in this notice, no other rights or licenses, express or implied, are granted by Apple herein, including but not limited to any patent rights that may be infringed by your derivative works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Apple16X50UARTTypes.h
 * Determine what type of UART is present, determine it's capabilities such as FIFO depth
 * and Master Clock frequency, and verify that it is working as expected.  These routines
 * are expected to be called prior to Apple16X50UARTSync::start(), and so they take
 * liberties with the UART registers that would probably cause problems if called while
 * the port is open and/or active.  XXX - lots more to do here; this driver needs to be
 * aware of a great many more UART variants, and should actually test the FIFO size
 * and FIFO trigger levels, since they vary from vendor to vendor.
 *
 * 2002-04-10	dreece	created.
 */

#ifndef _APPLEPCI16X50UARTTYPES_H
#define _APPLEPCI16X50UARTTYPES_H

#include "Apple16X50Serial.h"

extern const IONamedValue gUARTnames[];

enum tUART_Type {
    kUART_Unknown = 0,
    kUART_8250, 	// Original UART, No FIFO
    kUART_16450,	// Improved UART, No FIFO
    kUART_16550,	// UART with Broken FIFO (unusable)
    kUART_16550C,	// UART with working 16 byte FIFO
    kUART_16C1550,	// UART with Powerdown Mode & FIFO
    kUART_16C650,	// UART with 32 byte FIFO
    kUART_16C750,	// UART with 128 byte FIFO
    kUART_16C950,	// UART with 128 byte FIFO & auto flow control
};

// The following define all register locations that this hardware
// presents.  The values are relative to the chip's base address.
// Many ports have multiple functions, and not all meanings apply
// to every supported UART.
enum tUARTregisters {
    kREG_Data		=0,	kREG_DivisorLSB  =0,
    kREG_IRQ_Enable	=1,	kREG_DivisorMSB  =1,
    kREG_IRQ_Ident	=2,	kREG_FIFOControl =2,	kREG_FuncControl =2,
    kREG_LineControl	=3,
    kREG_ModemControl	=4,
    kREG_LineStatus	=5,
    kREG_ModemStatus	=6,
    kREG_Scratch	=7,
    kREG_Size		=8
};

enum {	// bit masks for the IRQ_ENABLE register:
    kIRQEN_RxDataAvail	=0x01,
    kIRQEN_TxDataEmpty	=0x02,
    kIRQEN_LineStatus	=0x04,
    kIRQEN_ModemStatus	=0x08
};

enum {	// bit masks for the IRQ_ID register:
    kIRQID_ModemStatus	=0x00,
    kIRQID_None		=0x01,
    kIRQID_TxDataEmpty	=0x02,
    kIRQID_RxDataAvail	=0x04,
    kIRQID_LineStatus	=0x06,
    kIRQID_CharTimeout	=0x0C,	// >=1655x only
    kIRQID_FIFOEnabled	=0xC0	// >=1655x only
};

enum { // bit masks for the FIFO_CTRL (>=1655x only) register:
    kFIFO_Enable  =0x01,
    kFIFO_ResetRx =0x02,
    kFIFO_ResetTx =0x04,
    kFIFO_01of16  =0x00,	kFIFO_08of32 =0x00,	kFIFO_001of128 =0x00,
    kFIFO_04of16  =0x40,	kFIFO_16of32 =0x40,	kFIFO_032of128 =0x40,
    kFIFO_08of16  =0x80,	kFIFO_24of32 =0x80,	kFIFO_064of128 =0x80,
    kFIFO_14of16  =0xC0,	kFIFO_28of32 =0xC0,	kFIFO_112of128 =0xC0
};

enum {	// bit masks for the LINE_CTRL register:
    kLCR_5bitData   =0x00,	kLCR_6bitData      =0x01,
    kLCR_7bitData   =0x02,	kLCR_8bitData      =0x03,
    kLCR_1bitStop   =0x00,	kLCR_2bitStop      =0x04,
    kLCR_OddParity  =0x08,	kLCR_EvenParity    =0x18,
    kLCR_MarkParity =0x28,	kLCR_SpaceParity   =0x38,
    kLCR_NoParity   =0x00,
    kLCR_SendBreak  =0x40,	kLCR_DivisorAccess =0x80
};

enum {	// bit masks for the MODEM_CTRL register:
    kMCR_DTR  =0x01,
    kMCR_RTS  =0x02,
    kMCR_Out1 =0x04,	// Wired to RI if LOOP set
    kMCR_Out2 =0x08,	// Gates IRQ; Wired to D_DCD if LOOP set
    kMCR_Loop =0x10
};

enum {	// bit masks for the LINE_STAT register:
    kLSR_DataReady	=0x01,
    kLSR_Overrun	=0x02,
    kLSR_ParityError	=0x04,
    kLSR_FramingError	=0x08,
    kLSR_GotBreak	=0x10,
    kLSR_TxDataEmpty	=0x20,
    kLSR_TxIdle		=0x40,
    kLSR_RxFIFOError	=0x80	// >=1655x only
};

enum {	// bit masks for the ModemStatus register:
    kMSR_CTS =0x10,	kMSR_CTS_Changed =0x01,
    kMSR_DSR =0x20,	kMSR_DSR_Changed =0x02,
    kMSR_RI  =0x40,	kMSR_RI_Changed  =0x04,
    kMSR_DCD =0x80,	kMSR_DCD_Changed =0x08
};

#endif //_APPLEPCI16X50UARTTYPES_H