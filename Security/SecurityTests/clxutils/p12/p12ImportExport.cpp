/*
 * p12ImportExport.cpp - high-level libnsspkcs12 exerciser
 */
 
#include <security_pkcs12/SecPkcs12.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <Security/Security.h>
#include <stdio.h>
#include <stdlib.h>
#include <security_cdsa_utilities/KeySchema.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include "p12GetPassKey.h"

static void printOsError(
	const char *op,
	OSStatus ortn)
{
	char *errStr = NULL;
	switch(ortn) {
		case CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA:
			errStr = "CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA"; break;
		case CSSMERR_DL_DATASTORE_DOESNOT_EXIST:
			errStr = "CSSMERR_DL_DATASTORE_DOESNOT_EXIST"; break;
		case errSecDuplicateItem:
			errStr = "errSecDuplicateItem"; break;
		case errSecNotAvailable:
			errStr = "errSecNotAvailable"; break;
		case errSecAuthFailed:
			errStr = "errSecAuthFailed"; break;
		case errSecItemNotFound:
			errStr = "errSecItemNotFound"; break;
		case errSecInvalidItemRef:
			errStr = "errSecInvalidItemRef"; break;
		default:
			break;
	}
	if(errStr) {
		printf("%s returned %s\n", op, errStr);
	}
	else {
		printf("%s returned %d\n", op, (int)ortn);
	}
}

/*
 * For now we assume "import everything"
 */
int p12Import(
	const char *pfxFile,
	const char *kcName,
	CFStringRef pwd,			// explicit passphrase, mutually exclusive with...
	bool usePassKey,			// use SECURE_PASSPHRASE key
	const char *kcPwd)			// optional
{
	OSStatus 		ortn;
	unsigned char 	*pfx;
	unsigned 		pfxLen;
	CSSM_KEY		passKey;
	
	/* get the PFX */
	if(readFile(pfxFile, &pfx, &pfxLen)) {
		printf("***Error reading pfx from %s. Aborting.\n", pfxFile);
		return 1;
	}
	CFDataRef cfd = CFDataCreate(NULL, pfx, pfxLen);
	
	/* import to keychain specified by kcName */
	SecKeychainRef kcRef = NULL;
	ortn = SecKeychainOpen(kcName, &kcRef);
	if(ortn) {
		printOsError("SecKeychainOpen", ortn);
		return ortn;
	}

	if(kcPwd) {
		ortn = SecKeychainUnlock(kcRef, strlen(kcPwd), (void *)kcPwd, true);
		if(ortn) {
			printOsError("SecKeychainUnlock", ortn);
		}
	}

	/* set up a pkcs12 coder for import */
	SecPkcs12CoderRef coder;
	ortn = SecPkcs12CoderCreate(&coder);
	if(ortn) {
		printOsError("SecPkcs12CoderCreate", ortn);
		return ortn;
	}

	ortn = SecPkcs12SetKeychain(coder, kcRef);
	if(ortn) {
		printOsError("SecPkcs12SetKeychain", ortn);
		return ortn;
	}

	if(usePassKey) {
		CSSM_CSP_HANDLE cspHand;
		ortn =	SecKeychainGetCSPHandle(kcRef, &cspHand);
		if(ortn) {
			printOsError("SecPkcs12SetKeychain", ortn);
			return ortn;
		}
		ortn = p12GetPassKey(cspHand, GPK_Decode, false, &passKey);
		if(ortn) {
			return ortn;
		}
		ortn = SecPkcs12SetMACPassKey(coder, &passKey);
		if(ortn) {
			printOsError("SecPkcs12SetMACPassKey", ortn);
			return ortn;
		}
	}
	else {
		ortn = SecPkcs12SetMACPassphrase(coder, pwd);
		if(ortn) {
			printOsError("SecPkcs12SetMACPassphrase", ortn);
			return ortn;
		}
	}
	
	/*
	 * For now we assume "import everything"
	 */
	ortn = SecPkcs12SetImportToKeychain(coder,
		kSecImportCertificates | 
		kSecImportCRLs |
		kSecImportKeys);
	if(ortn) {
		printOsError("SecPkcs12SetImportFromKeychain", ortn);
		return ortn;
	}
	
	/* Go! */
	ortn = SecPkcs12Decode(coder, cfd);
	if(ortn) {
		printOsError("SecPkcs12Decode", ortn);
		return ortn;
	}
	
	/* report how many of each item got imported */
	CFIndex num;
	SecPkcs12CertificateCount(coder, &num);
	printf("...%d certs imported\n", (int)num);
	SecPkcs12CrlCount(coder, &num);
	printf("...%d CRLs imported\n", (int)num);
	SecPkcs12PrivateKeyCount(coder, &num);
	printf("...%d private keys imported\n", (int)num);
	
	SecPkcs12CoderRelease(coder);
	CFRelease(cfd);
	free(pfx);			// mallocd by readFile()
	return 0;
}

/* 
 * Use the kludge from hell to get the name-as-int form of a specified
 * "known" name-as-string for the Key Schema.
 */
OSStatus attrNameToInt(
	const char *name, 
	uint32 *attrInt)
{
	const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *attrList = 
		KeySchema::KeySchemaAttributeList;
	unsigned numAttrs = KeySchema::KeySchemaAttributeCount;
	for(unsigned dex=0; dex<numAttrs; dex++) {
		const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *info = &attrList[dex];
		if(!strcmp(name, info->AttributeName)) {
			*attrInt = info->AttributeId;
			return noErr;
		}
	}
	return paramErr;
}

static int p12AddExportedItem(
	SecKeychainItemRef item,
	CFMutableArrayRef itemArray,
	bool noPrompt)
{
	if(noPrompt) {
		CFArrayAppendValue(itemArray, item);
		return 1;
	}
	
	CFTypeID itemId = CFGetTypeID(item);
	OSStatus ortn;
	
	/* the printable name attr */
	UInt32 nameAttr = 0;
	char *itemClass = "";
	if(itemId == SecCertificateGetTypeID()) {
		itemClass = "Certificate";
		nameAttr = kSecLabelItemAttr;
	}
	else if(itemId == SecKeyGetTypeID()) {
		itemClass = "Private Key";
		ortn = attrNameToInt("PrintName", &nameAttr);
		if(ortn) {
			/* out of sync with Sec layer? With KeySchema? */
			printf("warning: attrNameToInt failure\n");
			return 0;
		}
	}
	else {
		/* we don't know how to deal with this */
		printf("p12AddExportedItem: internal screwup\n");
		return 0;
	}
	
	/* get the printable name attr */
	SecKeychainAttributeInfo attrInfo;
	attrInfo.count = 1;
	attrInfo.tag = &nameAttr;
	attrInfo.format = NULL;	// ???
	
	/* FIXME header says this is an IN/OUT param, but it's not */
	SecKeychainAttributeList *attrList = NULL;
	
	ortn = SecKeychainItemCopyAttributesAndData(
		item, 
		&attrInfo,
		NULL,			// itemClass
		&attrList, 
		NULL,			// don't need the data
		NULL);
	if(ortn) {
		printOsError("SecKeychainItemCopyAttributesAndData", ortn);
		return 0;
	}
	if(attrList->count != 1) {
		printf("***Unexpected attribute count (%u) for %s\n",
			(unsigned)attrList->count, itemClass);
		return 0;
	}
	SecKeychainAttribute *attr = attrList->attr;
	
	/* it's a UTF8 string: use CFString to convert to C ASCII string */
	CFStringRef cfStr = CFStringCreateWithBytes(NULL, 
		(UInt8 *)attr->data, attr->length, 
		kCFStringEncodingUTF8,	false);
	SecKeychainItemFreeAttributesAndData(attrList, NULL);
	if(cfStr == NULL) {
		printf("***Error converting %s name to UTF CFSTring.\n",
			itemClass);
		return 0;
	}
	
	CFIndex strLen = CFStringGetLength(cfStr);
	char *printName = (char *)malloc(strLen + 1);
	if(!CFStringGetCString(cfStr, printName, strLen + 1, kCFStringEncodingASCII)) {
		printf("***Error converting %s name to ASCII\n", itemClass);
		return 0;
	} 
	CFRelease(cfStr);
	
	char *aliasCStr = NULL;
	if((itemId == SecCertificateGetTypeID())) {
		/* the alias attr, for cert email */
		CFStringRef aliasCFStr = NULL;
		nameAttr = kSecAlias;
		attrInfo.count = 1;
		attrInfo.tag = &nameAttr;
		attrInfo.format = NULL;	// ???
		attrList = NULL;
		
		ortn = SecKeychainItemCopyAttributesAndData(
			item, 
			&attrInfo,
			NULL,			// itemClass
			&attrList, 
			NULL,			// don't need the data
			NULL);
		if(ortn) {
			printOsError("SecKeychainItemCopyAttributesAndData", ortn);
			return 0;
		}
		if(attrList->count != 1) {
			printf("***Unexpected attribute count (%u) for Alias\n",
				(unsigned)attrList->count);
			return 0;
		}
		attr = attrList->attr;
		
		/* it's a UTF8 string: use CFString to convert to C ASCII string */
		aliasCFStr = CFStringCreateWithBytes(NULL, 
			(UInt8 *)attr->data, attr->length, 
			kCFStringEncodingUTF8,	false);
		if(aliasCFStr == NULL) {
			printf("***Error converting Alias name to UTF CFSTring.\n");
			return 0;
		}
		
		strLen = CFStringGetLength(aliasCFStr);
		aliasCStr = (char *)malloc(strLen + 1);
		if(!CFStringGetCString(aliasCFStr, aliasCStr, strLen + 1, 
				kCFStringEncodingASCII)) {
			printf("***Error converting Alias name to ASCII\n");
			return 0;
		} 
		CFRelease(aliasCFStr);
	}
	
	int ourRtn = 0;
	fpurge(stdin);
	printf("Found %s\n", itemClass);
	printf("   printable name : %s\n", printName);
	if(aliasCStr != NULL) {
		printf("   alias          : %s\n", aliasCStr);
	}
	printf("Export (y/anything)? ");
	char c = getchar();
	if(c == 'y') {
		CFArrayAppendValue(itemArray, item);
		ourRtn = 1;
	}
	free(printName);
	if(aliasCStr) {
		free(aliasCStr);
	}
	return ourRtn;
}

int p12Export(
	const char *pfxFile,
	const char *kcName,
	CFStringRef pwd,			// explicit passphrase, mutually exclusive with...
	bool usePassKey,			// use SECURE_PASSPHRASE key
	const char *kcPwd,			// optional
	bool noPrompt)				// true --> export all 
{
	OSStatus ortn;
	CSSM_KEY		passKey;

	/* set up a pkcs12 coder for export */
	SecPkcs12CoderRef coder;
	ortn = SecPkcs12CoderCreate(&coder);
	if(ortn) {
		printOsError("SecPkcs12CoderCreate", ortn);
		return ortn;
	}

	/* 
	Ê* Since we're not providing the SecPkcs12CoderRef with a
	 * keychain, we have to provide the CSPDL handle 
	 */
	CSSM_CSP_HANDLE cspHand = cuCspStartup(CSSM_FALSE);
	if(cspHand == 0) {
		printf("***Error attaching to CSPDL. Aborting.\n");
		return 1;
	}

	if(usePassKey) {
		ortn = p12GetPassKey(cspHand, GPK_Encode, false, &passKey);
		if(ortn) {
			return ortn;
		}
		ortn = SecPkcs12SetMACPassKey(coder, &passKey);
		if(ortn) {
			printOsError("SecPkcs12SetMACPassKey", ortn);
			return ortn;
		}
	}
	else {
		ortn = SecPkcs12SetMACPassphrase(coder, pwd);
		if(ortn) {
			printOsError("SecPkcs12SetMACPassphrase", ortn);
			return ortn;
		}
	}

	ortn = SecPkcs12SetCspHandle(coder, cspHand);
	if(ortn) {
		printOsError("SecPkcs12SetCspHandle", ortn);
		return ortn;
	}
	
	/* the array of things we want to export */
	CFMutableArrayRef items = CFArrayCreateMutable(NULL, 0, NULL);
	
	/* export from keychain specified by kcName */
	SecKeychainRef kcRef = NULL;
	ortn = SecKeychainOpen(kcName, &kcRef);
	if(ortn) {
		printOsError("SecKeychainOpen", ortn);
		return ortn;
	}
	
	if(kcPwd) {
		ortn = SecKeychainUnlock(kcRef, strlen(kcPwd), (void *)kcPwd, true);
		if(ortn) {
			printOsError("SecKeychainUnlock", ortn);
		}
	}

	/* 
	 * Prompt user for each known item - it would be nice if we 
	 * could search for anything, eh? 
	 * Certs first...
	 */
	SecKeychainSearchRef srchRef;
	ortn = SecKeychainSearchCreateFromAttributes(kcRef,
		kSecCertificateItemClass,
		NULL,		// no attrs
		&srchRef);
	if(ortn) {
		printOsError("SecKeychainSearchCreateFromAttributes", ortn);
		return ortn;
	}
	int exported = 0;
	for(;;) {
		SecKeychainItemRef certRef;
		ortn = SecKeychainSearchCopyNext(srchRef, &certRef);
		if(ortn) {
			break;
		}
		exported += p12AddExportedItem(certRef, items, noPrompt);
	}
	CFRelease(srchRef);

	/* now private keys */
	ortn = SecKeychainSearchCreateFromAttributes(kcRef,
		CSSM_DL_DB_RECORD_PRIVATE_KEY,	// undocumented
		NULL,							// no attrs
		&srchRef);
	if(ortn) {
		printOsError("SecKeychainSearchCreateFromAttributes", ortn);
		return ortn;
	}
	for(;;) {
		SecKeychainItemRef keyRef;
		ortn = SecKeychainSearchCopyNext(srchRef, &keyRef);
		if(ortn) {
			break;
		}
		exported += p12AddExportedItem(keyRef, items, noPrompt);
	}
	
	if(exported == 0) {
		printf("...Hmmm, no items to export. Done.\n");
		return 0;
	}
	ortn = SecPkcs12ExportKeychainItems(coder, items);
	if(ortn) {
		printOsError("SecPkcs12ExportKeychainItems", ortn);
		return ortn;
	}
	
	/* go */
	CFDataRef pfx;
	ortn = SecPkcs12Encode(coder, &pfx);
	if(ortn) {
		printOsError("SecPkcs12ExportKeychainItems", ortn);
		return ortn;
	}

	if(writeFile(pfxFile, CFDataGetBytePtr(pfx),
				CFDataGetLength(pfx))) {
		printf("***Error writing pfx to %s\n", pfxFile);
		return 1;
	}
	printf("...%u items exported; %ld bytes written to %s\n",
		exported, CFDataGetLength(pfx), pfxFile);
		
	/* cleanup */
	SecPkcs12CoderRelease(coder);
	CFRelease(pfx);
	CFRelease(srchRef);
	CFRelease(kcRef);
	return 0;
}
