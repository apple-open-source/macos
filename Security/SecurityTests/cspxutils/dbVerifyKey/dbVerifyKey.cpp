/* Copyright (c) 2004-2005,2008 Apple Inc.
 *
 * dbVerifyKey.cpp - verify that specified DB has exactly one key of specified
 * algorithm, class, and key size - and no other keys.
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <strings.h>
#include <ctype.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include "common.h"
#include "cspdlTesting.h"


static void usage(char **argv)
{
	printf("usage: %s dbFileName alg class keysize [options]\n", argv[0]);
	printf("   alg   : rsa|dsa|dh|ecdsa\n");
	printf("   class : priv|pub\n");
	printf("Options:\n");
	printf("   -q   quiet\n");
	exit(1);
}

static const char *recordTypeStr(
	CSSM_DB_RECORDTYPE		recordType)
{
	static char unk[100];
	
	switch(recordType) {
		case CSSM_DL_DB_RECORD_PRIVATE_KEY:
			return "Private Key";
		case CSSM_DL_DB_RECORD_PUBLIC_KEY:
			return "Public Key";
		case CSSM_DL_DB_RECORD_SYMMETRIC_KEY:
			return "Symmetric Key";
		default:
			sprintf(unk, "**Unknown record type %u\n", (unsigned)recordType);
			return unk;
	}
}

/*
 * Search for specified record type; verify there is exactly one or zero
 * of them as specified.
 * Verify key algorthm and key size. Returns nonzero on error. 
 */
static int doVerify(
	CSSM_DL_DB_HANDLE 		dlDbHand,
	unsigned				numRecords,		// zero or one
	CSSM_DB_RECORDTYPE		recordType,
	uint32					keySize,
	CSSM_ALGORITHMS			keyAlg)
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE						resultHand;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	
	/* no predicates, all records of specified type, no attrs, get the key */
	query.RecordType = recordType;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 0;	
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = 0; // CSSM_QUERY_RETURN_DATA;	// FIXME - used?
	
	recordAttrs.DataRecordType		 = recordType;
	recordAttrs.NumberOfAttributes   = 0;
	recordAttrs.AttributeData        = NULL;

	CSSM_DATA recordData = {0, NULL};

	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		&resultHand,
		&recordAttrs,
		&recordData,	
		&record);
	switch(crtn) {
		case CSSM_OK:
			if(numRecords == 0) {
				printf("***Expected zero records of type %s, found one\n",
					recordTypeStr(recordType));
				CSSM_DL_FreeUniqueRecord(dlDbHand, record);
				CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
				return 1;
			}
			break;		// proceed
		case CSSMERR_DL_ENDOFDATA:
			if(numRecords == 0) {
				/* cool */
				return 0;
			}
			printf("**Error: no records of type %s found\n",
				recordTypeStr(recordType));
			return 1;
		default:
			printError("DataGetFirst", crtn);
			return 1;
	}

	CSSM_KEY_PTR theKey = (CSSM_KEY_PTR)recordData.Data;
	int ourRtn = 0;
	CSSM_KEYHEADER &hdr = theKey->KeyHeader;
	if(hdr.AlgorithmId != keyAlg) {
		printf("***Algorithm mismatch: expect %u, got %u\n",
			(unsigned)keyAlg, (unsigned)hdr.AlgorithmId);
		ourRtn++;
	}
	if(hdr.LogicalKeySizeInBits != keySize) {
		printf("***Key Size: expect %u, got %u\n",
			(unsigned)keySize, (unsigned)hdr.LogicalKeySizeInBits);
		ourRtn++;
	}
	CSSM_DL_FreeUniqueRecord(dlDbHand, record);

	/* see if there are any more */
	crtn = CSSM_DL_DataGetNext(dlDbHand,
		resultHand, 
		&recordAttrs,
		NULL,
		&record);
	if(crtn == CSSM_OK) {
		printf("***More than 1 record of type %s found\n", 
			recordTypeStr(recordType));
		ourRtn++;
		CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	}
	CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
	return ourRtn;
}

int main(
	int argc, 
	char **argv)
{
	int					arg;
	char				*argp;
	char				*dbFileName;
	CSSM_ALGORITHMS		keyAlg;
	CSSM_DB_RECORDTYPE	recordType;
	uint32				keySize;
	CSSM_DL_DB_HANDLE	dlDbHand;
	CSSM_BOOL			quiet = CSSM_FALSE;
	CSSM_RETURN 		crtn = CSSM_OK;
	
	if(argc < 5) {
		usage(argv);
	}
	dbFileName = argv[1];
	
	/* key algorithm */
	if(!strcmp(argv[2], "rsa")) {
		keyAlg = CSSM_ALGID_RSA;
	}
	else if(!strcmp(argv[2], "dsa")) {
		keyAlg = CSSM_ALGID_DSA;
	}
	else if(!strcmp(argv[2], "dh")) {
		keyAlg = CSSM_ALGID_DH;
	}
	else if(!strcmp(argv[2], "ecdsa")) {
		keyAlg = CSSM_ALGID_ECDSA;
	}
	else {
		usage(argv);
	}
	
	/* key class */
	if(!strcmp(argv[3], "priv")) {
		recordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
	}
	else if(!strcmp(argv[3], "pub")) {
		recordType = CSSM_DL_DB_RECORD_PUBLIC_KEY;
	}
	else {
		usage(argv);
	}

	keySize = atoi(argv[4]);
	
	for(arg=5; arg<argc; arg++) {
		argp = argv[arg];
		if(!strcmp(argp, "-q")) {
			quiet = CSSM_TRUE;
		}
		else {
			usage(argv);
		}
	}
	
	dlDbHand.DLHandle = dlStartup();
	if(dlDbHand.DLHandle == 0) {
		exit(1);
	}
	crtn = dbCreateOpen(dlDbHand.DLHandle, dbFileName, 
		CSSM_FALSE, CSSM_FALSE, NULL, &dlDbHand.DBHandle);
	if(crtn) {
		exit(1);
	}
	
	if(doVerify(dlDbHand, 1, recordType, keySize, keyAlg)) {
		return 1;
	}
	if(doVerify(dlDbHand, 0, 
			(recordType == CSSM_DL_DB_RECORD_PRIVATE_KEY) ?
				CSSM_DL_DB_RECORD_PUBLIC_KEY : CSSM_DL_DB_RECORD_PRIVATE_KEY,
			keySize, keyAlg)) {
		return 1;
	}
	if(!quiet) {
		printf("...%s verify succussful\n", recordTypeStr(recordType));
	}
	CSSM_DL_DbClose(dlDbHand);
	CSSM_ModuleDetach(dlDbHand.DLHandle);
	return 0;
}
