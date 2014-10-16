/*
 * Attempt to decode either one file, or every file in cwd,
 * as a cert. Used to study vulnerability to NISCC cert DOS attacks. 
 */
#include <Security/SecAsn1Coder.h>
#include <Security/X509Templates.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

static void usage(char **argv)
{
	printf("usage: %s [-l(oop))] [certFile]\n", argv[0]);
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
	/* the certs we know crash */
	#if 0
	"00000668",
	"00000681",
	"00001980",
	"00002040",
	"00002892",
	"00007472",
	"00008064",
	"00008656",
	"00009840",
	"00010432",
	"00011614",	// trouble somewhere in this neighborhood
	"00011615",
	"00011616",
	#endif
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
 * Just try to decode - if SecAsn1Decode returns, good 'nuff.
 * Returns true if it does (i.e. ignore decode error; we're trying
 * to detect a crash when the decoder should return an error).
 */
bool decodeCert(
	const void *certData, 
	size_t certDataLen)
{
	SecAsn1CoderRef coder = NULL;
	NSS_Certificate nssCert;
	NSS_SignedCertOrCRL certOrCrl;
	
	SecAsn1CoderCreate(&coder);
	
	/* first the full decode */
	memset(&nssCert, 0, sizeof(nssCert));
	SecAsn1Decode(coder, certData, certDataLen,	kSecAsn1SignedCertTemplate, &nssCert);

	/* now the "just TBS and sig" decode - this is actually harder
	 * due to nested SEC_ASN1_SAVE ops */
	memset(&certOrCrl, 0, sizeof(NSS_SignedCertOrCRL));
	SecAsn1Decode(coder, certData, certDataLen,	kSecAsn1SignedCertOrCRLTemplate, &certOrCrl);

	SecAsn1CoderRelease(coder);
	return true;
}

int main(int argc, char **argv)
{
	bool quiet = false;
	unsigned char *certData;
	unsigned certDataLen;
	bool loop = false;
	int filearg = 1;
	
	if(argc > 3 ) {
		usage(argv);
	}
	if((argc > 1) && (argv[1][0] == '-')) {
		switch(argv[1][1]) {
			case 'l':
				loop = true;
				break;
			default:
				usage(argv);
		}
		filearg++;
		argc--;
	}
	if(argc == 2) {
		/* read & parse one file */
		char *oneFile = argv[filearg];
		if(readFile(oneFile, &certData, &certDataLen)) {
			printf("\n***Error reading file %s. Aborting.\n", oneFile);
			exit(1);
		}
		do {
			if(!quiet) {
				printf("...%s", oneFile);
				fflush(stdout);
			}
			if(!decodeCert(certData, certDataLen)) {
				printf("\n***GOT AN EXCEPTION ON %s\n", oneFile);
				exit(1);
			}
		} while(loop);
		free(certData);
		exit(0);
	}
	DIR *dir = opendir(".");
	if(dir == NULL) {
		printf("Huh? Can't open . as a directory.\n");
		exit(1);
	}
	struct dirent *de = readdir(dir);
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
			if(!decodeCert(certData, certDataLen)) {
				printf("\n***GOT AN EXCEPTION ON %s\n", filename);
				exit(1);
			}
			free(certData);
		}
		de = readdir(dir);
	}	
	closedir(dir);
	printf("\ncertDecode did not crash.\n");
	return 0;
}
	
