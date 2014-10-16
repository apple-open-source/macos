/*
 * sslProt.cpp - test SSL protocol negotiation, client and server side
 *
 * This executes a preposterously exhaustive set of client/server runs
 * in which just about every permutation of server and client
 * protocol enables (using both SSLSetProtocolVersionEnabled and
 * SSLSetProtocolVersion) is examined. Resulting negotiated protocols
 * and error returns are verified. 
 *
 * There are three different basic negotiation scenarios:
 *
 * -- Normal case, server and client agree. 
 * 
 * -- Server detects negotiation error. This can happen in two ways:
 *    -- server doesn't allow SSL3 or TLS1 but gets an SSL3 client hello
 *       (regardless of the requested protocol version in the packet)
 *    -- server gets a client hello containing a protocol version
 *       when the server supports neither that version not any 
 *       version below that. For example, server allows TLS1 only and 
 *       gets an SSL3 hello with requested version SSL3. 
 *
 * -- Client detects negotiation error. In this case the server hello
 *    contains a different version than the client hello (I.e., server
 *    downgraded), but the client doesn't support the version the 
 *    server requested. 
 *
 * In both of the failure cases, the peer which detects the error
 * drops the connection and returns errSSLNegotiation from its
 * SSLHandshake() call. The other peer sees a dropped connection
 * and returns errSSLClosedAbort from its SSLHandshake() call. 
 * IN both cases, the negotiated protocol seen by the client is 
 * kSSLProtocolUnknown. However when the client detects the error, the
 * server will see a valid negotiated protocol containing whatever it
 * sent to the client in its server hello message. 
 */
#include <Security/SecureTransport.h>
#include <Security/Security.h>
#include <clAppUtils/sslAppUtils.h>
#include <clAppUtils/ioSock.h>
#include <clAppUtils/sslThreading.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <security_utilities/threading.h>
#include <security_utilities/devrandom.h>
#include "dhParams512.h"

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/param.h>

#define STARTING_PORT			3000

/* 
 * localcert is a KC containing server cert and signing key
 * assumptions: 
 *	-- common name = "localcert"
 *  -- password of KC = "localcert"
 */
#define SERVER_KC		"localcert"
#define SERVER_ROOT		"localcert.cer"

/* main() fills this in using sslKeychainPath() */
static char serverKcPath[MAXPATHLEN];

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("options:\n");
	printf("   q(uiet)\n");
	printf("   v(erbose)\n");
	printf("   d (Diffie-Hellman, no keychain needed)\n");
	printf("   p=startingPortNum\n");
	printf("   t=startTestNum\n");
	printf("   b (non blocking I/O)\n");
	printf("   s=serverCertName; default %s\n", SERVER_ROOT);
	printf("   R (ringBuffer I/O)\n");
	exit(1);
}

/*
 * Parameters defining one run
 */
typedef struct {
	const char		*groupDesc;			// optional
	const char		*testDesc;			// required
	bool			noServeProt;		// don't set server protocol version 
	SSLProtocol		servTryVersion;
	const char		*serveAcceptProts;	// use TryVersion if this is NULL
	SSLProtocol		expectServerProt;	// expected negotiated result
	OSStatus		serveStatus;		// expected OSStatus
	bool			noClientProt;		// don't set client protocol version 
	SSLProtocol		clientTryVersion;
	const char		*clientAcceptProts;
	SSLProtocol		expectClientProt;
	OSStatus		clientStatus;
	bool			serverAbort;		// allows server to close connection early
} SslProtParams;

SslProtParams protTestParams[] = 
{
/*
 * FIXME this fails to compile to to radar 4104919. I really don't want to 
 * remove these pragmas unless/until I hear a positive confirmation that that
 * Radar is not to be fixed. 
 */
// #pragma mark Unrestricted server via SSLSetProtocolVersion
	{ 	
		"unrestricted server via SSLSetProtocolVersion",
		"client SSLSetProtocolVersion(TLS1)",
		false, kTLSProtocol1, NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1, NULL, kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(TLS1 only)",
		false, kTLSProtocol1, NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1Only, NULL, kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3)",
		false, kTLSProtocol1, NULL, kSSLProtocol3, noErr,
		false, kSSLProtocol3, NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3 only)",
		false, kTLSProtocol1, NULL, kSSLProtocol3, noErr,
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL2)",
		false, kTLSProtocol1, NULL, kSSLProtocol2, noErr,
		false, kSSLProtocol2, NULL, kSSLProtocol2, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(t)",
		false, kTLSProtocol1, NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1, "t", kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3t)",
		false, kTLSProtocol1, NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1, "3t", kTLSProtocol1, noErr, false
	},
	/* make sure default client is the same as "3t" - Radar 4233139 - 
	 * SSLv3 no longer enabled by default */
	{	
		NULL, "client default",
		false, kTLSProtocol1, NULL, kTLSProtocol1, noErr,
		true, kSSLProtocolUnknown, NULL, kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23t)",
		false, kTLSProtocol1, NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1, "23t", kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3)",
		false, kTLSProtocol1, NULL, kSSLProtocol3, noErr,
		false, kTLSProtocol1, "3", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23)",
		false, kTLSProtocol1, NULL, kSSLProtocol3, noErr,
		false, kTLSProtocol1, "23", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2)",
		false, kTLSProtocol1, NULL, kSSLProtocol2, noErr,
		false, kTLSProtocol1, "2", kSSLProtocol2, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2t)",
		false, kTLSProtocol1, NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1, "2t", kTLSProtocol1, noErr, false
	},

	// #pragma mark === Server SSLSetProtocolVersion(TLS1 only)
	{ 	
		"server SSLSetProtocolVersion(TLS1 only)",
		"client SSLSetProtocolVersion(TLS1)",
		false, kTLSProtocol1Only, NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1,     NULL, kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(TLS1 only)",
		false, kTLSProtocol1Only, NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1Only, NULL, kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3)",
		false, kTLSProtocol1Only, NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kSSLProtocol3,     NULL, kSSLProtocolUnknown, errSSLClosedAbort,
		false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3 only)",
		false, kTLSProtocol1Only, NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kSSLProtocol3Only, NULL, kSSLProtocolUnknown, errSSLConnectionRefused,
		true
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL2)",
		false, kTLSProtocol1Only, NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kSSLProtocol2,     NULL, kSSLProtocolUnknown, errSSLClosedAbort,
		true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(t)",
		false, kTLSProtocol1Only, NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1,    "t",   kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3t)",
		false, kTLSProtocol1Only, NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1,    "3t",  kTLSProtocol1, noErr, false
	},
	/* make sure default client is the same as "3t" */
	{	
		NULL, "client default",
		false, kTLSProtocol1Only, NULL, kTLSProtocol1, noErr,
		true, kSSLProtocolUnknown,  NULL,  kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23t)",
		false, kTLSProtocol1Only, NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1,    "23t", kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3)",
		false, kTLSProtocol1Only, NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1,    "3",   kSSLProtocolUnknown, errSSLConnectionRefused,
		true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23)",
		false, kTLSProtocol1Only, NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1,    "23",  kSSLProtocolUnknown, errSSLClosedAbort,
		true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2)",
		false, kTLSProtocol1Only, NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1,    "2",   kSSLProtocolUnknown, errSSLClosedAbort,
		true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2t)",
		false, kTLSProtocol1Only, NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1,    "2t",  kTLSProtocol1, noErr, false
	},

	// #pragma mark === Server SSLSetProtocolVersion(SSL3)
	{ 	
		"server SSLSetProtocolVersion(SSL3)",
		"client SSLSetProtocolVersion(TLS1)",
		false, kSSLProtocol3,     NULL, kSSLProtocol3, noErr,
		false, kTLSProtocol1,     NULL, kSSLProtocol3, noErr, false
	},
	{
		NULL, "client SSLSetProtocolVersion(TLS1 only)",
		/* negotiation error detected by client, not server */
		false, kSSLProtocol3,     NULL, kSSLProtocol3, errSSLClosedAbort,
		false, kTLSProtocol1Only, NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3)",
		false, kSSLProtocol3,     NULL, kSSLProtocol3, noErr,
		false, kSSLProtocol3,     NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3 only)",
		false, kSSLProtocol3,     NULL, kSSLProtocol3, noErr,
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL2)",
		false, kSSLProtocol3,     NULL, kSSLProtocol3, noErr,
		false, kSSLProtocol3,     NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(t)",
		false, kSSLProtocol3,    NULL, kSSLProtocol3, errSSLClosedAbort,
		false, kTLSProtocol1,    "t",  kSSLProtocolUnknown, errSSLNegotiation,
		false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3t)",
		false, kSSLProtocol3,    NULL, kSSLProtocol3, noErr,
		false, kTLSProtocol1,    "3t", kSSLProtocol3, noErr, false
	},
	/* make sure default client is the same as "3t" */
	{	
		NULL, "client default",
		false, kSSLProtocol3,    NULL, kSSLProtocol3, noErr,
		true, kSSLProtocolUnknown,  NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23t)",
		false, kSSLProtocol3,    NULL,  kSSLProtocol3, noErr,
		false, kTLSProtocol1,    "23t", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3)",
		false, kSSLProtocol3,    NULL,  kSSLProtocol3, noErr,
		false, kTLSProtocol1,    "3",  kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23)",
		false, kSSLProtocol3,    NULL,  kSSLProtocol3, noErr,
		false, kTLSProtocol1,    "23",  kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2)",
		false, kSSLProtocol3,    NULL,  kSSLProtocol2, noErr,
		false, kTLSProtocol1,    "2",   kSSLProtocol2, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2t)",
		false, kSSLProtocol3,    NULL,  kSSLProtocol2, noErr,
		false, kTLSProtocol1,    "2",   kSSLProtocol2, noErr, false
	},
	
	// #pragma mark === Server SSLSetProtocolVersion(SSL3 only)
	{ 	
		"server SSLSetProtocolVersion(SSL3 only)",
		"client SSLSetProtocolVersion(TLS1)",
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr,
		false, kTLSProtocol1,     NULL, kSSLProtocol3, noErr, false
	},
	{
		NULL, "client SSLSetProtocolVersion(TLS1 only)",
		/* negotiation error detected by client, not server */
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, errSSLClosedAbort,
		false, kTLSProtocol1Only, NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3)",
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr,
		false, kSSLProtocol3,     NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3 only)",
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr,
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL2)",
		false, kSSLProtocol3Only, NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kSSLProtocol2,     NULL, kSSLProtocolUnknown, errSSLClosedAbort,
		true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(t)",
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, errSSLClosedAbort,
		false, kTLSProtocol1,    "t",   kSSLProtocolUnknown, errSSLNegotiation,
		false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3t)",
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr,
		false, kTLSProtocol1,    "3t",  kSSLProtocol3, noErr, false
	},
	/* make sure default client is the same as "3t" */
	{	
		NULL, "client default",
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr,
		true, kSSLProtocolUnknown,  NULL,  kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23t)",
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr,
		false, kTLSProtocol1,    "23t", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3)",
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr,
		false, kTLSProtocol1,    "3",   kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23)",
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr,
		false, kTLSProtocol1,    "23",  kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2)",
		false, kSSLProtocol3Only, NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1,    "2",   kSSLProtocolUnknown, errSSLClosedAbort,
		false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2t)",
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, errSSLClosedAbort,
		false, kTLSProtocol1,    "2t",  kSSLProtocolUnknown, errSSLNegotiation,
		false
	},

	// #pragma mark === Server SSLSetProtocolVersion(SSL2)
	{ 	
		"server SSLSetProtocolVersion(SSL2)",
		"client SSLSetProtocolVersion(TLS1)",
		false, kSSLProtocol2,     NULL, kSSLProtocol2, noErr,
		false, kTLSProtocol1,     NULL, kSSLProtocol2, noErr, false
	},
	{
		NULL, "client SSLSetProtocolVersion(TLS1 only)",
		/* server won't even accept the non-SSL2 hello */
		false, kSSLProtocol2,     NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1Only, NULL, kSSLProtocolUnknown, errSSLConnectionRefused,
		true
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3)",
		false, kSSLProtocol2,     NULL, kSSLProtocol2, noErr,
		false, kSSLProtocol3,     NULL, kSSLProtocol2, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3 only)",
		false, kSSLProtocol2,     NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kSSLProtocol3Only, NULL, kSSLProtocolUnknown, errSSLConnectionRefused,
		true
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL2)",
		false, kSSLProtocol2,     NULL, kSSLProtocol2, noErr,
		false, kSSLProtocol2,     NULL, kSSLProtocol2, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(t)",
		false, kSSLProtocol2,     NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1,    "t",   kSSLProtocolUnknown, errSSLConnectionRefused,
		true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3t)",
		false, kSSLProtocol2,     NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1,    "3t",  kSSLProtocolUnknown, errSSLConnectionRefused,
		true
	},
	/* make sure default client is the same as "3t" */
	{	
		NULL, "client default",
		false, kSSLProtocol2,     NULL, kSSLProtocolUnknown, errSSLNegotiation,
		true, kSSLProtocolUnknown,  NULL,  kSSLProtocolUnknown, errSSLConnectionRefused,
		true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23t)",
		false, kSSLProtocol2,     NULL, kSSLProtocol2, noErr,
		false, kTLSProtocol1,    "23t", kSSLProtocol2, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3)",
		false, kSSLProtocol2,     NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1,    "3",   kSSLProtocolUnknown, errSSLConnectionRefused, 
		true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23)",
		false, kSSLProtocol2,     NULL, kSSLProtocol2, noErr,
		false, kTLSProtocol1,    "23",  kSSLProtocol2, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2)",
		false, kSSLProtocol2,     NULL, kSSLProtocol2, noErr,
		false, kTLSProtocol1,    "2",   kSSLProtocol2, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2t)",
		false, kSSLProtocol2,     NULL, kSSLProtocol2, noErr,
		false, kTLSProtocol1,    "2t",  kSSLProtocol2, noErr, false
	},

	//#pragma mark === Unrestricted server via SSLSetProtocolVersionEnabled
	{ 	
		"unrestricted server via SSLSetProtocolVersionEnabled",
		"client SSLSetProtocolVersion(TLS1)",
		false, kTLSProtocol1, "23t", kTLSProtocol1, noErr,
		false, kTLSProtocol1, NULL, kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(TLS1 only)",
		false, kTLSProtocol1, "23t", kTLSProtocol1, noErr,
		false, kTLSProtocol1Only, NULL, kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3)",
		false, kTLSProtocol1, "23t", kSSLProtocol3, noErr,
		false, kSSLProtocol3, NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3 only)",
		false, kTLSProtocol1, "23t", kSSLProtocol3, noErr,
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL2)",
		false, kTLSProtocol1, "23t", kSSLProtocol2, noErr,
		false, kSSLProtocol2, NULL, kSSLProtocol2, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(t)",
		false, kTLSProtocol1, "23t", kTLSProtocol1, noErr,
		false, kTLSProtocol1, "t", kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3t)",
		false, kTLSProtocol1, "23t", kTLSProtocol1, noErr,
		false, kTLSProtocol1, "3t", kTLSProtocol1, noErr
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23t)",
		false, kTLSProtocol1, "23t", kTLSProtocol1, noErr,
		false, kTLSProtocol1, "23t", kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3)",
		false, kTLSProtocol1, "23t", kSSLProtocol3, noErr,
		false, kTLSProtocol1, "3", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23)",
		false, kTLSProtocol1, "23t", kSSLProtocol3, noErr,
		false, kTLSProtocol1, "23", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2)",
		false, kTLSProtocol1, "23t", kSSLProtocol2, noErr,
		false, kTLSProtocol1, "2", kSSLProtocol2, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2t)",
		false, kTLSProtocol1, "23t", kTLSProtocol1, noErr,
		false, kTLSProtocol1, "2t", kTLSProtocol1, noErr, false
	},

	// #pragma mark === Server SSLSetProtocolVersionEnabled(t)

	{ 	
		"server SSLSetProtocolVersionEnabled(t)",
		"client SSLSetProtocolVersion(TLS1)",
		false, kTLSProtocol1,      "t", kTLSProtocol1, noErr,
		false, kTLSProtocol1,     NULL, kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(TLS1 only)",
		false, kTLSProtocol1,      "t", kTLSProtocol1, noErr,
		false, kTLSProtocol1Only, NULL, kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3)",
		false, kTLSProtocol1,      "t", kSSLProtocolUnknown, errSSLNegotiation,
		false, kSSLProtocol3,     NULL, kSSLProtocolUnknown, errSSLClosedAbort, true
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3 only)",
		false, kTLSProtocol1,      "t", kSSLProtocolUnknown, errSSLNegotiation,
		false, kSSLProtocol3Only, NULL, kSSLProtocolUnknown, errSSLConnectionRefused, true
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL2)",
		false, kTLSProtocol1,      "t", kSSLProtocolUnknown, errSSLNegotiation,
		false, kSSLProtocol2,     NULL, kSSLProtocolUnknown, errSSLClosedAbort, true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(t)",
		false, kTLSProtocol1,      "t", kTLSProtocol1, noErr,
		false, kTLSProtocol1,    "t",   kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3t)",
		false, kTLSProtocol1,      "t", kTLSProtocol1, noErr,
		false, kTLSProtocol1,    "3t",  kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23t)",
		false, kTLSProtocol1,      "t", kTLSProtocol1, noErr,
		false, kTLSProtocol1,    "23t", kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3)",
		false, kTLSProtocol1,      "t", kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1,    "3",   kSSLProtocolUnknown, errSSLConnectionRefused, 
		true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23)",
		false, kTLSProtocol1,      "t", kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1,    "23",  kSSLProtocolUnknown, errSSLClosedAbort, 
		true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2)",
		false, kTLSProtocol1,      "t", kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1,    "2",   kSSLProtocolUnknown, errSSLClosedAbort, 
		true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2t)",
		false, kTLSProtocol1,      "t", kTLSProtocol1, noErr,
		false, kTLSProtocol1,    "2t",  kTLSProtocol1, noErr, false
	},

	// #pragma mark === Server SSLSetProtocolVersionEnabled(23)
	{ 	
		"server SSLSetProtocolVersionEnabled(23)",
		"client SSLSetProtocolVersion(TLS1)",
		false, kSSLProtocol2,     "23", kSSLProtocol3, noErr,
		false, kTLSProtocol1,     NULL, kSSLProtocol3, noErr, false
	},
	{
		NULL, "client SSLSetProtocolVersion(TLS1 only)",
		/* negotiation error detected by client, not server */
		false, kSSLProtocol2,     "23", kSSLProtocol3, errSSLClosedAbort,
		false, kTLSProtocol1Only, NULL, kSSLProtocolUnknown, errSSLNegotiation, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3)",
		false, kSSLProtocol2,     "23", kSSLProtocol3, noErr,
		false, kSSLProtocol3,     NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3 only)",
		false, kSSLProtocol2,     "23", kSSLProtocol3, noErr,
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL2)",
		false, kSSLProtocol2,     "23", kSSLProtocol3, noErr,
		false, kSSLProtocol3,     NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(t)",
		false, kSSLProtocol2,    "23", kSSLProtocol3, errSSLClosedAbort,
		false, kTLSProtocol1,    "t",  kSSLProtocolUnknown, errSSLNegotiation, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3t)",
		false, kSSLProtocol2,    "23", kSSLProtocol3, noErr,
		false, kTLSProtocol1,    "3t", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23t)",
		false, kSSLProtocol2,    "23",  kSSLProtocol3, noErr,
		false, kTLSProtocol1,    "23t", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3)",
		false, kSSLProtocol2,    "23",  kSSLProtocol3, noErr,
		false, kTLSProtocol1,    "3",  kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23)",
		false, kSSLProtocol2,    "23",  kSSLProtocol3, noErr,
		false, kTLSProtocol1,    "23",  kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2)",
		false, kSSLProtocol2,    "23",  kSSLProtocol2, noErr,
		false, kTLSProtocol1,    "2",   kSSLProtocol2, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2t)",
		false, kSSLProtocol2,    "23",  kSSLProtocol2, noErr,
		false, kTLSProtocol1,    "2",   kSSLProtocol2, noErr, false
	},
	
	// #pragma mark === Server SSLSetProtocolVersionEnabled(3)
	{ 	
		"server SSLSetProtocolVersionEnabled(3)",
		"client SSLSetProtocolVersion(TLS1)",
		false, kSSLProtocol2,      "3", kSSLProtocol3, noErr,
		false, kTLSProtocol1,     NULL, kSSLProtocol3, noErr, false
	},
	{
		NULL, "client SSLSetProtocolVersion(TLS1 only)",
		/* negotiation error detected by client, not server */
		false, kSSLProtocol2,      "3", kSSLProtocol3, errSSLClosedAbort,
		false, kTLSProtocol1Only, NULL, kSSLProtocolUnknown, errSSLNegotiation, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3)",
		false, kSSLProtocol2,      "3", kSSLProtocol3, noErr,
		false, kSSLProtocol3,     NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3 only)",
		false, kSSLProtocol2,      "3", kSSLProtocol3, noErr,
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL2)",
		false, kSSLProtocol2,      "3", kSSLProtocolUnknown, errSSLNegotiation,
		false, kSSLProtocol2,     NULL, kSSLProtocolUnknown, errSSLClosedAbort, true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(t)",
		false, kSSLProtocol2,      "3", kSSLProtocol3, errSSLClosedAbort,
		false, kTLSProtocol1,      "t", kSSLProtocolUnknown, errSSLNegotiation, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3t)",
		false, kSSLProtocol2,      "3", kSSLProtocol3, noErr,
		false, kTLSProtocol1,     "3t", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23t)",
		false, kSSLProtocol2,      "3", kSSLProtocol3, noErr,
		false, kTLSProtocol1,    "23t", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3)",
		false, kSSLProtocol2,      "3", kSSLProtocol3, noErr,
		false, kTLSProtocol1,      "3", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23)",
		false, kSSLProtocol2,      "3", kSSLProtocol3, noErr,
		false, kTLSProtocol1,     "23", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2)",
		false, kSSLProtocol2,      "3", kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1,      "2", kSSLProtocolUnknown, errSSLClosedAbort, true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2t)",
		false, kSSLProtocol2,      "3", kSSLProtocol3, errSSLClosedAbort,
		false, kTLSProtocol1,     "2t", kSSLProtocolUnknown, errSSLNegotiation, false
	},

	/* 
	 * This is the real difference between 
	 * SSLSetProtocolVersionEnabled and SSLSetProtocolVersion
	 */
	// #pragma mark === Server SSLSetProtocolVersionEnabled(3t)
	{ 	
		"server SSLSetProtocolVersionEnabled(3t)",
		"client SSLSetProtocolVersion(TLS1)",
		false, kSSLProtocol2,     "t3", kTLSProtocol1, noErr,
		false, kTLSProtocol1,     NULL, kTLSProtocol1, noErr, false
	},
	/* make sure default server is the same as "t3" */
	{ 	
		NULL,
		"client SSLSetProtocolVersion(TLS1), server default",
		true,  kSSLProtocolUnknown,  NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1,     NULL, kTLSProtocol1, noErr, false
	},
	{
		NULL, "client SSLSetProtocolVersion(TLS1 only)",
		false, kSSLProtocol2,     "t3", kTLSProtocol1, noErr,
		false, kTLSProtocol1Only, NULL, kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3)",
		false, kSSLProtocol2,     "t3", kSSLProtocol3, noErr,
		false, kSSLProtocol3,     NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL3 only)",
		false, kSSLProtocol2,     "t3", kSSLProtocol3, noErr,
		false, kSSLProtocol3Only, NULL, kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersion(SSL2)",
		false, kSSLProtocol2,     "t3", kSSLProtocolUnknown, errSSLNegotiation,
		false, kSSLProtocol2,     NULL, kSSLProtocolUnknown, errSSLClosedAbort, true
	},
	/* make sure default server is the same as "t3" */
	{	
		NULL, "client SSLSetProtocolVersion(SSL2), server default",
		true,  kSSLProtocolUnknown,  NULL, kSSLProtocolUnknown, errSSLNegotiation,
		false, kSSLProtocol2,     NULL, kSSLProtocolUnknown, errSSLClosedAbort, true
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(t)",
		false, kSSLProtocol2,     "t3", kTLSProtocol1, noErr,
		false, kTLSProtocol1,      "t", kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3t)",
		false, kSSLProtocol2,     "t3", kTLSProtocol1, noErr,
		false, kTLSProtocol1,     "3t", kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23t)",
		false, kSSLProtocol2,     "t3", kTLSProtocol1, noErr,
		false, kTLSProtocol1,    "23t", kTLSProtocol1, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(3)",
		false, kSSLProtocol2,     "t3", kSSLProtocol3, noErr,
		false, kTLSProtocol1,      "3", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(23)",
		false, kSSLProtocol2,     "t3", kSSLProtocol3, noErr,
		false, kTLSProtocol1,     "23", kSSLProtocol3, noErr, false
	},
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2)",
		false, kSSLProtocol2,     "t3", kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1,      "2", kSSLProtocolUnknown, errSSLClosedAbort, true
	},
	/* make sure default server is the same as "t3" */
	{	
		NULL, "client SSLSetProtocolVersionEnabled(2)",
		false, kSSLProtocol2,     "t3", kSSLProtocolUnknown, errSSLNegotiation,
		false, kTLSProtocol1,      "2", kSSLProtocolUnknown, errSSLClosedAbort, true
	},
	{	
		"server default", "client SSLSetProtocolVersionEnabled(2t)",
		true,  kSSLProtocolUnknown,  NULL, kTLSProtocol1, noErr,
		false, kTLSProtocol1,  "2t", kTLSProtocol1, noErr, false
	},
};

#define NUM_SSL_PROT_TESTS	(sizeof(protTestParams) / sizeof(protTestParams[0]))

#define IGNORE_SIGPIPE	1
#if 	IGNORE_SIGPIPE
#include <signal.h>

void sigpipe(int sig) 
{ 
}
#endif	/* IGNORE_SIGPIPE */

#define CERT_VFY_DISABLE	false

/*
 * Default params for each test. Main() will make a copy of this and 
 * adjust its copy on a per-test basis.
 */
SslAppTestParams serverDefaults = 
{
	"no name here",
	false,				// skipHostNameCHeck
	0,					// port - test must set this	
	NULL, NULL,			// RingBuffers
	false,				// noProtSpec
	kTLSProtocol1,
	NULL,				// acceptedProts
	serverKcPath,		// myCerts
	SERVER_KC,			// password
	true,				// idIsTrustedRoot
	CERT_VFY_DISABLE,	// disableCertVerify
	NULL,				// anchorFile
	false,				// replaceAnchors
	kNeverAuthenticate,
	false,				// resumeEnable
	NULL,				// ciphers,
	false,				// nonBlocking
	NULL,				// dhParams
	0,					// dhParamsLen
	noErr,				// expectRtn
	kTLSProtocol1,		// expectVersion
	kSSLClientCertNone,
	SSL_CIPHER_IGNORE,
	false,				// quiet
	false,				// silent
	false,				// verbose
	{0},				// lock
	{0},				// cond
	false,				// serverReady
	0,					// clientDone
	false,				// serverAbort
	/* returned */
	kSSLProtocolUnknown,
	SSL_NULL_WITH_NULL_NULL,
	kSSLClientCertNone,
	noHardwareErr
	
};

SslAppTestParams clientDefaults = 
{
	"localhost",
	false,				// skipHostNameCHeck
	0,					// port - test must set this
	NULL, NULL,			// RingBuffers
	false,				// noProtSpec
	kTLSProtocol1,
	NULL,				// acceptedProts
	NULL,				// myCertKcName
	NULL,				// password
	false,				// idIsTrustedRoot
	CERT_VFY_DISABLE,	// disableCertVerify
	SERVER_ROOT,		// anchorFile
	false,				// replaceAnchors
	kNeverAuthenticate,
	false,				// resumeEnable
	NULL,				// ciphers
	false,				// nonBlocking
	NULL,				// dhParams
	0,					// dhParamsLen
	noErr,				// expectRtn
	kTLSProtocol1,		// expectVersion
	kSSLClientCertNone,
	SSL_CIPHER_IGNORE,
	false,				// quiet
	false,				// silent
	false,				// verbose
	{0},				// lock
	{0},				// cond
	false,				// serverReady
	0,					// clientDone
	false,				// serverAbort
	/* returned */
	kSSLProtocolUnknown,
	SSL_NULL_WITH_NULL_NULL,
	kSSLClientCertNone,
	noHardwareErr
};


int main(int argc, char **argv)
{
	int 				ourRtn = 0;
	char	 			*argp;
	SslAppTestParams 	clientParams;
	SslAppTestParams 	serverParams;
	unsigned short		portNum = STARTING_PORT;
	SslProtParams		*protParams;
	unsigned			testNum;
	int					thisRtn;
	unsigned			startTest = 0;
	SSLCipherSuite		ciphers[2];		// for Diffie-Hellman
	bool				diffieHellman = false;
	RingBuffer			serverToClientRing;
	RingBuffer			clientToServerRing;
	bool				ringBufferIo = false;
	
	for(int arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'q':
				serverDefaults.quiet = clientDefaults.quiet = true;
				break;
			case 'v':
				serverDefaults.verbose = clientDefaults.verbose = true;
				break;
			case 'p':
				portNum = atoi(&argp[2]);
				break;
			case 't':
				startTest = atoi(&argp[2]);
				break;
			case 'd':
				diffieHellman = true;
				break;
			case 'b':
				serverDefaults.nonBlocking = clientDefaults.nonBlocking = 
						true;
				break;
			case 's':
				clientDefaults.anchorFile = &argp[2];
				break;
			case 'R':
				ringBufferIo = true;
				break;
			default:
				usage(argv);
		}
	}

	if(sslCheckFile(clientDefaults.anchorFile)) {
		exit(1);
	}
	if(ringBufferIo) {
		/* set up ring buffers */
		ringBufSetup(&serverToClientRing, "serveToClient", DEFAULT_NUM_RB_BUFS, DEFAULT_BUF_RB_SIZE);
		ringBufSetup(&clientToServerRing, "clientToServe", DEFAULT_NUM_RB_BUFS, DEFAULT_BUF_RB_SIZE);
		serverDefaults.serverToClientRing = &serverToClientRing;
		serverDefaults.clientToServerRing = &clientToServerRing;
		clientDefaults.serverToClientRing = &serverToClientRing;
		clientDefaults.clientToServerRing = &clientToServerRing;
	}
	
	#if IGNORE_SIGPIPE
	signal(SIGPIPE, sigpipe);
	#endif

	/* convert keychain names to paths for root */
	sslKeychainPath(SERVER_KC, serverKcPath);

	testStartBanner("sslProt", argc, argv);
	
	serverParams.port = portNum - 1;	// gets incremented by SSL_THR_SETUP
	if(diffieHellman) {
		ciphers[0] = SSL_DH_anon_WITH_RC4_128_MD5;
		ciphers[1] = SSL_NO_SUCH_CIPHERSUITE;
		serverDefaults.ciphers = ciphers;
		serverDefaults.dhParams = dhParams512;
		serverDefaults.dhParamsLen = sizeof(dhParams512);
		serverDefaults.myCertKcName = NULL;
		clientDefaults.anchorFile = NULL;
	}
	for(testNum=startTest; testNum<NUM_SSL_PROT_TESTS; testNum++) {
		protParams = &protTestParams[testNum];
		
		/* 
		 * Hack for Diffie-Hellman: just skip any tests which attempt
		 * or expect SSL2 negotiation, since SSL2 doesn't have DH.
		 */
		if(diffieHellman) {
			if((protParams->servTryVersion == kSSLProtocol2) ||
			   (protParams->clientTryVersion == kSSLProtocol2) ||
			   (protParams->serveAcceptProts && 
			     !strcmp(protParams->serveAcceptProts, "2")) ||
			   (protParams->clientAcceptProts && 
			     !strcmp(protParams->clientAcceptProts, "2"))) {
				if(serverDefaults.verbose) {
					printf("...skipping %s for D-H\n", 
						protParams->testDesc);
				}
				continue;
			}
		}
		if(protParams->groupDesc && !serverDefaults.quiet) {
			printf("...%s\n", protParams->groupDesc);
		}
		SSL_THR_SETUP(serverParams, clientParams, clientDefaults, 
				serverDefault);
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
		}
		serverParams.tryVersion = protParams->servTryVersion;
		clientParams.tryVersion = protParams->clientTryVersion;
		serverParams.acceptedProts = protParams->serveAcceptProts;
		clientParams.acceptedProts = protParams->clientAcceptProts;
		serverParams.expectVersion = protParams->expectServerProt;
		clientParams.expectVersion = protParams->expectClientProt;
		serverParams.expectRtn = protParams->serveStatus;
		clientParams.expectRtn = protParams->clientStatus;
		serverParams.serverAbort = protParams->serverAbort;
		
		SSL_THR_RUN_NUM(serverParams, clientParams, protParams->testDesc, 
			ourRtn, testNum);
	}
	
done:
	if(!clientParams.quiet) {
		if(ourRtn == 0) {
			printf("===== sslProt test PASSED =====\n");
		}
		else {
			printf("****FAIL: %d errors detected\n", ourRtn);
		}
	}
	
	return ourRtn;
}
