#include "test.h"

#include "mit-KerberosLogin.h"

/* Structures and prototypes from Krb4DeprecatedAPIs.c */
typedef struct KClientSession *KClientSession;
typedef KClientSession KClientSessionInfo;

struct KClientPrincipalOpaque;
typedef struct KClientPrincipalOpaque* KClientPrincipal;

struct KClientAddress {
        UInt32                          address;
        UInt16                          port;
};
typedef struct KClientAddress KClientAddress;

OSStatus KClientDecrypt (KClientSession, void *, UInt32, UInt32 *, UInt32 *);
OSStatus KClientDisposePrincipal (KClientPrincipal);
OSStatus KClientEncrypt (KClientSession, const void *, UInt32, void *, UInt32 *);
OSStatus KClientGetLocalAddress (KClientSession, KClientAddress*);
OSStatus KClientGetRemoteAddress (KClientSession, KClientAddress*);
OSStatus KClientGetTicketForService (KClientSession, UInt32, void*, UInt32*);
OSStatus KClientGetVersion (UInt16*, UInt16*, const char**);
OSStatus KClientNewClientSession (KClientSession *);
OSStatus KClientProtectIntegrity (KClientSession, const void*, UInt32, void*, UInt32*);
OSStatus KClientSetLocalAddress (KClientSession, const KClientAddress*);
OSStatus KClientSetRemoteAddress (KClientSession, const KClientAddress*);
OSStatus KClientSetServerPrincipal (KClientSession, KClientPrincipal);
OSStatus KClientV4StringToPrincipal (const char*, KClientPrincipal*);
OSErr KClientVerifyIntegrityCompat (KClientSessionInfo*, void*, UInt32, UInt32*, UInt32*);
OSStatus KClientVerifyProtectedServiceReply (KClientSession, const void*, UInt32);
OSStatus KClientVerifyIntegrity (KClientSession,void*, UInt32,UInt32*, UInt32*);

int main(int argc, char **argv)
{

	UInt32 i32 = 0;
	UInt16 *i16 = NULL;
	char *cbuffer = NULL;
	void *vbuffer = NULL;
	KClientSession session = NULL;
	KClientSessionInfo session_info = NULL;
	KClientPrincipal principal = NULL;
	KClientAddress address = {0,0};

	VERIFY_DEPRECATED_I(
		"KClientDecrypt",
		KClientDecrypt(session, vbuffer, i32, &i32, &i32));

	VERIFY_DEPRECATED_I(
		"KClientDisposePrincipal",
		KClientDisposePrincipal(principal));

	VERIFY_DEPRECATED_I(
		"KClientEncrypt",
		KClientEncrypt(session, vbuffer, i32, vbuffer, &i32));

	VERIFY_DEPRECATED_I(
		"KClientGetLocalAddress",
		KClientGetLocalAddress(session, &address));

	VERIFY_DEPRECATED_I(
		"KClientGetRemoteAddress",
		KClientGetRemoteAddress(session, &address));

	VERIFY_DEPRECATED_I(
		"KClientGetTicketForService",
		KClientGetTicketForService(session, i32, vbuffer, &i32));

	VERIFY_DEPRECATED_I(
		"KClientGetVersion",
		KClientGetVersion(i16, i16, (const char**)&cbuffer));

	VERIFY_DEPRECATED_I(
		"KClientNewClientSession",
		KClientNewClientSession(&session));

	VERIFY_DEPRECATED_I(
		"KClientProtectIntegrity",
		KClientProtectIntegrity(session, vbuffer, i32, vbuffer, &i32));

	VERIFY_DEPRECATED_I(
		"KClientSetLocalAddress",
		KClientSetLocalAddress(session, &address));

	VERIFY_DEPRECATED_I(
		"KClientSetRemoteAddress",
		KClientSetRemoteAddress(session, &address));

	VERIFY_DEPRECATED_I(
		"KClientSetServerPrincipal",
		KClientSetServerPrincipal(session, principal));

	VERIFY_DEPRECATED_I(
		"KClientV4StringToPrincipal",
		KClientV4StringToPrincipal(cbuffer, &principal));

	VERIFY_DEPRECATED_I(
		"KClientVerifyIntegrity",
		KClientVerifyIntegrity(session_info, vbuffer, i32, &i32, &i32));

	VERIFY_DEPRECATED_I(
		"KClientVerifyProtectedServiceReply",
		KClientVerifyProtectedServiceReply(session, vbuffer, i32));

	printf("Test passed.\n");
	return 0;
}
