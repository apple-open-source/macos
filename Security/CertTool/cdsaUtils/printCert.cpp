/*
 * Parse a cert, dump contents.
 */
#include "cdsaUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include <Security/oidscert.h>
#include <Security/x509defs.h>
#include <Security/oidsattr.h>
#include <Security/cssmapple.h>
#include <string.h>
#include "printCert.h"
#include "oidParser.h"
#include "timeStr.h"
#include <Security/certextensions.h>

static char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" 
};
	
/*
 * Convert a CSSM_DATA_PTR, referring to a DER-encoded int, to a
 * uint32.
 */
static uint32 DER_ToInt(const CSSM_DATA *DER_Data)
{
	uint32		rtn = 0;
	uint32		i = 0;

	while(i < DER_Data->Length) {
		rtn |= DER_Data->Data[i];
		if(++i == DER_Data->Length) {
			break;
		}
		rtn <<= 8;
	}
	return rtn;
}

static void printTime(const CSSM_X509_TIME *cssmTime)
{
	struct tm tm;
	
	/* ignore cssmTime->timeType for now */
	if(appTimeStringToTm((char *)cssmTime->time.Data, cssmTime->time.Length, &tm)) {
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

static void printDataAsHex(
	const CSSM_DATA *d,
	unsigned maxToPrint = 0)		// optional, 0 means print it all
{
	unsigned i;
	bool more = false;
	uint32 len = d->Length;
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
static char *tagTypeString(
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
	parser.oidParse(oid->Data, oid->Length, strBuf);
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
	char 			*blobType,
	char 			*quanta,		// e.g., "bytes', "bits"
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
			printf("%u\n", (unsigned)DER_ToInt(thing));
			return;
		case BER_TAG_BOOLEAN:
			if(thing->Length != 1) {
				printf("***malformed BER_TAG_BOOLEAN: length %d data ",
					(unsigned)thing->Length);
			}
			printf("%u\n", (unsigned)DER_ToInt(thing));
			return;
		case BER_TAG_PRINTABLE_STRING:
		case BER_TAG_IA5_STRING:	
		case BER_TAG_T61_STRING:		// mostly printable....	
			printString(thing);
			return;
		case BER_TAG_OCTET_STRING:
			printBlobBytes("Byte string", "bytes", thing->Length, thing);
			return;
		case BER_TAG_BIT_STRING:
			printBlobBytes("Bit string", "bits", (thing->Length + 7) / 8, thing);
			return;
		case BER_TAG_SEQUENCE:
			printBlobBytes("Sequence", "bytes", thing->Length, thing);
			return;
		case BER_TAG_SET:
			printBlobBytes("Set", "bytes", thing->Length, thing);
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

static void printSigAlg(
	CSSM_X509_ALGORITHM_IDENTIFIER  *sigAlg,
	OidParser 						&parser)
{
	printOid(parser, &sigAlg->algorithm);
	if(sigAlg->parameters.Data != NULL) {
		printf("    alg params     : ");
		printDataAsHex(&sigAlg->parameters, 8);
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

static CSSM_RETURN printName(
	const CSSM_X509_NAME_PTR 	x509Name,
	OidParser 					&parser)
{
	CSSM_X509_TYPE_VALUE_PAIR 	*ptvp;
	CSSM_X509_RDN_PTR    		rdnp;
	unsigned					rdnDex;
	unsigned					pairDex;
	char						*fieldName;
	
	for(rdnDex=0; rdnDex<x509Name->numberOfRDNs; rdnDex++) {
		rdnp = &x509Name->RelativeDistinguishedName[rdnDex];
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
			else {
				fieldName = "Other name    ";      
			}
			printf("    %s : ", fieldName);
			printDerThing(ptvp->valueType, &ptvp->value, parser);
		}	/* for each type/value pair */
	}		/* for each RDN */
	
	return CSSM_OK;
}

static void printKeyHeader(
	const CSSM_KEYHEADER &hdr)
{
	printf("    Algorithm      : ");
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
		default:
			printf("Unknown(%d(d), 0x%x)\n", (unsigned)hdr.AlgorithmId, 
					(unsigned)hdr.AlgorithmId);
	}
	printf("    Key Size       : %u bits\n", (unsigned)hdr.LogicalKeySizeInBits);
	printf("    Key Use        : ");
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
 * Print contents of a CE_GeneralNames as best we can.
 */
static void printGeneralNames(
	CE_GeneralNames	*generalNames,
	OidParser 		&parser)
{
	unsigned			i;
	CE_GeneralName		*name;
	
	for(i=0; i<generalNames->numNames; i++) {
		name = &generalNames->generalName[i];
		switch(name->nameType) {
			case GNT_RFC822Name:
				printf("    RFC822Name     : ");
				printString(&name->name);
				break;
			case GNT_DNSName:
				printf("    DNSName        : ");
				printString(&name->name);
				break;
			case GNT_URI:
				printf("    URI            : ");
				printString(&name->name);
				break;
			case GNT_IPAddress:
				printf("    IP Address     : ");
				for(unsigned i=0; i<name->name.Length; i++) {
					printf("%d", name->name.Data[i]);
					if(i < (name->name.Length - 1)) {
						printf(".");
					}
				}
				printf("\n");
				break;
			case GNT_RegisteredID:
				printf("    RegisteredID   : ");
				printOid(parser, &name->name);
				break;
			case GNT_X400Address:
				/* ORAddress, a very complicated struct - punt */
				printf("    X400Address    : ");
				printBlobBytes("Sequence", "bytes", name->name.Length, &name->name);
				break;
			case GNT_DirectoryName:
				/* encoded Name (i.e. CSSM_X509_NAME) */
				printf("    Dir Name       : ");
				printBlobBytes("Byte string", "bytes", name->name.Length, &name->name);
				break;
			case GNT_EdiPartyName:
				/* sequence EDIPartyName */
				printf("    EdiPartyName   : ");
				printBlobBytes("Sequence", "bytes", name->name.Length, &name->name);
				break;
			case GNT_OtherName:
				printf("    OtherName      : ");
				printOid(parser, &name->name);
				break;
		}
	}
}

static int printExtensionCommon(
	const CSSM_DATA		&value,
	OidParser			&parser,
	bool				expectParsed = true)
{
	if(value.Length != sizeof(CSSM_X509_EXTENSION)) {
		printf("***malformed CSSM_FIELD (1)\n");
		return 1;
	}
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	printf("Extension struct   : "); printOid(parser, &cssmExt->extnId);
	printf("    Critical       : %s\n", cssmExt->critical ? "TRUE" : "FALSE");
	switch(cssmExt->format) {
		case CSSM_X509_DATAFORMAT_ENCODED:
			if(expectParsed) {
				printf("Bad CSSM_X509_EXTENSION; expected FORMAT_PARSED\n");
				return 1;
			}
			if((cssmExt->BERvalue.Data == NULL) || 
			   (cssmExt->value.parsedValue != NULL)) {
				printf("***Malformed CSSM_X509_EXTENSION (1)\n");
				return 1;
			}
			break;
		case CSSM_X509_DATAFORMAT_PARSED:
			if(!expectParsed) {
				printf("Bad CSSM_X509_EXTENSION; expected FORMAT_ENCODED\n");
				return 1;
			}
			if((cssmExt->BERvalue.Data != NULL) || 
			   (cssmExt->value.parsedValue == NULL)) {
				printf("***Malformed CSSM_X509_EXTENSION (2)\n");
				return 1;
			}
			break;
		default:
			printf("***Unknown CSSM_X509_EXTENSION.format\n");
			return 1;
	}
	return 0;
}

static void printKeyUsage(
	const CSSM_DATA &value)
{
	CE_KeyUsage usage;
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	
	usage = *((CE_KeyUsage *)cssmExt->value.parsedValue);
	printf("    usage          : ");
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
	printf("    CA             : %s\n", bc->cA ? "TRUE" : "FALSE");
	if(bc->pathLenConstraintPresent) {
		printf("    pathLenConstr  : %u\n", 
			(unsigned)bc->pathLenConstraint);
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
		printf("    purpose %2d     : ", oidDex);
		printOid(parser, &eku->purposes[oidDex]);
	}
}

static void printAuthorityKeyId(
	const CSSM_DATA 	&value,
	OidParser 			&parser)
{
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	CE_AuthorityKeyID *akid = (CE_AuthorityKeyID *)cssmExt->value.parsedValue;
	if(akid->keyIdentifierPresent) {
		printf("    keyIdentifier  : "); printDataAsHex(&akid->keyIdentifier, 8);
	}
	if(akid->generalNamesPresent) {
		printGeneralNames(akid->generalNames, parser);
	}
	if(akid->serialNumberPresent) {
		printf("    serialNumber   : "); printDataAsHex(&akid->serialNumber, 8);
	}
}

static void printSubjectAltName(
	const CSSM_DATA 	&value,
	OidParser 			&parser)
{
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	CE_GeneralNames *san = (CE_GeneralNames *)cssmExt->value.parsedValue;
	printGeneralNames(san, parser);
}

static void printCertPolicies(
	const CSSM_DATA 	&value,
	OidParser 			&parser)
{
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value.Data;
	CE_CertPolicies *cdsaObj = (CE_CertPolicies *)cssmExt->value.parsedValue;
	for(unsigned polDex=0; polDex<cdsaObj->numPolicies; polDex++) {
		CE_PolicyInformation *cPolInfo = &cdsaObj->policies[polDex];
		printf("    Policy %2d      : ID ", polDex); 
		printOid(parser, &cPolInfo->certPolicyId);
		for(unsigned qualDex=0; qualDex<cPolInfo->numPolicyQualifiers; qualDex++) {
			CE_PolicyQualifierInfo *cQualInfo = &cPolInfo->policyQualifiers[qualDex];
			printf("       Qual %2d     : ID ", qualDex); 
			printOid(parser, &cQualInfo->policyQualifierId);
			if(cuCompareCssmData(&cQualInfo->policyQualifierId,
					&CSSMOID_QT_CPS)) {
				printf("          CPS      : ");
				printString(&cQualInfo->qualifier);
			}
			else {
				printf("          unparsed : ");
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
	printf("    certType       : ");
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
			printf("Version            : %u\n", 
				(unsigned)DER_ToInt(thisData));
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
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V1Signature)) {
		printf("Signature          : %u bytes : ", 
			(unsigned)thisData->Length);
		printDataAsHex(thisData, 8);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_X509V3CertificateExtensionCStruct)) {
		if(printExtensionCommon(*thisData, parser, false)) {
			return;
		}
		CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)thisData->Data;
		printf("    Unparsed data  : "); printDataAsHex(&cssmExt->BERvalue, 8);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_KeyUsage)) {
		if(printExtensionCommon(*thisData, parser)) {
			return;
		}
		printKeyUsage(*thisData);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_BasicConstraints)) {
		if(printExtensionCommon(*thisData, parser)) {
			return;
		}
		printBasicConstraints(*thisData);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_ExtendedKeyUsage)) {
		if(printExtensionCommon(*thisData, parser)) {
			return;
		}
		printExtKeyUsage(*thisData, parser);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_SubjectKeyIdentifier)) {
		if(printExtensionCommon(*thisData, parser)) {
			return;
		}
		CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)thisData->Data;
		CSSM_DATA_PTR cdata = (CSSM_DATA_PTR)cssmExt->value.parsedValue;
		if((cdata == NULL) || (cdata->Data == NULL)) {
			printf("****Malformed extension (no parsedValue)\n");
		}
		else {
			printf("    Subject KeyID  : "); printDataAsHex(cdata, 8);
		}
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_AuthorityKeyIdentifier)) {
		if(printExtensionCommon(*thisData, parser)) {
			return;
		}
		printAuthorityKeyId(*thisData, parser);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_SubjectAltName)) {
		if(printExtensionCommon(*thisData, parser)) {
			return;
		}
		printSubjectAltName(*thisData, parser);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_CertificatePolicies)) {
		if(printExtensionCommon(*thisData, parser)) {
			return;
		}
		printCertPolicies(*thisData, parser);
	}
	else if(cuCompareCssmData(thisOid, &CSSMOID_NetscapeCertType)) {
		if(printExtensionCommon(*thisData, parser)) {
			return;
		}
		printNetscapeCertType(*thisData);
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
	else {
		printf("other field:        : "); printOid(parser, thisOid);
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
			printf("***Error connecting to CSSM cert module; aborting cert display\n");
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

void printCertShutdown()
{
	if(clHand != 0) {
		CSSM_ModuleDetach(clHand);
	}
}
