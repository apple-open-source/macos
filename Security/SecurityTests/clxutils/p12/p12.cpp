/*
 * multipurpose pkcs12 tool. 
 */
#include <security_cdsa_utils/cuFileIo.h>
#include <stdlib.h>
#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include "p12.h"
#include <security_cdsa_utils/cuCdsaUtils.h>

static void usage(char **argv)
{
	printf("Usage:\n");
	printf("  %s p infile [options]    parse\n", argv[0]);
	printf("  %s d infile [options]    decode\n", argv[0]);
	printf("  %s e infile [options]    decode-->encode\n", argv[0]);
	printf("  %s i infile keychain     import to keychain\n", argv[0]);
	printf("  %s x outfile keychain    export from keychain\n", argv[0]);
	
	printf("Options:\n");
	printf("   p=password\n");
	printf("   z=keychainPassword\n");
	printf("   P (use secure passphrase)\n");
	printf("   k=keychain\n");
	printf("   l=loops\n");
	printf("   n(o prompt; export only)\n");
	printf("   v(erbose)\n");
	/* others here */
	exit(1);
}

typedef enum {
	PR_Parse,
	PR_Decode,
	PR_Reencode,
	PR_Import,
	PR_Export
} P12op;

int main(int argc, char **argv)
{	
	char *inFile;
	P12op op;
	int minArgs = 1;
	CFStringRef pwd = NULL;
	bool verbose = false;
	unsigned loops = 1;
	char *kcName = NULL;
	bool noPrompt = false;
	char *kcPwd = NULL;
	bool usePassKey = false;
	
	if(argc < 2) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'p':
			op = PR_Parse;
			minArgs = 3;
			break;
		case 'd':
			op = PR_Decode;
			minArgs = 3;
			break;
		case 'e':
			op = PR_Reencode;
			minArgs = 3;
			break;
		case 'i':
			op = PR_Import;
			minArgs = 4;
			break;
		case 'x':
			op = PR_Export;
			minArgs = 4;
			break;
		default:
			usage(argv);
	}
	if(argc < minArgs) {
		usage(argv);
	}
	for(int arg=minArgs; arg<argc; arg++) {
		char *argp = argv[arg];
		switch(argp[0]) {
			case 'p':
				pwd = CFStringCreateWithCString(NULL, &argp[2],
					kCFStringEncodingASCII);
				break;
			case 'k':
				kcName = &argp[2];
				break;
			case 'P':
				usePassKey = true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'n':
				noPrompt = true;
				break;
			case 'l':
				loops = atoi(&argp[2]);
				break;
			case 'z':
				kcPwd = &argp[2];
				break;
			default:
				usage(argv);

		}
	}
	
	/* import/export - ready to go right now */
	switch(op) {
		case PR_Import:
			return p12Import(argv[2], argv[3], pwd, usePassKey, kcPwd);
		case PR_Export:
			return p12Export(argv[2], argv[3], pwd, usePassKey, kcPwd, noPrompt);
		default:
			break;
	}
	
	/* all other ops: read infile */
	inFile = argv[2];	
	CSSM_DATA rawBlob;
	unsigned len;
	if(readFile(inFile, &rawBlob.Data, &len)) {
		printf("***Error reading %s. Aborting.\n", inFile);
		exit(1);
	}
	rawBlob.Length = len;
	
	CSSM_CSP_HANDLE cspHand = cuCspStartup(CSSM_TRUE);
	int rtn = 0;
	switch(op) {
		case PR_Decode:
			rtn = p12Decode(rawBlob, cspHand, pwd, usePassKey, verbose, loops);
			break;
		case PR_Reencode:
			rtn = p12Reencode(rawBlob, cspHand, pwd, verbose, loops);
			break;
		case PR_Parse:
			rtn = p12ParseTop(rawBlob, cspHand, pwd, verbose);
			break;
		default:
			/* NOT REACHED */
			printf("GAK!\n");
			rtn = -1;
			break;
	}
	return rtn;
}
