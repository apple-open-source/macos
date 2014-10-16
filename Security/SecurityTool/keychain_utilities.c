/*
 * Copyright (c) 2003-2009,2012,2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 *
 * keychain_utilities.c
 */

#include "keychain_utilities.h"
#include "security.h"

#include <Security/cssmapi.h>
#include <Security/SecAccess.h>
#include <Security/SecACL.h>
#include <Security/SecTrustedApplication.h>
#include <Security/SecKeychainItem.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <libkern/OSByteOrder.h>

#include "readline.h"

// SecTrustedApplicationValidateWithPath
#include <Security/SecTrustedApplicationPriv.h>


void check_obsolete_keychain(const char *kcName)
{
	if(kcName == NULL) {
		return;
	}
	if(!strcmp(kcName, "/System/Library/Keychains/X509Anchors")) {
		fprintf(stderr, "***************************************************************\n");
		fprintf(stderr, "                         WARNING\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "The keychain you are accessing, X509Anchors, is no longer\n");
		fprintf(stderr, "used by Mac OS X as the system root certificate store.\n");
		fprintf(stderr, "Please read the security man page for information on the \n");
		fprintf(stderr, "add-trusted-cert command. New system root certificates should\n");
		fprintf(stderr, "be added to the Admin Trust Settings domain and to the \n");
		fprintf(stderr, "System keychain in /Library/Keychains.\n");
		fprintf(stderr, "***************************************************************\n");
	}
	else if(!strcmp(kcName, "/System/Library/Keychains/X509Certificates")) {
		fprintf(stderr, "***************************************************************\n");
		fprintf(stderr, "                         WARNING\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "The keychain you are accessing, X509Certificates, is no longer\n");
		fprintf(stderr, "used by Mac OS X as the system intermediate certificate\n");
		fprintf(stderr, "store. New system intermediate certificates should be added\n");
		fprintf(stderr, "to the System keychain in /Library/Keychains.\n");
		fprintf(stderr, "***************************************************************\n");
	}
}

SecKeychainRef
keychain_open(const char *name)
{
	SecKeychainRef keychain = NULL;
	OSStatus result;

	check_obsolete_keychain(name);
	if (name && name[0] != '/')
	{
		CFArrayRef dynamic = NULL;
		result = SecKeychainCopyDomainSearchList(
			kSecPreferencesDomainDynamic, &dynamic);
		if (result)
		{
			sec_error("SecKeychainCopyDomainSearchList %s: %s",
				name, sec_errstr(result));
			return NULL;
		}
		else
		{
			uint32_t i;
			uint32_t count = dynamic ? CFArrayGetCount(dynamic) : 0;

			for (i = 0; i < count; ++i)
			{
				char pathName[MAXPATHLEN];
				UInt32 ioPathLength = sizeof(pathName);
				bzero(pathName, ioPathLength);
				keychain = (SecKeychainRef)CFArrayGetValueAtIndex(dynamic, i);
				result = SecKeychainGetPath(keychain, &ioPathLength, pathName);
				if (result)
				{
					sec_error("SecKeychainGetPath %s: %s",
						name, sec_errstr(result));
					return NULL;
				}
				if (!strncmp(pathName, name, ioPathLength))
				{
					CFRetain(keychain);
					CFRelease(dynamic);
					return keychain;
				}
			}
			CFRelease(dynamic);
		}
	}

	result = SecKeychainOpen(name, &keychain);
	if (result)
	{
		sec_error("SecKeychainOpen %s: %s", name, sec_errstr(result));
	}

	return keychain;
}

CFTypeRef
keychain_create_array(int argc, char * const *argv)
{
	if (argc == 0)
		return NULL;
	else if (argc == 1)
		return keychain_open(argv[0]);
	else
	{
		CFMutableArrayRef keychains = CFArrayCreateMutable(NULL, argc, &kCFTypeArrayCallBacks);
		int ix;
		for (ix = 0; ix < argc; ++ix)
		{
			SecKeychainRef keychain = keychain_open(argv[ix]);
			if (keychain)
			{
				CFArrayAppendValue(keychains, keychain);
				CFRelease(keychain);
			}
		}

		return keychains;
	}
}

int
parse_fourcharcode(const char *name, UInt32 *code)
{
	UInt32 cc = 0;
	int len = (name) ? strlen(name) : 0;

	// error check the name
	if (len != 4)
	{
		fprintf(stderr, "Error: four-character types must be exactly 4 characters long.\n");
		if (len == 3) {
			fprintf(stderr, "(Try \"%s \" instead of \"%s\")\n", name, name);
		}
		return 1;
	}

	int i;
	for (i = 0; i < 4; ++i)
	{
		cc = (cc << 8) | name[i];
	}

	*code = cc; // note: this is in host byte order, suitable for passing to APIs

	return 0;
}

int
print_keychain_name(FILE *stream, SecKeychainRef keychain)
{
	int result = 0;
	char pathName[MAXPATHLEN];
	UInt32 ioPathLength = sizeof(pathName);
	OSStatus status = SecKeychainGetPath(keychain, &ioPathLength, pathName);
	if (status)
	{
		sec_perror("SecKeychainGetPath", status);
		result = 1;
		goto loser;
	}

	print_buffer(stream, ioPathLength, pathName);

loser:
	return result;
}

static void
print_cfdata(FILE *stream, CFDataRef data)
{
	if (data)
		return print_buffer(stream, CFDataGetLength(data), CFDataGetBytePtr(data));
	else
		fprintf(stream, "<NULL>");
}

static void
print_cfstring(FILE *stream, CFStringRef string)
{
	if (!string)
		fprintf(stream, "<NULL>");
	else
	{
		const char *utf8 = CFStringGetCStringPtr(string, kCFStringEncodingUTF8);
		if (utf8)
			fprintf(stream, "%s", utf8);
		else
		{
			CFRange rangeToProcess = CFRangeMake(0, CFStringGetLength(string));
			while (rangeToProcess.length > 0)
			{
				UInt8 localBuffer[256];
				CFIndex usedBufferLength;
				CFIndex numChars = CFStringGetBytes(string, rangeToProcess,
					kCFStringEncodingUTF8, '?', FALSE, localBuffer,
					sizeof(localBuffer), &usedBufferLength);
				if (numChars == 0)
					break;   // Failed to convert anything...

				fprintf(stream, "%.*s", (int)usedBufferLength, localBuffer);
				rangeToProcess.location += numChars;
				rangeToProcess.length -= numChars;
			}
		}
	}
}

static int
print_access(FILE *stream, SecAccessRef access, Boolean interactive)
{
	CFArrayRef aclList = NULL;
	CFIndex aclix, aclCount;
	int result = 0;
	OSStatus status;

	status = SecAccessCopyACLList(access, &aclList);
	if (status)
	{
		sec_perror("SecAccessCopyACLList", status);
		result = 1;
		goto loser;
	}

	aclCount = CFArrayGetCount(aclList);
	fprintf(stream, "access: %lu entries\n", aclCount);
	for (aclix = 0; aclix < aclCount; ++aclix)
	{
		CFArrayRef applicationList = NULL;
		CFStringRef description = NULL;
		CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR promptSelector = {};
		CFIndex appix, appCount;

		SecACLRef acl = (SecACLRef)CFArrayGetValueAtIndex(aclList, aclix);
		CSSM_ACL_AUTHORIZATION_TAG tags[64]; // Pick some upper limit
		uint32 tagix, tagCount = sizeof(tags) / sizeof(*tags);
		status = SecACLGetAuthorizations(acl, tags, &tagCount);
		if (status)
		{
			sec_perror("SecACLGetAuthorizations", status);
			result = 1;
			goto loser;
		}

		fprintf(stream, "    entry %lu:\n        authorizations (%lu):", aclix,
			(unsigned long)tagCount);
		for (tagix = 0; tagix < tagCount; ++tagix)
		{
			CSSM_ACL_AUTHORIZATION_TAG tag = tags[tagix];
			switch (tag)
			{
			case CSSM_ACL_AUTHORIZATION_ANY:
				fputs(" any", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_LOGIN:
				fputs(" login", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_GENKEY:
				fputs(" genkey", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_DELETE:
				fputs(" delete", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED:
				fputs(" export_wrapped", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR:
				fputs(" export_clear", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_IMPORT_WRAPPED:
				fputs(" import_wrapped", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_IMPORT_CLEAR:
				fputs(" import_clear", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_SIGN:
				fputs(" sign", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_ENCRYPT:
				fputs(" encrypt", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_DECRYPT:
				fputs(" decrypt", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_MAC:
				fputs(" mac", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_DERIVE:
				fputs(" derive", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_DBS_CREATE:
				fputs(" dbs_create", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_DBS_DELETE:
				fputs(" dbs_delete", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_DB_READ:
				fputs(" db_read", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_DB_INSERT:
				fputs(" db_insert", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_DB_MODIFY:
				fputs(" db_modify", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_DB_DELETE:
				fputs(" db_delete", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_CHANGE_ACL:
				fputs(" change_acl", stream);
				break;
			case CSSM_ACL_AUTHORIZATION_CHANGE_OWNER:
				fputs(" change_owner", stream);
				break;
			default:
				fprintf(stream, " tag=%lu", (unsigned long)tag);
				break;
			}
		}
		fputc('\n', stream);

		status = SecACLCopySimpleContents(acl, &applicationList, &description, &promptSelector);
		if (status)
		{
			sec_perror("SecACLCopySimpleContents", status);
			continue;
		}

		if (promptSelector.flags & CSSM_ACL_KEYCHAIN_PROMPT_REQUIRE_PASSPHRASE)
			fputs("        require-password\n", stream);
		else
			fputs("        don't-require-password\n", stream);

		fputs("        description: ", stream);
		print_cfstring(stream, description);
		fputc('\n', stream);

		if (applicationList)
		{
			appCount = CFArrayGetCount(applicationList);
			fprintf(stream, "        applications (%lu):\n", appCount);
		}
		else
		{
			appCount = 0;
			fprintf(stream, "        applications: <null>\n");
		}

		for (appix = 0; appix < appCount; ++appix)
		{
			const UInt8* bytes;
			SecTrustedApplicationRef app = (SecTrustedApplicationRef)CFArrayGetValueAtIndex(applicationList, appix);
			CFDataRef data = NULL;
			fprintf(stream, "            %lu: ", appix);
			status = SecTrustedApplicationCopyData(app, &data);
			if (status)
			{
				sec_perror("SecTrustedApplicationCopyData", status);
				continue;
			}

			bytes = CFDataGetBytePtr(data);
			if (bytes && bytes[0] == 0x2f) {
				fprintf(stream, "%s", (const char *)bytes);
				if ((status = SecTrustedApplicationValidateWithPath(app, (const char *)bytes)) == noErr) {
					fprintf(stream, " (OK)");
				} else {
					fprintf(stream, " (status %d)", (int)status);
				}
				fprintf(stream, "\n");
			} else {
				print_cfdata(stream, data);
				fputc('\n', stream);
			}
			if (data)
				CFRelease(data);
		}

		if (applicationList)
			CFRelease(applicationList);

		if (description)
			CFRelease(description);

		if (interactive)
		{
			char buffer[10] = {};
			fprintf(stderr, "Remove this acl? ");
			if (readline(buffer, sizeof(buffer)) && buffer[0] == 'y')
			{
				fprintf(stderr, "removing acl\n");
				status = SecACLRemove(acl);
				if (status)
				{
					sec_perror("SecACLRemove", status);
					continue;
				}
			}
		}
	}

loser:
	if (aclList)
		CFRelease(aclList);

	return result;
}

int
print_keychain_item_attributes(FILE *stream, SecKeychainItemRef item, Boolean show_data, Boolean show_raw_data, Boolean show_acl, Boolean interactive)
{
	int result = 0;
	unsigned int ix;
	OSStatus status;
	SecKeychainRef keychain = NULL;
	SecAccessRef access = NULL;
	SecItemClass itemClass = 0;
	UInt32 itemID;
	SecKeychainAttributeList *attrList = NULL;
	SecKeychainAttributeInfo *info = NULL;
	UInt32 length = 0;
	void *data = NULL;

	status = SecKeychainItemCopyKeychain(item, &keychain);
	if (status)
	{
		sec_perror("SecKeychainItemCopyKeychain", status);
		result = 1;
		goto loser;
	}

	fputs("keychain: ", stream);
	result = print_keychain_name(stream, keychain);
	fputc('\n', stream);
	if (result)
		goto loser;

	/* First find out the item class. */
	status = SecKeychainItemCopyAttributesAndData(item, NULL, &itemClass, NULL, NULL, NULL);
	if (status)
	{
		sec_perror("SecKeychainItemCopyAttributesAndData", status);
		result = 1;
		goto loser;
	}

	fputs("class: ", stream);
	char buffer[4];
	buffer[3] = itemClass & 0xFF;
	buffer[2] = (itemClass >> 8) & 0xFF;
	buffer[1] = (itemClass >> 16) & 0xFF;
	buffer[0] = (itemClass >> 24) & 0xFF;

	print_buffer(stream, 4, buffer);
	fputs("\nattributes:\n", stream);

	switch (itemClass)
	{
    case kSecInternetPasswordItemClass:
		itemID = CSSM_DL_DB_RECORD_INTERNET_PASSWORD;
		break;
    case kSecGenericPasswordItemClass:
		itemID = CSSM_DL_DB_RECORD_GENERIC_PASSWORD;
		break;
    case 'ashp': /* kSecAppleSharePasswordItemClass */
		itemID = CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD;
		break;
	default:
		itemID = itemClass;
		break;
	}

	/* Now get the AttributeInfo for it. */
	status = SecKeychainAttributeInfoForItemID(keychain, itemID, &info);
	if (status)
	{
		sec_perror("SecKeychainAttributeInfoForItemID", status);
		result = 1;
		goto loser;
	}

	status = SecKeychainItemCopyAttributesAndData(item, info, &itemClass, &attrList,
		show_data ? &length : NULL,
		show_data ? &data : NULL);
	if (status)
	{
		sec_perror("SecKeychainItemCopyAttributesAndData", status);
		result = 1;
		goto loser;
	}

	if (info->count != attrList->count)
	{
		sec_error("info count: %ld != attribute count: %ld", info->count, attrList->count);
		result = 1;
		goto loser;
	}

	for (ix = 0; ix < info->count; ++ix)
	{
		UInt32 tag = info->tag[ix];
		UInt32 format = info->format[ix];
		SecKeychainAttribute *attribute = &attrList->attr[ix];
		if (tag != attribute->tag)
		{
			sec_error("attribute %d of %ld info tag: %ld != attribute tag: %ld", ix, info->count, tag, attribute->tag);
			result = 1;
			goto loser;
		}

		fputs("    ", stream);
		print_uint32(stream, tag);
		switch (format)
		{
		case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
			fputs("<string>", stream);
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:
			fputs("<sint32>", stream);
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
			fputs("<uint32>", stream);
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM:
			fputs("<bignum>", stream);
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_REAL:
			fputs("<real>", stream);
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE:
			fputs("<timedate>", stream);
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:
			fputs("<blob>", stream);
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32:
			fputs("<uint32>", stream);
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX:
			fputs("<complex>", stream);
			break;
		default:
			fprintf(stream, "<format: %d>", (int)format);
			break;
		}
		fputs("=", stream);
		if (!attribute->length && !attribute->data)
			fputs("<NULL>", stream);
		else
		{	switch (format)
			{
				case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:
				case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
				{
					print_uint32(stream, *(UInt32*) attribute->data);
					break;
				}

				case CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32:
				{
					int n = attribute->length / sizeof(UInt32);
					UInt32* ptr = (UInt32*) attribute->data;

					while (n--)
					{
						print_uint32(stream, *ptr++);
					}
				}
				break;

				default:
				{
					print_buffer(stream, attribute->length, attribute->data);
				}
				break;
			}
		}
		fputc('\n', stream);
	}

	if (show_data)
	{
		fputs("data:\n", stream);
		print_buffer(stream, length, data);
		fputc('\n', stream);
	}

	if (show_raw_data)
	{
		CSSM_DL_DB_HANDLE dldbHandle = {};
		const CSSM_DB_UNIQUE_RECORD *uniqueRecordID = NULL;
		CSSM_DATA data = {};
		status = SecKeychainItemGetDLDBHandle(item, &dldbHandle);
		if (status)
		{
			sec_perror("SecKeychainItemGetDLDBHandle", status);
			result = 1;
			goto loser;
		}

		status = SecKeychainItemGetUniqueRecordID(item, &uniqueRecordID);
		if (status)
		{
			sec_perror("SecKeychainItemGetUniqueRecordID", status);
			result = 1;
			goto loser;
		}

		status = CSSM_DL_DataGetFromUniqueRecordId(dldbHandle, uniqueRecordID, NULL, &data);
		if (status)
		{
			sec_perror("CSSM_DL_DataGetFromUniqueRecordId", status);
			result = 1;
			goto loser;
		}

		fputs("raw data:\n", stream);
		print_buffer(stream, data.Length, data.Data);
		fputc('\n', stream);

		/* @@@ Hmm which allocators should we use here? */
		free(data.Data);
	}

	if (show_acl)
	{
		status = SecKeychainItemCopyAccess(item, &access);
		if (status == errSecNoAccessForItem)
			fprintf(stream, "no access control for this item\n");
		else
		{
			if (status)
			{
				sec_perror("SecKeychainItemCopyAccess", status);
				result = 1;
				goto loser;
			}

			result = print_access(stream, access, interactive);
			if (result)
				goto loser;

			if (interactive)
			{
				char buffer[10] = {};
				fprintf(stderr, "Update access? ");
				if (readline(buffer, sizeof(buffer)) && buffer[0] == 'y')
				{
					fprintf(stderr, "Updating access\n");
					status = SecKeychainItemSetAccess(item, access);
					if (status)
					{
						sec_perror("SecKeychainItemSetAccess", status);
						result = 1;
						goto loser;
					}
				}
			}
		}
	}

loser:
	if (access)
		CFRelease(access);

	if (attrList)
	{
		status = SecKeychainItemFreeAttributesAndData(attrList, data);
		if (status)
			sec_perror("SecKeychainItemFreeAttributesAndData", status);
	}

	if (info)
	{
		status = SecKeychainFreeAttributeInfo(info);
		if (status)
			sec_perror("SecKeychainFreeAttributeInfo", status);
	}

	if (keychain)
		CFRelease(keychain);

	return result;
}

static void
print_buffer_hex(FILE *stream, UInt32 length, const void *data)
{
	uint8 *p = (uint8 *) data;
	while (length--)
	{
		int ch = *p++;
		fprintf(stream, "%02X", ch);
	}
}

static void
print_buffer_ascii(FILE *stream, UInt32 length, const void *data)
{
	uint8 *p = (uint8 *) data;
	while (length--)
	{
		int ch = *p++;
		if (ch >= ' ' && ch <= '~' && ch != '\\')
		{
			fputc(ch, stream);
		}
		else
		{
			fputc('\\', stream);
			fputc('0' + ((ch >> 6) & 7), stream);
			fputc('0' + ((ch >> 3) & 7), stream);
			fputc('0' + ((ch >> 0) & 7), stream);
		}
	}
}

void
print_buffer(FILE *stream, UInt32 length, const void *data)
{
	uint8 *p = (uint8 *) data;
	Boolean hex = FALSE;
	Boolean ascii = FALSE;
	UInt32 ix;
	for (ix = 0; ix < length; ++ix)
	{
		int ch = *p++;
		if (ch >= ' ' && ch <= '~' && ch != '\\')
			ascii = TRUE;
		else
			hex = TRUE;
	}

	if (hex)
	{
		fputc('0', stream);
		fputc('x', stream);
		print_buffer_hex(stream, length, data);
		if (ascii)
			fputc(' ', stream);
			fputc(' ', stream);
	}
	if (ascii)
	{
		fputc('"', stream);
		print_buffer_ascii(stream, length, data);
		fputc('"', stream);
	}
}

void
print_uint32(FILE *stream, uint32 n)
{
	n = OSSwapHostToBigInt32 (n);
	print_buffer(stream, sizeof(UInt32), &n);
}

unsigned char
hexValue(char c)
{
	static const char digits[] = "0123456789abcdef";
	char *p;
	if ((p = strchr(digits, tolower(c))))
		return p - digits;
	else
		return 0;
}

void
fromHex(const char *hexDigits, CSSM_DATA *data)
{
	size_t bytes = strlen(hexDigits) / 2;	// (discards malformed odd end)
	if (bytes > data->Length)
		return;
	//	length(bytes);	// (will assert if we try to grow it)
	size_t n;
	for (n = 0; n < bytes; n++) {
		data->Data[n] = (uint8)(hexValue(hexDigits[2*n]) << 4 | hexValue(hexDigits[2*n+1]));
	}
}

void
safe_CFRelease(void *cfTypeRefPtr)
{
	CFTypeRef *obj = (CFTypeRef *)cfTypeRefPtr;
	if (obj && *obj) {
		CFRelease(*obj);
		*obj = NULL;
	}
}

/*
 * map a 6-bit binary value to a printable character.
 */
static const
unsigned char bintoasc[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * map 6 bits to a printing char
 */
#define ENC(c) (bintoasc[((c) & 0x3f)])

#define PAD		'='

/*
 * map one group of up to 3 bytes at inp to 4 bytes at outp.
 * Count is number of valid bytes in *inp; if less than 3, the
 * 1 or two extras must be zeros.
 */
static void
encChunk(const unsigned char *inp,
	unsigned char *outp,
	int count)
{
	unsigned char c1, c2, c3, c4;

	c1 = *inp >> 2;
	c2 = ((inp[0] << 4) & 0x30) | ((inp[1] >> 4) & 0xf);
	c3 = ((inp[1] << 2) & 0x3c) | ((inp[2] >> 6) & 0x3);
	c4 = inp[2] & 0x3f;
	*outp++ = ENC(c1);
	*outp++ = ENC(c2);
	if (count == 1) {
	    *outp++ = PAD;
	    *outp   = PAD;
	} else {
	    *outp++ = ENC(c3);
	    if (count == 2) {
		*outp = PAD;
	    }
	    else {
		*outp = ENC(c4);
	    }
	}
}

static unsigned char *
malloc_enc64_with_lines(const unsigned char *inbuf,
	unsigned inlen,
	unsigned linelen,
	unsigned *outlen)
{
	unsigned		outTextLen;
	unsigned 		len;			// to malloc, liberal
	unsigned		olen = 0;		// actual output size
	unsigned char 	*outbuf;
	unsigned char 	endbuf[3];
	unsigned		i;
	unsigned char 	*outp;
	unsigned		numLines;
	unsigned		thisLine;

	outTextLen = ((inlen + 2) / 3) * 4;
	if(linelen) {
	    /*
	     * linelen must be 0 mod 4 for this to work; round up...
	     */
	    if((linelen & 0x03) != 0) {
	        linelen = (linelen + 3) & 0xfffffffc;
	    }
	    numLines = (outTextLen + linelen - 1)/ linelen;
	}
	else {
	    numLines = 1;
	}

	/*
	 * Total output size = encoded text size plus one newline per
	 * line of output, plus trailing NULL. We always generate newlines
	 * as \n; when decoding, we tolerate \r\n (Microsoft) or \n.
	 */
	len = outTextLen + (2 * numLines) + 1;
	outbuf = (unsigned char*)malloc(len);
	outp = outbuf;
	thisLine = 0;

	while(inlen) {
	    if(inlen < 3) {
			for(i=0; i<3; i++) {
				if(i < inlen) {
					endbuf[i] = inbuf[i];
				}
				else {
					endbuf[i] = 0;
				}
			}
			encChunk(endbuf, outp, inlen);
			inlen = 0;
	    }
	    else {
			encChunk(inbuf, outp, 3);
			inlen -= 3;
			inbuf += 3;
	    }
	    outp += 4;
	    thisLine += 4;
	    olen += 4;
	    if((linelen != 0) && (thisLine >= linelen) && inlen) {
	        /*
			 * last trailing newline added below
			 * Note we don't split 4-byte output chunks over newlines
			 */
	    	*outp++ = '\n';
			olen++;
			thisLine = 0;
	    }
	}
	*outp++ = '\n';
	olen += 1;
	*outlen = olen;
	return outbuf;
}

void
print_buffer_pem(FILE *stream, const char *headerString, UInt32 length, const void *data)
{
	unsigned char *buf;
	unsigned bufLen;

	if (headerString)
		fprintf(stream, "-----BEGIN %s-----\n", headerString);
	buf = malloc_enc64_with_lines(data, length, 64, &bufLen);
	fwrite(buf, bufLen, 1, stream);
	free(buf);
	if (headerString)
		fprintf(stream, "-----END %s-----\n", headerString);
}
