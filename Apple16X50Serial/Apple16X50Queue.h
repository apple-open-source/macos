/*
Copyright (c) 1997-2002 Apple Computer, Inc. All rights reserved.
Copyright (c) 1994-1996 NeXT Software, Inc.  All rights reserved.
 
IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (“Apple”) in consideration of your agreement to the following terms, and your use, installation, modification or redistribution of this Apple software constitutes acceptance of these terms.  If you do not agree with these terms, please do not use, install, modify or redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject to these terms, Apple grants you a personal, non-exclusive license, under Apple’s copyrights in this original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute the Apple Software, with or without modifications, in source and/or binary forms; provided that if you redistribute the Apple Software in its entirety and without modifications, you must retain this notice and the following text and disclaimers in all such redistributions of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used to endorse or promote products derived from the Apple Software without specific prior written permission from Apple.  Except as expressly stated in this notice, no other rights or licenses, express or implied, are granted by Apple herein, including but not limited to any patent rights that may be infringed by your derivative works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * 1995-06-28	dreece	Created
 * 2002-03-05	dreece	I/O Kit port, based on drvISASerialPort:queue_primatives.m.
 */

#ifndef _APPLEPCI16X50QUEUE_H
#define _APPLEPCI16X50QUEUE_H

#include "Apple16X50Serial.h"
#include <IOKit/serial/IOSerialStreamSync.h>

#define BIGGEST_EVENT		((UInt32)3)
#define QUEUE_SIZE_MINIMUM	((UInt32)64)
#define QUEUE_SIZE_DEFAULT	((UInt32)2048)
#define QUEUE_SIZE_MAXIMUM	((UInt32)32768)

enum tQueueState {
    kQueueEmpty = (PD_S_TXQ_EMPTY | PD_S_TXQ_LOW_WATER), // 0 events in queue
    kQueueLow = PD_S_TXQ_LOW_WATER,	// 1..(LowWater-1) events in queue
    kQueueMedium = 0,			// LowWater..(HighWater-1) events in queue
    kQueueHigh = PD_S_TXQ_HIGH_WATER,	// HighWater..(Size-1) events in queue
    kQueueFull = (PD_S_TXQ_HIGH_WATER | PD_S_TXQ_FULL)	// Size events in queue
};

enum tDirection { kRxQ, kTxQ };

class Apple16X50Queue
{
public:
    Apple16X50Queue(tDirection dir); // constructor
    ~Apple16X50Queue();		     // destructor
    
    UInt32	HighWater;	// high water mark in chars
    UInt32	LowWater;	// low water mark in chars
    UInt32	Size;		// maximum number of chars in queue
    UInt32	Count;		// current number of chars in queue

    void	setSize(UInt32 size=QUEUE_SIZE_DEFAULT);
    void	setLowWater(UInt32 low=0);
    void	setHighWater(UInt32 high=0);
    inline bool	enqueueThresholdExceeded() { return (Count > EnqueueThreshold); };
    inline bool	dequeueThresholdExceeded() { return (Count < DequeueThreshold); };
    void	flush();
    tQueueState getState();

    void	enqueueEvent(UInt8 event, UInt32 data=0);
    bool	enqueueEventTry(UInt8 event, UInt32 data=0);
    UInt8	dequeueEvent(UInt32 *datap=NULL);
    UInt8	peekEvent(UInt32 depth=0);
    
private:
    UInt32	EnqueueIndex;		// ptr to next enqueue element
    UInt32	DequeueIndex;		// ptr to next dequeue element
    UInt32	EnqueueThreshold;	// enqueue threshold
    UInt32	DequeueThreshold;	// dequeue threshold
    UInt16	*Queue;			// actual array of events;
    char	Name[14];
    bool	OverRun;		// if true then enqueues have failed
    
    inline void pushWord(UInt16 data);	
    inline UInt16 pullWord();
};
#endif // _APPLEPCI16X50QUEUE_H
