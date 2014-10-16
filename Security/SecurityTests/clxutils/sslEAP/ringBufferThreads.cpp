/* 
 * ringBufferThreads.cpp - SecureTransport client and server thread
 *		routines which use ringBufferIo for I/O (no sockets).
 * 
 * Customized for EAP-FAST testing; uses SSLInternalSetMasterSecretFunction()
 * and SSLInternalSetSessionTicket().
 */
 
#include "ringBufferThreads.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <strings.h>
#include <clAppUtils/sslAppUtils.h>
#include <utilLib/common.h>
#include <CommonCrypto/CommonDigest.h>

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

/* 
 * Callback from ST to calculate master secret. 
 * We do a poor person's T_PRF(), taking the hash of:
 *
 *   serverRandom | clientRandom | sharedSecret
 *
 * ...to prove that both sides can come up with a master secret 
 * independently, using both sides' random values and the shared secret
 * supplied by the app. 
 *
 * We happen to have a digest that produces the required number 
 * of bytes (48)...
 */
static void sslMasterSecretFunction(
	SSLContextRef ctx,
	const void *arg,		/* actually a RingBufferArgs */
	void *secret,			/* mallocd by caller, SSL_MASTER_SECRET_SIZE */
	size_t *secretLength)	/* in/out */
{
	RingBufferArgs *sslArgs = (RingBufferArgs *)arg;
	if(*secretLength < SSL_MASTER_SECRET_SIZE) {
		printf("**Hey! insufficient space for master secret!\n");
		return;
	}

	unsigned char r[SSL_CLIENT_SRVR_RAND_SIZE];
	size_t rSize = SSL_CLIENT_SRVR_RAND_SIZE;
	CC_SHA512_CTX digestCtx;
	CC_SHA384_Init(&digestCtx);
	SSLInternalServerRandom(ctx, r, &rSize);
	CC_SHA384_Update(&digestCtx, r, rSize);
	SSLInternalClientRandom(ctx, r, &rSize);
	CC_SHA384_Update(&digestCtx, r, rSize);
	CC_SHA384_Update(&digestCtx, sslArgs->sharedSecret, SHARED_SECRET_SIZE);
	CC_SHA384_Final((unsigned char *)secret, &digestCtx);
	*secretLength = CC_SHA384_DIGEST_LENGTH;
}

/* client thread - handshake and write some data */
void *rbClientThread(void *arg)
{
	RingBufferArgs		*sslArgs = (RingBufferArgs *)arg;
    OSStatus            ortn;
    SSLContextRef       ctx = NULL;
	RingBuffers			ringBufs = {sslArgs->ringRead, sslArgs->ringWrite};
	char				sessionID[MAX_SESSION_ID_LENGTH];
	size_t				sessionIDLen = MAX_SESSION_ID_LENGTH;
	unsigned			toMove;
	unsigned			thisMove;
	
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
	/* EAP is TLS only - disable the SSLv2-capable handshake */
	ortn = SSLSetProtocolVersionEnabled(ctx, kSSLProtocol2, false);
	if(ortn) {
		printSslErrStr("SSLSetProtocolVersionEnabled", ortn);
		goto cleanup;
	}
	ortn = SSLInternalSetMasterSecretFunction(ctx, sslMasterSecretFunction, sslArgs);
	if(ortn) {
		printSslErrStr("SSLInternalSetMasterSecretFunction", ortn);
		goto cleanup;
	}
	ortn = SSLInternalSetSessionTicket(ctx, sslArgs->sessionTicket, 
		sslArgs->sessionTicketLen);
	if(ortn) {
		printSslErrStr("SSLInternalSetSessionTicket", ortn);
		goto cleanup;
	}
	if(sslArgs->trustedRoots) {
		ortn = SSLSetTrustedRoots(ctx, sslArgs->trustedRoots, true);
		if(ortn) {
			printSslErrStr("SSLSetTrustedRoots", ortn);
			goto cleanup;
		}
	}
	if(sslArgs->hostName) {
		ortn = SSLSetPeerDomainName(ctx, sslArgs->hostName, strlen(sslArgs->hostName));
		if(ortn) {
			printSslErrStr("SSLSetPeerDomainName", ortn);
			goto cleanup;
		}
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

	ortn = SSLGetResumableSessionInfo(ctx, &sslArgs->sessionWasResumed, sessionID, &sessionIDLen);
	if(ortn) {
		printSslErrStr("SSLGetResumableSessionInfo", ortn);
		goto cleanup;
	}
	
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
		toMove -= moved;
		if(*sslArgs->abortFlag) {
			goto cleanup;
		}
	} while(toMove);

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
/* handshake and read some data */
OSStatus rbServerThread(RingBufferArgs *sslArgs)
{
    OSStatus            ortn;
    SSLContextRef       ctx = NULL;
	RingBuffers			ringBufs = {sslArgs->ringRead, sslArgs->ringWrite};
	char				sessionID[MAX_SESSION_ID_LENGTH];
	size_t				sessionIDLen = MAX_SESSION_ID_LENGTH;
	unsigned			toMove;
	unsigned			thisMove;
	
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
	if(sslArgs->setMasterSecret) {
		ortn = SSLInternalSetMasterSecretFunction(ctx, sslMasterSecretFunction, sslArgs);
		if(ortn) {
			printSslErrStr("SSLInternalSetMasterSecretFunction", ortn);
			goto cleanup;
		}
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
	ortn = SSLGetResumableSessionInfo(ctx, &sslArgs->sessionWasResumed, sessionID, &sessionIDLen);
	if(ortn) {
		printSslErrStr("SSLGetResumableSessionInfo", ortn);
		goto cleanup;
	}

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
		toMove -= moved;
		if(*sslArgs->abortFlag) {
			goto cleanup;
		}
	} while(toMove);

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
