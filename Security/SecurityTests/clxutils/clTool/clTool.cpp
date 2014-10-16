/*
 * clTool.cpp - menu-driven CL exerciser
 */
 
#include <security_cdsa_utils/cuPrintCert.h>
#include <security_cdsa_utils/cuOidParser.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <clAppUtils/clutils.h>
#include <utilLib/common.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <Security/cssmtype.h>
#include <Security/cssmapi.h>
#include <Security/oidscert.h>

/*
 * A list of OIDs we inquire about. 
 */
static const CSSM_OID *knownOids[] = 
{
	&CSSMOID_X509V1Version,			// not always present
	&CSSMOID_X509V1SerialNumber,
	&CSSMOID_X509V1IssuerNameCStruct,
	&CSSMOID_X509V1SubjectNameCStruct,
	&CSSMOID_CSSMKeyStruct,
	&CSSMOID_X509V1SubjectPublicKeyCStruct,
	&CSSMOID_X509V1ValidityNotBefore,
	&CSSMOID_X509V1ValidityNotAfter,
	&CSSMOID_X509V1SignatureAlgorithmTBS,
	&CSSMOID_X509V1SignatureAlgorithm,
	&CSSMOID_X509V1Signature,
	&CSSMOID_X509V3CertificateExtensionCStruct,
	&CSSMOID_KeyUsage,
	&CSSMOID_BasicConstraints,
	&CSSMOID_ExtendedKeyUsage,
	&CSSMOID_CertificatePolicies,
	&CSSMOID_NetscapeCertType
};

#define NUM_KNOWN_OIDS	(sizeof(knownOids) / sizeof(CSSM_OID *))

static const char *oidNames[] = 
{
	"CSSMOID_X509V1Version",
	"CSSMOID_X509V1SerialNumber",
	"CSSMOID_X509V1IssuerNameCStruct",
	"CSSMOID_X509V1SubjectNameCStruct",
	"CSSMOID_CSSMKeyStruct",
	"CSSMOID_X509V1SubjectPublicKeyCStruct",
	"CSSMOID_X509V1ValidityNotBefore",
	"CSSMOID_X509V1ValidityNotAfter",
	"CSSMOID_X509V1SignatureAlgorithmTBS",
	"CSSMOID_X509V1SignatureAlgorithm",
	"CSSMOID_X509V1Signature",
	"CSSMOID_X509V3CertificateExtensionCStruct",
	"CSSMOID_KeyUsage",
	"CSSMOID_BasicConstraints",
	"CSSMOID_ExtendedKeyUsage",
	"CSSMOID_CertificatePolicies",
	"CSSMOID_NetscapeCertType"
};

static void usage(char **argv)
{
	printf("Usage: %s certFile\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	CSSM_DATA 		certData = {0, NULL};
	CSSM_CL_HANDLE 	clHand = CSSM_INVALID_HANDLE;
	CSSM_HANDLE 	cacheHand = CSSM_INVALID_HANDLE;
	CSSM_HANDLE 	searchHand = CSSM_INVALID_HANDLE;
	char 			resp;
	CSSM_RETURN 	crtn;
	unsigned 		fieldDex;
	CSSM_DATA_PTR	fieldValue;
	uint32			numFields;
	CSSM_FIELD		field;
	CSSM_FIELD_PTR	fieldPtr;
	OidParser 		parser;
	unsigned		len;
	
	if(argc != 2) {
		usage(argv);
	}
	if(readFile(argv[1], &certData.Data, &len)) {
		printf("Can't read file %s' aborting.\n", argv[1]);
		exit(1);
	}
	certData.Length = len;
	
	while(1) {
		fpurge(stdin);
		printf("a  load/attach\n");
		printf("d  detach/unload\n");
		printf("c  cache the cert\n");
		printf("u  uncache the cert\n");
		printf("g  get field (uncached)\n");
		printf("G  get field (cached)\n");
		printf("f  get all fields, then free\n");
		printf("q  quit\n");
		printf("Enter command: ");
		resp = getchar();
		switch(resp) {
			case 'a':
				if(clHand != CSSM_INVALID_HANDLE) {
					printf("***Multiple attaches; expect leaks\n");
				}
				clHand = clStartup();
				if(clHand == CSSM_INVALID_HANDLE) {
					printf("***Error attaching to CL.\n");
				}
				else {
					printf("...ok\n");
				}
				break;
				
			case 'd':
				/* 
				 * Notes:
				 * -- this should cause the CL to free up all cached certs
				 *    no matter what - even if we've done multiple certCache
				 *    ops. However the plugin framework doesn't delete the
				 *    session object on detach (yet) so expect leaks in 
				 *    that case.
				 * -- we don't  clear out cacheHand or searchHand here; this
				 *    allows verification of proper handling of bogus handles. 
				 */
				clShutdown(clHand);
				clHand = CSSM_INVALID_HANDLE;
				printf("...ok\n");
				break;
				
			case 'c':
				/* cache the cert */
				if(cacheHand != CSSM_INVALID_HANDLE) {
					printf("***NOTE: a cert is already cached. Expect leaks.\n");						}
				crtn = CSSM_CL_CertCache(clHand, &certData, &cacheHand);
				if(crtn) {
					printError("CSSM_CL_CertCache", crtn);
				}
				else {
					printf("...ok\n");
				}
				break;
				
			case 'u':
				/* abort cache */
				crtn = CSSM_CL_CertAbortCache(clHand, cacheHand);
				if(crtn) {
					printError("CSSM_CL_CertAbortCache", crtn);
				}
				else {
					cacheHand = CSSM_INVALID_HANDLE;
					printf("...ok\n");
				}
				break;
				
			case 'g':
				/* get one field (uncached) */
				fieldDex = genRand(0, NUM_KNOWN_OIDS - 1);
				crtn = CSSM_CL_CertGetFirstFieldValue(clHand,
					&certData,
					knownOids[fieldDex],
					&searchHand,
					&numFields, 
					&fieldValue);
				if(crtn) {
					printf("***Error fetching field %s\n", oidNames[fieldDex]);
					printError("CSSM_CL_CertGetFirstFieldValue", crtn);
					break;
				}
				printf("%s: %u fields found\n", oidNames[fieldDex], (unsigned)numFields);
				field.FieldValue = *fieldValue;
				field.FieldOid   = *(knownOids[fieldDex]);
				printCertField(field, parser, CSSM_TRUE);
				crtn = CSSM_CL_FreeFieldValue(clHand, knownOids[fieldDex], fieldValue);
				if(crtn) {
					printError("CSSM_CL_FreeFieldValue", crtn);
					/* keep going */
				}
				for(unsigned i=1; i<numFields; i++) {
					crtn = CSSM_CL_CertGetNextFieldValue(clHand,
						searchHand,
						&fieldValue);
					if(crtn) {
						printError("CSSM_CL_CertGetNextFieldValue", crtn);
						break;
					}
					field.FieldValue = *fieldValue;
					printCertField(field, parser, CSSM_TRUE);
					crtn = CSSM_CL_FreeFieldValue(clHand, 
						knownOids[fieldDex], fieldValue);
					if(crtn) {
						printError("CSSM_CL_FreeFieldValue", crtn);
						/* keep going */
					}
				} /* for additional fields */
				
				/* verify one more getField results in error */
				crtn = CSSM_CL_CertGetNextFieldValue(clHand,
					searchHand,
					&fieldValue);
				if(crtn != CSSMERR_CL_NO_FIELD_VALUES) {
					if(crtn == CSSM_OK) {
						printf("***unexpected success on final GetNextFieldValue\n");
					}
					else {
						printError("Wrong error on final GetNextFieldValue", crtn);
					}
				}
				crtn = CSSM_CL_CertAbortQuery(clHand, searchHand);
				if(crtn) {
					printError("CSSM_CL_CertAbortQuery", crtn);
				}
				break;
				
			case 'G':
				/* get one field (uncached) */
				fieldDex = genRand(0, NUM_KNOWN_OIDS - 1);
				crtn = CSSM_CL_CertGetFirstCachedFieldValue(clHand,
					cacheHand,
					knownOids[fieldDex],
					&searchHand,
					&numFields, 
					&fieldValue);
				if(crtn) {
					printf("***Error fetching field %s\n", oidNames[fieldDex]);
					printError("CSSM_CL_CertGetFirstCachedFieldValue", crtn);
					break;
				}
				printf("%s: %u fields found\n", oidNames[fieldDex], (unsigned)numFields);
				field.FieldValue = *fieldValue;
				field.FieldOid   = *(knownOids[fieldDex]);
				printCertField(field, parser, CSSM_TRUE);
				crtn = CSSM_CL_FreeFieldValue(clHand, knownOids[fieldDex], fieldValue);
				if(crtn) {
					printError("CSSM_CL_FreeFieldValue", crtn);
					/* keep going */
				}
				for(unsigned i=1; i<numFields; i++) {
					crtn = CSSM_CL_CertGetNextCachedFieldValue(clHand,
						searchHand,
						&fieldValue);
					if(crtn) {
						printError("CSSM_CL_CertGetNextCachedFieldValue", crtn);
						break;
					}
					field.FieldValue = *fieldValue;
					printCertField(field, parser, CSSM_TRUE);
					crtn = CSSM_CL_FreeFieldValue(clHand, 
						knownOids[fieldDex], fieldValue);
					if(crtn) {
						printError("CSSM_CL_FreeFieldValue", crtn);
						/* keep going */
					}
				} /* for additional cached fields */
				
				/* verify one more getField results in error */
				crtn = CSSM_CL_CertGetNextCachedFieldValue(clHand,
					searchHand,
					&fieldValue);
				if(crtn != CSSMERR_CL_NO_FIELD_VALUES) {
					if(crtn == CSSM_OK) {
						printf("***unexpected success on final GetNextCachedFieldValue\n");
					}
					else {
						printError("Wrong error on final GetNextCachedFieldValue", crtn);
					}
				}
				crtn = CSSM_CL_CertAbortQuery(clHand, searchHand);
				if(crtn) {
					printError("CSSM_CL_CertAbortQuery", crtn);
				}
				break;
				
			case 'f':
				/* get all fields (for leak testing) */
				crtn = CSSM_CL_CertGetAllFields(clHand,
					&certData,
					&numFields,
					&fieldPtr);
				if(crtn) {
					printError("CSSM_CL_CertGetAllFields", crtn);
					break;
				}
				printf("...numFields %u\n", (unsigned)numFields);
				crtn = CSSM_CL_FreeFields(clHand, numFields, &fieldPtr);
				if(crtn) {
					printError("CSSM_CL_FreeFields", crtn);
				}
				break;
				
			case 'q':
				goto quit;
				
			default:
				printf("Huh?\n");
				break;
		}	/* switch resp */
	}
quit:
	free(certData.Data);
	if(clHand != CSSM_INVALID_HANDLE) {
		clShutdown(clHand);
	}
	return 0;
}
