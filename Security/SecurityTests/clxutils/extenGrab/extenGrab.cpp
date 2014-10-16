/*
 * extenGrab - write the unparsed extension blobs of a specified
 * cert to files for external examination
 */
#include <Security/SecAsn1Coder.h>
#include <Security/X509Templates.h>
#include <Security/cssmapple.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <security_cdsa_utils/cuOidParser.h>
#include <security_cdsa_utils/cuFileIo.h>

static void usage(char **argv)
{
	printf("Usage: %s certFile outFileBase [r for CRL, default is cert]\n",
		argv[0]);
	exit(1);
}

/*
 * How many items in a NULL-terminated array of pointers?
 */
static unsigned nssArraySize(
	const void **array)
{
    unsigned count = 0;
    if (array) {
		while (*array++) {
			count++;
		}
    }
    return count;
}

int main(int argc, char **argv)
{
	if(argc < 3) {
		usage(argv);
	}
	
	bool 					doCert = true;
	NSS_Certificate 		signedCert;
	NSS_Crl					signedCrl;
	void 					*decodeTarget;
	const SecAsn1Template	*templ;
	NSS_CertExtension		***extenp;
	
	for(int arg=3; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'r':
				doCert = false;		// i.e. CRL
				break;
			default:
				usage(argv);
		}
	}
	
	if(doCert) {
		memset(&signedCert, 0, sizeof(signedCert));
		decodeTarget = &signedCert;
		templ = kSecAsn1SignedCertTemplate;
		extenp = &signedCert.tbs.extensions;
	}
	else {
		memset(&signedCrl, 0, sizeof(signedCrl));
		decodeTarget = &signedCrl;
		templ = kSecAsn1SignedCrlTemplate;
		extenp = &signedCrl.tbs.extensions;
	}
	
	const char *certFile = argv[1];
	const char *outBase = argv[2];
	unsigned char *rawCert;
	unsigned rawCertLen;
	
	if(readFile(certFile, &rawCert, &rawCertLen)) {
		printf("***Can't read cert file. Abortihng.\n");
		exit(1);
	}
	
	SecAsn1CoderRef		coder;
	CSSM_DATA			rawItem = {rawCertLen, rawCert};
	
	OSStatus ortn = SecAsn1CoderCreate(&coder);
	if(ortn) {
		cssmPerror("SecAsn1CoderCreate", ortn);
		
	}
	if(SecAsn1DecodeData(coder, &rawItem, templ, decodeTarget)) {
		printf("SecAsn1DecodeData(signed) error\n");
		exit(1);
	}
	
	NSS_CertExtension **extens = *extenp;
	unsigned numExtens = nssArraySize((const void **)extens);
	if(numExtens == 0) {
		printf("There appear to be zero extensions in this item.\n");
		exit(0);
	}
	
	OidParser parser;
	char oidStr[OID_PARSER_STRING_SIZE];
	char outFileName[200];
	
	for(unsigned dex=0; dex<numExtens; dex++) {
		NSS_CertExtension *exten = extens[dex];
		parser.oidParse(exten->extnId.Data, exten->extnId.Length, oidStr);
		printf("Extension %u : %s\n", dex, oidStr);
		sprintf(outFileName, "%s_%u", outBase, dex);
		if(writeFile(outFileName, exten->value.Data, exten->value.Length)) {
			printf("***Error writing %s. Aborting.\n",
				outFileName);
			exit(1);
		}
		else {
			printf("...wrote %lu bytes to %s\n", 
				exten->value.Length, outFileName);
		}
	}
	SecAsn1CoderRelease(coder);
	printf("..done.\n");
	return 0;
}
