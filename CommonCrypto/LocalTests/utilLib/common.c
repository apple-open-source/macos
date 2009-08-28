/* Copyright 1997 Apple Computer, Inc.
 *
 * common.c - Common CSP test code
 *
 * Revision History
 * ----------------
 *   4 May 2000 Doug Mitchell
 *		Ported to X/CDSA2. 
 *   6 Jul 1998 Doug Mitchell at Apple
 *		Added clStartup().
 *  12 Aug 1997	Doug Mitchell at Apple
 *		Created.
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Security/cssm.h>
#include "common.h"
#include <Security/cssmapple.h>			/* apple, not intel */
#include <time.h>

static CSSM_VERSION vers = {2, 0};
//const static uint32 guidPrefix = 0xFADE;
const CSSM_GUID testGuid = { 0xFADE, 0, 0, { 1,2,3,4,5,6,7,0 }};

/*
 * We can't enable this until all of these are fixed and integrated:
 * 2890978 CSP
 * 2927474 CSPDL
 * 2928357 TP
 */
#define DETECT_MALLOC_ABUSE		1	

#if		DETECT_MALLOC_ABUSE

/* 
 * This set of allocator functions detects when we free something
 * which was mallocd by CDSA or a plugin using something other than
 * our callback malloc/realloc/calloc. With proper runtime support
 * (which is present in Jaguar 6C35), the reverse is also detected
 * by malloc (i.e., we malloc something and CDSA or a plugin frees
 * it).
 */
#define APP_MALLOC_MAGIC		'Util'

void * appMalloc (CSSM_SIZE size, void *allocRef) {
	void *ptr;

	/* scribble magic number in first four bytes */
	ptr = malloc(size + 4);
	*(uint32 *)ptr = APP_MALLOC_MAGIC;
	ptr = (char *)ptr + 4;

	return ptr;
}

void appFree (void *ptr, void *allocRef) {
	if(ptr == NULL) {
		return;
	}
	ptr = (char *)ptr - 4;
	if(*(uint32 *)ptr != APP_MALLOC_MAGIC) {
		printf("ERROR: appFree() freeing a block that we didn't allocate!\n");
		return;		// this free is not safe
	}
	*(uint32 *)ptr = 0;
	free(ptr);
}

/* Realloc - adjust both original pointer and size */
void * appRealloc (void *ptr, CSSM_SIZE size, void *allocRef) {
	if(ptr == NULL) {
		/* no ptr, no existing magic number */
		return appMalloc(size, allocRef);
	}
	ptr = (char *)ptr - 4;
	if(*(uint32 *)ptr != APP_MALLOC_MAGIC) {
		printf("ERROR: appRealloc() on a block that we didn't allocate!\n");
	}
	*(uint32 *)ptr = 0;
	ptr = realloc(ptr, size + 4);
	*(uint32 *)ptr = APP_MALLOC_MAGIC;
	ptr = (char *)ptr + 4;
	return ptr;
}

/* Have to do this manually */
void * appCalloc (uint32 num, CSSM_SIZE size, void *allocRef) {
	uint32 memSize = num * size;
	
	void *ptr = appMalloc(memSize, allocRef);
	memset(ptr, 0, memSize);
	return ptr;
}

#else	/* DETECT_MALLOC_ABUSE */
/*
 * Standard app-level memory functions required by CDSA.
 */
void * appMalloc (CSSM_SIZE size, void *allocRef) {
	return( malloc(size) );
}
void appFree (void *mem_ptr, void *allocRef) {
	free(mem_ptr);
 	return;
}
void * appRealloc (void *ptr, CSSM_SIZE size, void *allocRef) {
	return( realloc( ptr, size ) );
}
void * appCalloc (uint32 num, CSSM_SIZE size, void *allocRef) {
	return( calloc( num, size ) );
}
#endif	/* DETECT_MALLOC_ABUSE */

static CSSM_API_MEMORY_FUNCS memFuncs = {
	appMalloc,
	appFree,
	appRealloc,
 	appCalloc,
 	NULL
 };
 
/*
 * Init CSSM; returns CSSM_FALSE on error. Reusable.
 */
static CSSM_BOOL cssmInitd = CSSM_FALSE;

CSSM_BOOL cssmStartup()
{
	CSSM_RETURN  crtn;
    CSSM_PVC_MODE pvcPolicy = CSSM_PVC_NONE;
	
	if(cssmInitd) {
		return CSSM_TRUE;
	}  
	crtn = CSSM_Init (&vers, 
		CSSM_PRIVILEGE_SCOPE_NONE,
		&testGuid,
		CSSM_KEY_HIERARCHY_NONE,
		&pvcPolicy,
		NULL /* reserved */);
	if(crtn != CSSM_OK) 
	{
		printError("CSSM_Init", crtn);
		return CSSM_FALSE;
	}
	else {
		cssmInitd = CSSM_TRUE;
		return CSSM_TRUE;
	}
}

/*
 * Init CSSM and establish a session with the Apple CSP.
 */
CSSM_CSP_HANDLE cspStartup()
{
	return cspDlDbStartup(CSSM_TRUE, NULL);	
}

/* like cspStartup, but also returns DB handle. If incoming dbHandPtr
 * is NULL, no DB startup. */
CSSM_CSP_HANDLE cspDbStartup(
	CSSM_DB_HANDLE *dbHandPtr)
{
	return cspDlDbStartup(CSSM_TRUE, NULL);	
}

CSSM_CSP_HANDLE cspDlDbStartup(
	CSSM_BOOL bareCsp,			// true ==> CSP, false ==> CSP/DL
	CSSM_DB_HANDLE *dbHandPtr)	// optional - TO BE DELETED
{
	CSSM_CSP_HANDLE cspHand;
	CSSM_RETURN		crtn;
	const CSSM_GUID *guid;
	char *modName;
	
	if(dbHandPtr) {
		*dbHandPtr = 0;
	}
	if(cssmStartup() == CSSM_FALSE) {
		return 0;
	}
	if(bareCsp) {
		guid = &gGuidAppleCSP;
		modName = (char*) "AppleCSP";
	}
	else {
		guid = &gGuidAppleCSPDL;
		modName = (char *) "AppleCSPDL";
	}
	crtn = CSSM_ModuleLoad(guid,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		char outStr[100];
		sprintf(outStr, "CSSM_ModuleLoad(%s)", modName);
		printError(outStr, crtn);
		return 0;
	}
	crtn = CSSM_ModuleAttach (guid,
		&vers,
		&memFuncs,			// memFuncs
		0,					// SubserviceID
		CSSM_SERVICE_CSP,	
		0,					// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,				// FunctionTable
		0,					// NumFuncTable
		NULL,				// reserved
		&cspHand);
	if(crtn) {
		char outStr[100];
		sprintf(outStr, "CSSM_ModuleAttach(%s)", modName);
		printError(outStr, crtn);
		return 0;
	}
	return cspHand;
}

/*
 * Detach and unload from a CSP.
 */
CSSM_RETURN cspShutdown(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_BOOL bareCsp)			// true ==> CSP, false ==> CSP/DL
{
	CSSM_RETURN crtn;
	const CSSM_GUID *guid;
	char *modName;
	
	if(bareCsp) {
		guid = &gGuidAppleCSP;
		modName = (char *) "AppleCSP";
	}
	else {
		guid = &gGuidAppleCSPDL;
		modName = (char *) "AppleCSPDL";
	}
	crtn = CSSM_ModuleDetach(cspHand);
	if(crtn) {
		printf("Error detaching from %s\n", modName);
		printError("CSSM_ModuleDetach", crtn);
		return crtn;
	}
	crtn = CSSM_ModuleUnload(guid, NULL, NULL);
	if(crtn) {
		printf("Error unloading %s\n", modName);
		printError("CSSM_ModuleUnload", crtn);
	}
	return crtn;
}

/* Attach to DL side of CSPDL */
CSSM_DL_HANDLE dlStartup()
{
	CSSM_DL_HANDLE 	dlHand = 0;
	CSSM_RETURN		crtn;
	
	if(cssmStartup() == CSSM_FALSE) {
		return 0;
	}
	crtn = CSSM_ModuleLoad(&gGuidAppleCSPDL,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		printError("CSSM_ModuleLoad(Apple CSPDL)", crtn);
		return 0;
	}
	crtn = CSSM_ModuleAttach (&gGuidAppleCSPDL,
		&vers,
		&memFuncs,			// memFuncs
		0,					// SubserviceID
		CSSM_SERVICE_DL,	
		0,					// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,				// FunctionTable
		0,					// NumFuncTable
		NULL,				// reserved
		&dlHand);
	if(crtn) {
		printError("CSSM_ModuleAttach(Apple CSPDL)", crtn);
		return 0;
	}
	return dlHand;
}

/*
 * Delete a DB.
 */
#define DELETE_WITH_AUTHENT		0
CSSM_RETURN dbDelete(
	CSSM_DL_HANDLE		dlHand,			// from dlStartup()
	const char 			*dbName)
{
	return CSSM_DL_DbDelete(dlHand, dbName, NULL, NULL);
}

/*
 * open a DB, ensure it's empty.
 */
CSSM_DB_HANDLE dbStartup(
	CSSM_DL_HANDLE		dlHand,			// from dlStartup()
	const char 			*dbName)
{
	CSSM_DB_HANDLE dbHand = 0;
	
	CSSM_RETURN crtn = dbCreateOpen(dlHand, dbName, 
		CSSM_TRUE,		// create
		CSSM_TRUE,		// delete
		NULL,			// pwd
		&dbHand);
	if(crtn == CSSM_OK) {
		return dbHand;
	}
	else {
		return 0;
	}
}

#if 0
/*
 * Attach to existing DB or create an empty new one.
 */
CSSM_DB_HANDLE dbStartupByName(CSSM_DL_HANDLE dlHand,
	char 		*dbName,
	CSSM_BOOL 	doCreate)
{
	CSSM_RETURN crtn;
	CSSM_DB_HANDLE				dbHand;
	
	/* try to open existing DB in either case */
	
	crtn = CSSM_DL_DbOpen(dlHand,
		dbName, 
		NULL,			// DbLocation
		CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
		NULL, 			// CSSM_ACCESS_CREDENTIALS *AccessCred
		NULL,			// void *OpenParameters
		&dbHand);
	if(dbHand != 0) {
		return dbHand;
	}
	if(!doCreate) {
		printf("***no such data base (%s)\n", dbName);
		printError("CSSM_DL_DbOpen", crtn);
		return 0;
	}
	/* have to create one */
	return dbStartup(dlHand, dbName);
}
#endif

/*
 * routines which convert various types to untyped byte arrays.
 */
void intToBytes(unsigned i, unsigned char *buf)
{
	*buf++ = (unsigned char)((i >> 24) & 0xff);
	*buf++ = (unsigned char)((i >> 16) & 0xff);
	*buf++ = (unsigned char)((i >> 8)  & 0xff);
	*buf   = (unsigned char)(i & 0xff);
}
void shortToBytes(unsigned short s, unsigned char *buf)
{
	*buf++ = (unsigned char)((s >> 8)  & 0xff);
	*buf   = (unsigned char)(s & 0xff);
}
unsigned bytesToInt(const unsigned char *buf) {
	unsigned result;
	result = (((unsigned)buf[0] << 24) & 0xff000000) |
		(((unsigned)buf[1] << 16) & 0x00ff0000) |
		(((unsigned)buf[2] << 8) & 0xff00) |
		(((unsigned)buf[3]) & 0xff);
	return result;
}
unsigned short bytesToShort(const unsigned char *buf) {
    	unsigned short result;
    	result = (((unsigned short)buf[0] << 8) & 0xff00) |
		 (((unsigned short)buf[1]) & 0xff);
    	return result;
}

/*
 * Given a context specified via a CSSM_CC_HANDLE, add a new
 * CSSM_CONTEXT_ATTRIBUTE to the context as specified by AttributeType,
 * AttributeLength, and an untyped pointer.
 *
 * This is currently used to add a second CSSM_KEY attribute when performing
 * ops with algorithm CSSM_ALGID_FEED and CSSM_ALGID_FEECFILE.
 */
CSSM_RETURN AddContextAttribute(CSSM_CC_HANDLE CCHandle,
	uint32 AttributeType,
	uint32 AttributeLength,
	ContextAttrType attrType,
	/* specify exactly one of these */
	const void *AttributePtr,
	uint32 attributeInt)
{
	CSSM_CONTEXT_ATTRIBUTE		newAttr;	
	CSSM_RETURN					crtn;
	
	newAttr.AttributeType     = AttributeType;
	newAttr.AttributeLength   = AttributeLength;
	if(attrType == CAT_Uint32) {
		newAttr.Attribute.Uint32  = attributeInt;
	}
	else {
		newAttr.Attribute.Data    = (CSSM_DATA_PTR)AttributePtr;
	}
	crtn = CSSM_UpdateContextAttributes(CCHandle, 1, &newAttr);
	if(crtn) {
		printError("CSSM_UpdateContextAttributes", crtn);
	}
	return crtn;
}

/*
 * Set up a CSSM data.
 */
CSSM_RETURN appSetupCssmData(
	CSSM_DATA_PTR	data,
	uint32			numBytes)
{
	if(data == NULL) {
		printf("Hey! appSetupCssmData with NULL Data!\n");
		return CSSMERR_CSSM_INTERNAL_ERROR;
	}
	data->Data = (uint8 *)CSSM_MALLOC(numBytes);
	if(data->Data == NULL) {
		return CSSMERR_CSSM_MEMORY_ERROR;
	}
	data->Length = numBytes;
	return CSSM_OK;
}

/*
 * Free the data referenced by a CSSM data, and optionally, the struct itself.
 */
void appFreeCssmData(CSSM_DATA_PTR data,
	CSSM_BOOL freeStruct)
{
	if(data == NULL) {
		return;
	}
	if(data->Length != 0) {
		CSSM_FREE(data->Data);
	}
	if(freeStruct) {
		CSSM_FREE(data);
	}
	else {
		data->Length = 0;
		data->Data = NULL;
	}
}

/*
 * Copy src to dst, mallocing dst.
 */
CSSM_RETURN appCopyCssmData(const CSSM_DATA *src, 
	CSSM_DATA_PTR dst)
{
	return appCopyData(src->Data, src->Length, dst);
}

/* copy raw data to a CSSM_DATA, mallocing dst. */
CSSM_RETURN  appCopyData(const void *src, 
	uint32 len,
	CSSM_DATA_PTR dst)
{
	dst->Length = 0;
	if(len == 0) {
		dst->Data = NULL;
		return CSSM_OK;
	}
	dst->Data = (uint8 *)CSSM_MALLOC(len);
	if(dst->Data == NULL) {
		return CSSM_ERRCODE_MEMORY_ERROR;
	}
	dst->Length = len;
	memcpy(dst->Data, src, len);
	return CSSM_OK;
}

CSSM_BOOL appCompareCssmData(const CSSM_DATA *d1,
	const CSSM_DATA *d2)
{	
	if(d1->Length != d2->Length) {
		return CSSM_FALSE;
	}
	if(memcmp(d1->Data, d2->Data, d1->Length)) {
		return CSSM_FALSE;
	}
	return CSSM_TRUE;	
}

/* min <= return <= max */
unsigned genRand(unsigned min, unsigned max)
{
	unsigned i;
	if(min == max) {
		return min;
	}
	appGetRandomBytes(&i, 4);
	return (min + (i % (max - min + 1)));
}	

void simpleGenData(CSSM_DATA_PTR dbuf, unsigned minBufSize, unsigned maxBufSize)
{
	unsigned len = genRand(minBufSize, maxBufSize);
	appGetRandomBytes(dbuf->Data, len);
	dbuf->Length = len;
}

#define MIN_OFFSET	0
#define MAX_OFFSET	99
#define MIN_ASCII	'a'
#define MAX_ASCII	'z'

/*
 * Calculate random data size, fill dataPool with that many random bytes.
 *
 * (10**minExp + MIN_OFFSET) <= size <= (10**maxExp + MAX_OFFSET)
 */
unsigned genData(unsigned char *dataPool,
	unsigned minExp,
	unsigned maxExp,
	dataType type)
{
	int 		exp;
	int 		offset;
	int 		size;
	char 		*cp;
	int 		i;
	char		ac;
	
	/*
	 * Calculate "random" size : (10 ** (random exponent)) + random offset
	 */
	exp = genRand(minExp, maxExp);
	offset = genRand(MIN_OFFSET, MAX_OFFSET);
	size = 1;
	while(exp--) {			// size = 10 ** exp
		size *= 10;
	}
	size += offset;
	switch(type) {
	    case DT_Zero:
			bzero(dataPool, size);
			break;
	    case DT_Increment:
			{
				int i;
				for(i=0; i<size; i++) {
					dataPool[i] = i;
				}
			}
			break;
	    case DT_ASCII:
	    	ac = MIN_ASCII;
			cp = (char *)dataPool;
	    	for(i=0; i<size; i++) {
				*cp++ = ac++;
				if(ac > MAX_ASCII) {
					ac = MIN_ASCII;
				}
			}
			break;
	    case DT_Random: 
			appGetRandomBytes(dataPool, size);
			break;
	}
	return size;
}

void dumpBuffer(
	const char *bufName,	// optional
	unsigned char *buf,
	unsigned len)
{
	unsigned i;
	
	if(bufName) {
		printf("%s\n", bufName);
	}
	printf("   ");
	for(i=0; i<len; i++) {
		printf("%02X ", buf[i]);
		if((i % 24) == 23) {
			printf("\n   ");
		}
	}
	printf("\n");
}

int testError(CSSM_BOOL quiet)
{
	char resp;
	
	if(quiet) {
		printf("\n***Test aborting.\n");
		exit(1);
	}
	fpurge(stdin);
	printf("a to abort, c to continue: ");
	resp = getchar();
	return (resp == 'a');
}

void testStartBanner(
	char *testName,
	int argc,
	char **argv)
{
	printf("Starting %s; args: ", testName);
	int i;
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
}

