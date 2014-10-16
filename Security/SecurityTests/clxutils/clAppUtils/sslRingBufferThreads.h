/* 
 * sslRingBufferThreads.h - SecureTransport client and server thread
 *		routines which use ringBufferIo for I/O (no sockets).
 */
 
#include <Security/SecureTransport.h>
#include <clAppUtils/ringBufferIo.h>
#include <CoreFoundation/CoreFoundation.h>

#ifndef	_SSL_RING_BUFFER_THREADS_H_
#define _SSL_RING_BUFFER_THREADS_H_

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * arguments to client thread and server pseudothread
 */
typedef struct {
	CFArrayRef			idArray;		/* required for server, optional for client */
	CFArrayRef			trustedRoots;	/* generally from server's idArray */
	unsigned			xferSize;		/* total bytes for client to write and server to
										 * read */
	void				*xferBuf;		/* move to/from here */
	unsigned			chunkSize;		/* size of xferBuf; client writes this much at 
										 *   a time */
	bool				runForever;		/* if true, ignore xferSize and move data forever
										 *   or until error */
	SSLCipherSuite		cipherSuite;
	SSLProtocol			prot;
	RingBuffer			*ringWrite;		/* I/O writes to this... */
	RingBuffer			*ringRead;		/* ...and reads from this */
	
	/* client's goFlag is &(server's iAmReady); vice versa */
	bool				iAmReady;		/* this thread is ready for handshake */
	bool				*goFlag;		/* when both threads see this, they start
										 * their handshakes */
	bool				*abortFlag;		/* anyone sets this on error */
										/* everyone aborts when they see this true */
	bool				pauseOnError;	/* call testError() on error */
	
	/* returned on success */
	SSLProtocol			negotiatedProt;
	SSLCipherSuite		negotiatedCipher;
	
	CFAbsoluteTime		startHandshake;
	CFAbsoluteTime		startData;
	CFAbsoluteTime		endData;
} SslRingBufferArgs;

/* 
 * Client thread - handshake and write sslArgs->xferSize bytes of data.
 */
void *sslRbClientThread(void *arg);

/* 
 * Server function - like clientThread except it runs from the main thread.
 * handshake and read sslArgs->xferSize bytes of data.
 */
OSStatus sslRbServerThread(SslRingBufferArgs *sslArgs);

#ifdef __cplusplus
}
#endif

#endif	/* _SSL_RING_BUFFER_THREADS_H_*/
