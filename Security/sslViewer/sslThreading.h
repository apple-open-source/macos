/*
 * Copyright (c) 2006-2007,2013 Apple Inc. All Rights Reserved.
 *
 * sslThreading.h - support for two-threaded SSL client/server tests.
 */
 
#ifndef _SSL_THREADING_H_
#define _SSL_THREADING_H_ 1

#include <Security/SecureTransport.h>
#include <Security/Security.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* "Don't bother verifying" values */
#define SSL_PROTOCOL_IGNORE		((SSLProtocol)0x123456)
#define SSL_CLIENT_CERT_IGNORE	((SSLClientCertificateState)0x234567)
#define SSL_CIPHER_IGNORE		((SSLCipherSuite)0x345678)

/*
 * Test params passed to both sslClient() and sslServer()
 */
typedef struct {
	
	/* client side only */
	const char					*hostName;
	bool						skipHostNameCheck;	
	
	/* common */
	unsigned short				port;
	SSLProtocol					tryVersion;			// only used if acceptedProts
													//   NULL
	const char					*acceptedProts;
	const char					*myCertKcName;		// required for server, 
													//   optional for client
	const char					*password;			// optional, to unlock keychain
	bool						idIsTrustedRoot;	// cert in KC is trusted root
	bool						disableCertVerify;
	const char					*anchorFile;		// to add/replace anchors
	bool						replaceAnchors;
	SSLAuthenticate				authenticate;
	bool						resumeEnable;
	const SSLCipherSuite 		*ciphers;			// optional array of allowed ciphers, 
													// terminated with SSL_NO_SUCH_CIPHERSUITE
	bool						nonBlocking;
	const unsigned char			*dhParams;			// optional Diffie-Hellman params
	unsigned					dhParamsLen;

	/* expected results */
	OSStatus					expectRtn;
	SSLProtocol					expectVersion;
	SSLClientCertificateState	expectCertState;
	SSLCipherSuite				expectCipher;
	
	/* UI parameters */
	bool						quiet;
	bool						silent;
	bool						verbose;
	
	/* 
	 * Server semaphore: 
	 *
	 * -- main thread inits and sets serverRady false
	 * -- main thread starts up server thread
	 * -- server thread inits and sets of a socket for listening
	 * -- serrver thread sets serverReady true and does pthread_cond_broadcast
	 */
	pthread_mutex_t				pthreadMutex;
	pthread_cond_t				pthreadCond;
	bool						serverReady;
	/* 
	 * To ensure error abort is what we expect instead of just "
	 * peer closed their socket", server avoids closing down the
	 * socket until client sets this flag. It's just polled, no
	 * locking. Setting the serverAbort flag skips this 
	 * step to facilitate testing cases where server explicitly
	 * drops connection (e.g. in response to an unacceptable 
	 * ClientHello). 
	 */
	unsigned					clientDone;
	bool						serverAbort;
	
	/* 
	 * Returned and also verified by sslRunSession().
	 * Conditions in which expected value NOT verified are listed
	 * in following comments.
	 *
	 * NegCipher is only verified if (ortn == errSecSuccess). 
	 */
	SSLProtocol					negVersion;		// SSL_PROTOCOL_IGNORE
	SSLCipherSuite				negCipher;		// SSL_CIPHER_IGNORE
	SSLClientCertificateState 	certState;		// SSL_CLIENT_CERT_IGNORE
	OSStatus					ortn;			// always checked

} SslAppTestParams;

/* client and server in sslClient.cpp and sslServe.cpp */
OSStatus sslAppClient(
	SslAppTestParams		*params);
OSStatus sslAppServe(
	SslAppTestParams		*params);

/*
 * Run one session, with the server in a separate thread.
 * On entry, serverParams->port is the port we attempt to run on;
 * the server thread may overwrite that with a different port if it's 
 * unable to open the port we specify. Whatever is left in 
 * serverParams->port is what's used for the client side. 
 */
int sslRunSession(
	SslAppTestParams	*serverParams,
	SslAppTestParams 	*clientParams,
	const char 			*testDesc);

void sslShowResult(
	char				*whichSide,		// "client" or "server"
	SslAppTestParams	*params);


/*
 * Macros which do the repetetive setup/run work
 */
#define SSL_THR_SETUP(serverParams, clientParams, clientDefaults, serverDefault) \
{										\
	unsigned short serverPort;			\
	serverPort = serverParams.port + 1;	\
	clientParams = clientDefaults; 		\
	serverParams = serverDefaults;		\
	serverParams.port = serverPort;		\
}

#define SSL_THR_RUN(serverParams, clientParams, desc, ourRtn)	\
{																\
	thisRtn = sslRunSession(&serverParams, &clientParams, desc);	\
	ourRtn += thisRtn;												\
	if(thisRtn) {													\
		if(testError(clientParams.quiet)) {						\
			goto done;											\
		}														\
	}															\
}

#define SSL_THR_RUN_NUM(serverParams, clientParams, desc, ourRtn, testNum)	\
{																\
	thisRtn = sslRunSession(&serverParams, &clientParams, desc);\
	ourRtn += thisRtn;											\
	if(thisRtn) {												\
		printf("***Error on test %u\n", testNum);				\
		if(testError(clientParams.quiet)) {						\
			goto done;											\
		}														\
	}															\
}

#define THREADING_DEBUG		0
#if		THREADING_DEBUG

#define sslThrDebug(side, end)	\
	printf("^^^%s thread %p %s\n", side, pthread_self(), end)
#else	/* THREADING_DEBUG */
#define sslThrDebug(side, end)
#endif	/* THREADING_DEBUG */
#ifdef	__cplusplus
}
#endif

#endif	/* _SSL_THREADING_H_ */
