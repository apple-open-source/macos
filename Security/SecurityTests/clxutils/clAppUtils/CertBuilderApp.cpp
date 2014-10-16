/*
 * CertBuilderApp.cpp - support for constructing certs, CDSA version
 */
 
#include "clutils.h"
#include <utilLib/common.h>
#include "CertBuilderApp.h"
#include "timeStr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Security/oidscert.h>
#include <Security/oidsalg.h>
#include <Security/x509defs.h>
#include <Security/SecAsn1Coder.h>
/* private header */
#include <Security/keyTemplates.h>

/*
 * Build up a CSSM_X509_NAME from an arbitrary list of name/OID pairs. 
 * We do one a/v pair per RDN. 
 */
CSSM_X509_NAME *CB_BuildX509Name(
	const CB_NameOid *nameArray,
	unsigned numNames)
{
	CSSM_X509_NAME *top = (CSSM_X509_NAME *)appMalloc(sizeof(CSSM_X509_NAME), 0);
	if(top == NULL) {
		return NULL;
	}
	top->numberOfRDNs = numNames;
	top->RelativeDistinguishedName = 
		(CSSM_X509_RDN_PTR)appMalloc(sizeof(CSSM_X509_RDN) * numNames, 0);
	if(top->RelativeDistinguishedName == NULL) {
		return NULL;
	}
	CSSM_X509_RDN_PTR rdn;
	const CB_NameOid *nameOid;
	unsigned nameDex;
	for(nameDex=0; nameDex<numNames; nameDex++) {
		rdn = &top->RelativeDistinguishedName[nameDex];
		nameOid = &nameArray[nameDex];
		rdn->numberOfPairs = 1;
		rdn->AttributeTypeAndValue = (CSSM_X509_TYPE_VALUE_PAIR_PTR)
			appMalloc(sizeof(CSSM_X509_TYPE_VALUE_PAIR), 0);
		CSSM_X509_TYPE_VALUE_PAIR_PTR atvp = rdn->AttributeTypeAndValue;
		if(atvp == NULL) {
			return NULL;
		}
		appCopyCssmData(nameOid->oid, &atvp->type);
		atvp->valueType = BER_TAG_PRINTABLE_STRING;
		atvp->value.Length = strlen(nameOid->string);
		atvp->value.Data = (uint8 *)CSSM_MALLOC(atvp->value.Length);
		memmove(atvp->value.Data, nameOid->string, atvp->value.Length);
	}
	return top;
}

/* free the CSSM_X509_NAME obtained from CB_BuildX509Name */
void CB_FreeX509Name(
	CSSM_X509_NAME *top)
{
	if(top == NULL) {
		return;
	}
	unsigned nameDex;
	CSSM_X509_RDN_PTR rdn;
	for(nameDex=0; nameDex<top->numberOfRDNs; nameDex++) {
		rdn = &top->RelativeDistinguishedName[nameDex];
		if(rdn->AttributeTypeAndValue) {
			for(unsigned aDex=0; aDex<rdn->numberOfPairs; aDex++) {
				CSSM_X509_TYPE_VALUE_PAIR_PTR atvp = 
					&rdn->AttributeTypeAndValue[aDex];
				CSSM_FREE(atvp->type.Data);
				CSSM_FREE(atvp->value.Data);
			}
			CSSM_FREE(rdn->AttributeTypeAndValue);
		}
	}
	CSSM_FREE(top->RelativeDistinguishedName);
	CSSM_FREE(top);
}

/* Obtain a CSSM_X509_TIME representing "now" plus specified seconds, or
 * from a preformatted gen time string */
CSSM_X509_TIME *CB_BuildX509Time(
	unsigned secondsFromNow,	/* ignored if timeStr non-NULL */
	const char *timeStr)		/* optional, from genTimeAtNowPlus */
{
	CSSM_X509_TIME *xtime = (CSSM_X509_TIME *)appMalloc(sizeof(CSSM_X509_TIME), 0);
	if(xtime == NULL) {
		return NULL;
	}
	xtime->timeType = BER_TAG_GENERALIZED_TIME;
	char *ts;
	if(timeStr == NULL) {
		ts = genTimeAtNowPlus(secondsFromNow);
	}
	else {
		ts = (char *)appMalloc(strlen(timeStr) + 1, 0);
		strcpy(ts, timeStr);
	}
	xtime->time.Data = (uint8 *)ts;
	xtime->time.Length = strlen(ts);
	return xtime;
}

/* Free CSSM_X509_TIME obtained in CB_BuildX509Time */
void CB_FreeX509Time(
	CSSM_X509_TIME	*xtime)
{
	if(xtime == NULL) {
		return;
	}
	freeTimeString((char *)xtime->time.Data);
	appFree(xtime, 0);
}

/* 
 * Encode an OID as a CSSM_X509_ALGORITHM_IDENTIFIER.
 * Returns nonzero on error.
 * Returned data is appMallocd's caller must appFree.
 */
int encodeParamOid(
	const CSSM_OID *paramOid, 
	CSSM_DATA *params)
{
	SecAsn1CoderRef coder = NULL;
	if(SecAsn1CoderCreate(&coder)) {
		printf("***Error in SecAsn1CoderCreate()\n");
		return -1;
	}
	
	CSSM_X509_ALGORITHM_IDENTIFIER algParams;
	memset(&algParams, 0, sizeof(algParams));
	algParams.algorithm = *paramOid;
	CSSM_DATA encoded = {0, NULL};
	int ourRtn = 0;
	if(SecAsn1EncodeItem(coder, &algParams, kSecAsn1AlgorithmIDTemplate,
			&encoded)) {
		printf("***Error encoding CSSM_X509_ALGORITHM_IDENTIFIER\n");
		ourRtn = -1;
		goto errOut;
	}
	
	/* That data is in the coder's memory space: copy ou9t to caller */
	if(appCopyCssmData(&encoded, params)) {
		printf("***encodeParamOid malloc failure\n");
		ourRtn = -1;
	}
errOut:
	SecAsn1CoderRelease(coder);
	return ourRtn;
}

/*
 * Cook up an unsigned cert.
 * This is just a wrapper for CSSM_CL_CertCreateTemplate().
 */
 
#define ALWAYS_SET_VERSION		0

CSSM_DATA_PTR CB_MakeCertTemplate(
	/* required */
	CSSM_CL_HANDLE			clHand,
	uint32					serialNumber,
	const CSSM_X509_NAME	*issuerName,	
	const CSSM_X509_NAME	*subjectName,
	const CSSM_X509_TIME	*notBefore,	
	const CSSM_X509_TIME	*notAfter,	
	const CSSM_KEY_PTR		subjectPubKey,
	CSSM_ALGORITHMS			sigAlg,			// e.g., CSSM_ALGID_SHA1WithRSA
	/* optional */
	const CSSM_DATA			*subjectUniqueId,
	const CSSM_DATA			*issuerUniqueId,
	CSSM_X509_EXTENSION		*extensions,
	unsigned				numExtensions)
{
	CSSM_FIELD		*certTemp;		
	unsigned		fieldDex = 0;		// index into certTemp
	CSSM_DATA_PTR	serialDER = NULL;	// serial number, DER format
	CSSM_DATA_PTR	rawCert;			// from CSSM_CL_CertCreateTemplate
	unsigned		version = 0;
	CSSM_DATA_PTR	versionDER = NULL;
	unsigned		extNum;
	int 			setVersion = ALWAYS_SET_VERSION;
	const CSSM_OID	*paramOid = NULL;
	
	/* convert uint32-style algorithm to the associated struct */
	CSSM_X509_ALGORITHM_IDENTIFIER algId;
	switch(sigAlg) {
		case CSSM_ALGID_SHA1WithRSA:
			algId.algorithm = CSSMOID_SHA1WithRSA;
			break;
		case CSSM_ALGID_MD5WithRSA:
			algId.algorithm = CSSMOID_MD5WithRSA;
			break;
		case CSSM_ALGID_MD2WithRSA:
			algId.algorithm = CSSMOID_MD2WithRSA;
			break;
		case CSSM_ALGID_FEE_MD5:
			algId.algorithm = CSSMOID_APPLE_FEE_MD5;
			break;
		case CSSM_ALGID_FEE_SHA1:
			algId.algorithm = CSSMOID_APPLE_FEE_SHA1;
			break;
		case CSSM_ALGID_SHA1WithECDSA:
			algId.algorithm = CSSMOID_ECDSA_WithSHA1;
			break;
		case CSSM_ALGID_SHA1WithDSA:
			algId.algorithm = CSSMOID_SHA1WithDSA_CMS;
			break;
		case CSSM_ALGID_SHA224WithRSA:
			algId.algorithm = CSSMOID_SHA224WithRSA;
			break;
		case CSSM_ALGID_SHA256WithRSA:
			algId.algorithm = CSSMOID_SHA256WithRSA;
			break;
		case CSSM_ALGID_SHA384WithRSA:
			algId.algorithm = CSSMOID_SHA384WithRSA;
			break;
		case CSSM_ALGID_SHA512WithRSA:
			algId.algorithm = CSSMOID_SHA512WithRSA;
			break;
		/* These specify the digest algorithm via an additional parameter OID */
		case CSSM_ALGID_SHA224WithECDSA:
			algId.algorithm = CSSMOID_ECDSA_WithSpecified;
			paramOid = &CSSMOID_SHA224;
			break;
		case CSSM_ALGID_SHA256WithECDSA:
			algId.algorithm = CSSMOID_ECDSA_WithSpecified;
			paramOid = &CSSMOID_SHA256;
			break;
		case CSSM_ALGID_SHA384WithECDSA:
			algId.algorithm = CSSMOID_ECDSA_WithSpecified;
			paramOid = &CSSMOID_SHA384;
			break;
		case CSSM_ALGID_SHA512WithECDSA:
			algId.algorithm = CSSMOID_ECDSA_WithSpecified;
			paramOid = &CSSMOID_SHA512;
			break;
		default:
			printf("CB_MakeCertTemplate: unknown sig alg (%u)\n", (unsigned)sigAlg);
			return NULL;
	}
	if(paramOid != NULL) {
		/* not-quite-trivial encoding of digest algorithm */
		if(encodeParamOid(paramOid, &algId.parameters)) {
			return NULL;
		}
	}
	else {
		algId.parameters.Data = NULL;
		algId.parameters.Length = 0;
	}
	
	/*
	 * version, we infer
	 * serialNumber thru subjectPubKey
	 */
	unsigned numFields = 7 + numExtensions;
	if(numExtensions) {
		version = 2;
	}
	if(subjectUniqueId) {
		numFields++;
		if(version == 0) {
			version = 1;
		}
	}
	if(issuerUniqueId) {
		numFields++;
		if(version == 0) {
			version = 1;
		}
	}
	if(version > 0) {
		setVersion = 1;
	}
	if(setVersion) {
		numFields++;
	}

	certTemp = (CSSM_FIELD *)CSSM_MALLOC(sizeof(CSSM_FIELD) * numFields);

	/* version */
	if(setVersion) {
		versionDER = intToDER(version);
		certTemp[fieldDex].FieldOid = CSSMOID_X509V1Version;
		certTemp[fieldDex++].FieldValue = *versionDER;
	}
	
	/* serial number */
	serialDER = intToDER(serialNumber);
	certTemp[fieldDex].FieldOid = CSSMOID_X509V1SerialNumber;
	certTemp[fieldDex++].FieldValue = *serialDER;

	/* subject and issuer name  */
	certTemp[fieldDex].FieldOid = CSSMOID_X509V1IssuerNameCStruct;
	certTemp[fieldDex].FieldValue.Data = (uint8 *)issuerName;
	certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_X509_NAME);
	
	certTemp[fieldDex].FieldOid = CSSMOID_X509V1SubjectNameCStruct;
	certTemp[fieldDex].FieldValue.Data = (uint8 *)subjectName;
	certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_X509_NAME);

	/* not before/after */
	certTemp[fieldDex].FieldOid = CSSMOID_X509V1ValidityNotBefore;
	certTemp[fieldDex].FieldValue.Data = (uint8 *)notBefore;
	certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_X509_TIME);

	certTemp[fieldDex].FieldOid = CSSMOID_X509V1ValidityNotAfter;
	certTemp[fieldDex].FieldValue.Data = (uint8 *)notAfter;
	certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_X509_TIME);

	/* the subject key */
	certTemp[fieldDex].FieldOid = CSSMOID_CSSMKeyStruct;
	certTemp[fieldDex].FieldValue.Data = (uint8 *)subjectPubKey;
	certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_KEY);

	/* signature algorithm */
	certTemp[fieldDex].FieldOid = CSSMOID_X509V1SignatureAlgorithmTBS;
	certTemp[fieldDex].FieldValue.Data = (uint8 *)&algId;
	certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_X509_ALGORITHM_IDENTIFIER);
	
	/* subject/issuer unique IDs */
	if(subjectUniqueId != 0) {
		certTemp[fieldDex].FieldOid = CSSMOID_X509V1CertificateSubjectUniqueId;
		certTemp[fieldDex++].FieldValue = *subjectUniqueId;
	}
	if(issuerUniqueId != 0) {
		certTemp[fieldDex].FieldOid = CSSMOID_X509V1CertificateIssuerUniqueId;
		certTemp[fieldDex++].FieldValue = *issuerUniqueId;
	}

	for(extNum=0; extNum<numExtensions; extNum++) {
		CSSM_X509_EXTENSION_PTR ext = &extensions[extNum];
		if(ext->format == CSSM_X509_DATAFORMAT_PARSED) {
			certTemp[fieldDex].FieldOid = ext->extnId;
		}
		else {
			certTemp[fieldDex].FieldOid = CSSMOID_X509V3CertificateExtensionCStruct;
		}
		certTemp[fieldDex].FieldValue.Data = (uint8 *)ext;
		certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_X509_EXTENSION);
	}
	if(fieldDex != numFields) {
		printf("CB_MakeCertTemplate numFields screwup\n");
		return NULL;
	}
	
	/*
	 * OK, here we go
	 */
	rawCert = (CSSM_DATA_PTR)CSSM_MALLOC(sizeof(CSSM_DATA));
	rawCert->Data = NULL;
	rawCert->Length = 0;
	CSSM_RETURN crtn = CSSM_CL_CertCreateTemplate(clHand,
		fieldDex,
		certTemp,
		rawCert);
	if(crtn) {
		printError("CSSM_CL_CertCreateTemplate", crtn);
		appFreeCssmData(rawCert, CSSM_TRUE);
		rawCert = NULL;
	}

	/* free the stuff we mallocd to get here */
	appFreeCssmData(serialDER, CSSM_TRUE);
	appFreeCssmData(versionDER, CSSM_TRUE);
	CSSM_FREE(certTemp);
	if((paramOid != NULL) && (algId.parameters.Data != NULL)) {
		CSSM_FREE(algId.parameters.Data);
	}
	return rawCert;
}
