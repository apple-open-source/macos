/* 
 * sslRingBufferThreads.cpp - SecureTransport client and server thread
 *		routines which use ringBufferIo for I/O (no sockets).
 */
 
#include "sslRingBufferThreads.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <strings.h>
#include <clAppUtils/sslAppUtils.h>
#include <utilLib/common.h>

#define LOG_TOP_IO	0
#if		LOG_TOP_IO	

static void logWrite(
	char *who,
	size_t written)
{
	pthread_mutex_lock(&printfMutex);
	printf("+++ %s wrote %4lu bytes\n", who, (unsigned long)written);
	pthread_mutex_unlock(&printfMutex);
}

static void logRead(
	char *who,
	size_t bytesRead)
{
	pthread_mutex_lock(&printfMutex);
	printf("+++ %s  read %4lu bytes\n", who, (unsigned long)bytesRead);
	pthread_mutex_unlock(&printfMutex);
}

#else	/* LOG_TOP_IO */
#define logWrite(who, w)
#define logRead(who, r)
#endif	/* LOG_TOP_IO */

/* client thread - handshake and write a ton of data */
void *sslRbClientThread(void *arg)
{
	SslRingBufferArgs	*sslArgs = (SslRingBufferArgs *)arg;
    OSStatus            ortn;
    SSLContextRef       ctx = NULL;
	RingBuffers			ringBufs = {sslArgs->ringRead, sslArgs->ringWrite};
        unsigned toMove = 0;
        unsigned thisMove;

	ortn = SSLNewContext(false, &ctx);
	if(ortn) {
		printSslErrStr("SSLNewContext", ortn);
		goto cleanup;
	} 
	ortn = SSLSetIOFuncs(ctx, ringReadFunc, ringWriteFunc);
	if(ortn) {
		printSslErrStr("SSLSetIOFuncs", ortn);
		goto cleanup;
	} 
	ortn = SSLSetConnection(ctx, (SSLConnectionRef)&ringBufs);
	if(ortn) {
		printSslErrStr("SSLSetConnection", ortn);
		goto cleanup;
	}
	ortn = SSLSetEnabledCiphers(ctx, &sslArgs->cipherSuite, 1);
	if(ortn) {
		printSslErrStr("SSLSetEnabledCiphers", ortn);
		goto cleanup;
	}
	if(sslArgs->idArray) {
		ortn = SSLSetCertificate(ctx, sslArgs->idArray);
		if(ortn) {
			printSslErrStr("SSLSetCertificate", ortn);
			goto cleanup;
		}
	}
	if(sslArgs->trustedRoots) {
		ortn = SSLSetTrustedRoots(ctx, sslArgs->trustedRoots, true);
		if(ortn) {
			printSslErrStr("SSLSetTrustedRoots", ortn);
			goto cleanup;
		}
	}
	SSLSetProtocolVersionEnabled(ctx, kSSLProtocolAll, false);
	ortn = SSLSetProtocolVersionEnabled(ctx, sslArgs->prot, true);
	if(ortn) {
		printSslErrStr("SSLSetProtocolVersionEnabled", ortn);
		goto cleanup;
	}

	/* tell main thread we're ready; wait for sync flag */
	sslArgs->iAmReady = true;
	while(!(*sslArgs->goFlag)) {
		if(*sslArgs->abortFlag) {
			goto cleanup;
		}
	}
	
	/* GO handshake */
	sslArgs->startHandshake = CFAbsoluteTimeGetCurrent();
	do {   
		ortn = SSLHandshake(ctx);
		if(*sslArgs->abortFlag) {
			goto cleanup;
		}
    } while (ortn == errSSLWouldBlock);

	if(ortn) {
		printSslErrStr("SSLHandshake", ortn);
		goto cleanup;
	}

	SSLGetNegotiatedCipher(ctx, &sslArgs->negotiatedCipher);
	SSLGetNegotiatedProtocolVersion(ctx, &sslArgs->negotiatedProt);

	sslArgs->startData = CFAbsoluteTimeGetCurrent();

	toMove = sslArgs->xferSize;
	
	if(toMove == 0) {
		sslArgs->endData = sslArgs->startData;
		goto cleanup;
	}
	
	/* GO data xfer */
	do {
		thisMove = sslArgs->chunkSize;
		if(thisMove > toMove) {
			thisMove = toMove;
		}
		size_t moved;
		ortn = SSLWrite(ctx, sslArgs->xferBuf, thisMove, &moved);
		/* should never fail - implemented as blocking */
		if(ortn) {
			printSslErrStr("SSLWrite", ortn);
			goto cleanup;
		}
		logWrite("client", moved);
		if(!sslArgs->runForever) {
			toMove -= moved;
		}
		if(*sslArgs->abortFlag) {
			goto cleanup;
		}
	} while(toMove || sslArgs->runForever);

	sslArgs->endData = CFAbsoluteTimeGetCurrent();
	
cleanup:
	if(ortn) {
		*sslArgs->abortFlag = true;
	}
	if(*sslArgs->abortFlag && sslArgs->pauseOnError) {
		/* abort for any reason - freeze! */
		testError(CSSM_FALSE);
	}
	if(ctx) {
		SSLClose(ctx);
		SSLDisposeContext(ctx);
	}
	if(ortn) {
		printf("***Client thread returning %lu\n", (unsigned long)ortn);
	}
	pthread_exit((void*)ortn);
	/* NOT REACHED */
	return (void *)ortn;

}

/* server function - like clientThread except it runs from the main thread */
/* handshake and read a ton of data */
OSStatus sslRbServerThread(SslRingBufferArgs *sslArgs)
{
    OSStatus            ortn;
    SSLContextRef       ctx = NULL;
	RingBuffers			ringBufs = {sslArgs->ringRead, sslArgs->ringWrite};
        unsigned toMove = 0;
        unsigned thisMove;

	ortn = SSLNewContext(true, &ctx);
	if(ortn) {
		printSslErrStr("SSLNewContext", ortn);
		goto cleanup;
	} 
	ortn = SSLSetIOFuncs(ctx, ringReadFunc, ringWriteFunc);
	if(ortn) {
		printSslErrStr("SSLSetIOFuncs", ortn);
		goto cleanup;
	} 
	ortn = SSLSetConnection(ctx, (SSLConnectionRef)&ringBufs);
	if(ortn) {
		printSslErrStr("SSLSetConnection", ortn);
		goto cleanup;
	}
	ortn = SSLSetEnabledCiphers(ctx, &sslArgs->cipherSuite, 1);
	if(ortn) {
		printSslErrStr("SSLSetEnabledCiphers", ortn);
		goto cleanup;
	}
	if(sslArgs->idArray) {
		ortn = SSLSetCertificate(ctx, sslArgs->idArray);
		if(ortn) {
			printSslErrStr("SSLSetCertificate", ortn);
			goto cleanup;
		}
	}
	if(sslArgs->trustedRoots) {
		ortn = SSLSetTrustedRoots(ctx, sslArgs->trustedRoots, true);
		if(ortn) {
			printSslErrStr("SSLSetTrustedRoots", ortn);
			goto cleanup;
		}
	}
	SSLSetProtocolVersionEnabled(ctx, kSSLProtocolAll, false);
	ortn = SSLSetProtocolVersionEnabled(ctx, sslArgs->prot, true);
	if(ortn) {
		printSslErrStr("SSLSetProtocolVersionEnabled", ortn);
		goto cleanup;
	}
	
	/* tell client thread we're ready; wait for sync flag */
	sslArgs->iAmReady = true;
	while(!(*sslArgs->goFlag)) {
		if(*sslArgs->abortFlag) {
			goto cleanup;
		}
	}
	
	/* GO handshake */
	sslArgs->startHandshake = CFAbsoluteTimeGetCurrent();
	do {   
		ortn = SSLHandshake(ctx);
		if(*sslArgs->abortFlag) {
			goto cleanup;
		}
    } while (ortn == errSSLWouldBlock);

	if(ortn) {
		printSslErrStr("SSLHandshake", ortn);
		goto cleanup;
	}

	SSLGetNegotiatedCipher(ctx, &sslArgs->negotiatedCipher);
	SSLGetNegotiatedProtocolVersion(ctx, &sslArgs->negotiatedProt);

	sslArgs->startData = CFAbsoluteTimeGetCurrent();

	toMove = sslArgs->xferSize;
	
	if(toMove == 0) {
		sslArgs->endData = sslArgs->startData;
		goto cleanup;
	}

	/* GO data xfer */
	do {
		thisMove = sslArgs->xferSize;
		if(thisMove > toMove) {
			thisMove = toMove;
		}
		size_t moved;
		ortn = SSLRead(ctx, sslArgs->xferBuf, thisMove, &moved);
		switch(ortn) {
			case noErr:
				break;
			case errSSLWouldBlock:
				/* cool, try again */
				ortn = noErr;
				break;
			default:
				break;
		}
		if(ortn) {
			printSslErrStr("SSLRead", ortn);
			goto cleanup;
		}
		logRead("server", moved);
		if(!sslArgs->runForever) {
			toMove -= moved;
		}
		if(*sslArgs->abortFlag) {
			goto cleanup;
		}
	} while(toMove || sslArgs->runForever);

	sslArgs->endData = CFAbsoluteTimeGetCurrent();
	
cleanup:
	if(ortn) {
		*sslArgs->abortFlag = true;
	}
	if(*sslArgs->abortFlag && sslArgs->pauseOnError) {
		/* abort for any reason - freeze! */
		testError(CSSM_FALSE);
	}
	if(ctx) {
		SSLClose(ctx);
		SSLDisposeContext(ctx);
	}
	if(ortn) {
		printf("***Server thread returning %lu\n", (unsigned long)ortn);
	}
	return ortn;
}
