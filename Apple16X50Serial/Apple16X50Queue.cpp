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

#include "Apple16X50Queue.h"

#ifdef IOASSERT
#define QCHECK() 						\
    if (Size) {							\
        assert(Count <= Size);					\
        assert(EnqueueIndex < Size);				\
        assert(DequeueIndex < Size);				\
        assert(EnqueueThreshold < Size);			\
        assert(DequeueThreshold < Size);			\
        assert(HighWater < Size);				\
        assert(LowWater < HighWater);				\
        assert(((DequeueIndex+Count)==EnqueueIndex)||		\
               ((DequeueIndex+Count)==(EnqueueIndex+Size)));	\
    }
#else
#define QCHECK()
#endif

// pushWord(data)
// Adds a word to the queue.  Wrapping is handled, but overflow is not;
// Do not call this function unless there is space on the queue
inline void Apple16X50Queue::
pushWord(UInt16 data)
{
    Queue[EnqueueIndex++] = data;
    if (EnqueueIndex >= Size)
        EnqueueIndex = 0;
    Count++;
}

// pullWord(data)
// Takes a word to the queue.  Wrapping is handled, but overflow is not;
// Do not call this function unless there is data on the queue
inline UInt16 Apple16X50Queue::
pullWord()
{
    register UInt16 data;

    data = Queue[DequeueIndex++];
    if (DequeueIndex >= Size)
        DequeueIndex = 0;
    Count--;

    return data;
}

// enqueueEvent(event,[data])
// General purpose enqueue routine.  If no space is available,
// the event is dropped and an OverRun condition is established.
void Apple16X50Queue::
enqueueEvent(UInt8 event, UInt32 data=0)
{
    OverRun = !enqueueEventTry(event, data);
}

// enqueueEventTry(event,[data])
// General purpose enqueue routine.  If no space is available,
// false is returned, but no OverRun condition is established.
// A return value of true indicates the event was queued.
bool Apple16X50Queue::
enqueueEventTry(UInt8 event, UInt32 data=0)
{
    register UInt32 words = (event & PD_DATA_MASK);

    if (OverRun || ((Size-Count) < (words?words:1)))
        return false;
    pushWord((UInt16)(event|(data<<8)));
    if (words>1) {
        pushWord((UInt16)(data&0xffff));
        if (words>2)
            pushWord((UInt16)(data>>16));
    }
    QCHECK();
    return true;
}

// dequeueEvent([datap])
// returns the next element in the queue, or PD_E_EOQ if the queue is
// empty.  
UInt8 Apple16X50Queue::
dequeueEvent(UInt32 *datap=NULL)
{
    if (Count > 0) {
        register UInt16 event = pullWord();
        register UInt32 data = event>>8;
        register int words = (event & PD_DATA_MASK);
        if (words>1) { // word or long event
            data = pullWord();
            if (words>2) // long event
                data |= (pullWord() << 16);
        }
        // we just made space, so check for overrun
        if (OverRun) {
            // XXX We should coalesce OverRun events, since two in a row
            // contains no more info than a single one.  Worse, if the
            // queue is running with only one element open, we can get
            // into an unfortunate rythm that results in a steady stream
            // of OverRuns.  Coalescing would prevent that...
            pushWord(PD_E_SW_OVERRUN_ERROR);
            OverRun = false;
        }
        QCHECK();
        if (words && datap) *datap = data;
        return (event & 0xff);
    }
    return PD_E_EOQ;
}

// peekEvent([depth])
// looks ahead in the queue without removing any data.  Depth indicates how
// many elements to skip; 0 causes the next element to be returned.
// If the specified depth exceeds the available data, PD_E_EOQ is returned
UInt8 Apple16X50Queue::
peekEvent(UInt32 depth=0)
{
    register UInt16 event;
    register UInt32 tmp=DequeueIndex;

    if (depth >= Count) return PD_E_EOQ;

    while (true) {
        register UInt32 words;
        event = Queue[tmp];
        if (depth-- == 0) break;
        words = (event & PD_DATA_MASK);
        tmp += words?words:1;
        if (tmp >= Size)
            tmp -= Size;
        if (tmp == DequeueIndex) {
            event = PD_E_EOQ;
            break;
        }
    }
    return (event & 0xff);
}

tQueueState Apple16X50Queue::
getState()
{
    register tQueueState status;
    if (Count < LowWater) {
        DequeueThreshold = 1;
        if (!Count) {
            status = kQueueEmpty; // 0 events in queue
            EnqueueThreshold = 0;
        } else {
            status = kQueueLow; // 1..(LowWater-1) events in queue
            EnqueueThreshold = LowWater-1;
        }
    } else if (Count > HighWater) {
        EnqueueThreshold = Size-BIGGEST_EVENT;
        if ((!OverRun) && (Count < Size)) {
            status = kQueueHigh; // HighWater..(Size-1) events in queue
            DequeueThreshold = HighWater;
        } else {
            status = kQueueFull; // (Size) events in queue
            DequeueThreshold = Count;
        }
    } else {
        status = kQueueMedium; // LowWater..(HighWater-1) events in queue
        EnqueueThreshold = HighWater-1;
        DequeueThreshold = LowWater;
    }
    QCHECK();
//  DEBUG_IOLog("%s::getState()=%p: Size=%d Count=%d HiWat=%d LoWat=%d DqT=%d EqT=%d, DqI=%d, EqI=%d\n",
//	Name, (void*)status, (int)Size, (int)Count, (int)HighWater, (int)LowWater,
//	(int)DequeueThreshold, (int)EnqueueThreshold, (int)DequeueIndex, (int)EnqueueIndex);
    return status;
}

void Apple16X50Queue::
flush()
{
    EnqueueIndex = DequeueIndex = Count = EnqueueThreshold = 0;
    DequeueThreshold = min(Size,1);
    OverRun = false;
#ifdef DEBUG
    if (Queue && Size) bzero((void *)Queue, (size_t)(Size*sizeof(UInt16)));
#endif
}

void Apple16X50Queue::
setSize(UInt32 size)
{
    DEBUG_IOLog("%s::setSize(%d)\n", Name, (int)size);
    if (size) {
        if (size != Size) {
            if (Queue) delete [] Queue;
            Size = CONSTRAIN(QUEUE_SIZE_MINIMUM, size, QUEUE_SIZE_MAXIMUM);
            Queue = new UInt16[Size];
            DEBUG_IOLog("%s: allocated Queue[%d]\n", Name, (int)Size);
        }
        flush();
        setLowWater();
        setHighWater();
    } else {
        if (Queue) { 
	    delete [] Queue;
	    Queue = NULL;
	}
        HighWater = LowWater = Size = 0;
        flush();
    }
}

void Apple16X50Queue::
setLowWater(UInt32 low)
{
    DEBUG_IOLog("%s::setLowWater(%d)", Name, (int)low);
    if (low==0) low = Size/3;
    LowWater = CONSTRAIN(BIGGEST_EVENT, low, Size-(BIGGEST_EVENT*2));
    HighWater = max (LowWater+BIGGEST_EVENT, HighWater);
    DEBUG_IOLog("=(0..%d..%d..%d)\n", (int)LowWater, (int)HighWater, (int)Size);
    getState(); // reestablish thresholds
}

void Apple16X50Queue::
setHighWater(UInt32 high)
{
    DEBUG_IOLog("%s::setHighWater(%d)", Name, (int)high);
    if (high==0) high = (Size*2)/3;
    HighWater = CONSTRAIN(BIGGEST_EVENT*2, high, Size-BIGGEST_EVENT);
    LowWater = min (LowWater, HighWater-BIGGEST_EVENT);
    DEBUG_IOLog("=(0..%d..%d..%d)\n", (int)LowWater, (int)HighWater, (int)Size);
    getState(); // reestablish thresholds
}

Apple16X50Queue::
Apple16X50Queue(tDirection dir)
{
    Queue=NULL;
    switch (dir) {
        case kRxQ : Name = "Apple16X50RxQ"; break;
        case kTxQ : Name = "Apple16X50TxQ"; break;
    }
    DEBUG_IOLog("%s::Apple16X50Queue() constructed\n", Name);
    setSize(0);
}

Apple16X50Queue::
~Apple16X50Queue()
{
    setSize(0);
    DEBUG_IOLog("%s::Apple16X50Queue() destroyed\n", Name);
}

