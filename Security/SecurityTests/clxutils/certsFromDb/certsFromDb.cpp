/*
 * certsFromDb.cpp - extract all certs from a DB, write to files or parse to stdout.
 */
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>	
#include <security_cdsa_utils/cuPrintCert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Security/cssm.h>
#include <string.h>

static void usage(char **argv)
{
	printf("Usage: \n");
	printf("   %s keychainFile f certFileBase [option...]\n", argv[0]);
	printf("   %s keychainFile p(arse) [option...]\n", argv[0]);
	printf("Options:\n");
	printf("   R   fetch CRLs, not certs\n");
	printf("   P   pause for MallocDebug one each item\n");
	printf("   q   Quiet\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int 						rtn;
	CSSM_DL_DB_HANDLE			dlDbHand;
	CSSM_RETURN 				crtn;
	char 						filePath[300];
	unsigned 					certNum=0;
	CSSM_QUERY					query;
	CSSM_DB_UNIQUE_RECORD_PTR	record = NULL;
	CSSM_HANDLE 				resultHand;
	CSSM_DATA					theData = {0, NULL};
	char						*fileBase = NULL;
	CSSM_BOOL					doPause = CSSM_FALSE;
    CSSM_BOOL					isCrl = CSSM_FALSE;
	CSSM_BOOL					quiet = CSSM_FALSE;
	int							optarg = 3;
	
	if(argc < 3) {
		usage(argv);
	}
	switch(argv[2][0]) {
		case 'f':
			if(argc < 4) {
				usage(argv);
			}
			fileBase = argv[3];
			optarg = 4;
			break;
		case 'p':
			/* default, parse mode */
			break;
		default:
			usage(argv);
	}
	for(int arg=optarg; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'q':
				quiet = CSSM_TRUE;
				break;
			case 'P':
				doPause = CSSM_TRUE;
				break;
			case 'R':
				isCrl = CSSM_TRUE;
				break;
			default:
				usage(argv);
		}
	}

	/* attach to specified keychain as a DL/DB */
	dlDbHand.DLHandle = dlStartup();
	if(dlDbHand.DLHandle == 0) {
		exit(1);
	}
	crtn = dbCreateOpen(dlDbHand.DLHandle, argv[1], 
		CSSM_FALSE, 		// doCreate
		CSSM_FALSE,			// deleteExist
		NULL,				// pwd
		&dlDbHand.DBHandle);
	if(crtn) {
		exit(1);
	}

loopTop:
	/* search by record type, no predicates, no returned attributes. We just want 
	 * the data. */
	query.RecordType = isCrl ? CSSM_DL_DB_RECORD_X509_CRL : 
			CSSM_DL_DB_RECORD_X509_CERTIFICATE;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 0;
	query.SelectionPredicate = NULL;	
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = CSSM_QUERY_RETURN_DATA;	// FIXME - used?

	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		&resultHand,
		NULL,
		&theData,
		&record);
	if(crtn) {
		printError("CSSM_DL_DataGetFirst", crtn);
		printf("Error fetching certs from %s. Aborting.\n", argv[1]);
		exit(1);
	}
	CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	
    if(doPause) {
        fpurge(stdin);
        printf("set up MallocDebug, then any key to continue: ");
		getchar();
    }
	if(fileBase) {
		/* write the data */
		sprintf(filePath, "%s_%d", fileBase, certNum);
		rtn = writeFile(filePath, theData.Data, theData.Length);
		if(rtn == 0) {
			if(!quiet) {
				printf("...wrote %u bytes to %s\n", (unsigned)theData.Length,
					filePath);
			}
		}
		else {
			printf("***Error writing %s: %s\n", filePath, strerror(rtn));
			exit(1);
		}
	}
	else {
		if(isCrl) {
			printf("CRL 0:\n");
			printCrl(theData.Data, theData.Length, CSSM_FALSE);
		}
		else {
			printf("Cert 0:\n");
			printCert(theData.Data, theData.Length, CSSM_FALSE);
		}
	}
	CSSM_FREE(theData.Data);
	certNum++;
	
	/* again */
	for(;;) {
		crtn = CSSM_DL_DataGetNext(dlDbHand,
			resultHand, 
			NULL,
			&theData,
			&record);
		switch(crtn) {
			case CSSM_OK:
				if(fileBase) {
					sprintf(filePath, "%s_%d", fileBase, certNum);
					rtn = writeFile(filePath, theData.Data, theData.Length);
					if(rtn == 0) {
						if(!quiet) {
							printf("...wrote %u bytes to %s\n", (unsigned)theData.Length,
								filePath);
						}
					}
					else {
						printf("***Error writing %s: %s\n", filePath, strerror(rtn));
						exit(1);
					}
				}
				else {
					if(isCrl) {
						printf("CRL 0:\n");
						printCrl(theData.Data, theData.Length, CSSM_FALSE);
					}
					else {
						printf("Cert %u:\n", certNum);
						printCert(theData.Data, theData.Length, CSSM_FALSE);
					}
				}
				certNum++;
				CSSM_FREE(theData.Data);
				CSSM_DL_FreeUniqueRecord(dlDbHand, record);
				break;		// and go again 
			case CSSMERR_DL_ENDOFDATA:
				/* normal termination */
				break;
			default:
				printError("DataGetNext", crtn);
				break;
		}
		if(crtn != CSSM_OK) {
			break;
		}
	}
	CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
    if(doPause) {
        fpurge(stdin);
        printf("End of loop, l to loop, enything else to end: ");
		char c = getchar();
        if(c == 'l') {
            goto loopTop;
        }
    }
	if(!quiet) {
		printf("...%d %s extracted.\n", certNum, isCrl ? "CRLs" : "certs");
	}
	return 0;
}

