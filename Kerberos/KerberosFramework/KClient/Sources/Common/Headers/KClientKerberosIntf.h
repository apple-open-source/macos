/*
 * KClient bottom level interface to Kerberos v4
 */
 
#pragma once

#include "KClientFile.h"
#include "KClientAddress.h"

class KClientKerberosInterface {
	public:
		/* Kerberos v4 encryption adds at most 26 bytes to the data */
		static	void 				AcquireInitialTicketsFromKeyFile (
										const UPrincipal&				inClientPrincipal,
										const UPrincipal&				inServerPrincipal,
										const KClientFilePriv&			inKeyFile);
        
		static	void				GetTicketForService (
										const UPrincipal&		inServerPrincipal,
										UInt32							inChecksum,
										void*							outBuffer,
										UInt32&							ioBufferLength);

		static	void				GetAuthenticatorForService (
										const UPrincipal&		inServerPrincipal,
										UInt32							inChecksum,
										const char*						inApplicationVersion,
										void*							outBuffer,
										UInt32&							ioBufferLength);

		static	void				VerifyEncryptedServiceReply (
										const void*						inBuffer,
										UInt32							inBufferLength,
										const KClientKey&				inSessionKey,
										const KClientKeySchedule&		inKeySchedule,
										const KClientAddressPriv&		inClientAddress,
										const KClientAddressPriv&		inServerAddress,
										UInt32							inChecksum);

		static	void				VerifyProtectedServiceReply (
										const void*						inBuffer,
										UInt32							inBufferLength,
										const KClientKey&				inSessionKey,
										const KClientAddressPriv&		inClientAddress,
										const KClientAddressPriv&		inServerAddress,
										UInt32							inChecksum);

		static	void				Encrypt (
										const void*						inPlainBuffer,
										UInt32							inPlainBufferLength,
										const KClientKey&				inSessionKey,
										const KClientKeySchedule&		inKeySchedule,
										const KClientAddressPriv&		inLocalAddress,
										const KClientAddressPriv&		inRemoteAddress,
										void*							outEncryptedBuffer,
										UInt32&							ioEncryptedBufferLength);

		static	void				Decrypt (
										void*							inEncryptedBuffer,
										UInt32							inEncryptedBufferLength,
										const KClientKey&				inSessionKey,
										const KClientKeySchedule&		inKeySchedule,
										const KClientAddressPriv&		inLocalAddress,
										const KClientAddressPriv&		inRemoteAddress,
										UInt32&							outPlainBufferOffset,
										UInt32&							outPlainBufferLength);
										
		static	void				ProtectIntegrity (
										const void*						inPlainBuffer,
										UInt32							inPlainBufferLength,
										const KClientKey&				inSessionKey,
										const KClientAddressPriv&		inLocalAddress,
										const KClientAddressPriv&		inRemoteAddress,
										void*							outProtectedBuffer,
										UInt32&							ioProtectedBufferLength);

		static	void				VerifyIntegrity (
										void*							inProtectedBuffer,
										UInt32							inProtectedBufferLength,
										const KClientKey&				inSessionKey,
										const KClientAddressPriv&		inLocalAddress,
										const KClientAddressPriv&		inRemoteAddress,
										UInt32&							outPlainBufferOffset,
										UInt32&							outPlainBufferLength);
		
		static	void				VerifyAuthenticator (
										const UPrincipal&				inService,
										const KClientAddressPriv&		inRemoteAddress,
										const void* 					inBuffer,
										UInt32							inBufferLength,
										AUTH_DAT&						outAuthenticationData,
										const KClientFile&				inKeyFile);
		
		static	void				GetEncryptedServiceReply (
										UInt32							inChecksum,
										const KClientKey&				inSessionKey,
										const KClientKeySchedule&		inKeySchedule,
										const KClientAddressPriv&		inLocalAddress,
										const KClientAddressPriv&		inRemoteAddress,
										void* 							outBuffer,
										UInt32& 						ioBufferSize);

		static	void				GetProtectedServiceReply (
										UInt32 							inChecksum,
										const KClientKey& 				inSessionKey,
										const KClientAddressPriv&		inLocalAddress,
										const KClientAddressPriv&		inRemoteAddress,
										void* 							outBuffer,
										UInt32& 						ioBufferSize);
										
		static	void				AddServiceKey (
										const KClientFilePriv&			inKeyFile,
										const UPrincipal&		inService,
										UInt32							inVersion,
										const KClientKey&				inKey);
										
		static	void				GetServiceKey (
										const KClientFilePriv&			inKeyFile,
										const UPrincipal&		inService,
										UInt32							inVersion,
										KClientKey&						inKey);
										
		static	std::string			DefaultCCache ();
		static	void				SetDefaultCCache (
										const std::string&				inName);
};
