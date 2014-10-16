/*
 * nisccSimpleClient.cpp - just do one SSL client session expecting 
 * errSSLPeerCertUnknown and ClientCertRejected
 */

#include <Security/SecureTransport.h>
#include <Security/Security.h>
#include <Security/SecBasePriv.h>
#include <clAppUtils/sslAppUtils.h>
#include <clAppUtils/ioSock.h>
#include <clAppUtils/sslThreading.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <security_utilities/threading.h>
#include <security_utilities/devrandom.h>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/param.h>

/* skip certs larger than this - ST can't fragment protocol msgs (yet) */
#define MAX_CERT_SIZE	16000

static void usage(char **argv)
{
	printf("Usage: %s hostname port keychain [q(uiet)]\n", argv[0]);
	exit(1);
}

#define IGNORE_SIGPIPE	1
#if 	IGNORE_SIGPIPE
#include <signal.h>

void sigpipe(int sig) 
{ 
}
#endif	/* IGNORE_SIGPIPE */

SslAppTestParams clientDefaults = 
{
	NULL,				// hostName - user-provided
	true,				// skipHostNameCHeck
	0,					// port - user-provided		
	NULL, NULL,			// RingBuffers
	false,				// noProtSpec
	kTLSProtocol1,	
	NULL,				// acceptedProts - not used in this test
	NULL,				// myCerts - user-provided
	NULL,				// password - same as myCerts
	false,				// idIsTrustedRoot
	true,				// disableCertVerify - SPECIAL FOR THIS TEST
	NULL,				// anchorFile - not needed - right?
	false,				// replaceAnchors
	kAlwaysAuthenticate,
	false,				// resumeEnable
	NULL,				// ciphers
	false,				// nonBlocking 
	NULL,				// dhParams
	0,					// dhParamsLen
	errSSLPeerCertUnknown,			// expectRtn
	kTLSProtocol1,		// expectVersion
	kSSLClientCertRejected,
	SSL_CIPHER_IGNORE,
	false,				// quiet - user-provided
	false,				// silent
	false,				// verbose
	NULL,				// lock
	0,					// clientDone
	false,				// serverAbort
	/* returned */
	kSSLProtocolUnknown,
	SSL_NULL_WITH_NULL_NULL,
	kSSLClientCertNone,
	noHardwareErr
	
};

static void testStartBanner(
	char *testName,
	int argc,
	char **argv)
{
	printf("Starting %s; args: ", testName);
	for(int i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
}

/* this normally comes from libcsputils.a, which we don't link against */

extern "C" {
char *cssmErrToStr(CSSM_RETURN err);
}

char *cssmErrToStr(CSSM_RETURN err)
{
	string errStr = cssmErrorString(err);
	return const_cast<char *>(errStr.c_str());
}


int main(int argc, char **argv)
{
	int 	ourRtn = 0;
	char	 *argp;
	int		errCount = 0;
	
	if(argc < 4) {
		usage(argv);
	}
	
	/* required args */
	clientDefaults.hostName = argv[1];
	clientDefaults.password = argv[1];
	clientDefaults.port = atoi(argv[2]);
	clientDefaults.myCertKcName = argv[3];
	
	/* optional args */
	for(int arg=4; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'q':
				clientDefaults.quiet = true;
				break;
			default:
				usage(argv);
		}
	}
	
	#if IGNORE_SIGPIPE
	signal(SIGPIPE, sigpipe);
	#endif
	
	if(!clientDefaults.quiet) {
		testStartBanner("nisccSimpleClient", argc, argv);
	}
	ourRtn = sslAppClient(&clientDefaults);
	
	/* accept a number of returns - even success! */
	if((ourRtn != errSSLPeerCertUnknown) &&
	   (ourRtn != errSSLPeerUnknownCA) &&
	   (ourRtn != errSSLPeerRecordOverflow) &&
	   (ourRtn != noErr)) {
		printf("***Unexpected error return (%s)\n", 
			sslGetSSLErrString(ourRtn));
		errCount++;
	}
	if(ourRtn == noErr) {
		errCount += sslVerifyClientCertState("client", 
			kSSLClientCertSent, 
			clientDefaults.certState);
	}
	else {
		errCount += sslVerifyClientCertState("client", 
			clientDefaults.expectCertState, 
			clientDefaults.certState);
	}

	if(!clientDefaults.quiet) {
		if(errCount == 0) {
			printf("===== %s test PASSED =====\n", argv[0]);
			ourRtn = noErr;
		}
		else {
			printf("****FAIL: sslAppClient detected %d errors\n", errCount); 
		}
	}
	
	return errCount;
}
