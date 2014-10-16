/* 
 * ringBufferThreads.h - SecureTransport client and server thread
 *		routines which use ringBufferIo for I/O (no sockets).
 */
 
#include <Security/SecureTransport.h>
#include <Security/SecureTransportPriv.h>
#include <clAppUtils/ringBufferIo.h>
#include <CoreFoundation/CoreFoundation.h>

#ifndef	_RING_BUFFER_THREADS_H_
#define _RING_BUFFER_THREADS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define SHARED_SECRET_SIZE		32

/* 
 * arguments to client thread and server pseudothread
 */
typedef struct {
	unsigned			xferSize;		/* total bytes for client to write and server to
										 * read */
	void				*xferBuf;		/* move to/from here */
	unsigned			chunkSize;		/* size of xferBuf; client writes this much at 
										 *   a time */
	RingBuffer			*ringWrite;		/* I/O writes to this... */
	RingBuffer			*ringRead;		/* ...and reads from this */
	
	/* client's goFlag is &(server's iAmReady); vice versa */
	bool				iAmReady;		/* this thread is ready for handshake */
	bool				*goFlag;		/* when both threads see this, they start
										 * their handshakes */
	bool				*abortFlag;		/* anyone sets this on error */
										/* everyone aborts when they see this true */
	bool				pauseOnError;	/* call testError() on error */
	
	char				*hostName;		/* optional for client */

	/* EAP-specific stuff */
	unsigned char		sharedSecret[SHARED_SECRET_SIZE];
	unsigned char		*sessionTicket;	/* for client only */
	unsigned			sessionTicketLen;

	/*
 	 * setMasterSecret indicates wheter we call SSLInternalSetMasterSecretFunction().
	 * If false, the server better have a signing identity in idArray.
	 */
	bool				setMasterSecret;	
	CFArrayRef			idArray;		/* optional, server only */
	CFArrayRef			trustedRoots;	/* generally from server's idArray */
	
	/* returned on success */
	SSLProtocol			negotiatedProt;
	SSLCipherSuite		negotiatedCipher;
	Boolean				sessionWasResumed;

	CFAbsoluteTime		startHandshake;
	CFAbsoluteTime		startData;
	CFAbsoluteTime		endData;
} RingBufferArgs;

/* 
 * Client thread - handshake and write sslArgs->xferSize bytes of data.
 */
void *rbClientThread(void *arg);

/* 
 * Server function - like clientThread except it runs from the main thread.
 * handshake and read sslArgs->xferSize bytes of data.
 */
OSStatus rbServerThread(RingBufferArgs *sslArgs);

#ifdef __cplusplus
}
#endif

#endif	/* _RING_BUFFER_THREADS_H_*/
