/*
 * vfyCert.c - simple "verify one cert with another"
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
	printf("Usage: %s rootCertFile [subjCertFile]\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	CSSM_DATA rootCert;
	CSSM_DATA subjCert;
	int rtn;
	CSSM_CL_HANDLE clHand;
	CSSM_RETURN crtn;
	char *subjName;
	unsigned len;
	
	if((argc < 2) || (argc > 3)) {
		usage(argv);
	}
	rtn = readFile(argv[1], &rootCert.Data, &len);
	if(rtn) {
		printf("Error reading %s; %s\n", argv[1], strerror(rtn));
		exit(1);
	}
	rootCert.Length = len;
	
	if(argc == 2) {
		subjName = argv[1];		// vfy a root cert
	}
	else {
		subjName = argv[2];
	}
	rtn = readFile(subjName, &subjCert.Data, (unsigned *)&subjCert.Length);
	if(rtn) {
		printf("Error reading %s; %s\n", argv[1], strerror(rtn));
		exit(1);
	}
	clHand = clStartup();
	if(clHand == CSSM_INVALID_HANDLE) {
		return 1;
	}
	crtn = CSSM_CL_CertVerify(
		clHand,
		CSSM_INVALID_HANDLE,		// CCHandle 
		&subjCert,
		&rootCert,
		NULL,						// VerifyScope
		0);							// ScopeSize
	if(crtn) {
		printError("CSSM_CL_CertVerify", crtn);
	}
	else {
		printf("cert %s verifies OK\n", subjName);
	}
	return 0;
}

