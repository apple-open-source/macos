
/*
	File:		sslBER_Dummy.cpp

	Contains:	stubs of routines in sslBER.cpp to enable standalone
				build for indexing purposes. Unlike the real sslBER.cpp,
				this version does not require the SecurityANS1 files 
				(which are not exported from Security.framework). 

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "ssl.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslBER.h"

#include <string.h>

/*
 * Given a PKCS-1 encoded RSA public key, extract the 
 * modulus and public exponent.
 *
 * RSAPublicKey ::= SEQUENCE {
 *		modulus INTEGER, -- n
 *		publicExponent INTEGER -- e }
 */
 
OSStatus sslDecodeRsaBlob(
	const SSLBuffer	*blob,			/* PKCS-1 encoded */
	SSLBuffer		*modulus,		/* data mallocd and RETURNED */
	SSLBuffer		*exponent)		/* data mallocd and RETURNED */
{
	return errSSLBadCert;
}

/*
 * Given a raw modulus and exponent, cook up a
 * BER-encoded RSA public key blob.
 */
OSStatus sslEncodeRsaBlob(
	const SSLBuffer	*modulus,		
	const SSLBuffer	*exponent,		
	SSLBuffer		*blob)			/* data mallocd and RETURNED */
{
	return errSSLCrypto;
}

