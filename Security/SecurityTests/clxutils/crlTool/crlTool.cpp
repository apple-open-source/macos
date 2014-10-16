/*
 * crlTool.cpp
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>
#include <clAppUtils/clutils.h>
#include <clAppUtils/CertParser.h>
#include <Security/Security.h>
#include "crlNetwork.h"
#include <security_cdsa_utils/cuPrintCert.h>
#define LOOPS_DEF			100

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  -c certFile    -- obtain CRL via net from this cert\n");
	printf("  -C crlFile     -- CRL from this file\n");
	printf("  -p             -- parse the CRL\n");
	printf("  -o outFile     -- write the fetched CRL to this file\n");
	printf("  -v             -- verbose CRL dump\n");
	/* etc. */
	exit(1);
}

static int fetchCrlViaGeneralNames(
	const CE_GeneralNames	*names,
	unsigned char			**crl,		// mallocd and RETURNED
	size_t					*crlLen)	// RETURNED
{
	CSSM_DATA crlData = {0, NULL};
	CSSM_RETURN crtn;
	
	for(unsigned nameDex=0; nameDex<names->numNames; nameDex++) {
		CE_GeneralName *name = &names->generalName[nameDex];
		switch(name->nameType) {
			case GNT_URI:
				if(name->name.Length < 5) {
					continue;
				}
				if(strncmp((char *)name->name.Data, "ldap:", 5) &&
				   strncmp((char *)name->name.Data, "http:", 5) && 
				   strncmp((char *)name->name.Data, "https:", 6)) {
					/* eventually handle other schemes here */
					continue;
				}
				
				/* OK, we can do this */
				crtn = crlNetFetch(&name->name, LT_Crl, &crlData);
				if(crtn) {
					printf("...net fetch error\n");
					return 1;
				}
				*crl = crlData.Data;
				*crlLen = crlData.Length;
				return 0;
				
			default:
				printf("fetchCrlViaGeneralNames: unknown"
					"nameType (%u)", (unsigned)name->nameType); 
				break;
		}
	}
	printf("...GNT_URI name not found in GeneralNames\n");
	return 1;
}

static int fetchCrl(
	CertParser &cert,
	unsigned char **crl,		// mallocd and RETURNED
	size_t *crlLen)				// RETURNED
{
	CE_CRLDistPointsSyntax *dps = (CE_CRLDistPointsSyntax *)
		cert.extensionForOid(CSSMOID_CrlDistributionPoints);
	
	*crl = NULL;
	*crlLen = 0;
	if(dps == NULL) {
		/* not an error, just indicate NULL return */
		printf("***No CrlDistributionPoints in this cert.\n");
		return 0;
	}
	for(unsigned dex=0; dex<dps->numDistPoints; dex++) {
		
		CE_CRLDistributionPoint *dp = &dps->distPoints[dex];
		if(dp->distPointName == NULL) {
			continue;
		}
		switch(dp->distPointName->nameType) {
			case CE_CDNT_NameRelativeToCrlIssuer:
				printf("...CE_CDNT_NameRelativeToCrlIssuer not implemented\n");
				break;
				
			case CE_CDNT_FullName:
			{
				CE_GeneralNames *names = dp->distPointName->dpn.fullName;
				int rtn = fetchCrlViaGeneralNames(names, crl, crlLen);
				if(rtn == 0) {
					return 0;
				}
				/* else try again if there's another name */
				break;
			}	/* CE_CDNT_FullName */
			
			default:
				/* not yet */
				printf("unknown distPointName->nameType (%u)\n",
						(unsigned)dp->distPointName->nameType);
				break;
		}	/* switch distPointName->nameType */
	}	/* for each distPoints */
	printf("...CrlDistributionPoints found, but nothing we can use.\n");
	return 0;
}

int main(int argc, char **argv)
{
	char *certFile = NULL;
	char *crlFile = NULL;
	unsigned char *certData;
	unsigned certDataLen;
	bool doParse = false;
	char *outFile = NULL;
	CSSM_BOOL verbose = CSSM_FALSE;
	unsigned char *crl = NULL;
	size_t crlLen = 0;
	int rtn = -1;
	
	if(argc < 2) {
		usage(argv);
	}
	
	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "c:C:po:vh")) != -1) {
		switch (arg) {
			case 'c':
				certFile = optarg;
				break;
			case 'C':
				crlFile = optarg;
				break;
			case 'p':
				doParse = true;
				break;
			case 'o':
				outFile = optarg;
				break;
			case 'v':
				verbose = CSSM_TRUE;
				break;
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	if((certFile != NULL) && (crlFile != NULL)) {
		printf("***crlFile and certFile are mutually exclusive.\n");
		usage(argv);
	}
	if((certFile == NULL) && (crlFile == NULL)) {
		printf("***Must specify either certFile or crlFile\n");
		usage(argv);
	}
	
	CSSM_RETURN crtn;
	CSSM_CL_HANDLE clHand = clStartup();
	CertParser parser(clHand);
	
	if(crlFile) {
		unsigned len;
		if(readFile(crlFile, &crl, &len)) {
			printf("***Error reading %s. Aborting.\n", crlFile);
			exit(1);
		}
		crlLen = len;
	}
	if(certFile) {
		if(readFile(certFile, &certData, &certDataLen)) {
			printf("***Error reading %s. Aborting.\n", certFile);
			exit(1);
		}
		CSSM_DATA cdata = {certDataLen, certData};
		crtn = parser.initWithData(cdata);
		if(crtn) {
			printf("Error parsing cert %s. Aborting.\n", certFile);
			exit(1);
		} 
		rtn = fetchCrl(parser, &crl, &crlLen);
		if(rtn) {
			printf("***aborting.\n");
			exit(1);
		}
	}
	
	if(doParse) {
		if(crl == NULL) {
			printf("...parse specified but no CRL found.\n");
		}
		else {
			if(certFile != NULL) {
				printf("============== CRL for cert %s ==============\n", certFile);
			}
			printCrl(crl, crlLen, verbose);
			if(certFile != NULL) {
				printf("============== end of CRL ==============\n");
			}
		}
	}
	if(outFile) {
		if(crl == NULL) {
			printf("...outFile specified but no CRL found.\n");
		}
		else {
			if(writeFile(outFile, crl, crlLen)) {
				printf("***Error writing CRL to %s.\n", outFile);
				rtn = 1;
			}
			else {
				printf("...wrote %u bytes to %s\n", (unsigned)crlLen, outFile);
			}
		}
	}
	return rtn;
}
