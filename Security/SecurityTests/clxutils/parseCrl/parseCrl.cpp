/*
 * parseCrl.cpp - CL-based cert parser.
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
	printf("Usage: %s crlFile [l=loops]\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	unsigned char *rawCrl = NULL;
	unsigned rawCrlSize;
	int rtn;
	int loops = 1;
	int loop;
	CSSM_BOOL verbose = CSSM_FALSE;
	
	if(argc < 2) {
		usage(argv);
	}

	for(int arg=2; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'l':
				loops = atoi(&argv[arg][2]);
				break;
			case 'v':
				verbose = CSSM_TRUE;
				break;
			default:
				usage(argv);
		}
	}

	rtn = readFile(argv[1], &rawCrl, &rawCrlSize);
	if(rtn) {
		printf("Error reading %s; %s\n", argv[1], strerror(rtn));
		exit(1);
	}

	/* optional loop for malloc debug */
	for(loop=0; loop<loops; loop++) {
		printCrl(rawCrl, rawCrlSize, verbose);
		if(loops != 1) {
			fpurge(stdin);
			printf("CR to continue, q to quit: ");
			char c = getchar();
			if(c == 'q') {
				break;
			}
		}
	}
	return 0;
}

