/*
 * certInCrl.c - simple "see if cert is in CRL"
 */
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>
#include <clAppUtils/clutils.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Security/cssm.h>

static void usage(char **argv)
{
	printf("Usage: %s certFile crlFile [l=loops]\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	CSSM_DATA cert;
	CSSM_DATA crl;
	int rtn;
	CSSM_CL_HANDLE clHand;
	CSSM_RETURN crtn;
	int loops = 1;
	int loop;
	int arg;
	
	if(argc < 3) {
		usage(argv);
	}
	for(arg=3; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'l':
				loops = atoi(&argv[arg][2]);
				break;
			default:
				usage(argv);
		}
	}
	unsigned len;
	rtn = readFile(argv[1], &cert.Data, &len);
	if(rtn) {
		printf("Error reading %s; %s\n", argv[1], strerror(rtn));
		exit(1);
	}
	cert.Length = len;
	rtn = readFile(argv[2], &crl.Data, &len);
	if(rtn) {
		printf("Error reading %s; %s\n", argv[1], strerror(rtn));
		exit(1);
	}
	crl.Length = len;

	clHand = clStartup();
	if(clHand == CSSM_INVALID_HANDLE) {
		return 1;
	}
	CSSM_BOOL found;
	for(loop=0; loop<loops; loop++) {
		crtn = CSSM_CL_IsCertInCrl(
			clHand,
			&cert,
			&crl,
			&found);
		if(crtn) {
			printError("CSSM_CL_IsCertInCrl", crtn);
			goto abort;
		}
		if(found) {
			printf("CertFound TRUE\n");
		}
		else {
			printf("CertFound FALSE\n");
		}
		if(loops != 1) {
			fpurge(stdin);
			printf("CR to continue, q to quit: ");
			char c = getchar();
			if(c == 'q') {
				break;
			}
		}
	}
abort:
	free(cert.Data);
	free(crl.Data);
	return 0;
}

