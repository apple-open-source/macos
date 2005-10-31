/*
 * Testing matrix
 *
 *	KClientGetVersion								Ã
 *					minorVersion = nil				Ã
 *					versionString = nil				Ã
 *					paramErr
 *
 *
 *	KClientNewClientSession
 *					noErr							Ã
 *	KClientLogin
 *					noErr							Ã
 *	KClientGetClientPrincipal
 *					client, noErr					Ã
 *	KClientSetServerPrincipal
 *					client, noErr					Ã
 *	KClientGetTicketForService
 *					noErr							Ã
 *	KClientGetAuthenticatorForService
 *					noErr							Ã
 *	KClientDisposeSession
 *					client, noErr					Ã
 *
 *
 *	KClientV4StringToPrincipal
 *					noErr							Ã
 *	KClientPrincipalToV4String
 *					noErr							Ã
 *	KClientDisposePrincipal
 *					noErr							Ã
 */

#define BeginTest_(inTestName)												\
	{																		\
		cout << "        " << (inTestName) << endl;
		
#define EndTest_()															\
		cout << "            ... passed" << endl;								\
	}
	
#define TestNumber_(inTest, inGood)											\
	if ((inTest) != (inGood)) {												\
		cout << "****        " << #inTest << " != " << #inGood << endl;		\
		throw -1;															\
	}
	
#define TestError_(inTest, inGood)											\
	if ((inTest) != (inGood)) {												\
		cout << "****        Error returned was not " << #inGood << endl;	\
		throw -1;															\
	}

#define TestString_(inTest, inGood)											\
	if (strcmp ((inTest), (inGood)) != 0) {									\
		cout << "****        " << inTest << " != " << inGood << endl;		\
		throw -1;															\
	}
	
#define TestPrint_(inWhat)													\
	cout << "            " << inWhat
	
#define TestFail_()															\
	throw -1;
	
#include <cstring>
#include <Kerberos/Kerberos.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <netdb.h>

using namespace std;

/* There aren't any useful Kerberos v4 test servers, so most of what we do is test
KClient against KClient */

void TestKClientAPI ();
void TestKClientClientAPI ();
void TestKClientServerAPI ();
void TestBothKClientAPIs ();

int main(void)
{
	TestKClientAPI ();
	return 0;
}

void TestKClientAPI ()
{
	BeginTest_("KClientGetVersion") {
		UInt16	majorVersion;
		UInt16	minorVersion;
		const char*	versionString;

		KClientGetVersion (&majorVersion, &minorVersion, &versionString);
		
		TestNumber_ (majorVersion, 3);
		TestNumber_ (minorVersion, 0);
		TestString_ (versionString, "KClient 3.0");
	} EndTest_()

	BeginTest_("KClientGetVersion, minorVersion = nil") {
		UInt16	majorVersion;
        const char*	versionString;

		KClientGetVersion (&majorVersion, nil, &versionString);
		
		TestNumber_ (majorVersion, 3);
		TestString_ (versionString, "KClient 3.0");
	} EndTest_()

	BeginTest_("KClientGetVersion, versionString = nil") {
		UInt16	majorVersion;
		UInt16	minorVersion;

		KClientGetVersion (&majorVersion, &minorVersion, nil);
		
		TestNumber_ (majorVersion, 3);
		TestNumber_ (minorVersion, 0);
	} EndTest_()

	TestKClientClientAPI ();
	TestKClientServerAPI ();
	TestBothKClientAPIs ();
}

void TestKClientClientAPI ()
{
	KClientSession	session = nil;
	KClientPrincipal	clientPrincipal = nil;	
	KClientPrincipal	serverPrincipal = nil;	
	char	v4Principal [ANAME_SZ];
	char	buffer		[10240];
	size_t	bufferLength;

	OSStatus err;

	BeginTest_ ("KClientNewClientSession") {
		OSStatus err = KClientNewClientSession (&session);
		TestError_ (err, noErr);
	} EndTest_()
	
	BeginTest_ ("KClientLogin") {
		err = KClientLogin (session);
		TestError_ (err, noErr);
	} EndTest_()

	BeginTest_ ("KClientGetClientPrincipal") {
		err = KClientGetClientPrincipal (session, &clientPrincipal);
		TestError_ (err, noErr);
	} EndTest_()
		
	BeginTest_ ("KClientPrincipalToV4String") {
		err = KClientPrincipalToV4String (clientPrincipal, v4Principal);
		TestError_ (err, noErr);
		TestPrint_ ("Client principal is \"" << v4Principal << "\"" << endl);
	} EndTest_()
		
	BeginTest_ ("KClientDisposePrincipal") {
		err = KClientDisposePrincipal (clientPrincipal);
		TestError_ (err, noErr);
	} EndTest_()
		
	BeginTest_ ("KClientV4StringToPrincipal") {
		err = KClientV4StringToPrincipal ("pop.po12@ATHENA.MIT.EDU", &serverPrincipal);
		TestError_ (err, noErr);
	} EndTest_()
		
	BeginTest_ ("KClientSetServerPrincipal") {
		err = KClientSetServerPrincipal (session, serverPrincipal);
		TestError_ (err, noErr);
	} EndTest_()
		
	BeginTest_ ("KClientDisposePrincipal") {
		err = KClientDisposePrincipal (serverPrincipal);
		TestError_ (err, noErr);
	} EndTest_()
		
	BeginTest_ ("KClientGetServerPrincipal") {
		err = KClientGetServerPrincipal (session, &serverPrincipal);
		TestError_ (err, noErr);
	} EndTest_()
		
	BeginTest_ ("KClientPrincipalToV4String") {
		err = KClientPrincipalToV4String (serverPrincipal, v4Principal);
		TestError_ (err, noErr);
		TestString_ (v4Principal, "pop.po12@ATHENA.MIT.EDU");
	} EndTest_()
	
	BeginTest_ ("KClientGetTicketForService") {
		bufferLength = sizeof (buffer);
		err = KClientGetTicketForService (session, 0, buffer, &bufferLength);
		TestError_ (err, noErr);
	} EndTest_()
	
	BeginTest_ ("KClientGetAuthenticatorForService") {
		bufferLength = sizeof (buffer);
		err = KClientGetAuthenticatorForService (session, 0, "KPOPV1.0", buffer, &bufferLength);
		TestError_ (err, noErr);
		
        sockaddr_in	po12;
        po12.sin_len = sizeof(struct in_addr);
        po12.sin_family = AF_INET;
        po12.sin_port = 1109;
        memset (po12.sin_zero, 0, sizeof (po12.sin_zero));
        struct hostent *he = gethostbyname ("po12.mit.edu");
        if (he != NULL) {
            memcpy (&po12.sin_addr.s_addr, he->h_addr, sizeof(struct in_addr));
        } else {
            po12.sin_addr.s_addr = (18 * 256 * 256 * 256) + (7 * 256 * 256) + (21 * 256) + 71;
        }
        
		int		popSocket = socket(AF_INET, SOCK_STREAM, /* PF_INET */ 0);
		if (popSocket == -1) {
			TestPrint_ ("socket failed" << endl);
            TestPrint_ (strerror(errno) << endl);
			TestFail_ ();
		}
		
		int err = connect (popSocket, (sockaddr*) &po12, sizeof (po12));
		if (err) {
			TestPrint_ ("socket_connect failed" << endl);
            TestPrint_ (strerror(errno) << endl);
			TestFail_ ();
		}
		
		err = write (popSocket, buffer, bufferLength);
		if ((size_t)err != bufferLength) {
			TestPrint_ ("socket_write failed" << endl);
            TestPrint_ (strerror(errno) << endl);
			TestFail_ ();
		}
		
		bufferLength = sizeof (buffer);
		err = read (popSocket, buffer, bufferLength);
		if (err == -1) {
			TestPrint_ ("socket_read failed" << endl);
            TestPrint_ (strerror(errno) << endl);
			TestFail_ ();
		}
		

		*(strchr (buffer, '\r')) = '\0';
		buffer [err] = '\0';
		TestPrint_ ("Server replied with:" << endl);
		TestPrint_ ("\"" << buffer << "\"" << endl);
		*(strchr (buffer, ' ')) = '\0';
		TestString_ (buffer, "+OK");
		
		close (popSocket);
	} EndTest_()
	
	BeginTest_ ("KClientDisposeSession") {
		err = KClientDisposeSession (session);
		TestError_ (err, noErr);
	} EndTest_()
}

void TestKClientServerAPI ()
{
}

void TestBothKClientAPIs ()
{
	KClientSession		clientSession;
	KClientSession		serverSession;
	KClientPrincipal	serverPrincipal;
	
	OSStatus err;
	
	BeginTest_ ("KClientNewClientSession") {
		OSStatus err = KClientNewClientSession (&clientSession);
		TestError_ (err, noErr);
	} EndTest_()
	
	BeginTest_ ("KClientV4StringToPrincipal") {
		err = KClientV4StringToPrincipal ("rcmd.the-good-of-the-one@ATHENA.MIT.EDU", &serverPrincipal);
		TestError_ (err, noErr);
	} EndTest_()
		
	BeginTest_ ("KClientNewServerSession") {
		OSStatus err = KClientNewServerSession (&serverSession, serverPrincipal);
		TestError_ (err, noErr);
	} EndTest_()
		
	BeginTest_ ("KClientDisposePrincipal") {
		err = KClientDisposePrincipal (serverPrincipal);
		TestError_ (err, noErr);
	} EndTest_()
		
	BeginTest_ ("KClientDisposeSession, client") {
		err = KClientDisposeSession (clientSession);
		TestError_ (err, noErr);
	} EndTest_()

	BeginTest_ ("KClientDisposeSession, server") {
		err = KClientDisposeSession (serverSession);
		TestError_ (err, noErr);
	} EndTest_()
}
