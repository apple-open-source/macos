/*
 * pemtool - convert between DER and PEM format
 */
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <security_cdsa_utils/cuPem.h>		/* private from CdsaUtils */
#include <security_cdsa_utils/cuEnc64.h>	/* private from CdsaUtils */

static void usage (char **argv)
{
	printf("Usage:\n");
	printf("  %s e infile outfile header_string [q(uiet)]  -- to PEM encode\n", 
		argv[0]);
	printf("  %s d infile outfile [q(uiet)]                -- to PEM decode\n", 
		argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	char *outFileName;
	unsigned char *inFile = NULL;
	unsigned inFileLen;
	unsigned char *outFile = NULL;
	unsigned outFileLen;
	char encFlag = 0;
	int arg;
	int rtn;
	int quiet = 0;
	int optarg = 0;
	
	if(argc < 4) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'e':
			encFlag = 1;
			if(argc < 5) {
				usage(argv);
			}
			optarg = 5;
			break;
		case 'd':
			encFlag = 0;
			if(argc < 4) {
				usage(argv);
			}
			optarg = 4;
			break;
		default:
			usage(argv);
	}
	for(arg=optarg; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'q':
				quiet = 1;
				break;
			default:
				usage(argv);
		}
	}
	if(readFile(argv[2], &inFile, &inFileLen)) {
		printf("***Error reading %s; aborting.\n", argv[2]);
		exit(1);
	}
	outFileName = argv[3];
	if(encFlag) {
		rtn = pemEncode(inFile, inFileLen, &outFile, &outFileLen,
			argv[4]);
	}
	else {
		if(isPem(inFile, inFileLen)) {
			rtn = pemDecode(inFile, inFileLen, &outFile, &outFileLen);
		}
		else {
			/* Maybe it's just base64, i.e., PEM without the header */
			outFile = cuDec64(inFile, inFileLen, &outFileLen);
			if(outFile == NULL) {
				rtn = 1;
				printf("***Error on base64 decode\n");
			}
			else {
				rtn = 0;
			}
		}
	}
	if(rtn == 0) {
		rtn = writeFile(outFileName, outFile, outFileLen);
		if(rtn) {
				printf("***Error writing to %s\n", outFileName);
		}
		else if(!quiet) {
			printf("...wrote %u bytes to %s.\n", outFileLen, outFileName);
		}
	}
	else {
		printf("***Error processing %s.\n", argv[2]);
	}
	if(inFile) {
		free(inFile);
	}
	if(outFile) {
		free(outFile);
	}
	return rtn;
}
