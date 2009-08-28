/* Copyright 1997 Apple Computer, Inc.
 *
 * common.h - Common CSP test code
 *
 * Revision History
 * ----------------
 *  12 Aug 1997	Doug Mitchell at Apple
 *		Created.
 */
 
#ifndef	_UTIL_LIB_COMMON_H_
#define _UTIL_LIB_COMMON_H_

#include <Security/cssm.h>

#ifdef	__cplusplus
extern "C" {
#endif

#undef COMMON_CSSM_MEMORY
#define COMMON_CSSM_MEMORY 0

#if		COMMON_CSSM_MEMORY
#define CSSM_MALLOC(size)			CSSM_Malloc(size)
#define CSSM_FREE(ptr)				CSSM_Free(ptr)
#define CSSM_CALLOC(num, size)		CSSM_Calloc(num, size)
#define CSSM_REALLOC(ptr, newSize)	CSSM_Realloc(ptr, newSize)
/* used in cspwrap when allocating memory on app's behalf */
#define appMalloc(size, allocRef)	CSSM_Malloc(size)

#else	/* !COMMON_CSSM_MEMORY */

void * appMalloc (CSSM_SIZE size, void *allocRef);
void appFree (void *mem_ptr, void *allocRef);
void * appRealloc (void *ptr, CSSM_SIZE size, void *allocRef);
void * appCalloc (uint32 num, CSSM_SIZE size, void *allocRef);

#define CSSM_MALLOC(size)			appMalloc(size, NULL)
#define CSSM_FREE(ptr)				appFree(ptr, NULL)
#define CSSM_CALLOC(num, size)		appCalloc(num, size, NULL)
#define CSSM_REALLOC(ptr, newSize)	appRealloc(ptr, newSize, NULL)

#endif	/* COMMON_CSSM_MEMORY */

/*
 * As of 23 March 1999, there is no longer a "default DB" available for
 * generating keys. This is the standard DB handle created when 
 * calling cspStartup().
 */
extern CSSM_DB_HANDLE commonDb;

/*
 * Init CSSM; returns CSSM_FALSE on error. Reusable.
 */
extern CSSM_BOOL cssmStartup();

/* various flavors of "start up the CSP with optional DB open" */
CSSM_CSP_HANDLE cspStartup();	// bare bones CSP
CSSM_CSP_HANDLE cspDbStartup(	// bare bones CSP, DB open
	CSSM_DB_HANDLE *dbHandPtr);	
CSSM_DL_HANDLE dlStartup();
CSSM_CSP_HANDLE cspDlDbStartup(	// one size fits all
	CSSM_BOOL bareCsp,			// true ==> CSP, false ==> CSP/DL
	CSSM_DB_HANDLE *dbHandPtr);	// optional
CSSM_RETURN cspShutdown(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_BOOL bareCsp);			// true ==> CSP, false ==> CSP/DL
CSSM_RETURN dbDelete(
	CSSM_DL_HANDLE		dlHand,			// from dlStartup()
	const char 			*dbName);
CSSM_DB_HANDLE dbStartup(
	CSSM_DL_HANDLE		dlHand,			// from dlStartup()
	const char 			*dbName);
CSSM_RETURN dbCreateOpen(
	CSSM_DL_HANDLE		dlHand,			// from dlStartup()
	const char 			*dbName,
	CSSM_BOOL			doCreate,		// if false, must already exist	
	CSSM_BOOL			deleteExist,
	const char			*pwd,			// optional
	CSSM_DB_HANDLE		*dbHand);

extern void intToBytes(unsigned i, unsigned char *buf);
void shortToBytes(unsigned short s, unsigned char *buf);
unsigned bytesToInt(const unsigned char *buf);
unsigned short bytesToShort(const unsigned char *buf);

/* specify either 32-bit integer or a pointer as an added attribute value */
typedef enum {
	CAT_Uint32,
	CAT_Ptr
} ContextAttrType;

CSSM_RETURN AddContextAttribute(CSSM_CC_HANDLE CCHandle,
	uint32 AttributeType,
	uint32 AttributeLength,
	ContextAttrType attrType,
	/* specify exactly one of these */
	const void *AttributePtr,
	uint32 attributeInt);
void printError(const char *op, CSSM_RETURN err);
CSSM_RETURN appSetupCssmData(
	CSSM_DATA_PTR	data,
	uint32			numBytes);
void appFreeCssmData(CSSM_DATA_PTR data,
	CSSM_BOOL freeStruct);
CSSM_RETURN appCopyCssmData(const CSSM_DATA *src, 
	CSSM_DATA_PTR dst);
/* copy raw data to a CSSM_DATAm mallocing dst. */
CSSM_RETURN  appCopyData(const void *src, 
	uint32 len,
	CSSM_DATA_PTR dst);
	
/* returns CSSM_TRUE on success, else CSSM_FALSE */
CSSM_BOOL appCompareCssmData(const CSSM_DATA *d1,
	const CSSM_DATA *d2);
	
const char *cssmErrToStr(CSSM_RETURN err);

/*
 * Calculate random data size, fill dataPool with that many random bytes.
 */
typedef enum {
	DT_Random,
	DT_Increment,
	DT_Zero,
	DT_ASCII
} dataType;

unsigned genData(unsigned char *dataPool,
	unsigned minExp,
	unsigned maxExp,
	dataType type);
void simpleGenData(CSSM_DATA_PTR dbuf, unsigned minBufSize, unsigned maxBufSize);
unsigned genRand(unsigned min, unsigned max);
extern void	appGetRandomBytes(void *buf, unsigned len);

void dumpBuffer(
	const char *bufName,	// optional
	unsigned char *buf,
	unsigned len);

int testError(CSSM_BOOL quiet);

void testStartBanner(
	char *testName,
	int argc,
	char **argv);

#ifdef	__cplusplus
}

#endif
#endif	/* _UTIL_LIB_COMMON_H_*/


