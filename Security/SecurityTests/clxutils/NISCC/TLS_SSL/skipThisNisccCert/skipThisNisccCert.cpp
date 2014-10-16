/*
 * skipThisNisccCert.cpp - decide whether to use specified NISCC cert
 * in SSL client tests. 
 */
#include <Security/cuFileIo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>

/*
 * Currently, SecureTransport does not fragment protocol messages
 * into record-size chunks. Max record size is 16K so our max cert
 * size is a little less than that. 
 */
#define MAX_CERT_SIZE	(16 * 1024)

static void usage(char **argv)
{
	printf("usage: %s file\n", argv[0]);
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
	/* the certs we know seem to be fine */
	#if 0
	/* handled OK by the client now */
	"00000023",
	"00000098",
	"00000116",
	"00000117",
	#endif
	/* certs with undiagnosed problems */
	NULL
};

/* returns true if specified fileName is in skipTheseFiles[] */
static bool shouldWeSkip(
	const char *fullPath)		// C string
{
	/* strip off leading path components */
	const char *lastSlash = NULL;
	const char *cp;
	for(cp=fullPath; *cp!=NULL; cp++) {
		if(*cp == '/') {
			lastSlash = cp;
		}
	}
	if(lastSlash == NULL) {
		/* no slashes, use full caller-specified filename */
		cp = fullPath;
	}
	else {
		/* start one char after last '/' */
		cp++;
	}
	char fileName[MAXPATHLEN];
	strcpy(fileName, cp);
	
	for(const char **stf=skipTheseFiles; *stf!=NULL; stf++) { 
		const char *tf = *stf;
		if(!strcmp(fileName, *stf)) {
			return true;
		}
	}
	return false;
}

int main(int argc, char **argv)
{
	if(argc != 2 ) {
		usage(argv);
	}
	
	/* in hard-coded list of files to skip? */
	const char *filename = argv[1];
	if(shouldWeSkip(filename)) {
		exit(1);
	}
	
	/* file size too big? */
	struct stat	sb;
	if(stat(filename, &sb)) {
		perror(filename);
		exit(2);
	}
	if(sb.st_size > MAX_CERT_SIZE) {
		exit(1);
	}
	
	exit(0);
}
	
