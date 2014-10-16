/*
 * standalone pkcs12 parser.
 */
#include <security_cdsa_utils/cuFileIo.h>
#include <stdlib.h>
#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include "p12Parse.h"
#include <security_cdsa_utils/cuCdsaUtils.h>

static void usage(char **argv)
{
	printf("Usage: %s infile password [v(erbose)\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{	
	char *inFile;
	CFStringRef pwd = NULL;
	bool verbose = false;
	
	if(argc < 3) {
		usage(argv);
	}
	for(int arg=3; arg<argc; arg++) {
		char *argp = argv[arg];
		switch(argp[0]) {
			case 'v':
				verbose = true;
				break;
			default:
				usage(argv);
		}
	}
	
	inFile = argv[1];	
	CSSM_DATA rawBlob;
	unsigned len;
	if(readFile(inFile, &rawBlob.Data, &len)) {
		printf("***Error reading %s. Aborting.\n", inFile);
		exit(1);
	}
	rawBlob.Length = len;
	pwd = CFStringCreateWithCString(NULL, argv[2], kCFStringEncodingASCII);
	
	CSSM_CSP_HANDLE cspHand = cuCspStartup(CSSM_TRUE);
	int rtn = p12ParseTop(rawBlob, cspHand, pwd, verbose);
	return rtn;
}
