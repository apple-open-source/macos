
/*
 * Copyright (c) 2002-2008 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "EAPSecurity.h"
#include <Security/SecureTransport.h>
#include <TargetConditionals.h>
/* 
 * Return string representation of {SecureTransport,Security}-related OSStatus.
 */
const char *
EAPSecurityErrorString(OSStatus err)
{
    switch(err) {
    case noErr:
	return "noErr";
    case memFullErr:
	return "memFullErr";
    case paramErr:
	return "paramErr";
    case unimpErr:
	return "unimpErr";

	/* SecureTransport.h: */
    case errSSLProtocol:
	return "errSSLProtocol";
    case errSSLNegotiation:
	return "errSSLNegotiation";
    case errSSLFatalAlert:
	return "errSSLFatalAlert";
    case errSSLWouldBlock:
	return "errSSLWouldBlock";
    case ioErr:
	return "ioErr";
    case errSSLSessionNotFound:
	return "errSSLSessionNotFound";
    case errSSLClosedGraceful:
	return "errSSLClosedGraceful";
    case errSSLClosedAbort:
	return "errSSLClosedAbort";
    case errSSLXCertChainInvalid:
	return "errSSLXCertChainInvalid";
    case errSSLBadCert:
	return "errSSLBadCert"; 
    case errSSLCrypto:
	return "errSSLCrypto";
    case errSSLInternal:
	return "errSSLInternal";
    case errSSLModuleAttach:
	return "errSSLModuleAttach";
    case errSSLUnknownRootCert:
	return "errSSLUnknownRootCert";
    case errSSLNoRootCert:
	return "errSSLNoRootCert";
    case errSSLCertExpired:
	return "errSSLCertExpired";
    case errSSLCertNotYetValid:
	return "errSSLCertNotYetValid";
    case badReqErr:
	return "badReqErr";
    case errSSLClosedNoNotify:
	return "errSSLClosedNoNotify";
    case errSSLBufferOverflow:
	return "errSSLBufferOverflow";
    case errSSLBadCipherSuite:
	return "errSSLBadCipherSuite";
    case errSSLPeerUnexpectedMsg:
	return "errSSLPeerUnexpectedMsg";
    case errSSLPeerBadRecordMac:
	return "errSSLPeerBadRecordMac";
    case errSSLPeerDecryptionFail:
	return "errSSLPeerDecryptionFail";
    case errSSLPeerRecordOverflow:
	return "errSSLPeerRecordOverflow";
    case errSSLPeerDecompressFail:
	return "errSSLPeerDecompressFail";
    case errSSLPeerHandshakeFail:
	return "errSSLPeerHandshakeFail";
    case errSSLPeerBadCert:
	return "errSSLPeerBadCert";
    case errSSLPeerUnsupportedCert:
	return "errSSLPeerUnsupportedCert";
    case errSSLPeerCertRevoked:
	return "errSSLPeerCertRevoked";
    case errSSLPeerCertExpired:
	return "errSSLPeerCertExpired";
    case errSSLPeerCertUnknown:
	return "errSSLPeerCertUnknown";
    case errSSLIllegalParam:
	return "errSSLIllegalParam";
    case errSSLPeerUnknownCA:
	return "errSSLPeerUnknownCA";
    case errSSLPeerAccessDenied:
	return "errSSLPeerAccessDenied";
    case errSSLPeerDecodeError:
	return "errSSLPeerDecodeError";
    case errSSLPeerDecryptError:
	return "errSSLPeerDecryptError";
    case errSSLPeerExportRestriction:
	return "errSSLPeerExportRestriction";
    case errSSLPeerProtocolVersion:
	return "errSSLPeerProtocolVersion";
    case errSSLPeerInsufficientSecurity:
	return "errSSLPeerInsufficientSecurity";
    case errSSLPeerInternalError:
	return "errSSLPeerInternalError";
    case errSSLPeerUserCancelled:
	return "errSSLPeerUserCancelled";
	/* SecBase.h: */
    case errSecNotAvailable:
	return "errSecNotAvailable";
    case errSecDuplicateItem:
	return "errSecDuplicateItem";
    case errSecItemNotFound:
	return "errSecItemNotFound";
#if ! TARGET_OS_EMBEDDED
    case errSecReadOnly:
	return "errSecReadOnly";
    case errSecAuthFailed:
	return "errSecAuthFailed";
    case errSecNoSuchKeychain:
	return "errSecNoSuchKeychain";
    case errSecInvalidKeychain:
	return "errSecInvalidKeychain";
    case errSecDuplicateKeychain:
	return "errSecDuplicateKeychain";
    case errSecDuplicateCallback:
	return "errSecDuplicateCallback";
    case errSecInvalidCallback:
	return "errSecInvalidCallback";
    case errSecBufferTooSmall:
	return "errSecBufferTooSmall";
    case errSecDataTooLarge:
	return "errSecDataTooLarge";
    case errSecNoSuchAttr:
	return "errSecNoSuchAttr";
    case errSecInvalidItemRef:
	return "errSecInvalidItemRef";
    case errSecInvalidSearchRef:
	return "errSecInvalidSearchRef";
    case errSecNoSuchClass:
	return "errSecNoSuchClass";
    case errSecNoDefaultKeychain:
	return "errSecNoDefaultKeychain";
    case errSecInteractionNotAllowed:
	return "errSecInteractionNotAllowed";
    case errSecReadOnlyAttr:
	return "errSecReadOnlyAttr";
    case errSecWrongSecVersion:
	return "errSecWrongSecVersion";
    case errSecKeySizeNotAllowed:
	return "errSecKeySizeNotAllowed";
    case errSecNoStorageModule:
	return "errSecNoStorageModule";
    case errSecNoCertificateModule:
	return "errSecNoCertificateModule";
    case errSecNoPolicyModule:
	return "errSecNoPolicyModule";
    case errSecInteractionRequired:
	return "errSecInteractionRequired";
    case errSecDataNotAvailable:
	return "errSecDataNotAvailable";
    case errSecDataNotModifiable:
	return "errSecDataNotModifiable";
    case errSecCreateChainFailed:
	return "errSecCreateChainFailed";
    case errSecACLNotSimple:
	return "errSecACLNotSimple";
    case errSecPolicyNotFound:
	return "errSecPolicyNotFound";
    case errSecInvalidTrustSetting:
	return "errSecInvalidTrustSetting";
    case errSecNoAccessForItem:
	return "errSecNoAccessForItem";
    case errSecInvalidOwnerEdit:
	return "errSecInvalidOwnerEdit";
#endif /* ! TARGET_OS_EMBEDDED */
    default:
	return "<unknown>";
    }
}

