
#include "KClientLoginIntf.h"
#include "KClientKerberosIntf.h"

const	char	kAuthenticatorVersionString[]	= "AUTHV0.1";
const	UInt32	kAuthenticatorVersionLength		= 8;

void KClientKerberosInterface::AcquireInitialTicketsFromKeyFile (
	const UPrincipal&				inClientPrincipal,
	const UPrincipal&				inServerPrincipal,
	const KClientFilePriv&			inKeyFile)
{
    const char *path = inKeyFile;

	int err = krb_get_svc_in_tkt (
		const_cast <char*> (inClientPrincipal.GetName (UPrincipal::kerberosV4).c_str ()),
		const_cast <char*> (inClientPrincipal.GetInstance (UPrincipal::kerberosV4).c_str ()),
		const_cast <char*> (inClientPrincipal.GetRealm (UPrincipal::kerberosV4).c_str ()),
		const_cast <char*> (inServerPrincipal.GetName (UPrincipal::kerberosV4).c_str ()),
		const_cast <char*> (inServerPrincipal.GetInstance (UPrincipal::kerberosV4).c_str ()),
		(int) KClientLoginInterface::GetDefaultTicketLifetime (), 
        const_cast <char*> (path));
	if (err != KSUCCESS)
		DebugThrow_ (KerberosRuntimeError (err));
}
 
void KClientKerberosInterface::GetTicketForService (
	const UPrincipal&		inServerPrincipal,
	UInt32 							inChecksum,
	void* 							outBuffer,
	UInt32& 						ioBufferLength)
{
	KTEXT_ST	ticket;
	int err = krb_mk_req (&ticket, 
		const_cast <char*> (inServerPrincipal.GetName (UPrincipal::kerberosV4).c_str ()),
		const_cast <char*> (inServerPrincipal.GetInstance (UPrincipal::kerberosV4).c_str ()),
		const_cast <char*> (inServerPrincipal.GetRealm (UPrincipal::kerberosV4).c_str ()), (SInt32) inChecksum);
	if (err != KSUCCESS)
		DebugThrow_ (KerberosRuntimeError (err));
	
	if ((SInt32) ioBufferLength < ticket.length)
		DebugThrow_ (KClientLogicError (kcErrBufferTooSmall));
	
	memmove (reinterpret_cast <unsigned char*> (outBuffer), ticket.dat, (UInt32) ticket.length);
	ioBufferLength = (UInt32) ticket.length;
}

void KClientKerberosInterface::GetAuthenticatorForService (
	const UPrincipal&		inServerPrincipal,
	UInt32 							inChecksum,
	const char* 					inApplicationVersion,
	void* 							outBuffer,
	UInt32& 						ioBufferLength)
{
	if (ioBufferLength < 2 * kAuthenticatorVersionLength + sizeof (UInt32))
		DebugThrow_ (KClientLogicError (kcErrBufferTooSmall));
		
	void* buffer = ((unsigned char*) outBuffer) + 2 * kAuthenticatorVersionLength + sizeof (UInt32);
	UInt32 bufferLength = ioBufferLength - 2 * kAuthenticatorVersionLength - sizeof (UInt32);
	
	GetTicketForService (inServerPrincipal, inChecksum, buffer, bufferLength);
	
	*(UInt32*) ((char*) outBuffer + 2 * kAuthenticatorVersionLength) = bufferLength;
	
	memset (outBuffer, 0, 2 * kAuthenticatorVersionLength);
	strncpy ((char*) outBuffer, kAuthenticatorVersionString, kAuthenticatorVersionLength);
	strncpy (((char*) outBuffer) + kAuthenticatorVersionLength, inApplicationVersion, kAuthenticatorVersionLength);
	ioBufferLength = bufferLength + 2 * kAuthenticatorVersionLength + sizeof (UInt32);
}

void KClientKerberosInterface::VerifyEncryptedServiceReply (
	const void* 					inBuffer,
	UInt32							inBufferLength,
	const KClientKey& 				inSessionKey,
	const KClientKeySchedule&		inKeySchedule,
	const KClientAddressPriv&		inClientAddress,
	const KClientAddressPriv&		inServerAddress,
	UInt32 							inChecksum)
{
	struct sockaddr_in sender = inServerAddress;
	struct sockaddr_in recipient = inClientAddress;
	
	// Decrypt the response
	MSG_DAT messageData;
	int err = krb_rd_priv ((unsigned char*) inBuffer, inBufferLength,
		const_cast <des_ks_struct*> (inKeySchedule.keySchedule),
		const_cast <des_cblock *> (&inSessionKey.key), &sender, &recipient, &messageData);
	
	if (err != KSUCCESS)
		DebugThrow_ (KerberosRuntimeError (err));
	
	// Find the new checksum
	UInt32 checksum = *(UInt32*)messageData.app_data;
	checksum = ntohl (checksum);
	
	// Verify the checksum
	if (checksum != inChecksum + 1)
		DebugThrow_ (KClientRuntimeError (kcErrChecksumMismatch));
}

void KClientKerberosInterface::VerifyProtectedServiceReply (
	const void* 					inBuffer,
	UInt32	 						inBufferLength,
	const KClientKey&				inSessionKey,
	const KClientAddressPriv&		inClientAddress,
	const KClientAddressPriv&		inServerAddress,
	UInt32 							inChecksum)
{
	struct sockaddr_in sender = inServerAddress;
	struct sockaddr_in recipient = inClientAddress;
	
	// Decrypt the response
	MSG_DAT messageData;
	int err = krb_rd_safe ((unsigned char*) inBuffer, inBufferLength,
		const_cast <des_cblock *> (&inSessionKey.key), &sender, &recipient, &messageData);
	
	if (err != KSUCCESS)
		DebugThrow_ (KerberosRuntimeError (err));
	
	// Find the new checksum
	UInt32 checksum = *(UInt32*)messageData.app_data;
	checksum = ntohl (checksum);
	
	// Verify the checksum
	if (checksum != inChecksum + 1)
		DebugThrow_ (KClientRuntimeError (kcErrChecksumMismatch));
}

void KClientKerberosInterface::Encrypt (
	const void* 					inPlainBuffer,
	UInt32 							inPlainBufferLength,
	const KClientKey&				inSessionKey,
	const KClientKeySchedule&		inKeySchedule,
	const KClientAddressPriv&		inLocalAddress,
	const KClientAddressPriv&		inRemoteAddress,
	void* 							outEncryptedBuffer,
	UInt32& 						ioEncryptedBufferLength)
{
	struct sockaddr_in sender = inLocalAddress;
	struct sockaddr_in recipient = inRemoteAddress;
	
	if (ioEncryptedBufferLength < inPlainBufferLength + kKClientEncryptionOverhead)
		DebugThrow_ (KClientLogicError (kcErrBufferTooSmall));
	
	ioEncryptedBufferLength = (UInt32) krb_mk_priv ((unsigned char*) inPlainBuffer,
		(unsigned char*) outEncryptedBuffer, inPlainBufferLength, 
		const_cast <des_ks_struct*> (inKeySchedule.keySchedule),
		const_cast <des_cblock *> (&inSessionKey.key),
		&sender, &recipient);
}

void KClientKerberosInterface::Decrypt (
	void* 							inEncryptedBuffer,
	UInt32 							inEncryptedBufferLength,
	const KClientKey& 				inSessionKey,
	const KClientKeySchedule&		inKeySchedule,
	const KClientAddressPriv&		inLocalAddress,
	const KClientAddressPriv&		inRemoteAddress,
	UInt32& 						outPlainBufferOffset,
	UInt32& 						outPlainBufferLength)
{
	struct sockaddr_in sender = inRemoteAddress;
	struct sockaddr_in recipient = inLocalAddress;
	
	MSG_DAT messageData;
	int err = krb_rd_priv ((unsigned char*) inEncryptedBuffer,
		inEncryptedBufferLength, 
		const_cast <des_ks_struct*> (inKeySchedule.keySchedule),
		const_cast <des_cblock *> (&inSessionKey.key),
		&sender, &recipient,
		&messageData);
		
	if (err != KSUCCESS)
		DebugThrow_ (KerberosRuntimeError (err));

	::BlockMoveData (messageData.app_data, inEncryptedBuffer, (SInt32) messageData.app_length);
	outPlainBufferLength = messageData.app_length;
	outPlainBufferOffset = 0;
}
	
void KClientKerberosInterface::ProtectIntegrity (
	const void* 					inPlainBuffer,
	UInt32 							inPlainBufferLength,
	const KClientKey& 				inSessionKey,
	const KClientAddressPriv&		inLocalAddress,
	const KClientAddressPriv&		inRemoteAddress,
	void* 							outProtectedBuffer,
	UInt32& 						ioProtectedBufferLength)
{
	struct sockaddr_in sender = inLocalAddress;
	struct sockaddr_in recipient = inRemoteAddress;

	if (ioProtectedBufferLength < inPlainBufferLength + kKClientProtectionOverhead)
		DebugThrow_ (KClientLogicError (kcErrBufferTooSmall));
	
	ioProtectedBufferLength = (UInt32) krb_mk_safe ((unsigned char*) inPlainBuffer,
		(unsigned char*) outProtectedBuffer, inPlainBufferLength, 
		const_cast <des_cblock *> (&inSessionKey.key),
		&sender, &recipient);
}

void KClientKerberosInterface::VerifyIntegrity (
	void* 							inProtectedBuffer,
	UInt32 							inProtectedBufferLength,
	const KClientKey&				inSessionKey,
	const KClientAddressPriv&		inLocalAddress,
	const KClientAddressPriv&		inRemoteAddress,
	UInt32& 						outPlainBufferOffset,
	UInt32& 						outPlainBufferLength)
{
	struct sockaddr_in sender = inRemoteAddress;
	struct sockaddr_in recipient = inLocalAddress;
	
	MSG_DAT messageData;
	int err = krb_rd_safe ((unsigned char*) inProtectedBuffer,
		inProtectedBufferLength, 
		const_cast <des_cblock *> (&inSessionKey.key),
		&sender, &recipient,
		&messageData);
	
	if (err != KSUCCESS)
		DebugThrow_ (KerberosRuntimeError (err));
		
	::BlockMoveData (messageData.app_data, inProtectedBuffer, (SInt32) messageData.app_length);
	outPlainBufferLength = messageData.app_length;
	outPlainBufferOffset = 0;
}

void KClientKerberosInterface::VerifyAuthenticator (
	const UPrincipal&				inService,
	const KClientAddressPriv&		inRemoteAddress,
	const void* 					inBuffer,
	UInt32							inBufferLength,
	AUTH_DAT&						outAuthenticationData,
	const KClientFile&				inKeyFile)
{
	struct ktext	authenticator;
	authenticator.length = (int) inBufferLength;
	memmove (authenticator.dat, inBuffer, inBufferLength);

	KClientKey	serviceKey;
	GetServiceKey (inKeyFile, inService, 0, serviceKey);
	int err = krb_rd_req_int (&authenticator, 
		const_cast <char*> (inService.GetName (UPrincipal::kerberosV4).c_str ()),
		const_cast <char*> (inService.GetInstance (UPrincipal::kerberosV4).c_str ()),
		inRemoteAddress.GetAddress (),
		&outAuthenticationData,
		serviceKey.key);
		
	if (err != KSUCCESS)
		DebugThrow_ (KerberosRuntimeError (err));
}

void KClientKerberosInterface::GetEncryptedServiceReply (
	UInt32							inChecksum,
	const KClientKey&				inSessionKey,
	const KClientKeySchedule&		inKeySchedule,
	const KClientAddressPriv&		inLocalAddress,
	const KClientAddressPriv&		inRemoteAddress,
	void* 							outBuffer,
	UInt32& 						ioBufferSize)
{
	struct sockaddr_in sender = inLocalAddress;
	struct sockaddr_in recipient = inRemoteAddress;
	
	if (ioBufferSize < sizeof (inChecksum) + kKClientEncryptionOverhead)
		DebugThrow_ (KClientLogicError (kcErrBufferTooSmall));
	
	ioBufferSize = (UInt32) krb_mk_priv ((unsigned char*) &inChecksum,
		(unsigned char*) outBuffer, sizeof (inChecksum), 
		const_cast <des_ks_struct*> (inKeySchedule.keySchedule),
		const_cast <des_cblock *> (&inSessionKey.key),
		&recipient, &sender);
}

void KClientKerberosInterface::GetProtectedServiceReply (
	UInt32 							inChecksum,
	const KClientKey& 				inSessionKey,
	const KClientAddressPriv&		inLocalAddress,
	const KClientAddressPriv&		inRemoteAddress,
	void* 							outBuffer,
	UInt32& 						ioBufferSize)
{
	struct sockaddr_in sender = inLocalAddress;
	struct sockaddr_in recipient = inRemoteAddress;

	if (ioBufferSize < sizeof (inChecksum) + kKClientProtectionOverhead)
		DebugThrow_ (KClientLogicError (kcErrBufferTooSmall));
	
	ioBufferSize = (UInt32) krb_mk_safe ((unsigned char*) &inChecksum,
		(unsigned char*) outBuffer, sizeof (inChecksum), 
		const_cast <des_cblock *> (&inSessionKey.key),
		&recipient, &sender);
}

void KClientKerberosInterface::AddServiceKey (
	const KClientFilePriv&			inKeyFile,
	const UPrincipal&				inService,
	UInt32							inVersion,
	const KClientKey&				inKey)
{
	const char *file = NULL;
	if (inKeyFile.IsValid ()) {
		file = inKeyFile;
	}
	
	int err = put_svc_key (const_cast <char*> (file), 
		const_cast <char*> (inService.GetName (UPrincipal::kerberosV4).c_str ()),
		const_cast <char*> (inService.GetInstance (UPrincipal::kerberosV4).c_str ()),
		const_cast <char*> (inService.GetRealm (UPrincipal::kerberosV4).c_str ()),
		(int) inVersion,
		const_cast <char*> (reinterpret_cast <const char*> (inKey.key)));
	
	if (err != KSUCCESS)
		DebugThrow_ (KClientRuntimeError (kcErrKeyFileAccess));
}
	
void KClientKerberosInterface::GetServiceKey (
	const KClientFilePriv&			inKeyFile,
	const UPrincipal&				inService,
	UInt32							inVersion,
	KClientKey&						inKey)
{
	const char* file = NULL;
	if (inKeyFile.IsValid ()) {
		file = inKeyFile;
	}
	
	int err = read_service_key (
		const_cast <char*> (inService.GetName (UPrincipal::kerberosV4).c_str ()),
		const_cast <char*> (inService.GetInstance (UPrincipal::kerberosV4).c_str ()),
		const_cast <char*> (inService.GetRealm (UPrincipal::kerberosV4).c_str ()),
		(int) inVersion, const_cast <char*> (file), 
		reinterpret_cast <char*> (inKey.key));
	
	if (err != KSUCCESS)
		DebugThrow_ (KClientRuntimeError (kcErrKeyFileAccess));
}
	
std::string KClientKerberosInterface::DefaultCCache ()
{
	const char*	defaultName = tkt_string ();
	if (defaultName == nil)
		throw std::bad_alloc ();
	
	return defaultName;
}

void KClientKerberosInterface::SetDefaultCCache (
	const std::string&		inCCacheName)
{
	krb_set_tkt_string (inCCacheName.c_str ());
}
