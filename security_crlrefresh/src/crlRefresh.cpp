/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */

/* 
 * crlrefresh: command line access to ocspd's CRL cache refresh mechanism
 */

#include <stdlib.h>
#include <stdio.h>
#include <security_ocspd/ocspdClient.h>
#include <security_utilities/alloc.h>
#include <security_cdsa_utils/cuTimeStr.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <Security/cssmtype.h>
#include <Security/cssmapple.h>

#define DEFAULT_STALE_DAYS				10
#define DEFAULT_EXPIRE_OVERLAP_SECONDS	3600

#ifdef	NDEBUG
#define DEBUG_PRINT		0
#else
#define DEBUG_PRINT		1
#endif

#if		DEBUG_PRINT
#define dprintf(args...)	fprintf(stderr, args)
#else
#define dprintf(args...)
#endif

static void usage(char **argv)
{
	printf("Usage\n");
	printf("Refresh    : %s r [options]\n", argv[0]);
	printf("Fetch CRL  : %s f URI [options]\n", argv[0]);
	printf("Fetch cert : %s F URI [options]\n", argv[0]);
	printf("Refresh options:\n");
	printf("   s=stale_period in DAYS; default=%d\n", DEFAULT_STALE_DAYS);
	printf("   o=expire_overlap in SECONDS; default=%d\n",
					DEFAULT_EXPIRE_OVERLAP_SECONDS);
	printf("   p (Purge all entries, ensuring refresh with fresh CRLs)\n");
	printf("   f (Full crypto CRL verification)\n");
	printf("Fetch options:\n");
	printf("   F=outFileName (default is stdout)\n");
	printf("   n (no write to cache after fetch)\n");
	exit(1);
}

/*
 * Fetch a CRL or Cert from net; write it to a file.
 */
int fetchItemFromNet(
	bool fetchCrl,
	const char *URI,
	char *outFileName,		// NULL indicates write to stdout
	bool writeToCache)
{
	const CSSM_DATA uriData = {strlen(URI) + 1, (uint8 *)URI};
	CSSM_DATA item;
	CSSM_RETURN crtn;
	int irtn;
	Allocator &alloc = Allocator::standard();
	char *op = "";
	
	dprintf("fetchItemFromNet %s outFile %s\n", 
		URI, outFileName ? outFileName : "stdout");
	
	if(fetchCrl) {
		char *cssmTime = cuTimeAtNowPlus(0, TIME_CSSM);
		op = "ocspdCRLFetch";
		crtn = ocspdCRLFetch(alloc, uriData,
			true,		// cacheRead
			writeToCache,
			cssmTime,
			item);
		APP_FREE(cssmTime);
	}
	else {
		op = "ocspdCertFetch";
		crtn = ocspdCertFetch(alloc, uriData, item);
	}
	
	if(crtn) {
		cssmPerror(op, crtn);
		return 1;
	}
	dprintf("fetchItemFromNet %s complete, %lu bytes read\n",
		op, item.Length);
	if(outFileName == NULL) {
		irtn = write(STDOUT_FILENO, item.Data, item.Length);
		if(irtn != (int)item.Length) {
			irtn = errno;
			perror("write");
		}
		else {
			irtn = 0;
		}
	}
	else {
		irtn = writeFile(outFileName, item.Data, item.Length);
		if(irtn) {
			perror(outFileName);
		}
	}
	alloc.free(item.Data);
	dprintf("fetchItemFromNet returning %d\n", irtn);
	return irtn;
}

int main(int argc, char **argv)
{
	CSSM_RETURN crtn;
	char		*argp;
	int			arg;
	int 		optArg = 1;
	
	/* user-specified variables */
	bool		verbose = false;
	bool		purgeAll = false;
	bool		fullCryptoValidation = false;
	int 		staleDays = DEFAULT_STALE_DAYS;
	int			expireOverlapSeconds = DEFAULT_EXPIRE_OVERLAP_SECONDS;

	/* fetch options */
	bool		fetchCrl = true;
	char		*outFileName = NULL;
	bool		writeToCache = true;
	char		*uri = NULL;
	
	if(argc < 2) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'F':
			fetchCrl = false;
			/* and drop thru */
		case 'f':
			if(argc < 3) {
				usage(argv);
			}
			uri = argv[2];
			optArg = 3;
			break;
		case 'r':
			optArg = 2;
			break;
		default:
			usage(argv);
	}
	/* refresh options */
	for(arg=optArg; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 's':
				if(argp[1] != '=') {
					usage(argv);
				}
				staleDays = atoi(&argp[2]);
				break;
			case 'o':
				if(argp[1] != '=') {
					usage(argv);
				}
				expireOverlapSeconds = atoi(&argp[2]);
				break;
			case 'p':
				purgeAll = true;
				break;
			case 'f':
				fullCryptoValidation = true;
				break;
			case 'k':
				/* keychain argument no longer used but we'll allow/ignore it */
				fprintf(stderr, "Warning: keychain specification no longer used\n");
				break;
			case 'n':
				writeToCache = false;
				break;
			case 'F':
				if(argp[1] != '=') {
					usage(argv);
				}
				outFileName = &argp[2];
				break;
			case 'v':
				verbose = true;
				break;
			default:
				usage(argv);
		}
	}
	if(argv[1][0] != 'r') {
		return fetchItemFromNet(fetchCrl, uri, outFileName, writeToCache);
	}

	dprintf("...staleDays %d  expireOverlapSeconds %d\n",
		staleDays, expireOverlapSeconds);
		
	crtn = ocspdCRLRefresh(staleDays, expireOverlapSeconds, purgeAll, 
		fullCryptoValidation);
	if(crtn) {
		cssmPerror("ocspdCRLRefresh", crtn);
		return -1;
	}
	return 0;
}
