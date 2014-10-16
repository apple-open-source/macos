/*
 * parseCert.cpp - CL-based cert parser.
 *
 * See oidParser.h for info on config file. 
 */
#include <security_cdsa_utils/cuPrintCert.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void usage(char **argv)
{
	printf("Usage: %s certFile [v(erbose) [l(oop)]\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	unsigned char *rawCert = NULL;
	unsigned rawCertSize;
	int rtn;
	CSSM_BOOL verbose = CSSM_FALSE;
	int arg;
	int loop = 0;
	
	if(argc < 2) {
		usage(argv);
	}
	for(arg=2; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'v':
				verbose = CSSM_TRUE;
				break;
			case 'l':
				loop = 1;
				break;
			default:
				usage(argv);
		}
	}
	rtn = readFile(argv[1], &rawCert, &rawCertSize);
	if(rtn) {
		printf("Error reading %s; %s\n", argv[1], strerror(rtn));
		exit(1);
	}
	do {
		printCert(rawCert, rawCertSize, verbose);
		if(loop) {
			printf("Enter q to quit, anything else to continue: ");
			fflush(stdout);
			char c = getchar();
			if(c == 'q') {
				break;
			}
		}
	} while(loop);
	return 0;
}

