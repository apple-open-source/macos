/*
 * IrDAUserClient.h
 *
 * to be included by both sides of the IrDA user-client interface.
 * keep free of kernel-only or C++ only includes.
 */
 
#ifndef __IrDAUserClient__
#define __IrDAUserClient__

#include "IrDAStats.h"

enum {
    kIrDAUserClientCookie   = 123       // pass to IOServiceOpen
};

enum {                                  // command bytes to send to the user client
    kIrDAUserCmd_GetLog     = 0x12,     // return irdalog buffers
    kIrDAUserCmd_GetStatus  = 0x13,     // return connection status and counters
    kIrDAUserCmd_Enable     = 0x14,     // Enable the hardware and the IrDA stack
    kIrDAUserCmd_Disable    = 0x15      // Disable the hardware and the IrDA stack
};

enum {                                  // messageType for the callback routines
    kIrDACallBack_Status    = 0x1000,   // Status Information is coming
    kIrDACallBack_Unplug    = 0x1001    // USB Device is unplugged
};

// This is the way the messages are sent from user space to kernel space:
typedef struct IrDACommand
{
    unsigned char commandID;    // one of the commands above (tbd)
    char data[1];               // this is not really one byte, it is as big as I like
				// I set it to 1 just to make the compiler happy
} IrDACommand;
typedef IrDACommand *IrDACommandPtr;

#endif // __IrDAUserClient__
