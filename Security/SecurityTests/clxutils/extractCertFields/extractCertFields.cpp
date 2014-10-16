/*
 * Parse a cert, dump its subject name (in normalized DER-encoded form),
 * key size, and public key blob to stdout.
 */
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <clAppUtils/clutils.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Security/cssm.h>
#include <Security/oidscert.h>
#include <Security/SecAsn1Coder.h>
#include <Security/X509Templates.h>

#define WRITE_NAME_FILE		1

/* allow checking various DER encoded fields */
#define SUBJECT_NAME_OID		CSSMOID_X509V1SubjectName		/* normalized */
// #define SUBJECT_NAME_OID		CSSMOID_X509V1SubjectNameStd	/* non normalized */

/*
 * Print the contents of a CSSM_DATA, like so:
 *
 * static const uint8 <label>_bytes[] = {
 *		....contents.....
 * };
 * static const CSSM_DATA <label> = { length, (uint8 *)<label>_bytes} ;
 */
static void dumpDataBlob(
	const char *label,
	const CSSM_DATA_PTR data)
{
	unsigned i;
	
	printf("static const uint8 %s_bytes[] = {\n   ", label);
	for(i=0; i<data->Length; i++) {
		printf("0x%02x", data->Data[i]);
		if(i != (data->Length - 1)) {
			printf(",  ");
		}
		if((i % 8) == 7) {
			printf("\n   ");
		}
	}
	printf("\n};\n");
	printf("static const CSSM_DATA %s = { %lu, (uint8 *)%s_bytes };\n",
		label, data->Length, label);
}

static void	printHeader(
	const char 		*fileName, 
	CSSM_DATA_PTR	subject)
{
	CSSM_FIELD field;
	OidParser parser(false);		// quick parser, no config file 
	
	printf("/***********************\n");
	printf("Cert File Name: %s\n", fileName); 
	field.FieldValue = *subject;
	field.FieldOid = CSSMOID_X509V1SubjectNameCStruct;
	printCertField(field, parser, true);
	printf(" ***********************/\n");
}

int main(int argc, char **argv)
{
	CSSM_CL_HANDLE		clHand;			// CL handle
	CSSM_DATA			rawCert = {0, NULL};	
	uint32				numFields;
	CSSM_HANDLE 		ResultsHandle = 0;
	char				baseName[255];
	char				blobName[255];
	char				*cp;
	int 				rtn;
	int					nameLen;
	CSSM_RETURN			crtn;
	CSSM_DATA_PTR		value;
	CSSM_KEY_PTR		key;
	uint32				keySize;
	NSS_Certificate 	signedCert;
	SecAsn1CoderRef		coder;
	
	if(argc != 2) {
		printf("usage: %s certFile\n", argv[0]);
		exit(1);
	}
	
	/* connect to CL */
	clHand = clStartup();
	if(clHand == 0) {
		printf("clStartup failure; aborting\n");
		return 0;
	}

	/* subsequent errors to abort: */
	/* read a in raw cert */
	unsigned len;
	rtn = readFile(argv[1], &rawCert.Data, &len);
	if(rtn) {
		printf("Error %s reading file %s\n", strerror(rtn), argv[1]);
		exit(1);
	}
	rawCert.Length = len;
	
	/* C string of file name, terminating at '.' or space */
	nameLen = strlen(argv[1]);
	memmove(baseName, argv[1], nameLen);
	baseName[nameLen] = '\0';
	cp = strchr(baseName, '.');
	if(cp) {
		*cp = '\0';
	}
	cp = strchr(baseName, ' ');
	if(cp) {
		*cp = '\0';
	}
	
	/* print filename and parsed subject name as comment */
	crtn = CSSM_CL_CertGetFirstFieldValue(
		clHand,
		&rawCert,
	    &CSSMOID_X509V1SubjectNameCStruct,
	    &ResultsHandle,
	    &numFields,
		&value);
	if(crtn) {
		printError("CSSM_CL_CertGetFirstFieldValue(CSSMOID_X509V1SubjectNameCStruct)", crtn);
  		goto abort;
	}
  	CSSM_CL_CertAbortQuery(clHand, ResultsHandle);
  	if(value == NULL) {
  		printf("Error extracting subject name\n");
  		goto abort;
  	}
	printHeader(argv[1], value);
  	CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1SubjectNameCStruct, value);

	/* print normalized & encoded subject name as C data */
	crtn = CSSM_CL_CertGetFirstFieldValue(
		clHand,
		&rawCert,
	    &SUBJECT_NAME_OID,
	    &ResultsHandle,
	    &numFields,
		&value);
	if(crtn) {
		printError("CSSM_CL_CertGetFirstFieldValue(CSSMOID_X509V1SubjectName)", crtn);
  		goto abort;
	}
  	CSSM_CL_CertAbortQuery(clHand, ResultsHandle);
  	if(value == NULL) {
  		printf("Error extracting subject name\n");
  		goto abort;
  	}
  	sprintf(blobName, "%s_subject", baseName);
  	dumpDataBlob(blobName, value);
	#if		WRITE_NAME_FILE
	writeFile(blobName, value->Data, (unsigned)value->Length);
	#endif
  	CSSM_CL_FreeFieldValue(clHand, &SUBJECT_NAME_OID, value);

	/* print key blob as data */
	crtn = CSSM_CL_CertGetFirstFieldValue(
		clHand,
		&rawCert,
		&CSSMOID_CSSMKeyStruct,
	    &ResultsHandle,
	    &numFields,
		&value);
	if(crtn) {
		printError("CSSM_CL_CertGetFirstFieldValue(CSSMOID_CSSMKeyStruct)", crtn);
  		goto abort;
	}
  	CSSM_CL_CertAbortQuery(clHand, ResultsHandle );
  	if(value == NULL) {
  		printf("Error extracting public key\n");
  		goto abort;
  	}
	if(value->Length != sizeof(CSSM_KEY)) {
		printf("CSSMOID_CSSMKeyStruct length error\n");
		goto abort;
	}
	key = (CSSM_KEY_PTR)value->Data;
	sprintf(blobName, "%s_pubKey", baseName);
  	dumpDataBlob(blobName, &key->KeyData);
	keySize = key->KeyHeader.LogicalKeySizeInBits;
  	CSSM_CL_FreeFieldValue(clHand, &CSSMOID_CSSMKeyStruct, value);
	
	/* unnormalized DER-encoded issuer */
	SecAsn1CoderCreate(&coder);
	memset(&signedCert, 0, sizeof(signedCert));
	if(SecAsn1DecodeData(coder, &rawCert, kSecAsn1SignedCertTemplate, &signedCert)) {
		printf("***Error NSS-decoding certificate\n");
	}
	else {
		sprintf(blobName, "%s_derIssuer", baseName);
		dumpDataBlob(blobName, &signedCert.tbs.derIssuer);
	}
	/* now the the struct containing all three */
	printf("\n    { &%s_subject, &%s_pubKey, %u },\n", baseName, baseName, (unsigned)keySize);
abort:
	free(rawCert.Data);
	if(clHand != 0) {
		CSSM_ModuleDetach(clHand);
	}
	SecAsn1CoderRelease(coder);
	return 0;
}
