/*
Copyright (c) 1997-2002 Apple Computer, Inc. All rights reserved.
Copyright (c) 1994-1996 NeXT Software, Inc.  All rights reserved.
 
IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (ÒAppleÓ) in consideration of your agreement to the following terms, and your use, installation, modification or redistribution of this Apple software constitutes acceptance of these terms.  If you do not agree with these terms, please do not use, install, modify or redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject to these terms, Apple grants you a personal, non-exclusive license, under AppleÕs copyrights in this original Apple software (the ÒApple SoftwareÓ), to use, reproduce, modify and redistribute the Apple Software, with or without modifications, in source and/or binary forms; provided that if you redistribute the Apple Software in its entirety and without modifications, you must retain this notice and the following text and disclaimers in all such redistributions of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used to endorse or promote products derived from the Apple Software without specific prior written permission from Apple.  Except as expressly stated in this notice, no other rights or licenses, express or implied, are granted by Apple herein, including but not limited to any patent rights that may be infringed by your derivative works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * Apple16X50UARTTypes.cpp
 * Determine what type of UART is present, determine it's capabilities such as FIFO depth
 * and Master Clock frequency, and verify that it is working as expected.  These routines
 * are expected to be called prior to Apple16X50UARTSync::start(), and so
 *
 * 2002-04-10	dreece	created.
 */

#include "Apple16X50UARTTypes.h"
#include "Apple16X50BusInterface.h"

extern const IONamedValue gUARTnames[] = {
    { kUART_Unknown,	"Unknown" },	// Unknown UART
    { kUART_8250, 	"i8250" },	// Original ACE UART
    { kUART_16450,	"16450" },	// Improved UART, No FIFO
    { kUART_16550,	"16550" },	// Broken FIFO (unusable)
    { kUART_16550C,	"16550AF/C/CF"},// Working 16 byte FIFO
    { kUART_16C1550,	"16C1550" },	// Powerdown Mode & FIFO
    { kUART_16C650,	"16C650" },	// 32 byte FIFO
    { kUART_16C750,	"16C750" },	// 128 byte FIFO
    { kUART_16C950,	"16C950" },	// 128 byte FIFO
};

#define INB(reg)	( provider->getReg((reg), refCon) )
#define OUTB(reg,val)	( provider->setReg((reg), val, refCon) )

/* identifyUART() - This function attempts to identify the type of UART
 * present.  This routine is only intended to be run from the init method, before the chip is
 * active.
 */
tUART_Type //Apple16X50UARTSync::
identifyUART(Apple16X50BusInterface *provider, void *refCon)
{
    register UInt32 tmp;
 
    DEBUG_IOLog("Apple16X50UARTSync::identifyUART()\n");
    
    /* Verify that the BRG Divisor Register is accessable */
    OUTB(kREG_LineControl, kLCR_DivisorAccess);	/* Set DLAB=1 */
    OUTB(kREG_DivisorLSB, 0x5a);
    if (INB(kREG_DivisorLSB)!=0x5a) return kUART_Unknown;
    OUTB(kREG_DivisorLSB, 0xa5);
    if (INB(kREG_DivisorLSB)!=0xa5) return kUART_Unknown;
    OUTB(kREG_LineControl, 0x00);	/* Set DLAB=0 */
    DEBUG_IOLog("Apple16X50UARTSync::identifyUART() BRG Divisor is accessable\n");
    
    /* Verify that the Scratch Pad Register is accessable */
    OUTB(kREG_Scratch, 0x5a);
    if (INB(kREG_Scratch)!=0x5a) return kUART_8250;
    OUTB(kREG_Scratch, 0xa5);
    if (INB(kREG_Scratch)!=0xa5) return kUART_8250;
    DEBUG_IOLog("Apple16X50UARTSync::identifyUART() Scratchpad is accessable\n");
    
    /* Look for FIFO Bits in FIFO Control Register */
    OUTB(kREG_FIFOControl, kFIFO_Enable);
    tmp = INB(kREG_IRQ_Ident) & kIRQID_FIFOEnabled;
    OUTB(kREG_FIFOControl, 0x00);
    switch (tmp) {
        case 0x00 : return kUART_16450;
        case 0x40 : return kUART_16C650;
        case 0x80 : return kUART_16550;
        case 0xC0 : break;
    }
    DEBUG_IOLog("Apple16X50UARTSync::identifyUART() FIFO Enable status returned 0xC0\n");

    // Check for an alternate version of the Scratchpad...
    OUTB(kREG_LineControl, 0x00);
    OUTB(kREG_Scratch, 0xde);
    OUTB(kREG_LineControl, kLCR_DivisorAccess);
    OUTB(kREG_Scratch, 0xa9);
    tmp = INB(kREG_Scratch);
    OUTB(kREG_Scratch, 0x00);
    OUTB(kREG_LineControl, 0x00);
    if ((INB(kREG_Scratch) == 0xde) && (tmp == 0xa9)) {
        DEBUG_IOLog("Apple16X50UARTSync::identifyUART() Alternate Scratchpad is accessable\n");
        return kUART_16C650;
    }

    // Check for power-down bit in the 16X1550
    tmp = INB(kREG_ModemControl) & 0x80;
    OUTB(kREG_ModemControl, 0x00);
    if (tmp == 0x80) return kUART_16C1550;

    // Look for the FCR, which is present only in the 16750 and later
    OUTB(kREG_FIFOControl, kFIFO_Enable);
    OUTB(kREG_LineControl, 0x00);	/* Set DLAB=0 */
    OUTB(kREG_LineControl, 0xbf);	/* Set DLAB=1 */
    tmp = INB(kREG_LineControl);
    OUTB(kREG_LineControl, 0x00);	/* Set DLAB=0 */
    OUTB(kREG_FIFOControl, 0x00);
    if (tmp == 0x80) {
        DEBUG_IOLog("Apple16X50UARTSync::identifyUART() 0xBF Magic number accepted!\n");
        return kUART_16C750;
    }
    
    return kUART_16550C;
}

// XXX Need to move a few other functions here, like determineMasterClock()

#undef INB
#undef OUTB
