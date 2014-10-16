/*
 * keyFromCert.cpp - extract public key from a cert.
 */
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <clAppUtils/clutils.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Security/cssm.h>
#include <Security/x509defs.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <string.h>

static void usage(char **argv)
{
	printf("Usage: %s [-q] certFile keyFile\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	CSSM_DATA rawCert;
	CSSM_KEY_PTR pubKey;
	int rtn;
	CSSM_CL_HANDLE	clHand;			// CL handle
	CSSM_RETURN crtn;
	bool quiet = false;
	
	const char *certFile = NULL;
	const char *keyFile = NULL;
	
	switch(argc) {
		case 3:
			certFile = argv[1];
			keyFile = argv[2];
			break;
		case 4:
			if(!strcmp(argv[1], "-q")) {
				quiet = true;
				certFile = argv[2];
				keyFile = argv[3];
			}
			else {
				usage(argv);
			}
			break;
		default:
			usage(argv);
	}
	
	unsigned len;
	rtn = readFile(certFile, &rawCert.Data, &len);
	if(rtn) {
		printf("Error reading %s; %s\n", certFile, strerror(rtn));
		exit(1);
	}
	rawCert.Length = len;
	clHand = clStartup();
	if(clHand == 0) {
		return 0;
	}
	crtn = CSSM_CL_CertGetKeyInfo(clHand, &rawCert, &pubKey);
	if(crtn) {
		printError("CSSM_CL_CertGetKeyInfo", crtn);
		exit(1);
	}
	rtn = writeFile(keyFile, pubKey->KeyData.Data, pubKey->KeyData.Length);
	if(!quiet & (rtn == 0)) {
		printf("...wrote %u key bytes to %s\n", (unsigned)pubKey->KeyData.Length,
			keyFile);
	}
	return 0;
}

