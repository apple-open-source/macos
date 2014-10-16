/* 
 * Ring buffer I/O for sslThroughput test.
 */

#include "ringBufferIo.h"
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

/* synchronizes multi-threaded access to printf() */
pthread_mutex_t printfMutex = PTHREAD_MUTEX_INITIALIZER;

/* initialize a RingBuffer */
void ringBufSetup(
	RingBuffer *ring,
	const char *bufName, 
	size_t numElements,
	size_t bufSize)
{
	unsigned dex;
	
	memset(ring, 0, sizeof(*ring));
	ring->numElements = numElements;
	ring->elements = (RingElement *)malloc(sizeof(RingElement) * numElements);
	memset(ring->elements, 0, sizeof(RingElement) * numElements);
	for(dex=0; dex<numElements; dex++) {
		RingElement *elt = &ring->elements[dex];
		elt->buf = (unsigned char *)malloc(bufSize);
		elt->capacity = bufSize;
	}
	ring->writerDex = 0;
	ring->readerDex = 0;
	ring->bufName = bufName;
}

#define LOG_RING		0
#define LOG_RING_DUMP	0
#if		LOG_RING	

static void logRingWrite(
	RingBuffer *ring,
	size_t written,
	unsigned dex,
	void *from)
{
	pthread_mutex_lock(&printfMutex);
	printf("+++ wrote %4u bytes   to %s buf %2u\n",
		(unsigned)written, ring->bufName, dex);
	#if LOG_RING_DUMP
	{
		unsigned i;
		unsigned char *cp = (unsigned char *)from;
		
		for(i=0; i<written; i++) {
			printf("%02X ", cp[i]);
			if((i < (written - 1)) && ((i % 16) == 15)) {
				printf("\n");
			}
		}
		printf("\n");
	}
	#endif
	pthread_mutex_unlock(&printfMutex);
}

static void logRingRead(
	RingBuffer *ring,
	size_t bytesRead,
	unsigned dex,
	void *to)
{
	pthread_mutex_lock(&printfMutex);
	printf("--- read  %4u bytes from %s buf %2u\n",
		(unsigned)bytesRead, ring->bufName, dex);
	#if LOG_RING_DUMP
	{
		unsigned i;
		unsigned char *cp = (unsigned char *)to;
		
		for(i=0; i<bytesRead; i++) {
			printf("%02X ", cp[i]);
			if((i < (bytesRead - 1)) && ((i % 16) == 15)) {
				printf("\n");
			}
		}
		printf("\n");
	}
	#endif
	pthread_mutex_unlock(&printfMutex);
}

static void logRingStall(
	RingBuffer *ring,
	char *readerOrWriter,
	unsigned dex)
{
	pthread_mutex_lock(&printfMutex);
	printf("=== %s stalled on %s buf %u\n", 
		readerOrWriter, ring->bufName, dex);
	pthread_mutex_unlock(&printfMutex);
}

static void logRingClose(
	RingBuffer *ring,
	char *readerOrWriter)
{
	pthread_mutex_lock(&printfMutex);
	printf("=== %s CLOSED by %s\n", 
		ring->bufName, readerOrWriter);
	pthread_mutex_unlock(&printfMutex);
}

static void logRingReset(
	RingBuffer *ring)
{
	pthread_mutex_lock(&printfMutex);
	printf("=== %s RESET\n", ring->bufName);
	pthread_mutex_unlock(&printfMutex);
}

#else	/* LOG_RING */
#define logRingWrite(r, w, d, t)
#define logRingRead(r, b, d, t)
#define logRingStall(r, row, d)
#define logRingClose(r, row)
#define logRingReset(r)
#endif	/* LOG_RING */

void ringBufferReset(
	RingBuffer *ring)
{
	unsigned dex;
	for(dex=0; dex<ring->numElements; dex++) {
		RingElement *elt = &ring->elements[dex];
		elt->validBytes = 0;
		elt->readOffset = 0;
	}
	ring->writerDex = 0;
	ring->readerDex = 0;
	ring->closed = false;
	logRingReset(ring);
}

/* 
 * The "I/O" callbacks for SecureTransport. 
 * The SSLConnectionRef is a RingBuffers *.
 */
OSStatus ringReadFunc(
	SSLConnectionRef	connRef,
	void				*data,
	size_t				*dataLen)	/* IN/OUT */
{
	RingBuffer	*ring = ((RingBuffers *)connRef)->rdBuf;
	
	if(ring->writerDex == ring->readerDex) {
		if(ring->closed) {
			/* 
			 * Handle race condition: we saw a stall, then writer filled a 
			 * RingElement and then set closed. Make sure we read the data before
			 * handling the close event.
			 */
			if(ring->writerDex == ring->readerDex) {
				/* writer closed: ECONNRESET */
				*dataLen = 0;
				return errSSLClosedAbort;
			}
			/* else proceed to read data */
		}
		else {
			/* read stalled, writer thread is writing to our next element */
			*dataLen = 0;
			return errSSLWouldBlock;
		}
	}
	
	unsigned char *outp = (unsigned char *)data;
	size_t toMove = *dataLen;
	size_t haveMoved = 0;
	
	/* we own ring->elements[ring->readerDex] */
	do {
		/* 
		 * Read as much data as there is in the buffer, or 
		 * toMove, whichever is less
		 */
		RingElement *elt = &ring->elements[ring->readerDex];
		size_t thisMove = elt->validBytes;
		if(thisMove > toMove) {
			thisMove = toMove;
		}
		memmove(outp, elt->buf + elt->readOffset, thisMove);
		logRingRead(ring, thisMove, ring->readerDex, outp);
		if(thisMove == 0) {
			/* should never happen */
			printf("***thisMove 0!\n");
			return internalComponentErr;
		}
		elt->validBytes -= thisMove;
		elt->readOffset += thisMove;
		toMove          -= thisMove;
		haveMoved       += thisMove;
		outp			+= thisMove;
		
		if(elt->validBytes == 0) {
			/* 
			 * End of this buffer - advance to next one and keep going if it's
			 * not in use
			 */
			unsigned nextDex;
			elt->readOffset = 0;
			/* increment and wrap must be atomic from the point of 
			 * view of readerDex */
			nextDex = ring->readerDex + 1;
			if(nextDex == ring->numElements) {
				nextDex = 0;
			}
			ring->readerDex = nextDex;
		}
		if(toMove == 0) {
			/* caller got what they want */
			break;
		}
		if(ring->readerDex == ring->writerDex) {
			logRingStall(ring, "reader ", ring->readerDex);
			/* stalled */
			break;
		}
	} while(toMove);
	
	OSStatus ortn = noErr;
	if(haveMoved != *dataLen) {
		if((haveMoved == 0) && ring->closed) {
			/* writer closed: ECONNRESET */
			ortn = errSSLClosedAbort;
		}
		else {
			ortn = errSSLWouldBlock;
		}
	}
	*dataLen = haveMoved;
	return ortn;
}

/* 
 * This never returns errSSLWouldBlock - we block (spinning) if 
 * we stall because we run into the reader's element.
 * Also, each call to this function uses up at least one 
 * RingElement - we don't coalesce multiple writes into one
 * RingElement. 
 *
 * On entry, writerDex is the element we're going to write to.
 * On exit, writerDex is the element we're going to write to next, 
 * and we might stall before we update it as such. 
 */
OSStatus ringWriteFunc(
	SSLConnectionRef	connRef,
	const void			*data,
	size_t				*dataLen)	/* IN/OUT */
{
	RingBuffer	*ring = ((RingBuffers *)connRef)->wrtBuf;
	unsigned char *inp = (unsigned char *)data;
	size_t toMove = *dataLen;
	size_t haveMoved = 0;
	unsigned nextDex;
	OSStatus ortn = noErr;

	/* we own ring->elements[ring->writerDex] */
	do {
		RingElement *elt = &ring->elements[ring->writerDex];
		elt->validBytes = 0;
		
		size_t thisMove = toMove;
		if(thisMove > elt->capacity) {
			thisMove = elt->capacity;
		}
		memmove(elt->buf, inp, thisMove);
		logRingWrite(ring, thisMove, ring->writerDex, inp);

		elt->validBytes  = thisMove;
		toMove          -= thisMove;
		haveMoved       += thisMove;
		inp				+= thisMove;
		
		/* move on to next element, when it becomes available */
		nextDex = ring->writerDex + 1;
		if(nextDex == ring->numElements) {
			nextDex = 0;
		}
		if(nextDex == ring->readerDex) {
			logRingStall(ring, "writer", nextDex);
			while(nextDex == ring->readerDex) {
				/* if(ring->closed) {
					break;
				} */
				;
				/* else stall */
			}
		}
		/* we own nextDex */
		ring->writerDex = nextDex;
		if(ring->closed) {
			break;
		}
	} while(toMove);
	if(ring->closed && (haveMoved == 0)) {
		/* reader closed socket: EPIPE */
		ortn = errSSLClosedAbort;
	}
	*dataLen = haveMoved;
	return ortn;
}

/* close both sides of a RingBuffers */
void ringBuffersClose(
	RingBuffers			*rbs)
{
	if(rbs == NULL) {
		return;
	}
	if(rbs->rdBuf) {
		logRingClose(rbs->rdBuf, "reader");
		rbs->rdBuf->closed = true;
	}
	if(rbs->wrtBuf) {
		logRingClose(rbs->wrtBuf, "writer");
		rbs->wrtBuf->closed = true;
	}
}

