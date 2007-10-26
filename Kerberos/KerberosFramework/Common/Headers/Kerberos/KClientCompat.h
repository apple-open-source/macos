/*
 * Copyright 1998-2003  by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.	Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/* 
 * KClient 1.9 deprecated API
 *
 * $Header$
 */

#ifndef	__KCLIENTCOMPAT__
#define	__KCLIENTCOMPAT__

#if defined(macintosh) || (defined(__MACH__) && defined(__APPLE__))
#    include <TargetConditionals.h>
#    include <AvailabilityMacros.h>
#    if TARGET_RT_MAC_CFM
#        error "Use KfM 4.0 SDK headers for CFM compilation."
#    endif
#endif

#ifndef DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER
#define DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER
#endif

#if TARGET_OS_MAC
#    include <Kerberos/krb.h>
#    include <Kerberos/KClientTypes.h>
#else
#    include <kerberosIV/krb.h>
#    include <KClientTypes.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if TARGET_OS_MAC
#    pragma pack(push,2)
#endif
    
/* Constants */

/* Error codes, only listing the ones actually returned by the library */
enum {
	cKrbMapDoesntExist		= -1020,	/* tried to access a map that doesn't exist (index too large, or criteria doesn't match anything) */
	cKrbSessDoesntExist		= -1019,	/* tried to access a session that doesn't exist */
	cKrbCredsDontExist		= -1018,	/* tried to access credentials that don't exist */
	cKrbUserCancelled		= -1016,	/* user cancelled a log in operation */
	cKrbConfigurationErr		= -1015,	/* Kerberos Preference file is not configured properly */
	cKrbServerRejected		= -1014,	/* A server rejected our ticket */
	cKrbServerImposter		= -1013,	/* Server appears to be a phoney */
	cKrbServerRespIncomplete	= -1012,	/* Server response is not complete */
	cKrbNotLoggedIn			= -1011,	/* Returned by cKrbGetUserName if user is not logged in */
	cKrbAppInBkgnd			= -1008,	/* driver won't put up password dialog when in background */
	cKrbInvalidSession		= -1007,	/* invalid structure passed to KClient/KServer routine */
		
	cKrbKerberosErrBlock		= -20000	/* start of block of 256 kerberos error numbers */
};

typedef KClientSession KClientSessionInfo 
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

enum {
	KClientLoggedIn,
	KClientNotLoggedIn
};

OSErr KClientVersionCompat (
	SInt16*						outMajorVersion,
	SInt16*						outMinorVersion,
	char*						outVersionString)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSErr KClientNewSessionCompat (
	KClientSessionInfo*			inSession,
	UInt32						inLocalAddress,
	UInt16						inLocalPort,
	UInt32						inRemoteAddress,
	UInt16						inRemotePort)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KClientDisposeSessionCompat (
	KClientSessionInfo*			inSession)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KClientGetTicketForServiceCompat (
	KClientSessionInfo*			inSession,
	char*						inService,
	void*						inBuffer,
	UInt32*						outBufferLength)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KClientGetTicketForServiceWithChecksumCompat (
	KClientSessionInfo*			inSession,
	UInt32						inChecksum,
	char*						inService,
	void*						inBuffer,
	UInt32*						outBufferLength)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KClientLoginCompat (
	KClientSessionInfo*			inSession,
	KClientKey*					outPrivateKey)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KClientPasswordLoginCompat (
	KClientSessionInfo*			inSession,
	char*						inPassword,
	KClientKey*					outPrivateKey)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KClientLogoutCompat (void)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

SInt16 KClientStatusCompat (void)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSErr KClientGetSessionKeyCompat (
	KClientSessionInfo*			inSession,
	KClientKey*					outSessionKey)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KClientEncryptCompat (
	KClientSessionInfo*			inSession,
	void*						inPlainBuffer,
	UInt32						inPlainBufferLength,
	void*						outEncryptedBuffer,
	UInt32*						ioEncryptedBufferLength)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KClientDecryptCompat (
	KClientSessionInfo*			inSession,
	void*						inEncryptedBuffer,
	UInt32						inEncryptedBufferLength,
	UInt32*						outPlainBufferOffset,
	UInt32*						outPlainBufferLength)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSErr KClientProtectIntegrityCompat (
	KClientSessionInfo*			inSession,
	void*						inPlainBuffer,
	UInt32						inPlainBufferLength,
	void*						outProtectedBuffer,
	UInt32*						ioProtectedBufferLength)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSErr KClientVerifyIntegrityCompat (
	KClientSessionInfo*			inSession,
	void*						inProtectedBuffer,
	UInt32						inProtectedBufferLength,
	UInt32*						outPlainBufferOffset,
	UInt32*						outPlainBufferLength)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSErr KServerNewSessionCompat (
	KClientSessionInfo*			inSession,
	char*						inService,
	UInt32						inLocalAddress,
	UInt16						inLocalPort,
	UInt32						inRemoteAddress,
	UInt16						inRemotePort)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSErr KServerVerifyTicketCompat (
	KClientSessionInfo*			inSession,
	void*						inBuffer,
	char*						inFilename)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KServerGetReplyTicketCompat (
	KClientSessionInfo*			inSession,
	void*						outBuffer,
	UInt32*						ioBufferLength)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KServerAddKeyCompat (
	KClientSessionInfo*			inSession,
	KClientKey*					inPrivateKey,
	char*						inService,
	SInt32						inVersion,
	char*						inFilename)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KServerGetKeyCompat (
	KClientSessionInfo*			inSession,
	KClientKey*					outPrivateKey,
	char*						inService,
	SInt32						inVersion,
	char*						inFilename)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSErr KServerGetSessionTimeRemainingCompat (
	KClientSessionInfo*			inSession,
	SInt32*						outSeconds)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KClientGetSessionUserNameCompat (
	KClientSessionInfo*			inSession,
	char*						outUserName,
	SInt16						inNameType)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

OSErr KClientMakeSendAuthCompat (
	KClientSessionInfo*			inSession,
	char*						inService,
	void*						outBuffer,
	UInt32*						ioBufferLength,
	SInt32						inChecksum,
	char*						inApplicationVersion)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KClientVerifyReplyTicketCompat (
	KClientSessionInfo*			inSession,
	void*						inBuffer,
	UInt32*						ioBufferLength)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
	
OSErr KClientVerifyUnencryptedReplyTicketCompat (
	KClientSessionInfo*			inSession,
	void*						inBuffer,
	UInt32*						ioBufferLength)
DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

#if TARGET_OS_MAC
#    pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __KCLIENTCOMPAT__ */
