/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*!
    @header SecTokend.h
    @copyright 2004 Apple Computer, Inc. All Rights Reserved.
    @abstract SPIs for creating a tokend.
 */

#ifndef _SECURITYTOKEND_SECTOKEND_H_
#define _SECURITYTOKEND_SECTOKEND_H_  1

#include <stdint.h>
#include <Security/cssm.h>
#include <PCSC/winscard.h>
#include <sys/param.h>

#ifdef __cplusplus
extern "C" {
#endif


//
// Constants and fixed values
//
enum {
	kSecTokendCallbackVersion = 11	// interface version for callback structure
};

enum {
	TOKEND_MAX_UID = 128	// max. length of token identifier string
};

// flags to establish()
typedef uint32 SecTokendCallbackFlags;
typedef uint32 SecTokendProbeFlags;
typedef uint32 SecTokendEstablishFlags;

enum {
	// flags in the callback structure passed to SecTokendMain()
	kSecTokendCallbacksDefault = 0,		// default flags in callbacks struct
	
	// flags to probe()
	kSecTokendProbeDefault = 0,			// default flags to probe() call
	kSecTokendProbeKeepToken = 0x0001,	// may keep token connected when returning

	// flags to establish()
	kSecTokendEstablishNewCache = 0x0001, // the on-disk cache is new (and empty)
	kSecTokendEstablishMakeMDS = 0x0002, // (must) write MDS files to cache
	
	// test/debug flags (not for normal operation)
	kSecTokendCallbacksTestNoServer = 0x0001, // don't start server; return after setup
};


/*
 * Common arguments for data retrieval
 */
typedef struct {
	CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes; // add attribute values here
	CSSM_DATA *data;				// store item data there unless NULL
	CSSM_HANDLE record;				// store record-id here
	CSSM_HANDLE keyhandle;			// key handle if key, 0 otherwise
} TOKEND_RETURN_DATA;


/*!
    @typedef
    @abstract Functions a particular tokend instance needs to implement
 */
typedef struct
{
    uint32_t version;
	SecTokendCallbackFlags flags;

	CSSM_RETURN (*initial)();
    CSSM_RETURN (*probe)(SecTokendProbeFlags flags, uint32 *score, char tokenUid[TOKEND_MAX_UID]);
	CSSM_RETURN (*establish)(const CSSM_GUID *guid, uint32 subserviceId,
		SecTokendEstablishFlags flags, const char *cacheDirectory, const char *workDirectory,
		char mdsDirectory[PATH_MAX], char printName[PATH_MAX]);
	CSSM_RETURN (*terminate)(uint32 reason, uint32 options);
		
	CSSM_RETURN (*findFirst)(const CSSM_QUERY *query, TOKEND_RETURN_DATA *data,
		CSSM_HANDLE *hSearch);
	CSSM_RETURN (*findNext)(CSSM_HANDLE hSearch, TOKEND_RETURN_DATA *data);
	CSSM_RETURN (*findRecordHandle)(CSSM_HANDLE hRecord, TOKEND_RETURN_DATA *data);
	CSSM_RETURN (*insertRecord)(CSSM_DB_RECORDTYPE recordType,
		const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes, const CSSM_DATA *data,
		CSSM_HANDLE *hRecord);
	CSSM_RETURN (*modifyRecord)(CSSM_DB_RECORDTYPE recordType, CSSM_HANDLE *hRecord,
		const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes, const CSSM_DATA *data,
		CSSM_DB_MODIFY_MODE modifyMode);
	CSSM_RETURN (*deleteRecord)(CSSM_HANDLE hRecord);
	CSSM_RETURN (*releaseSearch)(CSSM_HANDLE hSearch);
	CSSM_RETURN (*releaseRecord)(CSSM_HANDLE hRecord);
	
	CSSM_RETURN (*freeRetrievedData)(TOKEND_RETURN_DATA *data);
	
	CSSM_RETURN (*releaseKey)(CSSM_HANDLE hKey);
	CSSM_RETURN (*getKeySize)(CSSM_HANDLE hKey, CSSM_KEY_SIZE *size);
	CSSM_RETURN (*getOutputSize)(const CSSM_CONTEXT *context, CSSM_HANDLE hKey,
		uint32 inputSize, CSSM_BOOL encrypting,	uint32 *outputSize);
	
	CSSM_RETURN (*generateSignature)(const CSSM_CONTEXT *context, CSSM_HANDLE hKey, 
		CSSM_ALGORITHMS signOnly, const CSSM_DATA *input, CSSM_DATA *signature);
	CSSM_RETURN (*verifySignature)(const CSSM_CONTEXT *context, CSSM_HANDLE hKey,
		CSSM_ALGORITHMS signOnly, const CSSM_DATA *input, const CSSM_DATA *signature);
	CSSM_RETURN (*generateMac)(const CSSM_CONTEXT *context, CSSM_HANDLE hKey,
		const CSSM_DATA *input, CSSM_DATA *mac);
	CSSM_RETURN (*verifyMac)(const CSSM_CONTEXT *context, CSSM_HANDLE hKey,
		const CSSM_DATA *input, const CSSM_DATA *mac);
	CSSM_RETURN (*encrypt)(const CSSM_CONTEXT *context, CSSM_HANDLE hKey,
		const CSSM_DATA *clear, CSSM_DATA *cipher);
	CSSM_RETURN (*decrypt)(const CSSM_CONTEXT *context, CSSM_HANDLE hKey,
		const CSSM_DATA *cipher, CSSM_DATA *clear);
	
	CSSM_RETURN (*generateKey)(const CSSM_CONTEXT *context,
		const CSSM_ACCESS_CREDENTIALS *creds, const CSSM_ACL_ENTRY_PROTOTYPE *owner,
		CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs,
		CSSM_HANDLE *hKey, CSSM_KEY *header);
	CSSM_RETURN (*generateKeyPair)(const CSSM_CONTEXT *context,
		const CSSM_ACCESS_CREDENTIALS *creds, const CSSM_ACL_ENTRY_PROTOTYPE *owner,
		CSSM_KEYUSE pubUsage, CSSM_KEYATTR_FLAGS pubAttrs,
		CSSM_KEYUSE privUsage, CSSM_KEYATTR_FLAGS privAttrs,
		CSSM_HANDLE *hPubKey, CSSM_KEY *pubHeader,
		CSSM_HANDLE *hPrivKey, CSSM_KEY *privHeader);
	
	CSSM_RETURN (*wrapKey)(const CSSM_CONTEXT *context,
		CSSM_HANDLE hWrappingKey, const CSSM_KEY *wrappingKey, const CSSM_ACCESS_CREDENTIALS *cred,
		CSSM_HANDLE hKeyToBeWrapped, const CSSM_KEY *keyToBeWrapped, const CSSM_DATA *descriptiveData,
		CSSM_KEY *wrappedKey);
	CSSM_RETURN (*unwrapKey)(const CSSM_CONTEXT *context,
		CSSM_HANDLE hWrappingKey, const CSSM_KEY *wrappingKey, const CSSM_ACCESS_CREDENTIALS *cred,
		const CSSM_ACL_ENTRY_PROTOTYPE *access,
		CSSM_HANDLE hPublicKey, const CSSM_KEY *publicKey, const CSSM_KEY *wrappedKey,
		CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attributes,
		CSSM_DATA *descriptiveData,
		CSSM_HANDLE *hUnwrappedKey, CSSM_KEY *unwrappedKey);
	CSSM_RETURN (*deriveKey)(const CSSM_CONTEXT *context,
		CSSM_HANDLE hSourceKey, const CSSM_KEY *sourceKey, const CSSM_ACCESS_CREDENTIALS *cred,
		const CSSM_ACL_ENTRY_PROTOTYPE *access, CSSM_DATA *parameters,
		CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attributes,
		CSSM_HANDLE *hKey, CSSM_KEY *key);
	
	CSSM_RETURN (*getDatabaseOwner)(CSSM_ACL_OWNER_PROTOTYPE *owner);
	CSSM_RETURN (*getDatabaseAcl)(const char *tag, uint32 *count, CSSM_ACL_ENTRY_INFO **entries);
	CSSM_RETURN (*getObjectOwner)(CSSM_HANDLE hRecord, CSSM_ACL_OWNER_PROTOTYPE *owner);
	CSSM_RETURN (*getObjectAcl)(CSSM_HANDLE hRecord, const char *tag,
		uint32 *count, CSSM_ACL_ENTRY_INFO **entries);
	CSSM_RETURN (*getKeyOwner)(CSSM_HANDLE hKey, CSSM_ACL_OWNER_PROTOTYPE *owner);
	CSSM_RETURN (*getKeyAcl)(CSSM_HANDLE hKey,
		const char *tag, uint32 *count, CSSM_ACL_ENTRY_INFO **entries);
	
	CSSM_RETURN (*freeOwnerData)(CSSM_ACL_OWNER_PROTOTYPE *owner);
	CSSM_RETURN (*freeAclData)(uint32 count, CSSM_ACL_ENTRY_INFO *entries);
	
	CSSM_RETURN (*authenticateDatabase)(CSSM_DB_ACCESS_TYPE mode,
		const CSSM_ACCESS_CREDENTIALS *cred);
	
	CSSM_RETURN (*changeDatabaseOwner)(const CSSM_ACL_OWNER_PROTOTYPE *owner);
	CSSM_RETURN (*changeDatabaseAcl)(const CSSM_ACCESS_CREDENTIALS *cred, const CSSM_ACL_EDIT *edit);
	CSSM_RETURN (*changeObjectOwner)(CSSM_HANDLE hRecord, const CSSM_ACL_OWNER_PROTOTYPE *owner);
	CSSM_RETURN (*changeObjectAcl)(CSSM_HANDLE hRecord, const CSSM_ACCESS_CREDENTIALS *cred, const CSSM_ACL_EDIT *edit);
	CSSM_RETURN (*changeKeyOwner)(CSSM_HANDLE key, const CSSM_ACL_OWNER_PROTOTYPE *owner);
	CSSM_RETURN (*changeKeyAcl)(CSSM_HANDLE key, const CSSM_ACCESS_CREDENTIALS *cred, const CSSM_ACL_EDIT *edit);
	
	CSSM_RETURN (*generateRandom)(const CSSM_CONTEXT *context, CSSM_DATA *result);
	CSSM_RETURN (*getStatistics)(CSSM_CSP_OPERATIONAL_STATISTICS *result);
	CSSM_RETURN (*getTime)(CSSM_ALGORITHMS algorithm, CSSM_DATA *result);
	CSSM_RETURN (*getCounter)(CSSM_DATA *result);
	CSSM_RETURN (*selfVerify)();
	
	CSSM_RETURN (*cspPassThrough)(uint32 id, const CSSM_CONTEXT *context,
		CSSM_HANDLE hKey, const CSSM_KEY *key, const CSSM_DATA *input, CSSM_DATA *output);
	CSSM_RETURN (*dlPassThrough)(uint32 id, const CSSM_DATA *input, CSSM_DATA *output);
	CSSM_RETURN (*isLocked)(uint32 *locked);

} SecTokendCallbacks;


/*
 *
 */
typedef struct {
	const SCARD_READERSTATE *(*startupReaderInfo)(); // get reader information
	const char *(*tokenUid)();					// uid string in use for this token
	void *(*malloc)(uint32 size);				// ordinary memory to interface
	void (*free)(void *data);					// free malloc'ed data
	void *(*mallocSensitive)(uint32 size);		// sensitive memory to interface
	void (*freeSensitive)(void *data);			// free mallocSensitive'ed data
} SecTokendSupport;


/*!
    @function
    @abstract call this from main in your program
    @param argc The argc passed to main().
    @param argv The argv passed to main().
    @param callbacks Pointer to a SecTokendCallbacks struct containing pointers to all the functions this tokend instance implements.
    @description The main of your program should look like:

    int main(argc, argv) { ...Create context... return SecTokendMain(argc, argv, &callbacks, &supportCalls); }
 */
__attribute__((visibility("default")))
int SecTokendMain(int argc, const char * argv[], const SecTokendCallbacks *callbacks, SecTokendSupport *support);

#ifdef __cplusplus
}
#endif

#endif /* _SECURITYTOKEND_SECTOKEND_H_ */
