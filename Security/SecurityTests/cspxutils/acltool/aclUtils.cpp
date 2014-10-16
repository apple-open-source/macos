/*
 * Copyright (c) 2004-2006 Apple Computer, Inc. All Rights Reserved.
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
 */

/*
 * aclUtils.cpp - ACL utility functions, copied from the SecurityTool project. 
 */
 
#include "aclUtils.h"
#include <Security/SecTrustedApplicationPriv.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

/* Read a line from stdin into buffer as a null terminated string.  If buffer is
   non NULL use at most buffer_size bytes and return a pointer to buffer.  Otherwise
   return a newly malloced buffer.
   if EOF is read this function returns NULL.  */
char *
readline(char *buffer, int buffer_size)
{
	int ix = 0, bytes_malloced = 0;

	if (!buffer)
	{
		bytes_malloced = 64;
		buffer = (char *)malloc(bytes_malloced);
		buffer_size = bytes_malloced;
	}

	for (;;++ix)
	{
		int ch;

		if (ix == buffer_size - 1)
		{
			if (!bytes_malloced)
				break;
			bytes_malloced += bytes_malloced;
			buffer = (char *)realloc(buffer, bytes_malloced);
			buffer_size = bytes_malloced;
		}

		ch = getchar();
		if (ch == EOF)
		{
			if (bytes_malloced)
				free(buffer);
			return NULL;
		}
		if (ch == '\n')
			break;
		buffer[ix] = ch;
	}

	/* 0 terminate buffer. */
	buffer[ix] = '\0';

	return buffer;
}

void
print_buffer_hex(FILE *stream, UInt32 length, const void *data)
{
	unsigned i;
	const unsigned char *cp = (const unsigned char *)data;
	
	printf("\n   ");
	for(i=0; i<length; i++) {
		fprintf(stream, "%02X ", cp[i]);
		if((i % 24) == 23) {
			printf("\n   ");
		}
	}
}

void
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
	Boolean ascii = TRUE;		// unless we determine otherwise
	UInt32 ix;
	for (ix = 0; ix < length; ++ix) {
		int ch = *p++;
		if ((ch < ' ') || (ch > '~')) {
			if((ch == 0) && (ix == (length - 1))) {
				/* ignore trailing null */
				length--;
				break;
			}
			ascii = FALSE;
			break;
		}
	}

	if (ascii) {
		fputc('"', stream);
		print_buffer_ascii(stream, length, data);
		fputc('"', stream);
	}
	else {
		print_buffer_hex(stream, length, data);
	}
}

void
print_cfdata(FILE *stream, CFDataRef data)
{
	if (data)
		return print_buffer(stream, CFDataGetLength(data), CFDataGetBytePtr(data));
	else
		fprintf(stream, "<NULL>");
}

void
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


int
print_access(FILE *stream, SecAccessRef access, Boolean interactive)
{
	CFArrayRef aclList = NULL;
	CFIndex aclix, aclCount;
	int result = 0;
	OSStatus status;

	status = SecAccessCopyACLList(access, &aclList);
	if (status)
	{
		cssmPerror("SecAccessCopyACLList", status);
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
			cssmPerror("SecACLGetAuthorizations", status);
			result = 1;
			goto loser;
		}

		fprintf(stream, "    entry %lu:\n        authorizations (%lu):", aclix, (unsigned long)tagCount);
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
			cssmPerror("SecACLCopySimpleContents", status);
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
				cssmPerror("SecTrustedApplicationCopyData", status);
				continue;
			}

			bytes = CFDataGetBytePtr(data);
			if (bytes && bytes[0] == 0x2f) {
				fprintf(stream, "%s", (const char *)bytes);
				if ((status = SecTrustedApplicationValidateWithPath(app, (const char *)bytes)) == noErr) {
					fprintf(stream, " (OK)");
				} else {
					fprintf(stream, " (status %ld)", status);
				}
				fprintf(stream, "\n");
			} else {
				print_cfdata(stream, data);
				fputc('\n', stream);
			}
			if (data)
				CFRelease(data);
		}


		if (interactive)
		{
			char buffer[10] = {};
			if(applicationList != NULL) {
				fprintf(stderr, "NULL out this application list? ");
				if (readline(buffer, sizeof(buffer)) && buffer[0] == 'y')
				{
					/* 
					 * This makes the ops in this entry wide-open, no dialog or confirmation 
					 * other than requiring the keychain be open.
					 */
					fprintf(stderr, "setting app list to NULL\n");
					status = SecACLSetSimpleContents(acl, NULL, description, &promptSelector);
					if (status)
					{
						cssmPerror("SecACLSetSimpleContents", status);
						continue;
					}
				}
				else {
					fprintf(stderr, "Set this application list to empty array? ");
					if (readline(buffer, sizeof(buffer)) && buffer[0] == 'y')
					{
						/* 
						 * This means "always get confirmation, from all apps".
						 */
						fprintf(stderr, "setting app list to empty array\n");
						status = SecACLSetSimpleContents(acl, 
							CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks), 
							description, &promptSelector);
						if (status)
						{
							cssmPerror("SecACLSetSimpleContents", status);
							continue;
						}
					}
				}
			}
			else {
				fprintf(stderr, "Remove this acl? ");
				if (readline(buffer, sizeof(buffer)) && buffer[0] == 'y')
				{
					/* 
					 * This make ths ops in this entry completely inaccessible. 
					 */
					fprintf(stderr, "removing acl\n");
					status = SecACLRemove(acl);
					if (status)
					{
						cssmPerror("SecACLRemove", status);
						continue;
					}
				}
			}
		}
		if (description)
			CFRelease(description);
		if (applicationList)
			CFRelease(applicationList);

	}

loser:
	if (aclList)
		CFRelease(aclList);

	return result;
}

/* Simluate what StickyRecord is trying to do.... */

/* 
 * Given an Access object:
 *  -- extract the ACL for the specified CSSM_ACL_AUTHORIZATION_TAG. We expect there
 *     to exactly one of these - if the form of a default ACL changes we'll have to 
 *	   revisit this.
 *  -- set the ACL's app list to the provided CFArray, which may be NULL (meaning 
 *     "any app can access this, no problem"), an empty array (meaning "always
 *     prompt"), or an actual app list. 
 *  -- set or clear the PROMPT_REQUIRE_PASSPHRASE bit per the requirePassphrase
 *     argument
 */
static OSStatus srUpdateAcl(
	SecAccessRef accessRef,
	CSSM_ACL_AUTHORIZATION_TAG whichAcl,	// e.g. CSSM_ACL_AUTHORIZATION_DECRYPT
	CFArrayRef appArray,
	bool requirePassphrase)
{
	OSStatus ortn;
	CFArrayRef aclList = NULL;
	
	ortn = SecAccessCopySelectedACLList(accessRef, whichAcl, &aclList);
	if(ortn) {
		cssmPerror("SecAccessCopySelectedACLList", ortn);
		return ortn;
	}
	
	if(CFArrayGetCount(aclList) != 1) {
		printf("StickyRecord::updateAcl - unexpected ACL list count (%d)",
			(int)CFArrayGetCount(aclList));
		return internalComponentErr;
	}
	SecACLRef acl = (SecACLRef)CFArrayGetValueAtIndex(aclList, 0);
	
	CFArrayRef applicationList = NULL;
	CFStringRef description = NULL;
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR promptSelector = {};
	ortn = SecACLCopySimpleContents(acl, &applicationList, &description, &promptSelector);
	if(ortn) {
		cssmPerror("SecACLCopySimpleContents", ortn);
		return ortn;
	}
	if(applicationList != NULL) {
		CFRelease(applicationList);
	}
	if(requirePassphrase) {
		promptSelector.flags |= CSSM_ACL_KEYCHAIN_PROMPT_REQUIRE_PASSPHRASE;
	}
	else {
		promptSelector.flags &= ~CSSM_ACL_KEYCHAIN_PROMPT_REQUIRE_PASSPHRASE;
	}
	/* update */
	ortn = SecACLSetSimpleContents(acl, appArray, description, &promptSelector);
	
	/* we got this from SecACLCopySimpleContents - release it regardless */
	if(description != NULL) {
		CFRelease(description);
	}
	if(ortn) {
		cssmPerror("SecACLSetSimpleContents", ortn);
	}
	if(aclList != NULL) {
		CFRelease(aclList);
	}
	return ortn;
}

OSStatus stickyRecordUpdateAcl(
	SecAccessRef accessRef)
{
	OSStatus ortn;
	
	printf("...updating ACL to simulate a StickyRecord\n");
	
	/* First: decrypt. Wide open (NULL app list), !REQUIRE_PASSPHRASE. */
	ortn = srUpdateAcl(accessRef, CSSM_ACL_AUTHORIZATION_DECRYPT, NULL, false);
	if(ortn) {
		return ortn;
	}
	
	/* encrypt: always ask (empty app list, require passphrase */
	CFArrayRef nullArray = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
	return srUpdateAcl(accessRef, CSSM_ACL_AUTHORIZATION_ENCRYPT, nullArray, true);
	
}
