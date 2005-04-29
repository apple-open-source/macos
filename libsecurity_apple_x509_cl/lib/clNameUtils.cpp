/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
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
 * clNameUtils.cpp - support for Name, GeneralizedName, all sorts of names
 */
 
#include "clNameUtils.h"
#include "clNssUtils.h"
#include "cldebugging.h"
#include <security_utilities/utilities.h>

#pragma mark ----- NSS_Name <--> CSSM_X509_NAME -----

/* 
 * NSS_ATV --> CSSM_X509_TYPE_VALUE_PAIR
 */

void CL_nssAtvToCssm(
	const NSS_ATV				&nssObj,
	CSSM_X509_TYPE_VALUE_PAIR	&cssmObj,
	Allocator				&alloc)
{
	/* tag and decoded data */
	cssmObj.valueType = nssObj.value.tag;
	clAllocCopyData(alloc, nssObj.value.item, cssmObj.value);
	/* the OID */
	clAllocCopyData(alloc, nssObj.type, cssmObj.type);
}

/* NSS_RDN --> CSSM_X509_RDN */
void CL_nssRdnToCssm(
	const NSS_RDN	&nssObj,
	CSSM_X509_RDN	&cssmObj,
	Allocator	&alloc,
	SecNssCoder		&coder)		// conversion requires further decoding
{
	memset(&cssmObj, 0, sizeof(cssmObj));
	unsigned numAtvs = clNssArraySize((const void **)nssObj.atvs);
	if(numAtvs == 0) {
		return;
	}
	
	size_t len = numAtvs * sizeof(CSSM_X509_TYPE_VALUE_PAIR);
	cssmObj.AttributeTypeAndValue = 
			(CSSM_X509_TYPE_VALUE_PAIR_PTR)alloc.malloc(len);
	cssmObj.numberOfPairs = numAtvs;
	CSSM_X509_TYPE_VALUE_PAIR_PTR cssmAtvs = cssmObj.AttributeTypeAndValue;
	memset(cssmAtvs, 0, len);
	
	for(unsigned dex=0; dex<numAtvs; dex++) {
		CL_nssAtvToCssm(*(nssObj.atvs[dex]), cssmAtvs[dex], alloc);
	}
	return;
}

/* NSS_Name --> CSSM_X509_NAME */
void CL_nssNameToCssm(
	const NSS_Name	&nssObj,
	CSSM_X509_NAME	&cssmObj,
	Allocator	&alloc)
{
	memset(&cssmObj, 0, sizeof(cssmObj));
	unsigned numRdns = clNssArraySize((const void **)nssObj.rdns);
	if(numRdns == 0) {
		/* not technically an error */
		return;
	}
	
	size_t len = numRdns * sizeof(CSSM_X509_RDN);
	cssmObj.RelativeDistinguishedName = (CSSM_X509_RDN_PTR)alloc.malloc(len);
	cssmObj.numberOfRDNs = numRdns;
	CSSM_X509_RDN_PTR cssmRdns = cssmObj.RelativeDistinguishedName;
	memset(cssmRdns, 0, len);
	
	SecNssCoder	coder;		// conversion requires further decoding
	
	for(unsigned dex=0; dex<numRdns; dex++) {
		CL_nssRdnToCssm(*(nssObj.rdns[dex]), cssmRdns[dex], alloc, coder);
	}
	return;
}

/* 
 * CSSM_X509_TYPE_VALUE_PAIR --> NSS_ATV 
 */
void CL_cssmAtvToNss(
	const CSSM_X509_TYPE_VALUE_PAIR	&cssmObj,
	NSS_ATV							&nssObj,
	SecNssCoder						&coder)
{
	memset(&nssObj, 0, sizeof(nssObj));
	
	/* copy the OID */
	coder.allocCopyItem(cssmObj.type, nssObj.type);
	
	/* tag and value */
	nssObj.value.tag = cssmObj.valueType;
	coder.allocCopyItem(cssmObj.value, nssObj.value.item);
}

/* CSSM_X509_RDN --> NSS_RDN */
void CL_cssmRdnToNss(
	const CSSM_X509_RDN	&cssmObj,
	NSS_RDN				&nssObj,
	SecNssCoder			&coder)
{
	memset(&nssObj, 0, sizeof(nssObj));
	
	/* alloc NULL-terminated array of ATV pointers */
	unsigned numAtvs = cssmObj.numberOfPairs;
	unsigned size = (numAtvs + 1) * sizeof(void *);
	nssObj.atvs = (NSS_ATV **)coder.malloc(size);
	memset(nssObj.atvs, 0, size);
	
	/* grind thru the elements */
	for(unsigned atvDex=0; atvDex<numAtvs; atvDex++) {
		nssObj.atvs[atvDex] = (NSS_ATV *)coder.malloc(sizeof(NSS_ATV));
		CL_cssmAtvToNss(cssmObj.AttributeTypeAndValue[atvDex],
			*nssObj.atvs[atvDex], coder);
	}
}

/* CSSM_X509_NAME --> NSS_Name */
void CL_cssmNameToNss(
	const CSSM_X509_NAME	&cssmObj,
	NSS_Name				&nssObj,
	SecNssCoder				&coder)
{
	memset(&nssObj, 0, sizeof(nssObj));
	
	/* alloc NULL-terminated array of RDN pointers */
	unsigned numRdns = cssmObj.numberOfRDNs;
	nssObj.rdns = (NSS_RDN **)clNssNullArray(numRdns, coder);
	
	/* grind thru the elements */
	for(unsigned rdnDex=0; rdnDex<numRdns; rdnDex++) {
		nssObj.rdns[rdnDex] = (NSS_RDN *)coder.malloc(sizeof(NSS_RDN));
		CL_cssmRdnToNss(cssmObj.RelativeDistinguishedName[rdnDex],
			*nssObj.rdns[rdnDex], coder);
	}
}

#pragma mark ----- Name Normalization -----

void CL_normalizeString(
	char *strPtr,
	int &strLen)					// IN/OUT
{
	char *pCh = strPtr;				// working ptr
	char *pD = pCh;					// start of good string chars
	char *pEos = pCh + strLen - 1;
	
	if(strLen == 0) {
		return;
	}

	/* adjust if Length included NULL terminator */
	while(*pEos == 0) {
		pEos--;
	}
	
	/* Remove trailing spaces */
	while(isspace(*pEos)) {
		pEos--;
	}
	
	/* Point to one past last non-space character */
	pEos++;

	/* upper case */
	while(pCh < pEos) {
		*pCh = toupper(*pCh);
		pCh++;
	}
	
	/* clean out whitespace */
	/* 
	 * 1. skip all leading whitespace 
	 */
	pCh = pD;
	while(isspace(*pCh) && (pCh < pEos)) {
		pCh++;
	}
	
	/*
	 * 2. eliminate multiple whitespace.
	 *    pCh points to first non-white char.
	 *	  pD still points to start of string
	 */
	char ch;
	while(pCh < pEos) {
		ch = *pCh++;
		*pD++ = ch;		// normal case
		if( isspace(ch) ){
			/* skip 'til next nonwhite */
			while(isspace(*pCh) && (pCh < pEos)) {
				pCh++;
			}
		}
	};

	strLen = pD - strPtr;
}

/* 
 * Normalize an RDN. Per RFC2459 (4.1.2.4), printable strings are case 
 * insensitive and we're supposed to ignore leading and trailing 
 * whitespace, and collapse multiple whitespace characters into one. 
 *
 * Incoming NSS_Name is assumed to be entirely within specifed coder's
 * address space; we'll be munging some of that and possibly replacing
 * some pointers with others allocated from the same space. 
 */
void CL_normalizeX509NameNSS(
	NSS_Name &nssName,
	SecNssCoder &coder)
{
	unsigned numRdns = clNssArraySize((const void **)nssName.rdns);
	if(numRdns == 0) {
		/* not technically an error */
		return;
	}
	
	for(unsigned rdnDex=0; rdnDex<numRdns; rdnDex++) {
		NSS_RDN *rdn = nssName.rdns[rdnDex];
		assert(rdn != NULL);
		unsigned numAttrs = clNssArraySize((const void **)rdn->atvs);
		if(numAttrs == 0) {
			clFieldLog("clNormalizeX509Name: zero numAttrs at index %d", rdnDex);
			continue;
		}
		
		/* descend into array of attribute/values */
		for(unsigned attrDex=0; attrDex<numAttrs; attrDex++) {
			NSS_ATV *attr = rdn->atvs[attrDex];
			assert(attr != NULL);
			
			/* 
			 * attr->value is an ASN_ANY containing an encoded
			 * string. We only normalize Prinatable String types. 
			 * If we find one, decode it, normalize it, encode the
			 * result, and put the encoding back in attr->value.
			 * We temporarily "leak" the original string, which only
			 * has a lifetime of the incoming SecNssCoder. 
			 */
			NSS_TaggedItem &attrVal = attr->value;
			if(attrVal.tag != SEC_ASN1_PRINTABLE_STRING) {
				/* skip it */
				continue;
			}

			/* normalize */
			char *strPtr = (char *)attrVal.item.Data;
			int newLen = attrVal.item.Length;
			CL_normalizeString(strPtr, newLen);
			
			/* possible length adjustment */
			attrVal.item.Length = newLen;
		}	/* for each attribute/value */
	}		/* for each RDN */
}

#pragma mark ----- CE_GeneralNames <--> NSS_GeneralNames -----

void CL_nssGeneralNameToCssm(
	NSS_GeneralName &nssObj,
	CE_GeneralName &cdsaObj,
	SecNssCoder &coder,				// for temp decoding
	Allocator &alloc)			// destination 
{
	memset(&cdsaObj, 0, sizeof(cdsaObj));
	PRErrorCode prtn;

	/* for caller's CE_GeneralName */
	CSSM_BOOL berEncoded = CSSM_FALSE;
	CE_GeneralNameType cdsaTag;
	
	/*
	 * At this point, depending on the decoded object's tag, we either
	 * have the final bytes to copy out, or we need to decode further.
	 * After this switch, if doCopy is true, give the caller a copy
	 * of nssObj.item.
	 */
	bool doCopy = true;
	switch(nssObj.tag) {
		case NGT_OtherName:		// ASN_ANY -> CE_OtherName
		{
			cdsaTag = GNT_OtherName;
			
			/* decode to coder memory */
			CE_OtherName *nssOther = 
				(CE_OtherName *)coder.malloc(sizeof(CE_OtherName));
			memset(nssOther, 0, sizeof(CE_OtherName));
			prtn = coder.decodeItem(nssObj.item, 
				kSecAsn1GenNameOtherNameTemplate, 
				nssOther);
			if(prtn) {
				clErrorLog("CL_nssGeneralNameToCssm: error decoding "
						"OtherName\n");
				CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
			}
			
			/* copy out to caller */
			clAllocData(alloc, cdsaObj.name, sizeof(CE_OtherName));
			clCopyOtherName(*nssOther, *((CE_OtherName *)cdsaObj.name.Data), 
				alloc);
			doCopy = false;
			break;
		}
		case NGT_RFC822Name:	// IA5String, done
			cdsaTag = GNT_RFC822Name;
			break;
		case NGT_DNSName:		// IA5String
			cdsaTag = GNT_DNSName;
			break;
		case NGT_X400Address:	// ASY_ANY, leave alone
			cdsaTag = GNT_X400Address;
			berEncoded = CSSM_TRUE;
			break;
		case NGT_DirectoryName:	// ASN_ANY --> NSS_Name
		{
			cdsaTag = GNT_DirectoryName;
			
			/* Decode to coder memory */
			NSS_Name *nssName = (NSS_Name *)coder.malloc(sizeof(NSS_Name));
			memset(nssName, 0, sizeof(NSS_Name));
			prtn = coder.decodeItem(nssObj.item, kSecAsn1NameTemplate, nssName);
			if(prtn) {
				clErrorLog("CL_nssGeneralNameToCssm: error decoding "
						"NSS_Name\n");
				CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
			}
			
			/* convert & copy out to caller */
			clAllocData(alloc, cdsaObj.name, sizeof(CSSM_X509_NAME));
			CL_nssNameToCssm(*nssName, 
				*((CSSM_X509_NAME *)cdsaObj.name.Data), alloc);
			doCopy = false;
			break;
		}
		case NGT_EdiPartyName:	// ASN_ANY, leave alone
			cdsaTag = GNT_EdiPartyName;
			berEncoded = CSSM_TRUE;
			break;
		case NGT_URI:			// IA5String
			cdsaTag = GNT_URI;
			break;
		case NGT_IPAddress:		// OCTET_STRING
			cdsaTag = GNT_IPAddress;
			break;
		case NGT_RegisteredID:	// OID
			cdsaTag = GNT_RegisteredID;
			break;
		default:
			clErrorLog("CL_nssGeneralNameToCssm: bad name tag\n");
			CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
	}	
	
	cdsaObj.nameType = cdsaTag;
	cdsaObj.berEncoded = berEncoded;
	if(doCopy) {
		clAllocCopyData(alloc, nssObj.item, cdsaObj.name);
	}
}

void CL_nssGeneralNamesToCssm(
	const NSS_GeneralNames &nssObj,
	CE_GeneralNames &cdsaObj,
	SecNssCoder &coder,				// for temp decoding
	Allocator &alloc)			// destination 
{
	memset(&cdsaObj, 0, sizeof(cdsaObj));
	unsigned numNames = clNssArraySize((const void **)nssObj.names);
	if(numNames == 0) {
		return;
	}
	
	/*
	 * Decode each name element, currently a raw ASN_ANY blob.
	 * Then convert each result into CDSA form.
	 * This array of (NSS_GeneralName)s is temporary, it doesn't
	 * persist outside of this routine other than the fact that it's
	 * mallocd by the coder arena pool. 
	 */
	NSS_GeneralName *names = 
		(NSS_GeneralName *)coder.malloc(sizeof(NSS_GeneralName) * numNames);
	memset(names, 0, sizeof(NSS_GeneralName) * numNames);
	cdsaObj.generalName = (CE_GeneralName *)alloc.malloc(
		sizeof(CE_GeneralName) * numNames);
	cdsaObj.numNames = numNames;
	
	for(unsigned dex=0; dex<numNames; dex++) {
		if(coder.decodeItem(*nssObj.names[dex], kSecAsn1GeneralNameTemplate,
				&names[dex])) {
			clErrorLog("***CL_nssGeneralNamesToCssm: Error decoding "
				"General.name\n");
			CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
		}
		
		CL_nssGeneralNameToCssm(names[dex],
			cdsaObj.generalName[dex],
			coder, alloc);
	}
}

void CL_cssmGeneralNameToNss(
	CE_GeneralName &cdsaObj,
	NSS_GeneralName &nssObj,		// actually an NSSTaggedItem
	SecNssCoder &coder)				// for temp decoding
{
	memset(&nssObj, 0, sizeof(nssObj));
	
	/*
	 * The default here is just to use the app-supplied data as is...
	 */
	nssObj.item = cdsaObj.name;
	unsigned char itemTag;			// for nssObj.tag
	bool doCopy = false;			// unless we have to modify tag byte
	unsigned char overrideTag;		// to force context-specific tag for
									//   an ASN_ANY
	PRErrorCode prtn;
									
	switch(cdsaObj.nameType) {
		case GNT_OtherName:	
			/*
			 * Caller supplies an CE_OtherName. Encode it.
			 */
			if((cdsaObj.name.Length != sizeof(CE_OtherName)) ||
			   (cdsaObj.name.Data == NULL)) {
				clErrorLog("CL_cssmGeneralNameToNss: OtherName.Length"
					" error\n");
				CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
			}
			prtn = coder.encodeItem(cdsaObj.name.Data,
				kSecAsn1OtherNameTemplate, nssObj.item);
			if(prtn) {
				clErrorLog("CL_cssmGeneralNameToNss: OtherName encode"
					" error\n");
				CssmError::throwMe(CSSMERR_CL_MEMORY_ERROR);
			}
			itemTag = NGT_OtherName;
			break;
		case GNT_RFC822Name:		// IA5String
			itemTag = NGT_RFC822Name;
			break;
		case GNT_DNSName:			// IA5String
			itemTag = NGT_DNSName;
			break;
		case GNT_X400Address:		// caller's resposibility
			/*
			 * Encoded as ASN_ANY, the only thing we do is to 
			 * force the correct context-specific tag
			 */
			itemTag = GNT_X400Address;
			if(!cdsaObj.berEncoded) {
				clErrorLog("CL_cssmGeneralNameToNss: X400Address must"
					" be BER-encoded\n");
				CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
			}			
			overrideTag = SEC_ASN1_CONTEXT_SPECIFIC | 
				SEC_ASN1_CONSTRUCTED | NGT_X400Address;
			doCopy = true;
			break;
		case GNT_DirectoryName:	
		{
			/*
			 * Caller supplies an CSSM_X509_NAME. Convert to NSS
			 * format and encode it.
			 */
			if((cdsaObj.name.Length != sizeof(CSSM_X509_NAME)) || 
			   (cdsaObj.name.Data == NULL)) {
				clErrorLog("CL_cssmGeneralNameToNss: DirectoryName.Length"
					" error\n");
				CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
			}
			NSS_Name nssName;
			CSSM_X509_NAME_PTR cdsaName = 
				(CSSM_X509_NAME_PTR)cdsaObj.name.Data;
			CL_cssmNameToNss(*cdsaName, nssName, coder);
			prtn = coder.encodeItem(&nssName,
				kSecAsn1NameTemplate, nssObj.item);
			if(prtn) {
				clErrorLog("CL_cssmGeneralNameToNss: X509Name encode"
					" error\n");
				CssmError::throwMe(CSSMERR_CL_MEMORY_ERROR);
			}
			itemTag = GNT_DirectoryName;
			
			/*
			 * AND, munge the tag to make it a context-specific
			 * sequence
			 * no, not needed, this is wrapped in an explicit...
			 */
			//nssObj.item.Data[0] = SEC_ASN1_CONTEXT_SPECIFIC | 
			//	SEC_ASN1_CONSTRUCTED | GNT_DirectoryName;

			break;
		}
		case GNT_EdiPartyName:		// caller's resposibility
			/*
			 * Encoded as ASN_ANY, the only thing we do is to 
			 * force the correct context-specific tag
			 */
			itemTag = GNT_EdiPartyName;
			if(!cdsaObj.berEncoded) {
				clErrorLog("CL_cssmGeneralNameToNss: EdiPartyName must"
					" be BER-encoded\n");
				CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
			}
			overrideTag = SEC_ASN1_CONTEXT_SPECIFIC |  NGT_X400Address;
			doCopy = true;
			break;
		case GNT_URI:				// IA5String
			itemTag = GNT_URI;
			break;
		case GNT_IPAddress:			// OCTET_STRING
			itemTag = NGT_IPAddress;
			break;
		case GNT_RegisteredID:		// OID
			itemTag = NGT_RegisteredID;
			break;
		default:
			clErrorLog("CL_cssmGeneralNameToNss: bad name tag\n");
			CssmError::throwMe(CSSMERR_CL_UNKNOWN_TAG);
	}
	if(doCopy) {
		coder.allocCopyItem(cdsaObj.name, nssObj.item);
		nssObj.item.Data[0] = overrideTag;
	}
	nssObj.tag = itemTag;
}

void CL_cssmGeneralNamesToNss(
	const CE_GeneralNames &cdsaObj, 
	NSS_GeneralNames &nssObj,
	SecNssCoder &coder)
{
	uint32 numNames = cdsaObj.numNames;
	nssObj.names = (CSSM_DATA **)clNssNullArray(numNames, coder);
	
	/* 
	 * Convert each element in cdsaObj to NSS form, encode, drop into
	 * the ASN_ANY array.
	 *
	 * This array of (NSS_GeneralName)s is temporary, it doesn't
	 * persist outside of this routine other than the fact that it's
	 * mallocd by the coder arena pool. 
	 */
	NSS_GeneralName *names = 
		(NSS_GeneralName *)coder.malloc(sizeof(NSS_GeneralName) * numNames);
	memset(names, 0, sizeof(NSS_GeneralName) * numNames);
	for(unsigned dex=0; dex<cdsaObj.numNames; dex++) {
		nssObj.names[dex] = (CSSM_DATA_PTR)coder.malloc(sizeof(CSSM_DATA));
		memset(nssObj.names[dex], 0, sizeof(CSSM_DATA));
		CL_cssmGeneralNameToNss(cdsaObj.generalName[dex],
			names[dex], coder);
		if(coder.encodeItem(&names[dex], kSecAsn1GeneralNameTemplate,
				*nssObj.names[dex])) {
			clErrorLog("***Error encoding General.name\n");
			CssmError::throwMe(CSSMERR_CL_MEMORY_ERROR);
		}
	}
}

/* Copy a CE_OtherName */
void clCopyOtherName(
	const CE_OtherName &src,
	CE_OtherName &dst,
	Allocator &alloc)
{
	clAllocCopyData(alloc, src.typeId, dst.typeId);
	clAllocCopyData(alloc, src.value, dst.value);
}

#pragma mark ----- Name-related Free -----

void CL_freeAuthorityKeyId(
	CE_AuthorityKeyID 				&cdsaObj,
	Allocator					&alloc)
{
	alloc.free(cdsaObj.keyIdentifier.Data);
	CL_freeCssmGeneralNames(cdsaObj.generalNames, alloc);
	alloc.free(cdsaObj.generalNames);
	alloc.free(cdsaObj.serialNumber.Data);
	memset(&cdsaObj, 0, sizeof(CE_AuthorityKeyID));
}

void CL_freeCssmGeneralName(
	CE_GeneralName 		&genName,
	Allocator 			&alloc)
{
	switch(genName.nameType) {
		/* 
		 * Two special cases here.
		 */
		case GNT_DirectoryName:
			if((!genName.berEncoded) &&					// we're flexible
					(genName.name.Length == 
						sizeof(CSSM_X509_NAME))) {		// paranoia
				CL_freeX509Name((CSSM_X509_NAME_PTR)genName.name.Data, alloc);
			}
			break;
			
		case GNT_OtherName:
			if((!genName.berEncoded) &&					// we're flexible
					(genName.name.Length == 
						sizeof(CE_OtherName))) {		// paranoia
				CE_OtherName *con = (CE_OtherName *)genName.name.Data;
				CL_freeOtherName(con, alloc);
			}
			break;
		default:
			break;
	}
	/* and always free this */
	alloc.free(genName.name.Data);
}

void CL_freeCssmGeneralNames(
	CE_GeneralNames 		*cdsaObj,
	Allocator 				&alloc)
{
	if(cdsaObj == NULL) {
		return;
	}
	for(unsigned i=0; i<cdsaObj->numNames; i++) {
		CL_freeCssmGeneralName(cdsaObj->generalName[i], alloc);
	}
	if(cdsaObj->numNames) {
		memset(cdsaObj->generalName, 0, cdsaObj->numNames * sizeof(CE_GeneralName));
		alloc.free(cdsaObj->generalName);
	}
	memset(cdsaObj, 0, sizeof(CE_GeneralNames));
}

void CL_freeCssmDistPoints(
	CE_CRLDistPointsSyntax	*cssmDps,
	Allocator			&alloc)
{
	if(cssmDps == NULL) {
		return;
	}
	for(unsigned dex=0; dex<cssmDps->numDistPoints; dex++) {
		CE_CRLDistributionPoint *cssmDp = &cssmDps->distPoints[dex];
		if(cssmDp->distPointName) {
			CL_freeCssmDistPointName(cssmDp->distPointName, alloc);
			alloc.free(cssmDp->distPointName);
		}
		if(cssmDp->crlIssuer) {
			CL_freeCssmGeneralNames(cssmDp->crlIssuer, alloc);
			alloc.free(cssmDp->crlIssuer);
		}
	}
	memset(cssmDps->distPoints, 0, 
		cssmDps->numDistPoints * sizeof(CE_CRLDistributionPoint));
	alloc.free(cssmDps->distPoints);
	memset(cssmDps, 0, sizeof(*cssmDps));
}

void CL_freeCssmDistPointName(
	CE_DistributionPointName	*cssmDpn,
	Allocator				&alloc)
{
	if(cssmDpn == NULL) {
		return;
	}
	switch(cssmDpn->nameType) {
		case CE_CDNT_FullName:
			CL_freeCssmGeneralNames(cssmDpn->dpn.fullName, alloc);
			alloc.free(cssmDpn->dpn.fullName);
			break;
		case CE_CDNT_NameRelativeToCrlIssuer:
			CL_freeX509Rdn(cssmDpn->dpn.rdn, alloc);
			alloc.free(cssmDpn->dpn.rdn);
			break;
	}
	memset(cssmDpn, 0, sizeof(*cssmDpn));
}

/* free contents of an CSSM_X509_NAME */
void CL_freeX509Name(
	CSSM_X509_NAME_PTR	x509Name,
	Allocator		&alloc)
{
	if(x509Name == NULL) {
		return;
	}
	for(unsigned rdnDex=0; rdnDex<x509Name->numberOfRDNs; rdnDex++) {
		CSSM_X509_RDN_PTR rdn = &x509Name->RelativeDistinguishedName[rdnDex];
		CL_freeX509Rdn(rdn, alloc);
	}
	alloc.free(x509Name->RelativeDistinguishedName);
	memset(x509Name, 0, sizeof(CSSM_X509_NAME));
}

void CL_freeX509Rdn(
	CSSM_X509_RDN_PTR	rdn,
	Allocator		&alloc)
{
	if(rdn == NULL) {
		return;
	}
	for(unsigned atvDex=0; atvDex<rdn->numberOfPairs; atvDex++) {
		CSSM_X509_TYPE_VALUE_PAIR_PTR atv = 
			&rdn->AttributeTypeAndValue[atvDex];
		alloc.free(atv->type.Data);
		alloc.free(atv->value.Data);
		memset(atv, 0, sizeof(CSSM_X509_TYPE_VALUE_PAIR));
	}
	alloc.free(rdn->AttributeTypeAndValue);
	memset(rdn, 0, sizeof(CSSM_X509_RDN));
}

void CL_freeOtherName(
	CE_OtherName		*cssmOther,
	Allocator		&alloc)
{
	if(cssmOther == NULL) {
		return;
	}
	alloc.free(cssmOther->typeId.Data);
	alloc.free(cssmOther->value.Data);
	memset(cssmOther, 0, sizeof(*cssmOther));
}

void CL_freeCssmIssuingDistPoint(
	CE_IssuingDistributionPoint	*cssmIdp,
	Allocator				&alloc)
{
	CL_freeCssmDistPointName(cssmIdp->distPointName, alloc);
}

