/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*  *********************************************************************
    File: sslerrs.h

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslerrs.h    Errors SSLRef can return


    ****************************************************************** */

#ifndef _SSLERRS_H_
#define _SSLERRS_H_ 1

/*
 * FIXME - we should eventually do away with these and just use the ones
 * on SecureTransport.h. For now, public functions (mostly in sslctx.h)
 * call sslErrToOsStatus() to map these to the apropriate OSStatus.
 *
 * If you add to this, add to errSSLxxx list in SecureTransport.h and also
 * to the sslErrMap map in appleGlue.c.
 */
typedef enum
{   SSLNoErr = 0,
    SSLMemoryErr,
    SSLUnsupportedErr,
    SSLProtocolErr,
    SSLNegotiationErr,
    SSLFatalAlert,
    SSLWouldBlockErr,
    SSLIOErr,
    SSLSessionNotFoundErr,
    SSLConnectionClosedGraceful,
    SSLConnectionClosedError,
    X509CertChainInvalidErr,
    SSLBadCert,
    
    /* new errors for APPLE_CDSA */
    SSLCryptoError,
    SSLInternalError,
    SSLAttachFailure,				/* CSSM_ModuleAttach failure */
    SSLDataOverflow,				/* data buffer overflow */
    SSLUnknownRootCert,				/* valid cert chain, untrusted root */
    SSLNoRootCert,					/* cert chain not verified by root */
    SSLCertExpired,					/* chain had an expired cert */
    SSLBadStateErr,					/* connection in wrong state */
	SSLCertNotYetValid,
	SSLConnectionClosedNoNotify,	/* server closed session with no 
									 *     notification */
    /* etc. */
    
    SSL_NoSuchError					/* no comma, get it? */
} SSLErr;

#endif
