/*
 * Attempt to verify either one cert file, or every file in cwd,
 * with specified issuer cert. Used to study vulnerability to 
 * NISCC cert DOS attacks. 
 */
#include <Security/Security.h>
#include <Security/cuFileIo.h>
#include <Security/cuCdsaUtils.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>

static void usage(char **argv)
{
	printf("usage: %s [-v(erbose)] issuerCertFile [certFile]\n", argv[0]);
	exit(1);
}

/*
 * Known file names to NOT parse
 */
static const char *skipTheseFiles[] = 
{
	/* standard entries */
	".",
	"..",
	"CVS",
	".cvsignore",
	NULL
};

/* returns false if specified fileName is in skipTheseFiles[] */
static bool shouldWeParse(
	const char *fileName)		// C string
{
	for(const char **stf=skipTheseFiles; *stf!=NULL; stf++) { 
		const char *tf = *stf;
		if(!strcmp(fileName, *stf)) {
			return false;
		}
	}
	return true;
}

/* 
 * Just try to verify. Returns true on any reasonable outcome.
 */
static bool vfyCert(
	CSSM_CL_HANDLE clHand, 
	CSSM_CC_HANDLE ccHand, 
	const unsigned char *certData, 
	unsigned certDataLen, 
	bool verbose)
{
	CSSM_DATA cdata = {certDataLen, (uint8 *)certData};
	CSSM_RETURN crtn;

	crtn = CSSM_CL_CertVerifyWithKey(clHand, ccHand, &cdata);
	
	/* hard-coded list of acceptable outcomes */
	switch(crtn) {
		case CSSM_OK:
			if(verbose) {
				printf("-ok-");
			}	
			return true;
		case CSSMERR_CL_VERIFICATION_FAILURE:
			if(verbose) {
				printf("-vfy_fail-");
			}	
			return true;
		case CSSMERR_CL_UNKNOWN_FORMAT:
			if(verbose) {
				printf("-format-");
			}	
			return true;
		default:
			cuPrintError("CSSM_CL_CertVerifyWithKey", crtn);
			return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	bool quiet = false;
	unsigned char *certData;
	unsigned certDataLen;
	unsigned char *issuerData;
	unsigned issuerDataLen;
	DIR *dir;
	struct dirent *de;
	bool verbose = false;
	int filearg = 1;
	
	if((argc < 2 ) || (argc > 4)) {
		usage(argv);
	}
	if(argv[1][0] == '-') {
		switch(argv[1][1]) {
			case 'v':
				verbose = true;
				break;
			default:
				usage(argv);
		}
		filearg++;
		argc--;
	}
	
	CSSM_CSP_HANDLE cspHand = cuCspStartup(CSSM_TRUE);
	CSSM_CL_HANDLE clHand = cuClStartup();
	if((cspHand == 0) || (clHand == 0)) {
		exit(1);
	}
	
	/* read issuer cert, extract its public key for quick verify */
	char *fn = argv[filearg++];
	if(readFile(fn, &issuerData, &issuerDataLen)) {
		printf("\n***Error reading file %s. Aborting.\n", fn);
		exit(1);
	}
	CSSM_DATA issuerCert = {issuerDataLen, issuerData};
	CSSM_KEY_PTR issuerPubKey;
	CSSM_RETURN crtn = CSSM_CL_CertGetKeyInfo(clHand, &issuerCert, 
		&issuerPubKey);
	if(crtn) {
		cuPrintError("CSSM_CL_CertGetKeyInfo", crtn);
		exit(1);
	}	
	
	/* a reusable signature context */
	CSSM_CC_HANDLE ccHand;
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		CSSM_ALGID_SHA1WithRSA,
		NULL,		// AccessCred
		issuerPubKey,
		&ccHand);
	if(crtn) {
		cuPrintError("CSSM_CSP_CreateSignatureContext", crtn);
		exit(1);
	}	
	
	if(argc == 3) {
		/* read & parse one file */
		char *fn = argv[filearg++];
		if(!quiet) {
			printf("...reading %s\n", fn);
		}
		if(readFile(fn, &certData, &certDataLen)) {
			printf("\n***Error reading file %s. Aborting.\n", fn);
			exit(1);
		}
		if(!vfyCert(clHand, ccHand, certData, certDataLen, verbose)) {
			printf("\n***GOT AN EXCEPTION ON %s\n", fn);
			exit(1);
		}
		goto done;
	}
	dir = opendir(".");
	if(dir == NULL) {
		printf("Huh? Can't open . as a directory.\n");
		exit(1);
	}
	de = readdir(dir);
	while(de != NULL) {
		char filename[MAXNAMLEN + 1];
		memmove(filename, de->d_name, de->d_namlen);
		filename[de->d_namlen] = '\0';
		if(shouldWeParse(filename)) {
			if(!quiet) {
				printf("...%s", filename);
				fflush(stdout);
			}
			if(readFile(filename, &certData, &certDataLen)) {
				printf("\n***Error reading file %s. Aborting.\n", filename);
				exit(1);
			}
			if(!vfyCert(clHand, ccHand, certData, certDataLen, verbose)) {
				printf("\n***GOT AN EXCEPTION ON %s\n", filename);
				exit(1);
			}
			free(certData);
		}
		de = readdir(dir);
	}	
	closedir(dir);
done:
	printf("\nisccCertVerify did not crash.\n");
	free(issuerData);
	return 0;
}
	
