/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  keychain_utilities.c
 *  security
 *
 *  Created by Michael Brouwer on Tue May 07 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "keychain_utilities.h"

#include <Security/cssmapi.h>
#include <Security/SecKeychainItem.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>

SecKeychainRef
keychain_open(const char *name)
{
	SecKeychainRef keychain = NULL;
	OSStatus result = SecKeychainOpen(name, &keychain);
	if (result)
	{
		fprintf(stderr, "SecKeychainOpen(%s) returned %ld(0x%lx)\n", name, result, result);
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
print_keychain_name(FILE *stream, SecKeychainRef keychain)
{
	int result = 0;
	char pathName[MAXPATHLEN];
	UInt32 ioPathLength = sizeof(pathName);
	OSStatus status = SecKeychainGetPath(keychain, &ioPathLength, pathName);
	if (status)
	{
		fprintf(stderr, "SecKeychainGetPath() returned %ld(0x%lx)\n", status, status);
		result = 1;
		goto loser;
	}

	print_buffer(stream, ioPathLength, pathName);

loser:
	return result;
}

int
print_keychain_item_attributes(FILE *stream, SecKeychainItemRef item, Boolean show_data, Boolean show_raw_data)
{
	int result = 0;
	unsigned int ix;
	OSStatus status;
	SecKeychainRef keychain = NULL;
	SecItemClass itemClass = 0;
	UInt32 itemID;
	SecKeychainAttributeList *attrList = NULL;
	SecKeychainAttributeInfo *info = NULL;
	UInt32 length = 0;
	void *data = NULL;

	status = SecKeychainItemCopyKeychain(item, &keychain);
	if (status)
	{
		fprintf(stderr, "SecKeychainItemCopyKeychain() returned %ld(0x%lx)\n", status, status);
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
		fprintf(stderr, "SecKeychainItemCopyAttributesAndData() returned %ld(0x%lx)\n", status, status);
		result = 1;
		goto loser;
	}

	fputs("class: ", stream);
	print_buffer(stream, 4, &itemClass);
	fputs("\nattributes:\n", stream);

	switch (itemClass)
	{
    case kSecInternetPasswordItemClass:
		itemID = CSSM_DL_DB_RECORD_INTERNET_PASSWORD;
		break;
    case kSecGenericPasswordItemClass:
		itemID = CSSM_DL_DB_RECORD_GENERIC_PASSWORD;
		break;
    case kSecAppleSharePasswordItemClass:
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
		fprintf(stderr, "SecKeychainAttributeInfoForItemID() returned %ld(0x%lx)\n", status, status);
		result = 1;
		goto loser;
	}

	status = SecKeychainItemCopyAttributesAndData(item, info, &itemClass, &attrList,
		show_data ? &length : NULL,
		show_data ? &data : NULL);
	if (status)
	{
		fprintf(stderr, "SecKeychainItemCopyAttributesAndData() returned %ld(0x%lx)\n", status, status);
		result = 1;
		goto loser;
	}

	if (info->count != attrList->count)
	{
		fprintf(stderr, "info count: %ld != attribute count: %ld\n", info->count, attrList->count);
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
			fprintf(stderr, "attribute %d of %ld info tag: %ld != attribute tag: %ld\n", ix, info->count, tag, attribute->tag);
			result = 1;
			goto loser;
		}

		fputs("    ", stream);
		print_buffer(stream, 4, &tag);
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
			fprintf(stream, "<format: %ld>", format);
			break;
		}
		fputs("=", stream);
		if (!attribute->length && !attribute->data)
			fputs("<NULL>", stream);
		else
			print_buffer(stream, attribute->length, attribute->data);
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
			fprintf(stderr, "SecKeychainItemGetDLDBHandle() returned %ld(0x%lx)\n", status, status);
			result = 1;
			goto loser;
		}

		status = SecKeychainItemGetUniqueRecordID(item, &uniqueRecordID);
		if (status)
		{
			fprintf(stderr, "SecKeychainItemGetUniqueRecordID() returned %ld(0x%lx)\n", status, status);
			result = 1;
			goto loser;
		}

		status = CSSM_DL_DataGetFromUniqueRecordId(dldbHandle, uniqueRecordID, NULL, &data);
		if (status)
		{
			fprintf(stderr, "CSSM_DL_DataGetFromUniqueRecordId() returned %ld(0x%lx)\n", status, status);
			result = 1;
			goto loser;
		}

		fputs("raw data:\n", stream);
		print_buffer(stream, data.Length, data.Data);
		fputc('\n', stream);

		/* @@@ Hmm which allocators should we use here? */
		free(data.Data);
	}

loser:
	if (attrList)
	{
		status = SecKeychainItemFreeAttributesAndData(attrList, data);
		if (status)
			fprintf(stderr, "SecKeychainItemFreeAttributesAndData() returned %ld(0x%lx)\n", status, status);
	}

	if (info)
	{
		status = SecKeychainFreeAttributeInfo(info);
		if (status)
			fprintf(stderr, "SecKeychainFreeAttributeInfo() returned %ld(0x%lx)\n", status, status);
	}

	if (keychain)
		CFRelease(keychain);

	return result;
}

static void
print_buffer_hex(FILE *stream, UInt32 length, void *data)
{
	uint8 *p = (uint8 *) data;
	while (length--)
	{
		int ch = *p++;
		fprintf(stream, "%02X", ch);
	}
}

static void
print_buffer_ascii(FILE *stream, UInt32 length, void *data)
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
print_buffer(FILE *stream, UInt32 length, void *data)
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
print_buffer_pem(FILE *stream, const char *headerString, UInt32 length, void *data)
{
	unsigned char *buf;
	unsigned bufLen;

	fprintf(stream, "-----BEGIN %s-----\n", headerString);
	buf = malloc_enc64_with_lines(data, length, 64, &bufLen);
	fwrite(buf, bufLen, 1, stream);
	free(buf);
	fprintf(stream, "-----END %s-----\n", headerString);
}
