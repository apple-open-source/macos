/* 
 * Ring buffer I/O for sslThroughput test.
 */
 
#ifndef	_RING_BUFFER_IO_
#define _RING_BUFFER_IO_

#include <sys/types.h>
#include <pthread.h>
#include <MacTypes.h>
#include <Security/SecureTransport.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reasonable defaults for Ring Buffer params */
#define DEFAULT_NUM_RB_BUFS	16
#define DEFAULT_BUF_RB_SIZE	2048		/* in the ring buffers */


/* one element in a ring buffer */
typedef struct {
	size_t			validBytes;		// bytes written and not yet consumed
	size_t			readOffset;		// next byte to be read from this offset
	size_t			capacity;		// mallocd size of buf
	unsigned char	*buf;
} RingElement;

/* 
 * A ring buffer shared between one writer thread and one reader thread.
 * Per the DeMoney Theorem, we don't need to provide any locking between
 * the two threads if we have the appropriate protocol, which is as follows:
 *
 * -- the RingElements at which the reader and writer are currently
 *    processing are indicated by readerDex and writerDex.
 * -- the writer thread never advances writerDex to a RingElement
 *    currently in use by the reader thread.
 * -- the reader thread can advance to a RingElement in use by 
 *    the writer thread, but it can't read from that RingElement
 *    until the writer thread has advanced past that RingElement. 
 */
typedef struct {
	size_t			numElements;
	RingElement		*elements;
	unsigned		writerDex;		// writer thread is working on this one
	unsigned		readerDex;		// read thread is working on this one 
	const char		*bufName;		// e.g. serverToClient

	/* 
	 * Flag to emulate closing of socket. There's only one since the thread
	 * that sets this presumably will not be reading from or writing to
	 * this RingBuffer again; the "other" thread will detect this and abort
	 * as appropriate.
	 */
	bool 			closed;
} RingBuffer;

/* 
 * A pair of RingBuffer ptrs suitable for use as the SSLConnectionRef
 * for ringReadFunc() and ringWriteFunc().
 */
typedef struct {
	RingBuffer		*rdBuf;
	RingBuffer		*wrtBuf;
} RingBuffers;

void ringBufSetup(
	RingBuffer *ring,
	const char *bufName, 
	size_t numElements,
	size_t bufSize);

void ringBufferReset(
	RingBuffer *ring);
	
/* 
 * The "I/O" callbacks for SecureTransport. 
 * The SSLConnectionRef is a RingBuffers *.
 */
OSStatus ringReadFunc(
	SSLConnectionRef	connRef,
	void				*data,
	size_t				*dataLen);	/* IN/OUT */
OSStatus ringWriteFunc(
	SSLConnectionRef	connRef,
	const void			*data,
	size_t				*dataLen);	/* IN/OUT */

/* close both sides of a RingBuffers */
void ringBuffersClose(
	RingBuffers			*rbs);

/* to coordinate stdio from multi threads */
extern pthread_mutex_t printfMutex;

#ifdef __cplusplus
}
#endif

#endif	/* _RING_BUFFER_IO_ */
