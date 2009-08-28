
    /* Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

    /*****  For fans of kprintf, IOLog and debugging infrastructure of the  *****/
    /*****  string ilk, please modify the ELG and PAUSE macros or their *****/
    /*****  associated EvLog and Pause functions to suit your taste. These  *****/
    /*****  macros currently are set up to log events to a wraparound   *****/
    /*****  buffer with minimal performance impact. They take 2 UInt32  *****/
    /*****  parameters so that when the buffer is dumped 16 bytes per line, *****/
    /*****  time stamps (~1 microsecond) run down the left side while   *****/
    /*****  unique 4-byte ASCII codes can be read down the right side.  *****/
    /*****  Preserving this convention facilitates different maintainers    *****/
    /*****  using different debugging styles with minimal code clutter. *****/

#ifndef _APPLEIRDA_
#define _APPLEIRDA_

#include <IOKit/serial/IORS232SerialStreamSync.h>
#include "IrDAStats.h"

#define DEBUG       0               // for debugging
#define USE_ELG     0               // to event log - DEBUG must also be set (see below)
#define kEvLogSize  (4096*16)               // 16 pages = 64K = 4096 events
#define LOG_DATA    1               // logs data to the IOLog - DEBUG must also be set

#define Sleep_Time  20

#if DEBUG
    #define IOLogIt(A,B,ASCI,STRING)    IOLog( "AppleIrDA: %p %p " STRING "\n", (void *)(A), (void *)(B) )
    #if USE_ELG
	#define ELG(A,B,ASCI,STRING)    EvLog( (void *)(A), (void *)(B), (void *)(ASCI), STRING )
    #else /* not USE_ELG */
	#define ELG(A,B,ASCI,STRING)    {IOLog( "AppleIrDA: %p %p " STRING "\n", (void *)(A), (void *)(B) );IOSleep(Sleep_Time);}
    #endif /* USE_ELG */
    #if LOG_DATA
	#define LogData(D, C, b)    DEVLogData((UInt8)D, (UInt32)C, (char *)b)
    #else /* not LOG_DATA */
	#define LogData(D, C, b)
    #endif /* LOG_DATA */
#else /* not DEBUG */
    #define IOLogIt(A,B,ASCI,STRING)
    #define ELG(A,B,ASCI,S)
    #define LogData(D, C, b)
    #undef USE_ELG
    #undef LOG_DATA
#endif /* DEBUG */


#define initIrDAState       true            // False = off, true = on

#define baseName        "IrDA-IrCOMM"

    /* Globals  */

typedef struct IrDAglobals      /* Globals for USB module (not per instance) */
{
    UInt32       evLogFlag; // debugging only
    UInt8       *evLogBuf;
    UInt8       *evLogBufe;
    UInt8       *evLogBufp;
    UInt8       intLevel;
    class AppleIrDA *AppleIrDAInstance;
} IrDAglobals;

    
enum ParityType
{
    NoParity        = 0,
    OddParity,
    EvenParity
};
    
#define MAX_BLOCK_SIZE      PAGE_SIZE

#define kDefaultBaudRate    9600
#define kMaxBaudRate        4000000     // 4Mbs for IrDA
#define kMinBaudRate        300
#define kMaxCirBufferSize   4096
    
    /* IrDA stuff */
	
struct USBIrDAQoS               // encoded qos values as read from the irda pod (except for driver swabs of shorts)
{
    UInt8   bFunctionLength;        // descriptor header (length = 12)
    UInt8   bDescriptorType;        // descriptor type (0x21)
    UInt16  version;            // two bytes of version number, should be 0x01, 0x00 for 1.0
    UInt8   datasize;           // bytes per frame supported
    UInt8   windowsize;         // 1 thru 7 frames per ack
    UInt8   minturn;            // min turnaround time
    UInt8   baud1;              // 16 bits of baud
    UInt8   baud2;
    UInt8   bofs;               // number of bofs the pod wants
    UInt8   sniff;              // 1 if can receive at any speed
    UInt8   unicast;            // max number of entries in the unicast list
}; 
typedef struct USBIrDAQoS USBIrDAQoS;       // Todo - rename since we use this for SCC as well
    
    /* SccQueuePrimatives.h */

typedef struct CirQueue
{
    UInt8   *Start;
    UInt8   *End;
    UInt8   *NextChar;
    UInt8   *LastChar;
    size_t  Size;
    size_t  InQueue;
} CirQueue;

typedef enum QueueStatus
{
    queueNoError = 0,
    queueFull,
    queueEmpty,
    queueMaxStatus
} QueueStatus;
    
    /* Inline time conversions */
    
static inline unsigned long tval2long( mach_timespec val )
{
   return (val.tv_sec * NSEC_PER_SEC) + val.tv_nsec;
}

static inline mach_timespec long2tval( unsigned long val )
{
    mach_timespec   tval;

    tval.tv_sec  = val / NSEC_PER_SEC;
    tval.tv_nsec = val % NSEC_PER_SEC;
    return tval;
}

class IrDAComm;
class AppleIrDASerial;

class AppleIrDA : public IOService      // nub with nice name for user-client connections
{
    OSDeclareDefaultStructors(AppleIrDA)

public:
    static AppleIrDA *withNub(AppleIrDASerial *nub);        // constructor
    IOReturn    newUserClient( task_t,void*,UInt32, IOUserClient** );

private:
    AppleIrDASerial *fNub;
};



class AppleIrDASerial : public IORS232SerialStreamSync
{
    //OSDeclareAbstractStructors(AppleIrDASerial);
    OSDeclareDefaultStructors(AppleIrDASerial)
public:
    virtual void        Add_RXBytes( UInt8 *Buffer, size_t Size ) = 0;
    virtual SInt16      SetBofCount( SInt16 bof_count ) = 0;
    virtual UInt16      SetSpeed( UInt32 brate ) = 0;
    virtual bool        SetUpTransmit( void ) = 0;

    virtual IOReturn    StartTransmit( UInt32 control_length, UInt8 *control_buffer, UInt32 data_length, UInt8 *data_buffer ) = 0;
    virtual USBIrDAQoS* GetIrDAQoS( void ) = 0;
    virtual IrDAComm*   GetIrDAComm( void ) = 0;
    virtual void        GetIrDAStatus( IrDAStatus *status ) = 0;
    virtual IOReturn    SetIrDAUserClientState( bool IrDAOn ) = 0;
};


#endif
