/*
 * extendAttrTool.cpp
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include "singleItemPicker.h"
#include <Security/SecKeychainItemExtendedAttributes.h>
#include <Security/SecKeychainItemPriv.h>
#include <Security/cssmapple.h>

#define DEFAULT_ATTR_NAME	"someAttr"

static void usage(char **argv)
{
	printf("usage: %s op [options]\n", argv[0]);
	printf("Op: set, get, delete, getall, dump\n");
	printf("Options:\n");
	printf("  -t itemType     -- type = priv|pub|cert; default is public\n");
	printf("  -k keychain     -- default is default KC list\n");
	printf("  -p              -- use item picker; default is first matching item in keychain\n");
	printf("  -a attrName     -- default is %s\n", DEFAULT_ATTR_NAME);
	printf("  -A attrValue\n");
	printf("  -n              -- no values retrieved on op getall\n");
	printf("  -l              -- loop/pause for malloc debug\n");
	exit(1);
}

#define CFRELEASE(cf)	if(cf) { CFRelease(cf); }

/*
 * Print contents of a CFData assuming it's printable 
 */
static void printCfData(CFDataRef cfd)
{
	CFIndex len = CFDataGetLength(cfd);
	const UInt8 *cp = CFDataGetBytePtr(cfd);
	for(CFIndex dex=0; dex<len; dex++) {
		char c = cp[dex];
		if(isprint(c)) {
			putchar(c);
		}
		else {
			printf(".%02X.", c);
		}
	}
}

/* print the contents of a CFString */
void printCfStr(
	CFStringRef cfstr)
{
	CFDataRef strData = CFStringCreateExternalRepresentation(NULL, cfstr,
		kCFStringEncodingUTF8, true);
	if(strData == NULL) {
		printf("<<string decode error>>");
		return;
	}
	const char *cp = (const char *)CFDataGetBytePtr(strData);
	CFIndex len = CFDataGetLength(strData);
	for(CFIndex dex=0; dex<len; dex++) {
		putchar(*cp++);
	}
	CFRelease(strData);
}

/*
 * Print contents of a SecKeychainAttribute assuming it's printable 
 */
static void printAttribute(SecKeychainAttribute *kca)
{
	unsigned len = kca->length;
	const char *cp = (const char *)kca->data;
	for(unsigned dex=0; dex<len; dex++) {
		char c = cp[dex];
		if(isprint(c)) {
			putchar(c);
		}
		else {
			printf(".%02X.", c);
		}
	}
}

/* 
 * Print parallel arrays of CFStringRefs (attribute names) and optional CFDataRefs
 * (attribute values).
 */
static int printAttrs(
	CFArrayRef nameArray,
	CFArrayRef valuesArray)	/* optional */
{
	CFIndex numNames = CFArrayGetCount(nameArray);
	if(valuesArray) {
		if(numNames != CFArrayGetCount(valuesArray)) {
			printf("***Mismatch on sizes of nameArray (%ld) and valuesArray (%ld)\n",
				numNames, CFArrayGetCount(valuesArray));
			return -1;
		}
	}

	for(CFIndex dex=0; dex<numNames; dex++) {
		printf("Attribute %ld:\n", (long)dex);
		CFStringRef attrName = (CFStringRef)CFArrayGetValueAtIndex(nameArray, dex);
		printf("  name  : ");
		printCfStr(attrName);
		printf("\n");
		if(valuesArray) {
			CFDataRef attrValue = (CFDataRef)CFArrayGetValueAtIndex(valuesArray, dex);
			printf("  value : ");
			printCfData(attrValue);
			printf("\n");
		}
	}
	return 0;
}

/* entry in a table to map a uint32 to a string */
typedef struct {
	uint32			value;
	const char 		*name;
} NameValuePair;

/* declare one entry in a table of nameValuePairs */
#define NVP(attr)		{attr, #attr}

/* the NULL entry which terminates all nameValuePair tables */
#define NVP_END			{0, NULL}

/* CSSM_DB_RECORDTYPE names */
const NameValuePair recordTypeNames[] = 
{
	NVP(CSSM_DL_DB_SCHEMA_INFO),
	NVP(CSSM_DL_DB_SCHEMA_INDEXES),
	NVP(CSSM_DL_DB_SCHEMA_ATTRIBUTES),
	NVP(CSSM_DL_DB_SCHEMA_PARSING_MODULE),
	NVP(CSSM_DL_DB_RECORD_ANY),
	NVP(CSSM_DL_DB_RECORD_CERT),
	NVP(CSSM_DL_DB_RECORD_CRL),
	NVP(CSSM_DL_DB_RECORD_POLICY),
	NVP(CSSM_DL_DB_RECORD_GENERIC),
	NVP(CSSM_DL_DB_RECORD_PUBLIC_KEY),
	NVP(CSSM_DL_DB_RECORD_PRIVATE_KEY),
	NVP(CSSM_DL_DB_RECORD_SYMMETRIC_KEY),
	NVP(CSSM_DL_DB_RECORD_ALL_KEYS),
	/* Apple-specific */
	NVP(CSSM_DL_DB_RECORD_GENERIC_PASSWORD),
	NVP(CSSM_DL_DB_RECORD_INTERNET_PASSWORD),
	NVP(CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD),
	NVP(CSSM_DL_DB_RECORD_X509_CERTIFICATE),
	NVP(CSSM_DL_DB_RECORD_X509_CRL),
	NVP(CSSM_DL_DB_RECORD_USER_TRUST),
	/* private to Sec layer */
	NVP(CSSM_DL_DB_RECORD_UNLOCK_REFERRAL),
	NVP_END
};

static void printRecordType(
	const void *recordTypeAttr)
{
	UInt32 recordType = *((UInt32 *)recordTypeAttr);
	for(const NameValuePair *nvp=recordTypeNames; nvp->name; nvp++) {
		if(recordType == nvp->value) {
			printf("%s", nvp->name);
			return;
		}
	}
	printf("Unknown recordType (0x%x)\n", (unsigned)recordType);
	return;
}

	
static int dumpExtendAttrRecords(
	SecKeychainRef kcRef)
{
	OSStatus ortn;
	SecKeychainSearchRef srchRef = NULL;
	
	ortn = SecKeychainSearchCreateFromAttributes(kcRef, 
		CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE,
		NULL,		// no attrs - give me everything
		&srchRef);
	if(ortn) {
		cssmPerror("SecKeychainSearchCreateFromAttributes", ortn);
		return -1;
	}
	
	SecKeychainItemRef itemRef = NULL;
	unsigned numItems = 0;
	for(;;) {
		ortn = SecKeychainSearchCopyNext(srchRef, &itemRef);
		if(ortn) {
			if(ortn == errSecItemNotFound) {
				/* normal end of search */
				break;
			}
			else {
				cssmPerror("SecKeychainSearchCopyNext", ortn);
				break;
			}
		}
		
		/* get some info about the EA record - RecordType (that it's bound to) and 
		 * AttributeName */
		UInt32 tags[2] = { kExtendedAttrRecordTypeAttr, kExtendedAttrAttributeNameAttr };
		UInt32 formats[2] = {0};
		SecKeychainAttributeList *attrList = NULL;
		SecKeychainAttributeInfo attrInfo = {2, tags, formats};
		ortn = SecKeychainItemCopyAttributesAndData(itemRef, &attrInfo,
			NULL, &attrList, NULL, NULL);
		if(ortn) {
			cssmPerror("SecKeychainItemCopyAttributesAndData", ortn);
			return -1;
		}
		printf("Extended Attribute %u:\n", numItems);
		for(unsigned dex=0; dex<2; dex++) {
			SecKeychainAttribute *attr = &attrList->attr[dex];
			switch(attr->tag) {
				case kExtendedAttrRecordTypeAttr:
					printf("   Record type    : ");
					printRecordType(attr->data);
					printf("\n");
					break;

				case kExtendedAttrAttributeNameAttr:
					printf("   Attribute Name : ");
					printAttribute(attr);
					printf("\n");
					break;
					break;
				default:
					/* should never happen, right? */
					printf("***Unexpected attr tag when parsing an ExtendedAttr record\n");
					return -1;
			}
		}
		numItems++;
		SecKeychainItemFreeAttributesAndData(attrList, NULL);
		CFRelease(itemRef);
	}
	if(numItems == 0) {
		printf("...no Extended Attribute records found.\n");
	}
	CFRelease(srchRef);
	return 0;
}

typedef enum {
	OP_None,
	OP_Set,
	OP_Get,
	OP_Delete,
	OP_GetAll,
	OP_Dump			// search for EXTENDED_ATTRIBUTE records, dump contents
} Op;

int main(int argc, char **argv)
{
	const char *attrName = DEFAULT_ATTR_NAME;
	const char *attrValue = NULL;
	const char *kcName = NULL;
	KP_ItemType itemType = KPI_PublicKey;
	bool takeFirst = true;
	bool noValues = false;
	bool loopPause = false;
	Op op = OP_None;
	
	if(argc < 2) {
		usage(argv);
	}
	if(!strcmp(argv[1], "set")) {
		op = OP_Set;
	}
	else if(!strcmp(argv[1], "get")) {
		op = OP_Get;
	}
	else if(!strcmp(argv[1], "delete")) {
		op = OP_Delete;
	}
	else if(!strcmp(argv[1], "getall")) {
		op = OP_GetAll;
	}
	else if(!strcmp(argv[1], "dump")) {
		op = OP_Dump;
	}
	else {
		usage(argv);
		
	}
	
	extern int optind;
	optind = 2;
	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "t:k:pa:A:nlh")) != -1) {
		switch (arg) {
			case 't':
				if(!strcmp(optarg, "priv")) {
					itemType = KPI_PrivateKey;
				}
				else if(!strcmp(optarg, "pub")) {
					itemType = KPI_PublicKey;
				}
				else if(!strcmp(optarg, "cert")) {
					itemType = KPI_Cert;
				}
				else {
					printf("***Bad itemType specification.\n");
					usage(argv);
				}	
				break;
			case 'k':
				kcName = optarg;
				break;
			case 'p':
				takeFirst = false;
				break;
			case 'a':
				attrName = optarg;
				break;
			case 'A':
				attrValue = optarg;
				break;
			case 'n':
				noValues = true;
				break;
			case 'l':
				loopPause = true;
				break;
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	
	if((op == OP_Set) && (attrValue == NULL)) {
		printf("***I Need an attribute values (-A) to set\n");
		exit(1);
	} 
	 
	OSStatus ortn;
	SecKeychainItemRef theItem = NULL;
	SecKeychainRef kcRef = NULL;
	
	if(kcName) {
		ortn = SecKeychainOpen(kcName, &kcRef);
		if(ortn) {
			cssmPerror("SecKeychainOpen", ortn);
			exit(1);
		}
	}
	
	if(op == OP_Dump) {
		/* we're ready to roll */
		return dumpExtendAttrRecords(kcRef);
	}
	ortn = singleItemPicker(kcRef, itemType, takeFirst, &theItem);
	if(ortn) {
		printf("***Error picking item. Aborting.\n");
		exit(1);
	}
	
	CFStringRef attrNameStr = NULL;
	CFDataRef attrValueData = NULL;
	if(op != OP_GetAll) {
		attrNameStr = CFStringCreateWithCString(NULL, attrName, kCFStringEncodingASCII);
	}
	
	do {
		switch(op) {
			case OP_Set:
				attrValueData = CFDataCreate(NULL, (const UInt8 *)attrValue, strlen(attrValue));
				ortn = SecKeychainItemSetExtendedAttribute(theItem, attrNameStr, attrValueData);
				if(ortn) {
					cssmPerror("SecKeychainItemSetExtendedAttribute", ortn);
				}
				else {
					printf("attribute '%s' set to '%s'.\n", attrName, attrValue);
				}
				break;
			case OP_Get:
				ortn = SecKeychainItemCopyExtendedAttribute(theItem, 
					attrNameStr, &attrValueData);
				if(ortn) {
					cssmPerror("SecKeychainItemCopyExtendedAttribute", ortn);
				}
				else {
					printf("Attribute '%s' found; value = '", attrName);
					printCfData(attrValueData);
					printf("'\n");
				}
				break;
			case OP_Delete:
				ortn = SecKeychainItemSetExtendedAttribute(theItem, attrNameStr, NULL);
				if(ortn) {
					cssmPerror("SecKeychainItemSetExtendedAttribute", ortn);
				}
				else {
					printf("attribute '%s' deleted.\n", attrName);
				}
				break;
			case OP_GetAll:
			{
				CFArrayRef nameArray = NULL;
				CFArrayRef valuesArray = NULL;
				CFArrayRef *valuesArrayPtr = noValues ? NULL : &valuesArray;
				
				ortn = SecKeychainItemCopyAllExtendedAttributes(theItem,
					&nameArray, valuesArrayPtr);
				if(ortn) {
					cssmPerror("SecKeychainItemCopyAllExtendedAttributes", ortn);
					break;
				}
				if(nameArray == NULL) {
					printf("***NULL nameArray after successful "
						"SecKeychainItemCopyAllExtendedAttributes\n");
					ortn = -1;
					break;
				}
				if(!noValues) {
					if(valuesArray == NULL) {
						printf("***NULL valuesArray after successful "
							"SecKeychainItemCopyAllExtendedAttributes\n");
						ortn = -1;
						break;
					}
				}
				ortn = printAttrs(nameArray, valuesArray);
				CFRELEASE(nameArray);
				CFRELEASE(valuesArray);
				break;
			}
			case OP_Dump:
				/* already handled this; satisfy compiler */
				break;
			case OP_None:
				printf("***BRRRZAP!\n");
				exit(1);
		}
		if(ortn) {
			break;
		}
		CFRELEASE(attrValueData);
		attrValueData = NULL;
		
		if(loopPause) {
			fpurge(stdin);
			printf("End of loop; a to abort, anything else to continue: ");
			if(getchar() == 'a') {
				break;
			}
		}
	} while(loopPause);
	return ortn ? -1 : 0;
}
