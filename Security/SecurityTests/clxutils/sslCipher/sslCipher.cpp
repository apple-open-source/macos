/*
 * sslCipher.cpp - test SSL ciphersuite protocol negotiation, client 
 *				   and server side
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

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/param.h>

/* default start port; caller can specify random start port */
#define STARTING_PORT			5000

#define MIN_RAND_PORT			1500
#define MAX_RAND_PORT			7000

/*
 * Expected errors for negotiation failure 
 */
#define SERVER_NEGOTIATE_FAIL	errSSLNegotiation
#define CLIENT_NEGOTIATE_FAIL	errSSLPeerHandshakeFail

#define RSA_SERVER_KC			"localcert"
#define RSA_SERVER_ROOT			"localcert.cer"
#define DSA_SERVER_KC			"dsacert"
#define DSA_SERVER_ROOT			"dsacert.cer"

#define DH_PARAM_FILE_512		"dhParams_512.der"
#define DH_PARAM_FILE_1024		"dhParams_1024.der"

/* main() fills these in using sslKeychainPath() */
static char rsaKcPath[MAXPATHLEN];
static char dsaKcPath[MAXPATHLEN];

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("options:\n");
	printf("   q(uiet)\n");
	printf("   v(erbose)\n");
	printf("   p=startingPortNum\n");
	printf("   t=startTestNum\n");
	printf("   T=endTestNum\n");
	printf("   g=startGroupNum\n");
	printf("   l (large, 1024 bit Diffie-Hellman; default is 512)\n");
	printf("   r(andom start port, default=%d)\n", STARTING_PORT);
	printf("   b (non blocking I/O)\n");
	printf("   s=serverCertName; default %s\n", RSA_SERVER_ROOT);
	printf("   d=clientCertName; default %s\n", DSA_SERVER_ROOT);
	printf("   R (ringBuffer I/O)\n");
	exit(1);
}

/*
 * Parameters defining one group of tests
 */
typedef struct {
	const char		*groupDesc;	
	const char		*serveAcceptProts;
	const char		*clientAcceptProts;
	SSLProtocol		expectProt;	
} GroupParams;

/*
 * Certificate parameters
 */
typedef struct {
	const char	*kcName;		
	const char	*kcPassword;	// last component of KC name */
	const char 	*rootName;
} CertParams;

/*
 * Parameters defining one individual test
 */
typedef struct {
	const char				*testDesc;
	SSLCipherSuite			expectCipher;
	const CertParams		*certParams;
	/*
	 * In this test all failures are the same
	 */
	bool					shouldWork;
} CipherParams;

/* one of three cert params */
static CertParams certRSA = 	{ rsaKcPath, RSA_SERVER_KC, RSA_SERVER_ROOT };
static CertParams certDSA = 	{ dsaKcPath, DSA_SERVER_KC, DSA_SERVER_ROOT };
static CertParams certNone = 	{NULL, NULL};

/* Note we're skipping SSL2-specific testing for simplicity's sake */
static const GroupParams sslGroupParams[] = 
{
	{ "TLS1", "23t", "3t", kTLSProtocol1 },
	{ "SSL3", "23",  "3t", kSSLProtocol3 }
};
#define NUM_GROUP_PARAMS	\
	(sizeof(sslGroupParams) / sizeof(sslGroupParams[0]))
	
/* some special-purpose ciphersuite arrays */

#ifdef not_used
/* just SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA */
static const SSLCipherSuite suites_RsaExpDh40[] = {
	SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA,
	SSL_NO_SUCH_CIPHERSUITE
};
#endif

/* declare test name and expected cipher suite */
#define SSL_NAC(cname) #cname, cname

/*
 * Note: the client is the only side which actually gets to 
 * prioritize its requested CipherSuites. The server has to 
 * go along with the first one on the client's list which the 
 * server implements. 
 */
const CipherParams sslCipherParams[] = 
{
	{ 	
		SSL_NAC(TLS_RSA_WITH_AES_128_CBC_SHA),
		&certRSA, true
	},
	{ 	
		SSL_NAC(TLS_DH_DSS_WITH_AES_128_CBC_SHA),
		&certDSA, false
	},
	{ 	
		SSL_NAC(TLS_DH_RSA_WITH_AES_128_CBC_SHA),
		&certRSA, false
	},
	{ 	
		SSL_NAC(TLS_DHE_DSS_WITH_AES_128_CBC_SHA),
		&certDSA, true
	},
	{ 	
		SSL_NAC(TLS_DHE_RSA_WITH_AES_128_CBC_SHA),
		&certRSA, true
	},
	{ 	
		SSL_NAC(TLS_DH_anon_WITH_AES_128_CBC_SHA),
		&certNone, true
	},
	{ 	
		SSL_NAC(TLS_RSA_WITH_AES_256_CBC_SHA),
		&certRSA, true
	},
	{ 	
		SSL_NAC(TLS_DH_DSS_WITH_AES_256_CBC_SHA),
		&certDSA, false
	},
	{ 	
		SSL_NAC(TLS_DH_RSA_WITH_AES_256_CBC_SHA),
		&certRSA, false
	},
	{ 	
		SSL_NAC(TLS_DHE_DSS_WITH_AES_256_CBC_SHA),
		&certDSA, true
	},
	{ 	
		SSL_NAC(TLS_DHE_RSA_WITH_AES_256_CBC_SHA),
		&certRSA, true
	},
	{ 	
		SSL_NAC(TLS_DH_anon_WITH_AES_256_CBC_SHA),
		&certNone, true
	},
	{ 	
		SSL_NAC(SSL_RSA_EXPORT_WITH_RC4_40_MD5),
		&certRSA, true
	},
	{	
		SSL_NAC(SSL_RSA_WITH_RC4_128_MD5),
		&certRSA, true
	},
	{	
		SSL_NAC(SSL_RSA_WITH_RC4_128_SHA),
		&certRSA, true
	},
	{	
		SSL_NAC(SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5),
		&certRSA, true
	},
	/* skip SSL_RSA_WITH_IDEA_CBC_SHA, check later as unimpl */
	{	
		SSL_NAC(SSL_RSA_EXPORT_WITH_DES40_CBC_SHA),
		&certRSA, true
	},
	{	
		SSL_NAC(SSL_RSA_WITH_DES_CBC_SHA),
		&certRSA, true
	},
	{	
		SSL_NAC(SSL_RSA_WITH_3DES_EDE_CBC_SHA),
		&certRSA, true
	},
	{	
		SSL_NAC(SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA),
		&certDSA, false
	},
	{	
		SSL_NAC(SSL_DH_DSS_WITH_DES_CBC_SHA),
		&certDSA, false
	},
	{	
		SSL_NAC(SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA),
		&certDSA, false
	},
	{	
		SSL_NAC(SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA),
		&certRSA, false
	},
	{	
		SSL_NAC(SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA),
		&certDSA, true
	},
	{	
		SSL_NAC(SSL_DHE_DSS_WITH_DES_CBC_SHA),
		&certDSA, true
	},
	{	
		SSL_NAC(SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA),
		&certDSA, true
	},
	{	
		SSL_NAC(SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA),
		&certRSA, true
	},
	{	
		SSL_NAC(SSL_DHE_RSA_WITH_DES_CBC_SHA),
		&certRSA, true
	},
	{	
		SSL_NAC(SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA),
		&certRSA, true
	},
	{	
		SSL_NAC(SSL_DH_anon_EXPORT_WITH_RC4_40_MD5),
		&certNone, true
	},
	{	
		SSL_NAC(SSL_DH_anon_WITH_RC4_128_MD5),
		&certNone, true
	},
	{	
		SSL_NAC(SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA),
		&certNone, true
	},
	{	
		SSL_NAC(SSL_DH_anon_WITH_DES_CBC_SHA),
		&certNone, true
	},
	{	
		SSL_NAC(SSL_DH_anon_WITH_3DES_EDE_CBC_SHA),
		&certNone, true
	},
	{	
		SSL_NAC(SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA),
		&certNone, true
	},
	{	
		SSL_NAC(SSL_FORTEZZA_DMS_WITH_NULL_SHA),
		&certNone, false
	},
	{	
		SSL_NAC(SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA),
		&certNone, false
	},
};

#define NUM_CIPHER_PARAMS	\
	(sizeof(sslCipherParams) / sizeof(sslCipherParams[0]))

#define IGNORE_SIGPIPE	1
#if 	IGNORE_SIGPIPE
#include <signal.h>

void sigpipe(int sig) 
{ 
}
#endif	/* IGNORE_SIGPIPE */

/*
 * Default params for each test. Main() will make a copy of this and 
 * adjust its copy on a per-test basis.
 */
static SslAppTestParams serverDefaults = 
{
	"no name here",
	false,				// skipHostNameCHeck
	0,					// port - test must set this	
	NULL, NULL,			// RingBuffers
	false,				// noProtSpec
	kSSLProtocolUnknown,// not used
	NULL,				// acceptedProts
	NULL,				// myCerts
	NULL,				// password
	true,				// idIsTrustedRoot
	false,				// disableCertVerify
	NULL,				// anchorFile
	false,				// replaceAnchors
	kNeverAuthenticate,
	false,				// resumeEnable
	NULL,				// ciphers - default - server accepts all
	false,				// nonBlocking
	NULL,				// dhParams
	0,					// dhParamsLen
	noErr,				// expectRtn
	kTLSProtocol1,		// expectVersion
	kSSLClientCertNone,
	SSL_NULL_WITH_NULL_NULL,
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
	kSSLProtocolUnknown,// not used
	NULL,				// acceptedProts
	NULL,				// myCertKcName
	NULL,				// password
	true,				// idIsTrustedRoot
	false,				// disableCertVerify
	NULL,				// anchorFile
	true,				// replaceAnchors
	kNeverAuthenticate,
	false,				// resumeEnable
	NULL,				// ciphers - set in test loop
	false,				// nonBlocking
	NULL,				// dhParams
	0,					// dhParamsLen
	noErr,				// expectRtn
	kTLSProtocol1,		// expectVersion
	kSSLClientCertNone,
	SSL_NULL_WITH_NULL_NULL,
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
	const GroupParams	*groupParams;
	const CipherParams	*cipherParams;
	unsigned			testNum;
	unsigned			groupNum;
	int					thisRtn;
	SSLCipherSuite		clientCiphers[3];
	SSLCipherSuite		serverCiphers[3];
	RingBuffer			serverToClientRing;
	RingBuffer			clientToServerRing;
	bool				ringBufferIo = false;
	
	/* user-spec'd variables */
	unsigned			startTest = 0;
	unsigned			endTest = NUM_CIPHER_PARAMS;
	unsigned			startGroup = 0;
	const char			*dhParamFile = DH_PARAM_FILE_512;
	
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
			case 'T':
				endTest = atoi(&argp[2]) + 1;
				break;
			case 'g':
				startGroup = atoi(&argp[2]);
				break;
			case 'b':
				serverDefaults.nonBlocking = clientDefaults.nonBlocking = 
						true;
				break;
			case 'l':
				dhParamFile = DH_PARAM_FILE_1024;
				break;
			case 'r':
				portNum = genRand(MIN_RAND_PORT, MAX_RAND_PORT);
				break;
			case 's':
				certRSA.rootName = &argp[2];
				break;
			case 'd':
				certDSA.rootName = &argp[2];
				break;
			case 'R':
				ringBufferIo = true;
				break;
			default:
				usage(argv);
		}
	}
	
	if(sslCheckFile(certRSA.rootName)) {
		exit(1);
	}
	if(sslCheckFile(certDSA.rootName)) {
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
	sslKeychainPath(RSA_SERVER_KC, rsaKcPath);
	sslKeychainPath(DSA_SERVER_KC, dsaKcPath);

	/* Diffie-Hellman params, we're going to need them */
	int r = readFile(dhParamFile, (unsigned char **)&serverDefaults.dhParams, 
		&serverDefaults.dhParamsLen);
	if(r) {
		printf("***Error reading diffie-hellman params from %s; aborting\n",
			dhParamFile);
		exit(1);
	}

	testStartBanner("sslCipher", argc, argv);
	
	serverParams.port = portNum - 1;	// gets incremented by SSL_THR_SETUP
	
	/*
	 * To enable negotiation failures to occur, we have to pass
	 * in ciphersuite arrays which contain at least one valid
	 * ciphersuite to both client and server, but they can not
	 * be the same (or else that valid suite will be used). 
	 */ 
	clientCiphers[1] = SSL_RSA_WITH_RC4_128_MD5;
	serverCiphers[1] = SSL_RSA_WITH_RC4_128_SHA;
	clientCiphers[2] = SSL_NO_SUCH_CIPHERSUITE;
	serverCiphers[2] = SSL_NO_SUCH_CIPHERSUITE;
	
	for(groupNum=startGroup; groupNum<NUM_GROUP_PARAMS; groupNum++) {
		groupParams = &sslGroupParams[groupNum];
		if(!serverDefaults.quiet) {
			printf("...%s\n", groupParams->groupDesc);
		}
		for(testNum=startTest; testNum<endTest; testNum++) {
			cipherParams = &sslCipherParams[testNum];
			SSL_THR_SETUP(serverParams, clientParams, clientDefaults, 
					serverDefault);
			if(ringBufferIo) {
				ringBufferReset(&serverToClientRing);
				ringBufferReset(&clientToServerRing);
			}
			/* per-group (must be after SSL_THR_SETUP) */
			serverParams.acceptedProts = groupParams->serveAcceptProts;
			clientParams.acceptedProts = groupParams->clientAcceptProts;
			serverParams.expectVersion = groupParams->expectProt;
			clientParams.expectVersion = groupParams->expectProt;
		
			/* per-test */
			clientCiphers[0] = cipherParams->expectCipher;
			serverCiphers[0] = cipherParams->expectCipher;
			clientParams.ciphers = clientCiphers;
			serverParams.ciphers = serverCiphers;
			serverParams.expectCipher = cipherParams->expectCipher;
			clientParams.expectCipher = cipherParams->expectCipher;
			
			const CertParams *certParams = cipherParams->certParams;
			serverParams.myCertKcName = certParams->kcName;
			serverParams.password = certParams->kcPassword;
			clientParams.anchorFile = certParams->rootName;

			if(cipherParams->shouldWork) {
				serverParams.expectRtn = noErr;
				clientParams.expectRtn = noErr;
			}
			else {
				serverParams.expectRtn = SERVER_NEGOTIATE_FAIL;
				clientParams.expectRtn = CLIENT_NEGOTIATE_FAIL;
				
				/* server completed protocol version negotiation, 
				 * but client didn't */
				clientParams.expectVersion = kSSLProtocolUnknown;
			}
			SSL_THR_RUN_NUM(serverParams, clientParams, 
				cipherParams->testDesc, ourRtn, testNum);
		}
	}
	
done:
	if(!clientParams.quiet) {
		if(ourRtn == 0) {
			printf("===== %s test PASSED =====\n", argv[0]);
		}
		else {
			printf("****%s FAIL: %d errors detected\n", argv[0],ourRtn);
		}
	}
	
	return ourRtn;
}
