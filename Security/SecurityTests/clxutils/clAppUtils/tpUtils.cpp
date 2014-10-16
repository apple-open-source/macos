/*
 * tpUtils.cpp - TP and cert group test support
 */

#include <Security/cssmtype.h>
#include <clAppUtils/tpUtils.h>
#include <clAppUtils/clutils.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <clAppUtils/CertBuilderApp.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/SecTrustSettingsPriv.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychain.h>
#include <Security/SecCertificate.h>
#include <security_cdsa_utils/cuFileIo.h>

/*
 * Currently, DBs created with SecKeychainCreateNew() do not contain 
 * the schema for CSSM_DL_DB_RECORD_X509_CERTIFICATE records. Keychain 
 * code (Certificate::add()) does this on the fly, I don't know why.
 * To avoid dependencies on KC - other than SecKeychainCreateNew - we'll
 * emulate that "add this schema on the fly" logic here. 
 *
 * Turn this option off if and when Radar 2927378 is approved and 
 * integrated into Security TOT.
 */
#define FAKE_ADD_CERT_SCHEMA	1
#if 	FAKE_ADD_CERT_SCHEMA

/* defined in SecKeychainAPIPriv.h */
// static const int kSecAlias = 'alis';

/* Macro to declare a CSSM_DB_SCHEMA_ATTRIBUTE_INFO */
#define SCHEMA_ATTR_INFO(id, name, type)	\
	{ id, (char *)name, {0, NULL},  CSSM_DB_ATTRIBUTE_FORMAT_ ## type }
	
/* Too bad we can't get this from inside of the Security framework. */
static CSSM_DB_SCHEMA_ATTRIBUTE_INFO certSchemaAttrInfo[] = 
{
	SCHEMA_ATTR_INFO(kSecCertTypeItemAttr, "CertType", UINT32),
	SCHEMA_ATTR_INFO(kSecCertEncodingItemAttr, "CertEncoding", UINT32),
	SCHEMA_ATTR_INFO(kSecLabelItemAttr, "PrintName", BLOB),
	SCHEMA_ATTR_INFO(kSecAlias, "Alias", BLOB),
	SCHEMA_ATTR_INFO(kSecSubjectItemAttr, "Subject", BLOB),
	SCHEMA_ATTR_INFO(kSecIssuerItemAttr, "Issuer", BLOB),
	SCHEMA_ATTR_INFO(kSecSerialNumberItemAttr, "SerialNumber", BLOB),
	SCHEMA_ATTR_INFO(kSecSubjectKeyIdentifierItemAttr, "SubjectKeyIdentifier", BLOB),
	SCHEMA_ATTR_INFO(kSecPublicKeyHashItemAttr, "PublicKeyHash", BLOB)
};
#define NUM_CERT_SCHEMA_ATTRS	\
	(sizeof(certSchemaAttrInfo) / sizeof(CSSM_DB_SCHEMA_ATTRIBUTE_INFO))

/* Macro to declare a CSSM_DB_SCHEMA_INDEX_INFO */
#define SCHEMA_INDEX_INFO(id, indexNum, indexType)	\
	{ id, CSSM_DB_INDEX_ ## indexType,  CSSM_DB_INDEX_ON_ATTRIBUTE }
	

static CSSM_DB_SCHEMA_INDEX_INFO certSchemaIndices[] = 
{
	SCHEMA_INDEX_INFO(kSecCertTypeItemAttr, 0, UNIQUE),
	SCHEMA_INDEX_INFO(kSecIssuerItemAttr, 0, UNIQUE),
	SCHEMA_INDEX_INFO(kSecSerialNumberItemAttr, 0, UNIQUE),
	SCHEMA_INDEX_INFO(kSecCertTypeItemAttr, 1, NONUNIQUE),
	SCHEMA_INDEX_INFO(kSecSubjectItemAttr, 2, NONUNIQUE),
	SCHEMA_INDEX_INFO(kSecIssuerItemAttr, 3, NONUNIQUE),
	SCHEMA_INDEX_INFO(kSecSerialNumberItemAttr, 4, NONUNIQUE),
	SCHEMA_INDEX_INFO(kSecSubjectKeyIdentifierItemAttr, 5, NONUNIQUE),
	SCHEMA_INDEX_INFO(kSecPublicKeyHashItemAttr, 6, NONUNIQUE)
};
#define NUM_CERT_INDICES	\
	(sizeof(certSchemaIndices) / sizeof(CSSM_DB_SCHEMA_INDEX_INFO))


CSSM_RETURN tpAddCertSchema(
	CSSM_DL_DB_HANDLE	dlDbHand)
{
	return CSSM_DL_CreateRelation(dlDbHand,
		CSSM_DL_DB_RECORD_X509_CERTIFICATE,
		"CSSM_DL_DB_RECORD_X509_CERTIFICATE",
		NUM_CERT_SCHEMA_ATTRS,
		certSchemaAttrInfo,
		NUM_CERT_INDICES,
		certSchemaIndices);		
}
#endif	/* FAKE_ADD_CERT_SCHEMA */

/*
 * Given a raw cert, extract DER-encoded normalized subject and issuer names.
 */
static CSSM_DATA_PTR tpGetNormSubject(
	CSSM_CL_HANDLE			clHand,
	const CSSM_DATA			*rawCert)
{
	CSSM_RETURN crtn;
	CSSM_HANDLE searchHand = CSSM_INVALID_HANDLE;
	uint32 numFields;
	CSSM_DATA_PTR fieldValue;

	crtn = CSSM_CL_CertGetFirstFieldValue(clHand,
		rawCert,
		&CSSMOID_X509V1SubjectName,
		&searchHand,
		&numFields, 
		&fieldValue);
	if(crtn) {
		printError("CSSM_CL_CertGetFirstFieldValue", crtn);
		return NULL;
	}
	CSSM_CL_CertAbortQuery(clHand, searchHand);
	return fieldValue;
}

static CSSM_DATA_PTR tpGetNormIssuer(
	CSSM_CL_HANDLE			clHand,
	const CSSM_DATA			*rawCert)
{
	CSSM_RETURN crtn;
	CSSM_HANDLE searchHand = CSSM_INVALID_HANDLE;
	uint32 numFields;
	CSSM_DATA_PTR fieldValue;

	crtn = CSSM_CL_CertGetFirstFieldValue(clHand,
		rawCert,
		&CSSMOID_X509V1IssuerName,
		&searchHand,
		&numFields, 
		&fieldValue);
	if(crtn) {
		printError("CSSM_CL_CertGetFirstFieldValue", crtn);
		return NULL;
	}
	CSSM_CL_CertAbortQuery(clHand, searchHand);
	return fieldValue;
}


#define SERIAL_NUMBER_BASE	0x33445566

/*
 * Given an array of certs and an uninitialized CSSM_CERTGROUP, place the
 * certs into the certgroup and optionally into one of a list of DBs in 
 * random order. Optionally the first cert in the array is placed in the 
 * first element of certgroup. Only error is memory error. It's legal to 
 * pass in an empty cert array. 
 */
CSSM_RETURN tpMakeRandCertGroup(
	CSSM_CL_HANDLE			clHand,
	CSSM_DL_DB_LIST_PTR		dbList,
	const CSSM_DATA_PTR		certs,
	unsigned				numCerts,
	CSSM_CERTGROUP_PTR		certGroup,
	CSSM_BOOL				firstCertIsSubject,	// true: certs[0] goes to head 
												//   of certGroup
	CSSM_BOOL				verbose,
	CSSM_BOOL				allInDbs,			// all certs go to DBs
	CSSM_BOOL				skipFirstDb)		// no certs go to db[0]
{
	unsigned 		startDex = 0;		// where to start processing
	unsigned 		certDex;			// into certs and certGroup
	unsigned		die;
	CSSM_RETURN 	crtn;
	
	#if	TP_DB_ENABLE
	if((dbList == NULL) && (allInDbs | skipFirstDb)) {
		printf("need dbList for allInDbs or skipFirstDb\n");
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	if(skipFirstDb && (dbList->NumHandles == 1)) {
		printf("Need more than one DB for skipFirstDb\n");
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	#else 
	if(dbList != NULL) { 
		printf("TP/DB not supported yet\n");
		return CSSMERR_CSSM_INTERNAL_ERROR;
	}
	#endif
	
	certGroup->NumCerts = 0;
	certGroup->CertGroupType = CSSM_CERTGROUP_DATA;
	certGroup->CertType = CSSM_CERT_X_509v3;
	certGroup->CertEncoding = CSSM_CERT_ENCODING_DER; 
	if(numCerts == 0) {
		/* legal */
		certGroup->GroupList.CertList = NULL;
		return CSSM_OK;
	}
	
	/* make CertList big enough for all certs */
	certGroup->GroupList.CertList = (CSSM_DATA_PTR)CSSM_CALLOC(numCerts, sizeof(CSSM_DATA));
	if(certGroup->GroupList.CertList == NULL) {
		printf("Memory error!\n");
		return CSSMERR_CSSM_MEMORY_ERROR;
	}
	if(firstCertIsSubject) {
	 	certGroup->GroupList.CertList[0] = certs[0];
	 	certGroup->NumCerts = 1;
	 	startDex = 1;
	}
	for(certDex=startDex; certDex<numCerts; certDex++) {
		/* flip a coin, half of the certs go into a DB */
		die = genRand(1, 2);			// one random bit 
		if( ( (dbList != NULL) && (dbList->NumHandles != 0) ) &&
		    ( (die == 1) || allInDbs) ) {
			/* put this cert in one of the DBs */
			if(skipFirstDb) {
				die = genRand(1, dbList->NumHandles-1);
			}
			else {
				die = genRand(0, dbList->NumHandles-1);
			}
			if(verbose) {
				printf("   ...cert %d to DB[%d]\n", certDex, die);
			}
			crtn = tpStoreRawCert(dbList->DLDBHandle[die],
					clHand,
					&certs[certDex]);
			if(crtn) {
				return crtn;
			}				
		} 
		else {
			/* find a random unused place in certGroupFrag */
			CSSM_DATA_PTR	certData;
			
			while(1) {
				die = genRand(0, numCerts-1);
				certData = &certGroup->GroupList.CertList[die];
				if(certData->Data == NULL) {
					*certData = certs[certDex];
					certGroup->NumCerts++;
					if(verbose) {
						printf("   ...cert %d to frag[%d]\n", 
							certDex, die);
					}
					break;
				}
				/* else try again and hope we don't spin forever */
			}
		} 	/* random place in certGroup */
	} 		/* main loop */
	
	if(dbList != NULL) {
		/* 
		 * Since we put some of the certs in dlDb rather than in certGroup,
		 * compact the contents of certGroup. Its NumCerts is correct, 
		 * but some of the entries in CertList are empty.
		 */
		unsigned i;
		
		for(certDex=0; certDex<numCerts; certDex++) {
			if(certGroup->GroupList.CertList[certDex].Data == NULL) {
				/* find next non-NULL cert */
				for(i=certDex+1; i<numCerts; i++) {
					if(certGroup->GroupList.CertList[i].Data != NULL) {
						if(verbose) {
							printf("   ...frag[%d] to frag[%d]\n", 
								i, certDex);
						}
						certGroup->GroupList.CertList[certDex] = 
							certGroup->GroupList.CertList[i];
						certGroup->GroupList.CertList[i].Data = NULL;
						break;
					}
				}
			}
		}
	}
	return CSSM_OK;
}

/*
 * Store a cert in specified DL/DB. All attributes are optional except
 * as noted (right?). 
 */
CSSM_RETURN tpStoreCert(
	CSSM_DL_DB_HANDLE		dlDb,
	const CSSM_DATA_PTR		cert,
	/* REQUIRED fields */
	CSSM_CERT_TYPE			certType,		// e.g. CSSM_CERT_X_509v3
	uint32					serialNum,	
	const CSSM_DATA			*issuer,		// (shouldn't this be subject?)
											// normalized & encoded
	/* OPTIONAL fields */
	CSSM_CERT_ENCODING		certEncoding,	// e.g. CSSM_CERT_ENCODING_DER
	const CSSM_DATA			*printName,
	const CSSM_DATA			*subject)		// normalized & encoded
{
	CSSM_DB_ATTRIBUTE_DATA			attrs[6];
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA_PTR		attr = &attrs[0];
	CSSM_DB_UNIQUE_RECORD_PTR		recordPtr = NULL;
	CSSM_DATA						certTypeData;
	CSSM_DATA						certEncData;
	CSSM_DATA_PTR					serialNumData;
	uint32							numAttributes;
	
	if(issuer == NULL) {
		printf("***For now, must specify cert issuer when storing\n");
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	
	/* how many attributes are we storing? */
	numAttributes = 4;		// certType, serialNum, issuer, certEncoding
	if(printName != NULL) {
		numAttributes++;
	}
	if(subject != NULL) {
		numAttributes++;
	}
	
	/* cook up CSSM_DB_RECORD_ATTRIBUTE_DATA */
	recordAttrs.DataRecordType = CSSM_DL_DB_RECORD_X509_CERTIFICATE;
	recordAttrs.SemanticInformation = 0;
	recordAttrs.NumberOfAttributes = numAttributes;
	recordAttrs.AttributeData = attrs;
	
	/* grind thru the attributes - first the required ones plus certEncoding */
	certTypeData.Data = (uint8 *)&certType;
	certTypeData.Length = sizeof(CSSM_CERT_TYPE);
	certEncData.Data = (uint8 *)&certEncoding;
	certEncData.Length = sizeof(CSSM_CERT_ENCODING);
	serialNumData = intToDER(serialNum);
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char *)"CertType";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
	attr->NumberOfValues = 1;
	attr->Value = &certTypeData;
	attr++;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char *)"CertEncoding";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
	attr->NumberOfValues = 1;
	attr->Value = &certEncData;
	attr++;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char *)"SerialNumber";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = serialNumData;
	attr++;

	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char *)"Issuer";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = (CSSM_DATA_PTR)issuer;
	attr++;

	/* now the options */
	if(printName != NULL) {
		attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
		attr->Info.Label.AttributeName = (char *)"PrintName";
		attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
		attr->NumberOfValues = 1;
		attr->Value = (CSSM_DATA_PTR)printName;
		attr++;
	}
	if(subject != NULL) {
		attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING; 
		attr->Info.Label.AttributeName = (char *)"Subject";
		attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
		attr->NumberOfValues = 1;
		attr->Value = (CSSM_DATA_PTR)subject;
		attr++;
	}

	/* Okay, here we go */
	CSSM_RETURN crtn = CSSM_DL_DataInsert(dlDb,
			CSSM_DL_DB_RECORD_X509_CERTIFICATE,
			&recordAttrs,
			cert,
			&recordPtr);
	#if 	FAKE_ADD_CERT_SCHEMA
	if(crtn == CSSMERR_DL_INVALID_RECORDTYPE) {
		/* gross hack of inserting this "new" schema that Keychain didn't specify */
		crtn = tpAddCertSchema(dlDb);
		if(crtn == CSSM_OK) {
			/* Retry with a fully capable DLDB */
			crtn = CSSM_DL_DataInsert(dlDb,
				CSSM_DL_DB_RECORD_X509_CERTIFICATE,
				&recordAttrs,
				cert,
				&recordPtr);
		}
	}
	#endif	/* FAKE_ADD_CERT_SCHEMA */
	
	/* free resources allocated to get this far */
	appFreeCssmData(serialNumData, CSSM_TRUE);
	if(recordPtr != NULL) {
		CSSM_DL_FreeUniqueRecord(dlDb, recordPtr);
	}
	if(crtn) {
		printError("CSSM_DL_DataInsert", crtn);
	}
	return crtn;
}

/*
 * Store a cert when we don't already know the required fields. We'll 
 * extract them or make them up.
 */
CSSM_RETURN tpStoreRawCert(
	CSSM_DL_DB_HANDLE		dlDb,
	CSSM_CL_HANDLE			clHand,
	const CSSM_DATA_PTR		cert)
{
	CSSM_DATA_PTR		normSubj;
	CSSM_DATA_PTR		normIssuer;
	CSSM_DATA			printName;
	CSSM_RETURN			crtn;
	static uint32		fakeSerialNum = 0;
	
	normSubj   = tpGetNormSubject(clHand, cert);
	normIssuer = tpGetNormIssuer(clHand, cert);
	if((normSubj == NULL) || (normIssuer == NULL)) {
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	printName.Data = (uint8 *)"Some Printable Name";
	printName.Length = strlen((char *)printName.Data);
	crtn = tpStoreCert(dlDb,
		cert,
		CSSM_CERT_X_509v3,
		fakeSerialNum++,
		normIssuer,
		CSSM_CERT_ENCODING_DER,
		&printName,
		normSubj);
	appFreeCssmData(normSubj, CSSM_TRUE);
	appFreeCssmData(normIssuer, CSSM_TRUE);
	return crtn;
}

/* 
 * Generate numKeyPairs key pairs of specified algorithm and size.
 * Key labels will be 'keyLabelBase' concatenated with a 4-digit
 * decimal number.
 */
CSSM_RETURN tpGenKeys(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_DL_DB_HANDLE dbHand,			/* keys go here */
	unsigned		numKeyPairs,
	uint32			keyGenAlg,		/* CSSM_ALGID_RSA, etc. */
	uint32			keySizeInBits,			
	const char 		*keyLabelBase,	/* C string */
	CSSM_KEY_PTR	pubKeys,		/* array of keys RETURNED here */
	CSSM_KEY_PTR	privKeys,		/* array of keys RETURNED here */
	CSSM_DATA_PTR	paramData)		/* optional DSA params */
{
	CSSM_RETURN		crtn;
	unsigned		i;
	char			label[80];
	unsigned		labelLen = strlen(keyLabelBase);
	
	memset(pubKeys, 0, numKeyPairs * sizeof(CSSM_KEY));
	memset(privKeys, 0, numKeyPairs * sizeof(CSSM_KEY));
	memmove(label, keyLabelBase, labelLen);
	
	for(i=0; i<numKeyPairs; i++) {
		/* unique label */
		sprintf(label+labelLen, "%04d", i);	
		if(keyGenAlg == CSSM_ALGID_DSA) {
			crtn = cspGenDSAKeyPair(cspHand,
				label,
				labelLen + 4,
				keySizeInBits,
				&pubKeys[i],
				CSSM_FALSE,			// pubIsRef
				CSSM_KEYUSE_VERIFY,	// pubKeyUsage
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				&privKeys[i],
				CSSM_TRUE,			// privIsRef
				CSSM_KEYUSE_SIGN,
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				CSSM_FALSE,			// genParams
				paramData);
		}
		else {
			crtn = cspGenKeyPair(cspHand,
				keyGenAlg,
				// not used in X, yet dbHand,
				label,
				labelLen + 4,
				keySizeInBits,
				&pubKeys[i],
				CSSM_FALSE,			// pubIsRef
				CSSM_KEYUSE_VERIFY,	// pubKeyUsage
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				&privKeys[i],
				CSSM_TRUE,			/// privIsRef
				CSSM_KEYUSE_SIGN,
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				CSSM_FALSE);
		}
		if(crtn) {
			return crtn;
		}
	}
	
	/* verify they are all different keys */
	for(i=0; i<numKeyPairs; i++) {
		CSSM_DATA_PTR k1 = &pubKeys[i].KeyData;
		for(unsigned j=i+1; j<numKeyPairs; j++) {
			CSSM_DATA_PTR k2 = &pubKeys[j].KeyData;
			if(appCompareCssmData(k1, k2)) {
				printf("***HEY! public keys %d and %d are indentical!\n", i, j);
			}
		}
	}
	return crtn;
}

/* 
 * Generate a cert chain using specified key pairs. The last cert in the
 * chain (certs[numCerts-1]) is a root cert, self-signed. 
 */
CSSM_RETURN tpGenCerts(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_CL_HANDLE	clHand,
	unsigned		numCerts,
	uint32			sigAlg,			/* CSSM_ALGID_SHA1WithRSA, etc. */
	const char 		*nameBase,		/* C string */
	CSSM_KEY_PTR	pubKeys,		/* array of public keys */
	CSSM_KEY_PTR	privKeys,		/* array of private keys */
	CSSM_DATA_PTR	certs,			/* array of certs RETURNED here */
	const char		*notBeforeStr,	/* from genTimeAtNowPlus() */
	const char		*notAfterStr)	/* from genTimeAtNowPlus() */
{
	return tpGenCertsStore(cspHand,
		clHand,
		numCerts,
		sigAlg,
		nameBase,
		pubKeys,
		privKeys,
		NULL,		// storeArray
		certs,
		notBeforeStr,
		notAfterStr);
}

/* 
 * Generate a cert chain using specified key pairs. The last cert in the
 * chain (certs[numCerts-1]) is a root cert, self-signed. Store
 * the certs indicated by corresponding element on storeArray. If 
 * storeArray[n].DLHandle == 0, the cert is not stored. 
 */
CSSM_RETURN tpGenCertsStore(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_CL_HANDLE		clHand,
	unsigned			numCerts,
	uint32				sigAlg,			/* CSSM_ALGID_SHA1WithRSA, etc. */
	const char 			*nameBase,		/* C string */
	CSSM_KEY_PTR		pubKeys,		/* array of public keys */
	CSSM_KEY_PTR		privKeys,		/* array of private keys */
	CSSM_DL_DB_HANDLE	*storeArray,	/* array of certs stored here  */
	CSSM_DATA_PTR		certs,			/* array of certs RETURNED here */
	const char			*notBeforeStr,	/* from genTimeAtNowPlus() */
	const char			*notAfterStr)	/* from genTimeAtNowPlus() */

{
	int 				dex;
	CSSM_RETURN			crtn;
	CSSM_X509_NAME		*issuerName = NULL;
	CSSM_X509_NAME		*subjectName = NULL;
	CSSM_X509_TIME		*notBefore;			// UTC-style "not before" time
	CSSM_X509_TIME		*notAfter;			// UTC-style "not after" time
	CSSM_DATA_PTR		rawCert = NULL;		// from CSSM_CL_CertCreateTemplate
	CSSM_DATA			signedCert;			// from CSSM_CL_CertSign	
	uint32				rtn;
	CSSM_KEY_PTR		signerKey;			// signs the cert
	CSSM_CC_HANDLE		signContext;
	char				nameStr[100];
	CSSM_DATA_PTR		thisCert;			// ptr into certs[]
	CB_NameOid			nameOid;
	CE_BasicConstraints	bc;
	CSSM_X509_EXTENSION exten;
	
	nameOid.oid = &CSSMOID_OrganizationName;	// const
	nameOid.string = nameStr;
	
	/* one extension for nonleaf indicating cA */
	exten.extnId = CSSMOID_BasicConstraints;
	exten.critical = CSSM_TRUE;
	exten.format = CSSM_X509_DATAFORMAT_PARSED;
	exten.value.parsedValue = &bc;
	exten.BERvalue.Data = NULL;
	exten.BERvalue.Length = 0;
	bc.cA = CSSM_TRUE;
	bc.pathLenConstraintPresent = CSSM_FALSE;
	bc.pathLenConstraint = 0;

	/* main loop - once per keypair/cert - starting at end/root */
	for(dex=numCerts-1; dex>=0; dex--) {
		thisCert = &certs[dex];
		
		thisCert->Data = NULL;
		thisCert->Length = 0;
		
		sprintf(nameStr, "%s%04d", nameBase, dex);
		if(issuerName == NULL) {
			/* last (root) cert - subject same as issuer */
			issuerName = CB_BuildX509Name(&nameOid, 1); 
			/* self-signed */
			signerKey = &privKeys[dex];
		}
		else {
			/* previous subject becomes current issuer */
			CB_FreeX509Name(issuerName);
			issuerName = subjectName;
			signerKey = &privKeys[dex+1];
		}
		subjectName = CB_BuildX509Name(&nameOid, 1);
		if((subjectName == NULL) || (issuerName == NULL)) {
			printf("Error creating X509Names\n");
			crtn = CSSMERR_CSSM_MEMORY_ERROR;
			break;
		}
		
		/* 
		 * not before/after in Y2k-compliant generalized time format.
		 * These come preformatted from our caller. 
		 */
		notBefore = CB_BuildX509Time(0, notBeforeStr);
		notAfter  = CB_BuildX509Time(0, notAfterStr);

		/* 
		 * Cook up cert template 
		 * Note serial number would be app-specified in real world
		 */
		rawCert = CB_MakeCertTemplate(clHand,
			SERIAL_NUMBER_BASE + dex,			// serial number
			issuerName,
			subjectName,
			notBefore,
			notAfter,
			&pubKeys[dex],
			sigAlg,
			NULL,			// subj unique ID
			NULL,			// issuer unique ID
			&exten,			// extensions
			(dex == 0) ? 0 : 1);// numExtensions
	
		if(rawCert == NULL) {
			crtn = CSSM_ERRCODE_INTERNAL_ERROR;
			break;
		}

		/* Free the stuff we allocd to get here */
		CB_FreeX509Time(notBefore);
		CB_FreeX509Time(notAfter);

		/**** sign the cert ****/
		/* 1. get a signing context */
		crtn = CSSM_CSP_CreateSignatureContext(cspHand,
				sigAlg,
				NULL,			// no passphrase for now
				signerKey,
				&signContext);
		if(crtn) {
			printError("CreateSignatureContext", crtn);
			break;
		}
		
		/* 2. use CL to sign the cert */ 
		signedCert.Data = NULL;
		signedCert.Length = 0;
		crtn = CSSM_CL_CertSign(clHand,
			signContext,
			rawCert,			// CertToBeSigned
			NULL,				// SignScope per spec
			0,					// ScopeSize per spec
			&signedCert);
		if(crtn) {
			printError("CSSM_CL_CertSign", crtn);
			break;
		}
		
		/* 3. Optionally store the cert in DL */
		if((storeArray != NULL) && storeArray[dex].DBHandle != 0) {
			crtn = tpStoreRawCert(storeArray[dex],
				clHand,
				&signedCert);
			if(crtn) {
				break;
			}
		}
		
		/* 4. delete signing context */
		crtn = CSSM_DeleteContext(signContext);
		if(crtn) {
			printError("CSSM_DeleteContext", crtn);
			break;
		}

		/* 
		 * CSSM_CL_CertSign() returned us a mallocd CSSM_DATA. Copy
		 * its fields to caller's cert. 
		 */
		certs[dex] = signedCert;
		
		/* and the raw unsigned cert as well */
		appFreeCssmData(rawCert, CSSM_TRUE);
		rtn = 0;
	}
	
	/* free resources */
	if(issuerName != NULL) {
		CB_FreeX509Name(issuerName);
	}
	if(subjectName != NULL) {
		CB_FreeX509Name(subjectName);
	}
	return crtn;
}

/* compare two CSSM_CERTGROUPs, returns CSSM_TRUE on success */
CSSM_BOOL tpCompareCertGroups(
	const CSSM_CERTGROUP	*grp1,
	const CSSM_CERTGROUP	*grp2)
{
	unsigned i;
	CSSM_DATA_PTR	d1;
	CSSM_DATA_PTR	d2;
	
	if(grp1->NumCerts != grp2->NumCerts) {
		return CSSM_FALSE;
	}
	for(i=0; i<grp1->NumCerts; i++) {
		d1 = &grp1->GroupList.CertList[i];
		d2 = &grp2->GroupList.CertList[i];
		
		/* these are all errors */
		if((d1->Data == NULL) ||
		   (d1->Length == 0)  ||
		   (d2->Data == NULL) ||
		   (d2->Length == 0)) {
		   	printf("compareCertGroups: bad cert group!\n");
		   	return CSSM_FALSE;
		}
		if(d1->Length != d2->Length) {
			return CSSM_FALSE;
		}
		if(memcmp(d1->Data, d2->Data, d1->Length)) {
			return CSSM_FALSE;
		}
	}
	return CSSM_TRUE;
}

/* free a CSSM_CERT_GROUP */ 
void tpFreeCertGroup(
	CSSM_CERTGROUP_PTR	certGroup,
	CSSM_BOOL	 		freeCertData,		// free individual CertList.Data 
	CSSM_BOOL			freeStruct)			// free the overall CSSM_CERTGROUP
{
	unsigned dex;
	
	if(certGroup == NULL) {
		return;	
	}
	
	if(freeCertData) {
		/* free the individual cert Data fields */
		for(dex=0; dex<certGroup->NumCerts; dex++) {
			appFreeCssmData(&certGroup->GroupList.CertList[dex], CSSM_FALSE);
		}
	}

	/* and the array of CSSM_DATAs */
	if(certGroup->GroupList.CertList) {
		CSSM_FREE(certGroup->GroupList.CertList);
	}
	
	if(freeStruct) {
		CSSM_FREE(certGroup);
	}
}

CSSM_RETURN clDeleteAllCerts(CSSM_DL_DB_HANDLE dlDb)
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE						resultHand;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	
	recordAttrs.DataRecordType = CSSM_DL_DB_RECORD_X509_CERTIFICATE;
	recordAttrs.NumberOfAttributes = 0;
	recordAttrs.AttributeData = NULL;
	
	/* just search by recordType, no predicates */
	query.RecordType = CSSM_DL_DB_RECORD_X509_CERTIFICATE;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 0;
	query.SelectionPredicate = NULL;
	query.QueryLimits.TimeLimit = 0;			// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;			// FIXME - meaningful?
	query.QueryFlags = 0;		// CSSM_QUERY_RETURN_DATA...FIXME - used?

	crtn = CSSM_DL_DataGetFirst(dlDb,
		&query,
		&resultHand,
		&recordAttrs,
		NULL,			// No data
		&record);
	switch(crtn) {
		case CSSM_OK:
			break;		// proceed
		case CSSMERR_DL_ENDOFDATA:
			/* OK, no certs */
			return CSSM_OK;
		default:
			printError("DataGetFirst", crtn);
			return crtn;
	}

	crtn = CSSM_DL_DataDelete(dlDb, record);
	if(crtn) {
		printError("CSSM_DL_DataDelete", crtn);
		return crtn;
	}
	CSSM_DL_FreeUniqueRecord(dlDb, record);
	
	/* now the rest of them */
	for(;;) {
		crtn = CSSM_DL_DataGetNext(dlDb,
			resultHand, 
			&recordAttrs,
			NULL,
			&record);
		switch(crtn) {
			case CSSM_OK:
				crtn = CSSM_DL_DataDelete(dlDb, record);
				if(crtn) {
					printError("CSSM_DL_DataDelete", crtn);
					return crtn;
				}
				CSSM_DL_FreeUniqueRecord(dlDb, record);
				break;		// and go again 
			case CSSMERR_DL_ENDOFDATA:
				/* normal termination */
				break;
			default:
				printError("DataGetNext", crtn);
				return crtn;
		}
		if(crtn != CSSM_OK) {
			break;
		}
	}
	CSSM_DL_DataAbortQuery(dlDb, resultHand);
	return CSSM_OK;
}

/*
 * Wrapper for CSSM_TP_CertGroupVerify. What an ugly API.
 */
CSSM_RETURN tpCertGroupVerify(
	CSSM_TP_HANDLE						tpHand,
	CSSM_CL_HANDLE						clHand,
	CSSM_CSP_HANDLE 					cspHand,
	CSSM_DL_DB_LIST_PTR					dbListPtr,
	const CSSM_OID						*policy,		// optional
	const CSSM_DATA						*fieldOpts,		// optional
	const CSSM_DATA						*actionData,	// optional
	void								*policyOpts,
	const CSSM_CERTGROUP 				*certGroup,
	CSSM_DATA_PTR						anchorCerts,
	unsigned							numAnchorCerts,
	CSSM_TP_STOP_ON						stopOn,		// CSSM_TP_STOP_ON_POLICY, etc.
	CSSM_TIMESTRING						cssmTimeStr,// optional
	CSSM_TP_VERIFY_CONTEXT_RESULT_PTR	result)		// optional, RETURNED
{
	/* main job is building a CSSM_TP_VERIFY_CONTEXT and its components */
	CSSM_TP_VERIFY_CONTEXT		vfyCtx;
	CSSM_TP_CALLERAUTH_CONTEXT	authCtx;
	
	memset(&vfyCtx, 0, sizeof(CSSM_TP_VERIFY_CONTEXT));
	vfyCtx.Action = CSSM_TP_ACTION_DEFAULT;
	if(actionData) {
		vfyCtx.ActionData = *actionData;
	}
	else {
		vfyCtx.ActionData.Data = NULL;
		vfyCtx.ActionData.Length = 0;
	}
	vfyCtx.Cred = &authCtx;
	
	/* CSSM_TP_CALLERAUTH_CONTEXT components */
	/* 
		typedef struct cssm_tp_callerauth_context {
			CSSM_TP_POLICYINFO Policy;
			CSSM_TIMESTRING VerifyTime;
			CSSM_TP_STOP_ON VerificationAbortOn;
			CSSM_TP_VERIFICATION_RESULTS_CALLBACK CallbackWithVerifiedCert;
			uint32 NumberOfAnchorCerts;
			CSSM_DATA_PTR AnchorCerts;
			CSSM_DL_DB_LIST_PTR DBList;
			CSSM_ACCESS_CREDENTIALS_PTR CallerCredentials;
		} CSSM_TP_CALLERAUTH_CONTEXT, *CSSM_TP_CALLERAUTH_CONTEXT_PTR;
	*/
	/* zero or one policy here */
	CSSM_FIELD	policyId;
	if(policy != NULL) {
		policyId.FieldOid = (CSSM_OID)*policy;
		authCtx.Policy.NumberOfPolicyIds = 1;
		authCtx.Policy.PolicyIds = &policyId;
		if(fieldOpts != NULL) {
			policyId.FieldValue = *fieldOpts;
		}
		else {
			policyId.FieldValue.Data = NULL;
			policyId.FieldValue.Length = 0;
		}
	}
	else {
		authCtx.Policy.NumberOfPolicyIds = 0;
		authCtx.Policy.PolicyIds = NULL;
	}
	authCtx.Policy.PolicyControl = policyOpts;
	authCtx.VerifyTime = cssmTimeStr;			// may be NULL
	authCtx.VerificationAbortOn = stopOn;
	authCtx.CallbackWithVerifiedCert = NULL;
	authCtx.NumberOfAnchorCerts = numAnchorCerts;
	authCtx.AnchorCerts = anchorCerts;
	authCtx.DBList = dbListPtr;
	authCtx.CallerCredentials = NULL;
	
	return CSSM_TP_CertGroupVerify(tpHand,
		clHand,
		cspHand,
		certGroup,
		&vfyCtx,
		result);
}

/*
 * Open, optionally create, KC-style DLDB. 
 */
#define KC_DB_PATH		"Library/Keychains"		/* relative to home */

CSSM_RETURN tpKcOpen(
	CSSM_DL_HANDLE		dlHand,
	const char			*kcName,
	const char			*pwd,				// optional to avoid UI	
	CSSM_BOOL			doCreate,
	CSSM_DB_HANDLE		*dbHand)			// RETURNED
{
	char kcPath[300];
	const char *kcFileName = kcName;
	char *userHome = getenv("HOME");
	
	if(userHome == NULL) {
		/* well, this is probably not going to work */
		userHome = (char *)"";
	}
	sprintf(kcPath, "%s/%s/%s", userHome, KC_DB_PATH, kcFileName);
	return dbCreateOpen(dlHand, kcPath,
		doCreate, CSSM_FALSE, pwd, dbHand);
}

/*
 * Free the contents of a CSSM_TP_VERIFY_CONTEXT_RESULT returned from
 * CSSM_TP_CertGroupVerify().
 */
CSSM_RETURN freeVfyResult(
	CSSM_TP_VERIFY_CONTEXT_RESULT *ctx)
{
	int numCerts = -1;
	CSSM_RETURN crtn = CSSM_OK;
	
	for(unsigned i=0; i<ctx->NumberOfEvidences; i++) {
		CSSM_EVIDENCE_PTR evp = &ctx->Evidence[i];
		switch(evp->EvidenceForm) {
			case CSSM_EVIDENCE_FORM_APPLE_HEADER:
				/* Evidence = (CSSM_TP_APPLE_EVIDENCE_HEADER *) */
				appFree(evp->Evidence, NULL);
				evp->Evidence = NULL;
				break;
			case CSSM_EVIDENCE_FORM_APPLE_CERTGROUP:
			{
				/* Evidence = CSSM_CERTGROUP_PTR */
				CSSM_CERTGROUP_PTR cgp = (CSSM_CERTGROUP_PTR)evp->Evidence;
				numCerts = cgp->NumCerts;	
				tpFreeCertGroup(cgp, CSSM_TRUE, CSSM_TRUE);
				evp->Evidence = NULL;
				break;
			}
			case CSSM_EVIDENCE_FORM_APPLE_CERT_INFO:
			{
				/* Evidence = array of CSSM_TP_APPLE_EVIDENCE_INFO */
				if(numCerts < 0) {
					/* Haven't gotten a CSSM_CERTGROUP_PTR! */
					printf("***Malformed VerifyContextResult (2)\n");
					crtn = CSSMERR_TP_INTERNAL_ERROR;
					break;
				}
				CSSM_TP_APPLE_EVIDENCE_INFO *evInfo = 
					(CSSM_TP_APPLE_EVIDENCE_INFO *)evp->Evidence;
				for(unsigned k=0; k<(unsigned)numCerts; k++) {
					/* Dispose of StatusCodes, UniqueRecord */
					CSSM_TP_APPLE_EVIDENCE_INFO *thisEvInfo = 
						&evInfo[k];
					if(thisEvInfo->StatusCodes) {
						appFree(thisEvInfo->StatusCodes, NULL);
					}
					if(thisEvInfo->UniqueRecord) {
						CSSM_RETURN crtn = 
							CSSM_DL_FreeUniqueRecord(thisEvInfo->DlDbHandle,
								thisEvInfo->UniqueRecord);
						if(crtn) {
							printError("CSSM_DL_FreeUniqueRecord", crtn);
							printf("   Record %p\n", thisEvInfo->UniqueRecord);
							break;
						}
						thisEvInfo->UniqueRecord = NULL;
					}
				}	/* for each cert info */
				appFree(evp->Evidence, NULL);
				evp->Evidence = NULL;
				break;
			}	/* CSSM_EVIDENCE_FORM_APPLE_CERT_INFO */
		}		/* switch(evp->EvidenceForm) */
	}			/* for each evidence */
	if(ctx->Evidence) {
		appFree(ctx->Evidence, NULL);
		ctx->Evidence = NULL;
	}
	return crtn;
}

/* Display verify results */
static void statusBitTest(
	CSSM_TP_APPLE_CERT_STATUS certStatus, 
	uint32 bit,
	const char *str)
{
	if(certStatus & bit) {
		printf("%s  ", str);
	}
}

void printCertInfo(
	unsigned numCerts,							// from CertGroup
	const CSSM_TP_APPLE_EVIDENCE_INFO *info)
{
	CSSM_TP_APPLE_CERT_STATUS cs;
	
	for(unsigned i=0; i<numCerts; i++) {
		const CSSM_TP_APPLE_EVIDENCE_INFO *thisInfo = &info[i];
		cs = thisInfo->StatusBits;
		printf("   cert %u:\n", i);
		printf("      StatusBits     : 0x%x", (unsigned)cs);
		if(cs) {
			printf(" ( ");
			statusBitTest(cs, CSSM_CERT_STATUS_EXPIRED, "EXPIRED");
			statusBitTest(cs, CSSM_CERT_STATUS_NOT_VALID_YET, 
				"NOT_VALID_YET");
			statusBitTest(cs, CSSM_CERT_STATUS_IS_IN_INPUT_CERTS, 
				"IS_IN_INPUT_CERTS");
			statusBitTest(cs, CSSM_CERT_STATUS_IS_IN_ANCHORS, 
				"IS_IN_ANCHORS");
			statusBitTest(cs, CSSM_CERT_STATUS_IS_ROOT, "IS_ROOT");
			statusBitTest(cs, CSSM_CERT_STATUS_IS_FROM_NET, "IS_FROM_NET");
			statusBitTest(cs, CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_USER, 
				"TRUST_SETTINGS_FOUND_USER");
			statusBitTest(cs, CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_ADMIN, 
				"TRUST_SETTINGS_FOUND_ADMIN");
			statusBitTest(cs, CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_SYSTEM, 
				"TRUST_SETTINGS_FOUND_SYSTEM");
			statusBitTest(cs, CSSM_CERT_STATUS_TRUST_SETTINGS_TRUST, 
				"TRUST_SETTINGS_TRUST");
			statusBitTest(cs, CSSM_CERT_STATUS_TRUST_SETTINGS_DENY, 
				"TRUST_SETTINGS_DENY");
			statusBitTest(cs, CSSM_CERT_STATUS_TRUST_SETTINGS_IGNORED_ERROR, 
				"TRUST_SETTINGS_IGNORED_ERROR");
			printf(")\n");
		}
		else {
			printf("\n");
		}
		printf("      NumStatusCodes : %u ",
			(unsigned)thisInfo->NumStatusCodes);
		for(unsigned j=0; j<thisInfo->NumStatusCodes; j++) {
			printf("%s  ", 
				cssmErrToStr(thisInfo->StatusCodes[j]));
		}
		printf("\n");
		printf("      Index: %u\n", (unsigned)thisInfo->Index);
	}
	return;
}

/* we really only need CSSM_EVIDENCE_FORM_APPLE_CERT_INFO */
#define SHOW_ALL_VFY_RESULTS		0

void dumpVfyResult(
	const CSSM_TP_VERIFY_CONTEXT_RESULT *vfyResult)
{
	unsigned numEvidences = vfyResult->NumberOfEvidences;
	unsigned numCerts = 0;
	printf("Returned evidence:\n");
	for(unsigned dex=0; dex<numEvidences; dex++) {
		CSSM_EVIDENCE_PTR ev = &vfyResult->Evidence[dex];
		#if SHOW_ALL_VFY_RESULTS
		printf("   Evidence %u:\n", dex);
		#endif
		switch(ev->EvidenceForm) {
			case CSSM_EVIDENCE_FORM_APPLE_HEADER:
			{
				#if SHOW_ALL_VFY_RESULTS
				const CSSM_TP_APPLE_EVIDENCE_HEADER *hdr = 
					(const CSSM_TP_APPLE_EVIDENCE_HEADER *)(ev->Evidence);
				printf("      Form = HEADER; Version = %u\n", hdr->Version);
				#endif
				break;
			}
			case CSSM_EVIDENCE_FORM_APPLE_CERTGROUP:
			{
				const CSSM_CERTGROUP *grp = 
					(const CSSM_CERTGROUP *)ev->Evidence;
				numCerts = grp->NumCerts;
				#if SHOW_ALL_VFY_RESULTS
				/* parse the rest of this eventually */
				/* Note we depend on this coming before the CERT_INFO */
				printf("      Form = CERTGROUP; numCerts = %u\n", numCerts);
				#endif
				break;
			}
			case CSSM_EVIDENCE_FORM_APPLE_CERT_INFO:	
			{
				const CSSM_TP_APPLE_EVIDENCE_INFO *info = 
					(const CSSM_TP_APPLE_EVIDENCE_INFO *)ev->Evidence;
				printCertInfo(numCerts, info);
				break;
			}
			default:
				printf("***UNKNOWN Evidence form (%u)\n", 
					(unsigned)ev->EvidenceForm);
				break;
		}
	}
}

/* 
 * Obtain system anchors in CF and in CSSM_DATA form.
 * Caller must CFRelease the returned rootArray and 
 * free() the returned CSSM_DATA array, but not its
 * contents - SecCertificates themselves own that.
 */
OSStatus getSystemAnchors(
	CFArrayRef *rootArray,	/* RETURNED */
	CSSM_DATA **anchors,	/* RETURNED */
	unsigned *numAnchors)	/* RETURNED */
{
	OSStatus ortn;
	CFArrayRef cfAnchors;
	CSSM_DATA *cssmAnchors;

	ortn = SecTrustSettingsCopyUnrestrictedRoots(false, false, true,
		&cfAnchors);
	if(ortn) {
		cssmPerror("SecTrustSettingsCopyUnrestrictedRoots", ortn);
		return ortn;
	}
	unsigned _numAnchors = CFArrayGetCount(cfAnchors);
	cssmAnchors = (CSSM_DATA *)malloc(sizeof(CSSM_DATA) * _numAnchors);
	unsigned dex;
	for(dex=0; dex<_numAnchors; dex++) {
		SecCertificateRef root = (SecCertificateRef)CFArrayGetValueAtIndex(
			cfAnchors, dex);
		ortn = SecCertificateGetData(root, &cssmAnchors[dex]);
		if(ortn) {
			cssmPerror("SecCertificateGetData", ortn);
			return ortn;
		}
	}
	*rootArray = cfAnchors;
	*anchors = cssmAnchors;
	*numAnchors = _numAnchors;
	return noErr;
}

/* get a SecCertificateRef from a file */
SecCertificateRef certFromFile(
	const char *fileName)
{
	unsigned char *cp = NULL;
	unsigned len = 0;
	if(readFile(fileName, &cp, &len)) {
		printf("***Error reading file %s\n", fileName);
		return NULL;
	}
	SecCertificateRef certRef;
	CSSM_DATA certData = {len, cp};
	OSStatus ortn = SecCertificateCreateFromData(&certData, 
			CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER, &certRef);
	if(ortn) {
		cssmPerror("SecCertificateCreateFromData", ortn);
		return NULL;
	}
	free(cp);
	return certRef;
}

