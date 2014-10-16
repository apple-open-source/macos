/*
 * acltool.cpp - display and manipluate ACLs on SecKeychainItems
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <Security/Security.h>
#include "aclUtils.h"

static void usage(char **argv)
{
	printf("usage: %s op [options]\n", argv[0]);
	printf("op:\n");
	printf("   d   -- display ACL\n");
	printf("   a   -- add ACL\n");
	printf("   l   -- lookup, dump label; no ACL operation\n");
	printf("   m   -- modify data (only allowed for password items; must specify -p)\n");
	printf("Options:\n");
	printf("  -k keychain\n");
	printf("  -t itemType : k=privateKey, b=publicKey s=sessionKey, p=password; default is sessionKey\n");
	printf("  -l label_or_printName\n");
	printf("  -p new_password_data\n");
	printf("  -d   -- dump data\n");
	printf("  -e   -- edit ACL entries\n");
	printf("  -s   -- simulate StickyRecord ACL\n");
	/* etc. */
	exit(1);
}


/* print an item's label and (optionally) its data */
static OSStatus printItemLabelAndData(
	SecKeychainItemRef itemRef, 
	SecItemAttr labelAttr,
	bool dumpData)
{
	SecKeychainAttributeList	attrList;
	SecKeychainAttribute		attr;
	UInt32						length = 0;
	void						*outData = NULL;
	
	attr.tag = labelAttr;			
	attr.length = 0;
	attr.data = NULL;
	attrList.count = 1;
	attrList.attr = &attr;

	OSStatus ortn = SecKeychainItemCopyContent(itemRef,
		NULL,		// itemClass - we know
		&attrList,	// for label 
		dumpData ? &length   : 0,
		dumpData ? &outData : NULL);
	if(ortn) {
		cssmPerror("SecKeychainItemCopyContent", ortn);
		printf("***Error fetching label %s\n", dumpData ? "and data" : "");
		return ortn;
	}
	
	if(attr.data == NULL) {
		printf("**No label attribute found\n");
	}
	else {
		printf("Label: ");
		print_buffer(stdout, attr.length, attr.data);
		printf("\n");
	}
	if(dumpData) {
		if(outData == NULL) {
			printf("***Asked for data but none found\n");
		}
		else {
			printf("Data : ");
			print_buffer(stdout, length, outData);
			printf("\n");
		}
	}
	SecKeychainItemFreeContent(&attrList, outData);
	return noErr;
}

/* 
 * Lookup by itemClass and optional label. Then do one or more of:
 *
 * -- dump label (always done)
 * -- dump ACLs 
 * -- edit acl
 * -- dump data
 * -- set (modify) data
 */
static OSStatus dumpAcls(
	SecKeychainRef kcRef,
	
	/* item specification */
	SecItemClass itemClass,
	SecItemAttr labelAttr,		// to look up by label if specified
	const char *label,
	
	/* what we do with the item(s) found */
	bool dumpData,
	bool dumpAcl,
	bool editAcl,
	bool simulateStickyRecord,
	const unsigned char *newData,	// if non-NULL, set/modify new data
	unsigned newDataLen)
{
	OSStatus					ortn;
	SecKeychainSearchRef		srchRef;
	SecKeychainAttributeList	attrList;
	SecKeychainAttributeList	*attrListP = NULL;
	SecKeychainAttribute		attr;
	unsigned					numFound = 0;
	
	/* searching by label, or blindly? */
	if(label) {
		attr.tag = labelAttr;			
		attr.length = strlen(label);
		attr.data = (void *)label;
		attrList.count = 1;
		attrList.attr = &attr;
		attrListP = &attrList;
	}
	ortn = SecKeychainSearchCreateFromAttributes(kcRef,
		itemClass,
		attrListP,	
		&srchRef);
	if(ortn) {
		cssmPerror("SecKeychainSearchCreateFromAttributes", ortn);
		return ortn;
	}
	for(;;) {
		SecKeychainItemRef itemRef;
		ortn = SecKeychainSearchCopyNext(srchRef, &itemRef);
		if(ortn) {
			if(ortn == errSecItemNotFound) {
				/* normal search end */
				ortn = noErr;
			}
			else {
				cssmPerror("SecKeychainSearchCopyNext", ortn);
			}
			break;
		}
		
		printf("Item %u:\n", numFound++);
		printItemLabelAndData(itemRef, labelAttr, dumpData);

		if(newData) {
			ortn = SecKeychainItemModifyAttributesAndData(itemRef, 
				NULL,		// attrList - we don't change any attrs 
				newDataLen,
				newData);
			if(ortn) {
				cssmPerror("SecKeychainItemModifyAttributesAndData", ortn);
				printf("***Cannot modify data of this item***\n");
				goto endOfLoop;
			}
		}
		if(dumpAcl) {
			SecAccessRef accessRef = nil;
			ortn = SecKeychainItemCopyAccess(itemRef, &accessRef);
			if(ortn) {
				cssmPerror("SecKeychainItemCopyAccess", ortn);
				printf("***No SecAccessRef found***\n");
				goto endOfLoop;
			}
			print_access(stdout, accessRef, editAcl);
			if(simulateStickyRecord) {
				ortn = stickyRecordUpdateAcl(accessRef);
				if(ortn) {
					goto endOfLoop;
				}
			}
			if(editAcl || simulateStickyRecord) {
				ortn = SecKeychainItemSetAccess(itemRef, accessRef);
				if(ortn) {
					cssmPerror("SecKeychainItemSetAccess", ortn);
				}
			}
		}
endOfLoop:
		CFRelease(itemRef);
		if(ortn) {
			break;
		}
	}
	CFRelease(srchRef);
	printf("...%u items found\n", numFound);
	return ortn;
}

typedef enum {
	AO_Dump,
	AO_Add,
	AO_Lookup,
	AO_ModifyPassword
} AclOp;

int main(int argc, char **argv)
{
	/* user spec'd variables */
	const char		*kcName = NULL;
	const char		*labelOrName = NULL;			// attribute type varies per 
	AclOp			op = AO_Dump;
	SecItemClass	itemClass = CSSM_DL_DB_RECORD_SYMMETRIC_KEY;
	/* FIXME why does this work like this for keys? doc says kSecKeyPrintName but that doesn't work */
	SecItemAttr		labelAttr = kSecLabelItemAttr; 
	bool			dumpData = false;
	bool			editAcl = false;
	bool			dumpAcl = true;
	bool			simulateStickyRecord = false;
	const char		*newPassword = NULL;
	unsigned		newPasswordLen = 0;
	
	SecKeychainRef kcRef = nil;
	OSStatus ortn;
	
	if(argc < 2) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'd':
			op = AO_Dump;
			break;
		case 'a':
			op = AO_Add;
			break;
		case 'l':
			op = AO_Lookup;
			dumpAcl = false;
			break;
		case 'm':
			op = AO_ModifyPassword;
			dumpAcl = false;
			break;
		default:
			usage(argv);
	}
	
	extern char *optarg;
	int arg;
	extern int optind;
	optind = 2;
	while ((arg = getopt(argc, argv, "k:t:l:dep:sh")) != -1) {
		switch (arg) {
			case 'k':
				kcName = optarg;
				break;
			case 't':
				switch(optarg[0]) {
					case 'k':
						itemClass = CSSM_DL_DB_RECORD_PRIVATE_KEY;
						break;
					case 's':
						itemClass = CSSM_DL_DB_RECORD_SYMMETRIC_KEY;
						break;
					case 'b':
						itemClass = CSSM_DL_DB_RECORD_PUBLIC_KEY;
						break;
					case 'p':
						itemClass = kSecGenericPasswordItemClass;
						labelAttr = kSecLabelItemAttr;
						break;
					default:
						usage(argv);
				}
				break;
			case 'l':
				labelOrName = optarg;
				break;
			case 'd':
				dumpData = true;
				break;
			case 'e':
				editAcl = true;
				break;
			case 'p':
				newPassword = optarg;
				newPasswordLen = strlen(newPassword);
				break;
			case 's':
				simulateStickyRecord = true;
				break;
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	if(op == AO_ModifyPassword) {
		if(itemClass != kSecGenericPasswordItemClass) {
			printf("***You can only modify data on a password item.\n");
			exit(1);
		}
		if(newPassword == NULL) {
			printf("***You must supply new password data for this operation.\n");
			exit(1);
		}
	}
	if(kcName) {
		ortn = SecKeychainOpen(kcName, &kcRef);
		if(ortn) {
			cssmPerror("SecKeychainOpen", ortn);
			printf("***Error opening keychain %s. Aborting.\n", kcName);
			exit(1);
		}
	}
	
	switch(op) {
		case AO_Dump:
		case AO_Lookup:
		case AO_ModifyPassword:
			ortn = dumpAcls(kcRef, itemClass, labelAttr, labelOrName, dumpData, dumpAcl, editAcl,
				simulateStickyRecord, (unsigned char *)newPassword, newPasswordLen);
			break;
		case AO_Add:
			printf("Add ACL op to be implemented real soon now\n");
			ortn = -1;
			break;
	}
	if(kcRef) {
		CFRelease(kcRef);
	}
	return (int)ortn;
}
