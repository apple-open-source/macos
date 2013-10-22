/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
 * cuPrintCert.cpp - Parse a cert or CRL, dump contents.
 */
#include "cuCdsaUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include <Security/oidscert.h>
#include <Security/oidscrl.h>
#include <Security/x509defs.h>
#include <Security/oidsattr.h>
#include <Security/oidsalg.h>
#include <Security/cssmapple.h>
#include <string.h>
#include "cuPrintCert.h"
#include "cuOidParser.h"
#include "cuTimeStr.h"
#include <Security/certextensions.h>
#include <Security/SecAsn1Coder.h>
#include <Security/keyTemplates.h>

static const char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" 
};
	
static void printTimeStr(const CSSM_DATA *cssmTime)
{
	struct tm tm;
	
	/* ignore cssmTime->timeType for now */
	if(cuTimeStringToTm((char *)cssmTime->Data, (unsigned int)cssmTime->Length, &tm)) {
		printf("***Bad time string format***\n");
		return;
	}
	if(tm.tm_mon > 11) {
		printf("***Bad time string format***\n");
		return;
	}
	printf("%02d:%02d:%02d %s %d, %04d\n",
		tm.tm_hour, tm.tm_min, tm.tm_sec,
		months[tm.tm_mon], tm.tm_mday, tm.tm_year + 1900);

}


static void printTime(const CSSM_X509_TIME *cssmTime)
{
	/* ignore cssmTime->timeType for now */
	printTimeStr(&cssmTime->time);
}

static void printDataAsHex(
	const CSSM_DATA *d,
	unsigned maxToPrint = 0)		// optional, 0 means print it all
{
	unsigned i;
	bool more = false;
	uint32 len = (uint32)d->Length;
	uint8 *cp = d->Data;
	
	if((maxToPrint != 0) && (len > maxToPrint)) {
		len = maxToPrint;
		more = true;
	}	
	for(i=0; i<len; i++) {
		printf("%02X ", ((unsigned char *)cp)[i]);
	}
	if(more) {
		printf("...\n");
	}
	else {
		printf("\n");
	}
}

/*
 * Identify CSSM_BER_TAG with a C string.
 */
static const char *tagTypeString(
	CSSM_BER_TAG tagType)
{
	static char unknownType[80];
	
	switch(tagType) {
		case BER_TAG_UNKNOWN:
			return "BER_TAG_UNKNOWN";
		case BER_TAG_BOOLEAN:
			return "BER_TAG_BOOLEAN";
		case BER_TAG_INTEGER:
			return "BER_TAG_INTEGER";
		case BER_TAG_BIT_STRING:
			return "BER_TAG_BIT_STRING";
		case BER_TAG_OCTET_STRING:
			return "BER_TAG_OCTET_STRING";
		case BER_TAG_NULL:
			return "BER_TAG_NULL";
		case BER_TAG_OID:
			return "BER_TAG_OID";
		case BER_TAG_SEQUENCE:
			return "BER_TAG_SEQUENCE";
		case BER_TAG_SET:
			return "BER_TAG_SET";
		case BER_TAG_PRINTABLE_STRING:
			return "BER_TAG_PRINTABLE_STRING";
		case BER_TAG_T61_STRING:
			return "BER_TAG_T61_STRING";
		case BER_TAG_IA5_STRING:
			return "BER_TAG_IA5_STRING";
		case BER_TAG_UTC_TIME:
			return "BER_TAG_UTC_TIME";
		case BER_TAG_GENERALIZED_TIME:
			return "BER_TAG_GENERALIZED_TIME";
		default:
			sprintf(unknownType, "Other type (0x%x)", tagType);
			return unknownType;
	}
}

/*
 * Print an OID, assumed to be in BER encoded "Intel" format
 * Length is inferred from oid->Length
 * Tag is implied
 */
static void printOid(OidParser &parser, const CSSM_DATA *oid)
{
	char strBuf[OID_PARSER_STRING_SIZE];
	
	if(oid == NULL) {
		printf("NULL\n");
		return;
	}
	if((oid->Length == 0) || (oid->Data == NULL)) {
		printf("EMPTY\n");
		return;
	}
	parser.oidParse(oid->Data, (unsigned int)oid->Length, strBuf);
	printf("%s\n", strBuf);
}

/*
 * Used to print generic blobs which we don't really understand.
 * The bytesToPrint argument is usually thing->Length; it's here because snacc
 * peports lengths of bit strings in BITS. Caller knows this and
 * modifies bytesToPrint accordingly. In any case, bytesToPrint is the
 * max number of valid bytes in *thing->Data.
 */ 
#define BLOB_LENGTH_PRINT	3

static void printBlobBytes(
	const char 			*blobType,
	const char 			*quanta,		// e.g., "bytes', "bits"
	uint32			bytesToPrint,
	const CSSM_DATA	*thing)
{
	uint32 dex;
	uint32 toPrint = bytesToPrint;
	
	if(toPrint > BLOB_LENGTH_PRINT) {
		toPrint = BLOB_LENGTH_PRINT;
	}
	printf("%s; Length %u %s; data = ", 
		blobType, (unsigned)thing->Length, quanta);
	for(dex=0; dex<toPrint; dex++) {
		printf("0x%x ", thing->Data[dex]);
		if(dex == (toPrint - 1)) {
			break;
		}
	}
	if(dex < bytesToPrint) {
		printf(" ...\n");
	}
	else {
		printf("\n");
	}
}

/*
 * Print an IA5String or Printable string. Null terminator is not assumed. 
 * Trailing newline is printed.
 */
static void printString(
	const CSSM_DATA *str)
{
	unsigned i;
	char *cp = (char *)str->Data;
	for(i=0; i<str->Length; i++) {
		printf("%c", *cp++);
	}
	printf("\n");
}

static void printDerThing(
	CSSM_BER_TAG		tagType,
	const CSSM_DATA		*thing,
	OidParser 			&parser)
{
	switch(tagType) {
		case BER_TAG_INTEGER:
			printf("%d\n", cuDER_ToInt(thing));
			return;
		case BER_TAG_BOOLEAN:
			if(thing->Length != 1) {
				printf("***malformed BER_TAG_BOOLEAN: length %u data ",
					(unsigned)thing->Length);
			}
			printf("%u\n", cuDER_ToInt(thing));
			return;
		case BER_TAG_PRINTABLE_STRING:
		case BER_TAG_IA5_STRING:	
		case BER_TAG_T61_STRING:		
		case BER_TAG_PKIX_UTF8_STRING:	// mostly printable....	
			printString(thing);
			return;
		case BER_TAG_OCTET_STRING:
			printBlobBytes("Byte string", "bytes", (uint32)thing->Length, thing);
			return;
		case BER_TAG_BIT_STRING:
			printBlobBytes("Bit string", "bits", (uint32)(thing->Length + 7) / 8, thing);
			return;
		case BER_TAG_SEQUENCE:
			printBlobBytes("Sequence", "bytes", (uint32)thing->Length, thing);
			return;
		case BER_TAG_SET:
			printBlobBytes("Set", "bytes", (uint32)thing->Length, thing);
			return;
		case BER_TAG_OID:
			printf("OID = ");
			printOid(parser, thing);
			break;
		default:
			printf("not displayed (tagType = %s; length %u)\n", 
				tagTypeString(tagType), (unsigned)thing->Length);
			break;
			
	}
}

/* compare two OIDs, return CSSM_TRUE if identical */
static CSSM_BOOL compareOids(
	const CSSM_OID *oid1,
	const CSSM_OID *oid2)
{
	if((oid1 == NULL) || (oid2 == NULL)) {
		return CSSM_FALSE;
	}	
	if(oid1->Length != oid2->Length) {
		return CSSM_FALSE;
	}
	if(memcmp(oid1->Data, oid2->Data, oid1->Length)) {
		return CSSM_FALSE;
	}
	else {
		return CSSM_TRUE;
	}
}	

/* 
 * Following a CSSMOID_ECDSA_WithSpecified algorithm is another encoded
 * CSSM_X509_ALGORITHM_IDENTIFIER containing the digest algorithm OID. 
 * Decode and print the OID.
 */
static void printECDSA_SigAlgParams(
	const CSSM_DATA *params,
	OidParser &parser)
{
	SecAsn1CoderRef coder = NULL;
	if(SecAsn1CoderCreate(&coder)) {
		printf("***Error in SecAsn1CoderCreate()\n");
		return;
	}
	CSSM_X509_ALGORITHM_IDENTIFIER algParams;
	memset(&algParams, 0, sizeof(algParams));
	if(SecAsn1DecodeData(coder, params, kSecAsn1AlgorithmIDTemplate,
			&algParams)) {
		printf("***Error decoding CSSM_X509_ALGORITHM_IDENTIFIER\n");
		goto errOut;
	}
	printOid(parser, &algParams.algorithm);
errOut:
	SecAsn1CoderRelease(coder);
}

static void printSigAlg(
	const CSSM_X509_ALGORITHM_IDENTIFIER *sigAlg,
	OidParser 							&parser)
{
	printOid(parser, &sigAlg->algorithm);
	if(sigAlg->parameters.Data != NULL) {
		printf("   alg params      : ");
		if(compareOids(&sigAlg->algorithm, &CSSMOID_ecPublicKey) &&
		   (sigAlg->parameters.Data[0] == BER_TAG_OID) &&
		   (sigAlg->parameters.Length > 2)) {
			/* 
			 * An OID accompanying an ECDSA public key. The OID is an ECDSA curve. 
			 * Do a quickie DER-decode of the OID - it's here in encoded form
			 * because this field is an ASN_ANY - and print the resulting OID.
			 */
			CSSM_OID curveOid = {sigAlg->parameters.Length-2, sigAlg->parameters.Data+2};
			printOid(parser, &curveOid);
		}
		else if(compareOids(&sigAlg->algorithm, &CSSMOID_ECDSA_WithSpecified)) {
			/* 
			 * The accompanying params specify the digest algorithm.
			 */ 
			printECDSA_SigAlgParams(&sigAlg->parameters, parser);
		}
		else {
			/* All others - ASN_ANY - punt */
			printDataAsHex(&sigAlg->parameters, 8);
		}
	}
}

static void printRdn(
	const CSSM_X509_RDN			*rdnp,
	OidParser 					&parser)
{
	CSSM_X509_TYPE_VALUE_PAIR 	*ptvp;
	unsigned					pairDex;
	const char						*fieldName;
	
	for(pairDex=0; pairDex<rdnp->numberOfPairs; pairDex++) {
		ptvp = &rdnp->AttributeTypeAndValue[pairDex];
		if(compareOids(&ptvp->type, &CSSMOID_CountryName)) {
			fieldName = "Country       ";      
		}
		else if(compareOids(&ptvp->type, &CSSMOID_OrganizationName)) {
			fieldName = "Org           ";      
		}
		else if(compareOids(&ptvp->type, &CSSMOID_LocalityName)) {
			fieldName = "Locality      ";      
		}
		else if(compareOids(&ptvp->type, &CSSMOID_OrganizationalUnitName)) {
			fieldName = "OrgUnit       ";      
		}
		else if(compareOids(&ptvp->type, &CSSMOID_CommonName)) {
			fieldName = "Common Name   ";      
		}
		else if(compareOids(&ptvp->type, &CSSMOID_Surname)) {
			fieldName = "Surname       ";      
		}
		else if(compareOids(&ptvp->type, &CSSMOID_Title)) {
			fieldName = "Title         ";      
		}
		else if(compareOids(&ptvp->type, &CSSMOID_Surname)) {
			fieldName = "Surname       ";      
		}
		else if(compareOids(&ptvp->type, &CSSMOID_StateProvinceName)) {
			fieldName = "State         ";      
		}
		else if(compareOids(&ptvp->type, &CSSMOID_CollectiveStateProvinceName)) {
			fieldName = "Coll. State   ";      
		}
		else if(compareOids(&ptvp->type, &CSSMOID_EmailAddress)) {
			/* deprecated, used by Thawte */
			fieldName = "Email addrs   ";      
		}
		else if(compareOids(&ptvp->type, &CSSMOID_Description)) {
			fieldName = "Description   ";      
		}
		else {
			fieldName = "Other name    ";      
		}
		printf("   %s  : ", fieldName);
		printDerThing(ptvp->valueType, &ptvp->value, parser);
	}	/* for each type/value pair */
}

static CSSM_RETURN printName(
	const CSSM_X509_NAME	 	*x509Name,
	OidParser 					&parser)
{
	CSSM_X509_RDN_PTR    		rdnp;
	unsigned					rdnDex;
	
	for(rdnDex=0; rdnDex<x509Name->numberOfRDNs; rdnDex++) {
		rdnp = &x509Name->RelativeDistinguishedName[rdnDex];
		printRdn(rdnp, parser);
	}		
	
	return CSSM_OK;
}

static void printKeyHeader(
	const CSSM_KEYHEADER &hdr)
{
	printf("   Algorithm       : ");
	switch(hdr.AlgorithmId) {
		case CSSM_ALGID_RSA:
			printf("RSA\n");
			break;
		case CSSM_ALGID_DSA:
			printf("DSA\n");
			break;
		case CSSM_ALGID_FEE:
			printf("FEE\n");
			break;
		case CSSM_ALGID_DH:
			printf("Diffie-Hellman\n");
			break;
		case CSSM_ALGID_ECDSA:
			printf("ECDSA\n");
			break;
		default:
			printf("Unknown(%u(d), 0x%x)\n", (unsigned)hdr.AlgorithmId, 
				(unsigned)hdr.AlgorithmId);
	}
	printf("   Key Size        : %u bits\n", (unsigned)hdr.LogicalKeySizeInBits);
	printf("   Key Use         : ");
	CSSM_KEYUSE usage = hdr.KeyUsage;
	if(usage & CSSM_KEYUSE_ANY) {
		printf("CSSM_KEYUSE_ANY ");
	}
	if(usage & CSSM_KEYUSE_ENCRYPT) {
		printf("CSSM_KEYUSE_ENCRYPT ");
	}
	if(usage & CSSM_KEYUSE_DECRYPT) {
		printf("CSSM_KEYUSE_DECRYPT ");
	}
	if(usage & CSSM_KEYUSE_SIGN) {
		printf("CSSM_KEYUSE_SIGN ");
	}
	if(usage & CSSM_KEYUSE_VERIFY) {
		printf("CSSM_KEYUSE_VERIFY ");
	}
	if(usage & CSSM_KEYUSE_SIGN_RECOVER) {
		printf("CSSM_KEYUSE_SIGN_RECOVER ");
	}
	if(usage & CSSM_KEYUSE_VERIFY_RECOVER) {
		printf("CSSM_KEYUSE_VERIFY_RECOVER ");
	}
	if(usage & CSSM_KEYUSE_WRAP) {
		printf("CSSM_KEYUSE_WRAP ");
	}
	if(usage & CSSM_KEYUSE_UNWRAP) {
		printf("CSSM_KEYUSE_UNWRAP ");
	}
	if(usage & CSSM_KEYUSE_DERIVE) {
		printf("CSSM_KEYUSE_DERIVE ");
	}
	printf("\n");

}

/*
 * Print contents of a CE_GeneralName as best we can.
 */
static void printGeneralName(
	const CE_GeneralName	*name,
	OidParser 				&parser)
{
	switch(name->nameType) {
		case GNT_RFC822Name:
			printf("   RFC822Name      : ");
			printString(&name->name);
			break;
		case GNT_DNSName:
			printf("   DNSName         : ");
			printString(&name->name);
			break;
		case GNT_URI:
			printf("   URI             : ");
			printString(&name->name);
			break;
		case GNT_IPAddress:
			printf("   IP Address      : ");
			for(unsigned i=0; i<name->name.Length; i++) {
				printf("%d", name->name.Data[i]);
				if(i < (name->name.Length - 1)) {
					printf(".");
				}
			}
			printf("\n");
			break;
		case GNT_RegisteredID:
			printf("   RegisteredID    : ");
			printOid(parser, &name->name);
			break;
		case GNT_X400Address:
			/* ORAddress, a very complicated struct - punt */
			printf("   X400Address     : ");
			printBlobBytes("Sequence", "bytes", (uint32)name->name.Length, &name->name);
			break;
		case GNT_DirectoryName:
			if(!name->berEncoded) {
				/* CL parsed it for us into an CSSM_X509_NAME */
				if(name->name.Length != sizeof(CSSM_X509_NAME)) {
					printf("***MALFORMED GNT_DirectoryName\n");
					break;
				}
				const CSSM_X509_NAME *x509Name = 
					(const CSSM_X509_NAME *)name->name.Data;
				printf("   Dir Name        :\n");
				printName(x509Name, parser);
			}
			else {
				/* encoded Name (i.e. CSSM_X509_NAME) */
				printf("   Dir Name        : ");
				printBlobBytes("Byte string", "bytes", 
					(uint32)name->name.Length, &name->name);
			}
			break;
		case GNT_EdiPartyName:
			/* sequence EDIPartyName */
			printf("   EdiPartyName    : ");
			printBlobBytes("Sequence", "bytes", (uint32)name->name.Length, &name->name);
			break;
		case GNT_OtherName:
		{
			printf("   OtherName       :\n");
			if(name->name.Length != sizeof(CE_OtherName)) {
				printf("***Malformed CE_OtherName\n");
				break;
			}
			CE_OtherName *other = (CE_OtherName *)name->name.Data;
			printf("      typeID       : ");
			printOid(parser, &other->typeId);
			printf("      value        : ");
			printDataAsHex(&other->value, 0);
			break;
		}
	}
}


/*
 * Print contents of a CE_GeneralNames as best we can.
 */
static void printGeneralNames(
	const CE_GeneralNames	*generalNames,
	OidParser 				&parser)
{
	unsigned			i;
	CE_GeneralName		*name;
	
	for(i=0; i<generalNames->numNames; i++) {
		name = &generalNames->generalName[i];
		printGeneralName(name, parser);
	}
}

static int printCdsaExtensionCommon(
	const CSSM_X509_EXTENSION 	*cssmExt,
	OidParser					&parser,
	bool						expectParsed,
	CSSM_BOOL					verbose,
	bool						extraIndent = false)
{
	if(extraIndent) {
		printf("   Extension       : "); printOid(parser, &cssmExt->extnId);
		printf("      Critical     : %s\n", cssmExt->critical ? "TRUE" : "FALSE");
	}
	else {
		printf("Extension struct   : "); printOid(parser, &cssmExt->extnId);
		printf("   Critical        : %s\n", cssmExt->critical ? "TRUE" : "FALSE");
	}
	
	/* currently (since Radar 3593624), these are both always valid */
	#if 0
	/* this prevents printing pre-encoded extensions in clxutils/extenTest */
	if((cssmExt->BERvalue.Data == NULL) || 
	   (cssmExt->value.parsedValue == NULL)) {  /* actually, one of three variants */
		printf("***Malformed CSSM_X509_EXTENSION (1)\n");
		return 1;
	}
	#endif	
	switch(cssmExt->format) {
		case CSSM_X509_DATAFORMAT_ENCODED:
			if(expectParsed) {
				printf("Bad CSSM_X509_EXTENSION; expected FORMAT_PARSED\n");
				return 1;
			}
			break;
		case CSSM_X509_DATAFORMAT_PARSED:
			if(!expectParsed) {
				printf("Bad CSSM_X509_EXTENSION; expected FORMAT_ENCODED\n");
				return 1;
			}
			break;
		case CSSM_X509_DATAFORMAT_PAIR:
			/* unsupported */
			printf("Bad CSSM_X509_EXTENSION format:FORMAT_PAIR\n");
			return 1;
		default:
			printf("***Unknown CSSM_X509_EXTENSION.format\n");
			return 1;
	}
	return 0;
}

static int printExtensionCommon(
	const CSSM_DATA		&value,
	OidParser			&parser,
	CSSM_BOOL			verbose,
	bool				expectParsed = true)
{
	if(value.Length != sizeof(CSSM_X509_EXTENSION)) {
		printf("***malformed CSSM_FIELD (1)\n");
		return 1;
	}
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	return printCdsaExtensionCommon(cssmExt, parser, expectParsed, verbose);
}


static void printKeyUsage(
	const CSSM_DATA &value)
{
	CE_KeyUsage usage;
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	
	usage = *((CE_KeyUsage *)cssmExt->value.parsedValue);
	printf("   usage           : ");
	if(usage & CE_KU_DigitalSignature) {
		printf("DigitalSignature ");
	}
	if(usage & CE_KU_NonRepudiation) {
		printf("NonRepudiation ");
	}
	if(usage & CE_KU_KeyEncipherment) {
		printf("KeyEncipherment ");
	}
	if(usage & CE_KU_DataEncipherment) {
		printf("DataEncipherment ");
	}
	if(usage & CE_KU_KeyAgreement) {
		printf("KeyAgreement ");
	}
	if(usage & CE_KU_KeyCertSign) {
		printf("KeyCertSign ");
	}
	if(usage & CE_KU_CRLSign) {
		printf("CRLSign ");
	}
	if(usage & CE_KU_EncipherOnly) {
		printf("EncipherOnly ");
	}
	if(usage & CE_KU_DecipherOnly) {
		printf("DecipherOnly ");
	}
	printf("\n");

}

static void printBasicConstraints(
	const CSSM_DATA &value)
{
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	CE_BasicConstraints *bc = (CE_BasicConstraints *)cssmExt->value.parsedValue;
	printf("   CA              : %s\n", bc->cA ? "TRUE" : "FALSE");
	if(bc->pathLenConstraintPresent) {
		printf("   pathLenConstr   : %u\n", (unsigned)bc->pathLenConstraint);
	}
}
		
static void printExtKeyUsage(
	const CSSM_DATA 	&value,
	OidParser 			&parser)
{
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	CE_ExtendedKeyUsage *eku = (CE_ExtendedKeyUsage *)cssmExt->value.parsedValue;
	unsigned oidDex;
	for(oidDex=0; oidDex<eku->numPurposes; oidDex++) {
		printf("   purpose %2d      : ", oidDex);
		printOid(parser, &eku->purposes[oidDex]);
	}
}

static void printCssmAuthorityKeyId(
	const CE_AuthorityKeyID *akid,
	OidParser 				&parser)
{
	if(akid->keyIdentifierPresent) {
		printf("   Auth KeyID      : "); 
		printDataAsHex(&akid->keyIdentifier,
8);
	}
	if(akid->generalNamesPresent) {
		printGeneralNames(akid->generalNames, parser);
	}
	if(akid->serialNumberPresent) {
		printf("   serialNumber    : "); 
		printDataAsHex(&akid->serialNumber, 8);
	}
}

static void printAuthorityKeyId(
	const CSSM_DATA 	&value,
	OidParser 			&parser)
{
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	CE_AuthorityKeyID *akid = (CE_AuthorityKeyID *)cssmExt->value.parsedValue;
	printCssmAuthorityKeyId(akid, parser);
}

static void printSubjectIssuerAltName(
	const CSSM_DATA 	&value,
	OidParser 			&parser)
{
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	CE_GeneralNames *san = (CE_GeneralNames *)cssmExt->value.parsedValue;
	printGeneralNames(san, parser);
}

static void printDistPointName(
	const CE_DistributionPointName	*dpn,
	OidParser						&parser)
{
	switch(dpn->nameType) {
		case CE_CDNT_FullName:
			printGeneralNames(dpn->dpn.fullName, parser);
			break;
		case CE_CDNT_NameRelativeToCrlIssuer:
			printRdn(dpn->dpn.rdn, parser);
			break;
		default:
			printf("***BOGUS CE_DistributionPointName.nameType\n");
			break;
	}
}

static void printDistPoint(
	const CE_CRLDistributionPoint	*dp,
	OidParser						&parser)
{
	if(dp->distPointName) {
		printf("   Dist pt Name    :\n");
		printDistPointName(dp->distPointName, parser);
	}
	printf("   reasonsPresent  : %s\n", dp->reasonsPresent ? "TRUE" : "FALSE");
	if(dp->reasonsPresent) {
		/* FIXME - parse */
		printf("  reasons           : 0x%X\n", dp->reasons);
	}
	if(dp->crlIssuer) {
		printf("  CRLIssuer        :\n");
		printGeneralNames(dp->crlIssuer, parser);
	}
}

static void printDistributionPoints(
	const CSSM_DATA 	&value,
	OidParser 			&parser)
{
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	CE_CRLDistPointsSyntax *dps = (CE_CRLDistPointsSyntax *)cssmExt->value.parsedValue;
	
	for(unsigned dex=0; dex<dps->numDistPoints; dex++) {
		printf("   Dist pt %d       :\n", dex); 
		printDistPoint(&dps->distPoints[dex], parser);
	}
}

static void printValueOrNotPresent(
	CSSM_BOOL present,
	CSSM_BOOL value)
{
	if(!present) {
		printf("<Not Present>\n");
	}
	else if(value) {
		printf("TRUE\n");
	}
	else {
		printf("FALSE");
	}
}

static void printIssuingDistributionPoint(
	const CE_IssuingDistributionPoint 	*idp,
	OidParser 							&parser)
{
	if(idp->distPointName) {
		printf("   Dist pt          :\n"); 
		printDistPointName(idp->distPointName, parser);
	}
	printf("   Only user certs : "); 
	printValueOrNotPresent(idp->onlyUserCertsPresent, idp->onlyUserCerts);
	printf("   Only CA certs   : "); 
	printValueOrNotPresent(idp->onlyCACertsPresent, idp->onlyCACerts);
	printf("   Only some reason: "); 
	printValueOrNotPresent(idp->onlySomeReasonsPresent, idp->onlySomeReasons);
	printf("   Indirectl CRL   : "); 
	printValueOrNotPresent(idp->indirectCrlPresent, idp->indirectCrl);
}

static void printCertPolicies(
	const CSSM_DATA 	&value,
	OidParser 			&parser)
{
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	CE_CertPolicies *cdsaObj = (CE_CertPolicies *)cssmExt->value.parsedValue;
	for(unsigned polDex=0; polDex<cdsaObj->numPolicies; polDex++) {
		CE_PolicyInformation *cPolInfo = &cdsaObj->policies[polDex];
		printf("   Policy %2d       : ID ", polDex); 
		printOid(parser, &cPolInfo->certPolicyId);
		for(unsigned qualDex=0; qualDex<cPolInfo->numPolicyQualifiers; qualDex++) {
			CE_PolicyQualifierInfo *cQualInfo = &cPolInfo->policyQualifiers[qualDex];
			printf("      Qual %2d      : ID ", qualDex); 
			printOid(parser, &cQualInfo->policyQualifierId);
			if(cuCompareCssmData(&cQualInfo->policyQualifierId,
					&CSSMOID_QT_CPS)) {
				printf("         CPS       : ");
				printString(&cQualInfo->qualifier);
			}
			else {
				printf("         unparsed  : ");
				printDataAsHex(&cQualInfo->qualifier, 8);
			}
		}
	}
}

static void printNetscapeCertType(
	const CSSM_DATA &value)
{
	CE_NetscapeCertType certType;
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	
	certType = *((CE_NetscapeCertType *)cssmExt->value.parsedValue);
	printf("   certType        : ");
	if(certType & CE_NCT_SSL_Client) {
		printf("SSL_Client ");
	}
	if(certType & CE_NCT_SSL_Server) {
		printf("SSL_Server ");
	}
	if(certType & CE_NCT_SMIME) {
		printf("S/MIME ");
	}
	if(certType & CE_NCT_ObjSign) {
		printf("ObjectSign ");
	}
	if(certType & CE_NCT_Reserved) {
		printf("Reserved ");
	}
	if(certType & CE_NCT_SSL_CA) {
		printf("SSL_CA ");
	}
	if(certType & CE_NCT_SMIME_CA) {
		printf("SMIME_CA ");
	}
	if(certType & CE_NCT_ObjSignCA) {
		printf("ObjSignCA ");
	}
	printf("\n");
}

static void printAuthorityInfoAccess(
	const CSSM_DATA 	&value,
	OidParser 			&parser)
{
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	CE_AuthorityInfoAccess	*info = (CE_AuthorityInfoAccess *)cssmExt->value.parsedValue;

	printf("   numDescriptions : %lu\n", (unsigned long)info->numAccessDescriptions);
	for(unsigned dex=0; dex<info->numAccessDescriptions; dex++) {
		printf("   description %u   : \n", dex);
		printf("   accessMethod    : ");
		CE_AccessDescription *descr = &info->accessDescriptions[dex];
		printOid(parser, &descr->accessMethod);
		printGeneralName(&descr->accessLocation, parser);
	}
}

static void printQualCertStatements(
	const CSSM_DATA 	&value,
	OidParser 			&parser)
{
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	CE_QC_Statements	*qcss = (CE_QC_Statements *)cssmExt->value.parsedValue;

	printf("   numQCStatements : %lu\n", (unsigned long)qcss->numQCStatements);
	for(unsigned dex=0; dex<qcss->numQCStatements; dex++) {
		CE_QC_Statement *qcs = &qcss->qcStatements[dex];

		printf("   statement %u     : \n", dex);
		printf("   statementId     : ");
		printOid(parser, &qcs->statementId);
		if(qcs->semanticsInfo) {
			printf("   semanticsInfo   :\n");
			CE_SemanticsInformation *si = qcs->semanticsInfo;
			if(si->semanticsIdentifier) {
				printf("   semanticsId     : ");
				printOid(parser, si->semanticsIdentifier);
			}
			if(si->nameRegistrationAuthorities) {
				printf("   nameRegAuth     :\n");
				printGeneralNames(si->nameRegistrationAuthorities, parser);
			}
		}
		if(qcs->otherInfo) {
			printf("   otherInfo       : "); printDataAsHex(qcs->otherInfo, 8);
		}
	}
}

/* print one field */
void printCertField(
	const CSSM_FIELD 	&field,
	OidParser 			&parser,
	CSSM_BOOL			verbose)
{
	const CSSM_DATA *thisData = &field.FieldValue;
	const CSSM_OID  *thisOid = &field.FieldOid;
	
	if(cuCompareCssmData(thisOid, &CSSMOID_X509V1Version)) {
		if(verbose) {
			printf("Version            : %u\n", cuDER_ToInt(thisData));
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1SerialNumber)) {
		printf("Serial Number      : "); printDataAsHex(thisData, 0);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1IssuerNameCStruct)) {
		printf("Issuer Name        :\n");
		CSSM_X509_NAME_PTR name = (CSSM_X509_NAME_PTR)thisData->Data;
		if((name == NULL) || (thisData->Length != sizeof(CSSM_X509_NAME))) {
			printf("   ***malformed CSSM_X509_NAME\n");
		}
		else {
			printName(name, parser);
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1SubjectNameCStruct)) {
		printf("Subject Name       :\n");
		CSSM_X509_NAME_PTR name = (CSSM_X509_NAME_PTR)thisData->Data;
		if((name == NULL) || (thisData->Length != sizeof(CSSM_X509_NAME))) {
			printf("   ***malformed CSSM_X509_NAME\n");
		}
		else {
			printName(name, parser);
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1ValidityNotBefore)) {
		CSSM_X509_TIME *cssmTime = (CSSM_X509_TIME *)thisData->Data;
		if((cssmTime == NULL) || (thisData->Length != sizeof(CSSM_X509_TIME))) {
			printf("   ***malformed CSSM_X509_TIME\n");
		}
		else if(verbose) {
			printf("Not Before         : "); printString(&cssmTime->time);
			printf("                   : ");
			printTime(cssmTime);
		}
		else {
			printf("Not Before         : ");
			printTime(cssmTime);
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1ValidityNotAfter)) {
		CSSM_X509_TIME *cssmTime = (CSSM_X509_TIME *)thisData->Data;
		if((cssmTime == NULL) || (thisData->Length != sizeof(CSSM_X509_TIME))) {
			printf("   ***malformed CSSM_X509_TIME\n");
		}
		else if(verbose) {
			printf("Not After          : "); printString(&cssmTime->time);
			printf("                   : ");
			printTime(cssmTime);
		}
		else {
			printf("Not After          : ");
			printTime(cssmTime);
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1SignatureAlgorithmTBS)) {
		if(verbose) {
			/* normally skip, it's the same as TBS sig alg */
			printf("TBS Sig Algorithm  : ");
			CSSM_X509_ALGORITHM_IDENTIFIER *algId = 
				(CSSM_X509_ALGORITHM_IDENTIFIER *)thisData->Data;
			if((algId == NULL) || 
			(thisData->Length != sizeof(CSSM_X509_ALGORITHM_IDENTIFIER))) {
				printf("   ***malformed CSSM_X509_ALGORITHM_IDENTIFIER\n");
			}
			else {
				printSigAlg(algId, parser);
			}
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1SignatureAlgorithm)) {
		printf("Cert Sig Algorithm : ");
		CSSM_X509_ALGORITHM_IDENTIFIER *algId = 
			(CSSM_X509_ALGORITHM_IDENTIFIER *)thisData->Data;
		if((algId == NULL) || 
		   (thisData->Length != sizeof(CSSM_X509_ALGORITHM_IDENTIFIER))) {
			printf("   ***malformed CSSM_X509_ALGORITHM_IDENTIFIER\n");
		}
		else {
			printSigAlg(algId, parser);
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1CertificateIssuerUniqueId)) {
		if(verbose) {
			printf("Issuer UniqueId    : ");
			printDerThing(BER_TAG_BIT_STRING, thisData, parser);
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1CertificateSubjectUniqueId)) {
		if(verbose) {
			printf("Subject UniqueId   : ");
			printDerThing(BER_TAG_BIT_STRING, thisData, parser);
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1SubjectPublicKeyCStruct)) {
		CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *pubKeyInfo = 
			(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *)thisData->Data;
		printf("Pub Key Algorithm  : ");
		if((pubKeyInfo == NULL) || 
		   (thisData->Length != sizeof(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO))) {
			printf("   ***malformed CSSM_X509_SUBJECT_PUBLIC_KEY_INFO\n");
		}
		else {
			printSigAlg(&pubKeyInfo->algorithm, parser);
			printf("Pub key Bytes      : Length %u bytes : ",
				(unsigned)pubKeyInfo->subjectPublicKey.Length);
			printDataAsHex(&pubKeyInfo->subjectPublicKey, 8);
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_CSSMKeyStruct)) {
		CSSM_KEY_PTR cssmKey =  (CSSM_KEY_PTR)thisData->Data;
		printf("CSSM Key           :\n");
		if((cssmKey == NULL) || 
		   (thisData->Length != sizeof(CSSM_KEY))) {
			printf("   ***malformed CSSM_KEY\n");
		}
		else {
			printKeyHeader(cssmKey->KeyHeader);
			if(verbose) {
				printf("   Key Blob        : ");
				printDataAsHex(&cssmKey->KeyData, 8);
			}
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1Signature)) {
		printf("Signature          : %u bytes : ", (unsigned)thisData->Length);
		printDataAsHex(thisData, 8);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V3CertificateExtensionCStruct)) {
		if(printExtensionCommon(*thisData, parser, verbose, false)) {
			return;
		}
		CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)thisData->Data;
		printf("   Unparsed data   : "); printDataAsHex(&cssmExt->BERvalue, 8);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_KeyUsage)) {
		if(printExtensionCommon(*thisData, parser, verbose)) {
			return;
		}
		printKeyUsage(*thisData);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_BasicConstraints)) {
		if(printExtensionCommon(*thisData, parser, verbose)) {
			return;
		}
		printBasicConstraints(*thisData);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_ExtendedKeyUsage)) {
		if(printExtensionCommon(*thisData, parser, verbose)) {
			return;
		}
		printExtKeyUsage(*thisData, parser);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_SubjectKeyIdentifier)) {
		if(printExtensionCommon(*thisData, parser, verbose)) {
			return;
		}
		CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)thisData->Data;
		CSSM_DATA_PTR cdata = (CSSM_DATA_PTR)cssmExt->value.parsedValue;
		if((cdata == NULL) || (cdata->Data == NULL)) {
			printf("****Malformed extension (no parsedValue)\n");
		}
		else {
			printf("   Subject KeyID   : "); printDataAsHex(cdata, 8);
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_AuthorityKeyIdentifier)) {
		if(printExtensionCommon(*thisData, parser, verbose)) {
			return;
		}
		printAuthorityKeyId(*thisData, parser);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_SubjectAltName)) {
		if(printExtensionCommon(*thisData, parser, verbose)) {
			return;
		}
		printSubjectIssuerAltName(*thisData, parser);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_IssuerAltName)) {
		if(printExtensionCommon(*thisData, parser, verbose)) {
			return;
		}
		printSubjectIssuerAltName(*thisData, parser);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_CertificatePolicies)) {
		if(printExtensionCommon(*thisData, parser, verbose)) {
			return;
		}
		printCertPolicies(*thisData, parser);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_NetscapeCertType)) {
		if(printExtensionCommon(*thisData, parser, verbose)) {
			return;
		}
		printNetscapeCertType(*thisData);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_CrlDistributionPoints)) {
		if(printExtensionCommon(*thisData, parser, verbose)) {
			return;
		}
		printDistributionPoints(*thisData, parser);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_AuthorityInfoAccess)) {
		if(printExtensionCommon(*thisData, parser, verbose)) {
			return;
		}
		printAuthorityInfoAccess(*thisData, parser);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_SubjectInfoAccess)) {
		if(printExtensionCommon(*thisData, parser, verbose)) {
			return;
		}
		printAuthorityInfoAccess(*thisData, parser);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_QC_Statements)) {
		if(printExtensionCommon(*thisData, parser, verbose)) {
			return;
		}
		printQualCertStatements(*thisData, parser);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1IssuerName)) {
		if(verbose) {
			printf("Normalized Issuer  : ");
			printDataAsHex(thisData, 8);
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1SubjectName)) {
		if(verbose) {
			printf("Normalized Subject : ");
			printDataAsHex(thisData, 8);
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1IssuerNameStd)) {
		if(verbose) {
			printf("DER-encoded issuer : ");
			printDataAsHex(thisData, 8);
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1SubjectNameStd)) {
		if(verbose) {
			printf("DER-encoded subject: ");
			printDataAsHex(thisData, 8);
		}
	}
	else {
		printf("Other field:       : "); printOid(parser, thisOid);
	}
}

static
void printCrlExten(
	const CSSM_X509_EXTENSION *exten,
	CSSM_BOOL			verbose,
	OidParser 			&parser)
{
	const CSSM_OID *oid = &exten->extnId;
	const void *thisData = exten->value.parsedValue;
	
	if(exten->format == CSSM_X509_DATAFORMAT_ENCODED) {
		if(printCdsaExtensionCommon(exten, parser, false, verbose)) {
			return;
		}
		printf("   Unparsed data   : "); printDataAsHex(&exten->BERvalue, 8);
	}
	else if(exten->format != CSSM_X509_DATAFORMAT_PARSED) {
		printf("***Badly formatted CSSM_X509_EXTENSION\n");
		return;
	}
	else if(cuCompareCssmData(oid, &CSSMOID_AuthorityKeyIdentifier)) {
		if(printCdsaExtensionCommon(exten, parser, true, verbose)) {
			return;
		}
		printCssmAuthorityKeyId((CE_AuthorityKeyID *)thisData, parser);
	} 
	else if(cuCompareCssmData(oid, &CSSMOID_IssuerAltName)) {
		if(printCdsaExtensionCommon(exten, parser, true, verbose)) {
			return;
		}
		printGeneralNames((CE_GeneralNames *)thisData, parser);
	}
	else if(cuCompareCssmData(oid, &CSSMOID_CrlNumber)) {
		if(printCdsaExtensionCommon(exten, parser, true, verbose)) {
			return;
		}
		printf("   CRL Number      : %u\n", *((unsigned *)thisData));
	}
	else if(cuCompareCssmData(oid, &CSSMOID_DeltaCrlIndicator)) {
		if(printCdsaExtensionCommon(exten, parser, true, verbose)) {
			return;
		}
		printf("   Delta CRL Base  : %u\n", *((unsigned *)thisData));
	}
	else if(cuCompareCssmData(oid, &CSSMOID_IssuingDistributionPoint)) {
		if(printCdsaExtensionCommon(exten, parser, true, verbose)) {
			return;
		}
		printIssuingDistributionPoint((CE_IssuingDistributionPoint *)thisData,
			parser);
	}
	else {
		/* should never happen - we're out of sync with the CL */
		printf("UNKNOWN EXTENSION  : "); printOid(parser, oid);
	}
}


static
void printCrlEntryExten(
	const CSSM_X509_EXTENSION *exten,
	CSSM_BOOL			verbose,
	OidParser 			&parser)
{
	const CSSM_OID *oid = &exten->extnId;
	const void *thisData = exten->value.parsedValue;
	
	if(exten->format == CSSM_X509_DATAFORMAT_ENCODED) {
		if(printCdsaExtensionCommon(exten, parser, false, verbose, true)) {
			return;
		}
		printf("      Unparsed data: "); printDataAsHex(&exten->BERvalue, 8);
	}
	else if(exten->format != CSSM_X509_DATAFORMAT_PARSED) {
		printf("***Badly formatted CSSM_X509_EXTENSION\n");
		return;
	}
	else if(cuCompareCssmData(oid, &CSSMOID_CrlReason)) {
		if(printCdsaExtensionCommon(exten, parser, true, verbose, true)) {
			return;
		}
		CE_CrlReason *cr = (CE_CrlReason *)thisData;
		const char *reason = "UNKNOWN";
		switch(*cr) {
			case CE_CR_Unspecified: 
				reason = "CE_CR_Unspecified"; break;
			case CE_CR_KeyCompromise: 
				reason = "CE_CR_KeyCompromise"; break;
			case CE_CR_CACompromise: 
				reason = "CE_CR_CACompromise"; break;
			case CE_CR_AffiliationChanged: 
				reason = "CE_CR_AffiliationChanged"; break;
			case CE_CR_Superseded: 
				reason = "CE_CR_Superseded"; break;
			case CE_CR_CessationOfOperation: 
				reason = "CE_CR_CessationOfOperation"; break;
			case CE_CR_CertificateHold: 
				reason = "CE_CR_CertificateHold"; break;
			case CE_CR_RemoveFromCRL:
				reason = "CE_CR_RemoveFromCRL"; break;
			default:
				break;
		}
		printf("      CRL Reason   : %s\n", reason);
	}
	else if(cuCompareCssmData(oid, &CSSMOID_HoldInstructionCode)) {
		if(printCdsaExtensionCommon(exten, parser, true, verbose, true)) {
			return;
		}
		printf("      Hold Instr   : ");
		printOid(parser, (CSSM_OID_PTR)thisData);
	}
	else if(cuCompareCssmData(oid, &CSSMOID_InvalidityDate)) {
		if(printCdsaExtensionCommon(exten, parser, true, verbose, true)) {
			return;
		}
		printf("      Invalid Date : ");
		printTimeStr((CSSM_DATA_PTR)thisData);
	}
	else if(cuCompareCssmData(oid, &CSSMOID_CertIssuer)) {
		if(printCdsaExtensionCommon(exten, parser, true, verbose, true)) {
			return;
		}
		printGeneralNames((CE_GeneralNames *)thisData, parser);
	}
	else {
		/* should never happen - we're out of sync with the CL */
		printf("UNKNOWN EXTENSION  : "); printOid(parser, oid);
	}
}

static
void printCrlFields(
	const CSSM_X509_SIGNED_CRL *signedCrl,
	CSSM_BOOL					verbose,
	OidParser 					&parser)
{
	unsigned i;
	const CSSM_X509_TBS_CERTLIST *tbsCrl = &signedCrl->tbsCertList;
	
	if(tbsCrl->version.Data) {
		printf("Version            : %d\n", cuDER_ToInt(&tbsCrl->version));
	}
	
	printf("TBS Sig Algorithm  : ");
	const CSSM_X509_ALGORITHM_IDENTIFIER *algId = &tbsCrl->signature;
	printSigAlg(algId, parser);
	
	printf("Issuer Name        :\n");
	printName(&tbsCrl->issuer, parser);

	printf("This Update        : ");
	printTime(&tbsCrl->thisUpdate);
	printf("Next Update        : ");
	if(tbsCrl->nextUpdate.time.Data) {
		printTime(&tbsCrl->nextUpdate);
	}
	else {
		printf("<not present>\n");
	}
	
	CSSM_X509_REVOKED_CERT_LIST_PTR certList = tbsCrl->revokedCertificates;
	if(certList) {
		if(verbose) {
			printf("Num Revoked Certs  : %d\n", 
				(int)certList->numberOfRevokedCertEntries);
			for(i=0; i<certList->numberOfRevokedCertEntries; i++) {
				CSSM_X509_REVOKED_CERT_ENTRY_PTR entry;
				entry = &certList->revokedCertEntry[i];
				printf("Revoked Cert %d     :\n", (int)i);
				printf("   Serial number   : ");
				printDataAsHex(&entry->certificateSerialNumber, 0);
				printf("   Revocation time : ");
				printTime(&entry->revocationDate);
				const CSSM_X509_EXTENSIONS *cssmExtens = &entry->extensions;
				uint32 numExtens = cssmExtens->numberOfExtensions;
				if(numExtens == 0) {
					continue;
				}
				printf("   Num Extensions  : %u\n", (unsigned)numExtens);
				for(unsigned dex=0; dex<numExtens; dex++) {
					printCrlEntryExten(&cssmExtens->extensions[dex], verbose, 
						parser);
				}
			}
		}
		else {
			printf("Num Revoked Certs  : %d (use verbose option to see)\n", 
				(int)certList->numberOfRevokedCertEntries);
		}
	}

	const CSSM_X509_EXTENSIONS *crlExtens = &tbsCrl->extensions;
	if(crlExtens->numberOfExtensions) {
		printf("Num CRL Extensions : %d\n",
			(int)crlExtens->numberOfExtensions);
		for(i=0; i<crlExtens->numberOfExtensions; i++) {
			printCrlExten(&crlExtens->extensions[i], verbose, parser);
		}
	}
	
	const CSSM_X509_SIGNATURE *sig = &signedCrl->signature;
	if(sig->encrypted.Data) {
		printf("Signature          : %u bytes : ", (unsigned)sig->encrypted.Length);
		printDataAsHex(&sig->encrypted, 8);
	}
}


/* connect to CSSM/CL lazily, once */
static CSSM_CL_HANDLE clHand = 0;

int printCert(
	const unsigned char	*certData,
	unsigned		certLen,
	CSSM_BOOL		verbose)
{
	CSSM_FIELD_PTR				fieldPtr;		// mallocd by CL
	uint32						i;
	uint32						numFields;
	OidParser 					parser;
	CSSM_DATA					cert;
	
	if(clHand == 0) {
		clHand = cuClStartup();
		if(clHand == 0) {
			printf("***Error connecting to CSSM cert module; aborting cert "
				"display\n");
			return 0;
		}
	}
	cert.Data = (uint8 *)certData;
	cert.Length = certLen;
	
	CSSM_RETURN crtn = CSSM_CL_CertGetAllFields(clHand,
		&cert,
		&numFields,
		&fieldPtr);
	if(crtn) {
		cuPrintError("CSSM_CL_CertGetAllFields", crtn);
		return crtn;
	}

	for(i=0; i<numFields; i++) {
		printCertField(fieldPtr[i], parser, verbose);
	}	

	crtn = CSSM_CL_FreeFields(clHand, numFields, &fieldPtr);
	if(crtn) {
		cuPrintError("CSSM_CL_FreeFields", crtn);
		return crtn;
	}
	return 0;
}

/* parse CRL */
/* This one's easier, we just get one field - the whole parsed CRL */
int printCrl(
	const  unsigned char 	*crlData,
	unsigned				crlLen,
	CSSM_BOOL				verbose)
{
	CSSM_DATA_PTR				value;		// mallocd by CL
	uint32						numFields;
	OidParser 					parser;
	CSSM_DATA					crl;
	CSSM_HANDLE					result;
	
	if(clHand == 0) {
		clHand = cuClStartup();
		if(clHand == 0) {
			printf("***Error connecting to CSSM cert module; aborting CRL"
				"display\n");
			return 0;
		}
	}
	crl.Data = (uint8 *)crlData;
	crl.Length = crlLen;
	
	CSSM_RETURN crtn = CSSM_CL_CrlGetFirstFieldValue(clHand,
		&crl,
		&CSSMOID_X509V2CRLSignedCrlCStruct,
		&result,
		&numFields,
		&value);
	if(crtn) {
		cuPrintError("CSSM_CL_CrlGetFirstFieldValue", crtn);
		return crtn;
	}
	if(numFields != 1) {
		printf("***CSSM_CL_CrlGetFirstFieldValue: numFields error\n");
		printf("   expected 1, got %d\n", (int)numFields);
		return 1;
	}
	crtn = CSSM_CL_CrlAbortQuery(clHand, result);
	if(crtn) {
		cuPrintError("CSSM_CL_CertAbortQuery", crtn);
		return crtn;
	}
	
	if(value == NULL) {
		printf("***CSSM_CL_CrlGetFirstFieldValue: value error (1)\n");
		return 1;
	}
	if((value->Data == NULL) || 
	   (value->Length != sizeof(CSSM_X509_SIGNED_CRL))) {
		printf("***CSSM_CL_CrlGetFirstFieldValue: value error (2)\n");
		return 1;
	}
	const CSSM_X509_SIGNED_CRL *signedCrl = 
		(const CSSM_X509_SIGNED_CRL *)value->Data;
	printCrlFields(signedCrl, verbose, parser);

	crtn = CSSM_CL_FreeFieldValue(clHand, 
		&CSSMOID_X509V2CRLSignedCrlCStruct, 
		value);
	if(crtn) {
		cuPrintError("CSSM_CL_FreeFieldValue", crtn);
		return crtn;
	}
	return 0;
}


void printCertShutdown()
{
	if(clHand != 0) {
		CSSM_ModuleDetach(clHand);
	}
}
