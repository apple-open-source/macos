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


/*
 * CertBuilder.cpp - sublasses of various snacc-generated cert-related
 * classes. 
 *
 * Created 9/1/2000 by Doug Mitchell. 
 * Copyright (c) 2000 by Apple Computer. 
 */

#include "CertBuilder.h"
#include <Security/cssmerr.h>
#include <Security/utilities.h>

#define BUF_ENC_EXTRA	64

/*
 * Name is a complex structure which boils down to an arbitrarily
 * large array of (usually) printable names. We facilitate the
 * construction of the array, one AttributeTypeAndDistinguishedValue
 * per RelativeDistinguishedName. This is the format commonly used
 * in the real world, though it's legal to have multiple ATDVs
 * per RDN - we just don't do it here. 
 *
 * Typically the object manipulated here is inserted into a 
 * CertificateToSign object, as issuer or subject. 
 */
void NameBuilder::addATDV(  
		const AsnOid &type,		// id_at_commonName, etc. from sm_x501if
		const char *value,		// the bytes
		size_t valueLen,
		DirectoryString::ChoiceIdEnum stringType,	
								// printableStringCid, etc.
								//   from sm_x520sa
		bool primaryDistinguished)
{
	/* cook up the RDN sequence first time thru */
	if(rDNSequence == NULL) {
		rDNSequence = new RDNSequence;
		choiceId = rDNSequenceCid;		// no others available
	}
	
	/* one RelativeDistinguishedName and one ATDV */
	RelativeDistinguishedName *rdn = rDNSequence->Append();
	AttributeTypeAndDistinguishedValue *atdv = rdn->Append();
	
	/* 
	 * fill in the ATDV
	 * FIXME - AttributeTypeAndDistinguishedValueSetOf??? What's that?
	 */
	atdv->type = type;
	if(!primaryDistinguished) {
		/* default is true, only encode if not default */
		atdv->primaryDistinguished = new AsnBool(primaryDistinguished);
	}
	
	/* DirectoryString from sm_x520sa */
	DirectoryString dirStr;
	dirStr.choiceId = stringType;
	switch(stringType) {
		case DirectoryString::teletexStringCid:
			dirStr.teletexString = new TeletexString(value, valueLen);
			break;
		case DirectoryString::printableStringCid:
			dirStr.printableString = new PrintableString(value, valueLen);
			break;
		case DirectoryString::universalStringCid:
			dirStr.universalString = new UniversalString(value, valueLen);
			break;
		case DirectoryString::bmpStringCid:
			dirStr.bmpString = new BMPString(value, valueLen);
			break;
		case DirectoryString::utf8StringCid:
			dirStr.utf8String = new UTF8String(value, valueLen);
			break;
	}

	/* 
	 * As far as I can tell, atdv->value.value is a CSM_Buffer containing 
	 * the encoded dirStr. First malloc a dest buffer...
	 */
	size_t bufLen = valueLen + BUF_ENC_EXTRA;
	char *buf = (char *)calloc(1, bufLen);
	if(buf == NULL) {
		CssmError::throwMe(CSSMERR_CL_MEMORY_ERROR);
	}
	
	/* encode dirStr --> abuf */
	AsnBuf abuf;
	abuf.Init(buf, bufLen);
	abuf.ResetInWriteRvsMode();
	AsnLen bytesEnc;
	dirStr.BEncPdu(abuf, bytesEnc);
	if(bytesEnc > bufLen) {
		#ifndef NDEBUG
		printf("Whoops! Buffer overflow\n");
		#endif
		/* throw */
	}
	
	/* install the result into CSM_Buffer, which mallocs & copies */
	atdv->value.value = new CSM_Buffer(abuf.DataPtr(), abuf.DataLen());
	free(buf);
}

/*
 * Custom AsnOid, used for converting CssmOid to AsnOid. The Snacc class
 * declaration doesn't provide a means to construct from, or set by,
 * pre-encoded OID bytes (which are available in a CssmOid).
 */
OidBuilder::OidBuilder(const CSSM_OID &coid)
{
    oid =  Asn1Alloc (coid.Length);
	memcpy(oid, coid.Data, coid.Length);
	octetLen = coid.Length;
}

