/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
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

#include "webdavd.h"

#include <CoreFoundation/CoreFoundation.h>
#include <sys/dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include "webdav_parse.h"
#include "webdav_cache.h"
#include "webdav_network.h"
#include "LogMessage.h"

extern long long
strtoq(const char *, char **, int);

/*****************************************************************************/

/* local function prototypes */

static int from_base64(const char *base64str, unsigned char *outBuffer, size_t *lengthptr);
static void parser_file_count_create(void *ctx,
									 const xmlChar *localname,
									 const xmlChar *prefix,
									 const xmlChar *URI,
									 int nb_namespaces,
									 const xmlChar **namespaces,
									 int nb_attributes,
									 int nb_defaulted,
									 const xmlChar **attributes);
static void parser_stat_create(void *ctx,
							   const xmlChar *localname,
							   const xmlChar *prefix,
							   const xmlChar *URI,
							   int nb_namespaces,
							   const xmlChar **namespaces,
							   int nb_attributes,
							   int nb_defaulted,
							   const xmlChar **attributes);
static void parser_statfs_create(void *ctx,
								 const xmlChar *localname,
								 const xmlChar *prefix,
								 const xmlChar *URI,
								 int nb_namespaces,
								 const xmlChar **namespaces,
								 int nb_attributes,
								 int nb_defaulted,
								 const xmlChar **attributes);
static void parser_lock_create(void *ctx,
							   const xmlChar *localname,
							   const xmlChar *prefix,
							   const xmlChar *URI,
							   int nb_namespaces,
							   const xmlChar **namespaces,
							   int nb_attributes,
							   int nb_defaulted,
							   const xmlChar **attributes);
void parser_opendir_create (void *ctx,
							const xmlChar *localname,
							const xmlChar *prefix,
							const xmlChar *URI,
							int nb_namespaces,
							const xmlChar **namespaces,
							int nb_attributes,
							int nb_defaulted,
							const xmlChar **attributes);
static void parser_lock_add(void *ctx, const xmlChar *localname, int length);
static void parser_stat_add(void *ctx, const xmlChar *localname, int length);
static void parser_statfs_add(void *ctx, const xmlChar *localname, int length);
void parser_opendir_add(void *ctx, const xmlChar *localname, int length);
void parse_stat_end(void *ctx,
					const xmlChar *localname,
					const xmlChar *prefix,
					const xmlChar *URI);
void parser_statfs_end(void *ctx,
					   const xmlChar *localname,
					   const xmlChar *prefix,
					   const xmlChar *URI);
void parser_lock_end(void *ctx,
					 const xmlChar *localname,
					 const xmlChar *prefix,
					 const xmlChar *URI);
void parser_opendir_end(void *ctx,
						const xmlChar *localname,
						const xmlChar *prefix,
						const xmlChar *URI);
void parser_file_count_end(void *ctx,
						   const xmlChar *localname,
						   const xmlChar *prefix,
						   const xmlChar *URI);
void parser_cachevalidators_end(void *ctx,
								const xmlChar *localname,
								const xmlChar *prefix,
								const xmlChar *URI);
void parser_multistatus_end(void *ctx,
							const xmlChar *localname,
							const xmlChar *prefix,
							const xmlChar *URI);
/*****************************************************************************/
/* The from_base64 function decodes a base64 encoded c-string into outBuffer.
 * The outBuffer's size is *lengthptr. The actual number of bytes decoded into
 * outBuffer is also returned in *lengthptr. If outBuffer is large enough to
 * decode the base64 string and if the base64 encoding is valid, from_base64()
 * returns 0; otherwise -1 is returned. Note that outBuffer is just an array of
 * bytes... it is not a c-string.
 */
static int from_base64(const char *base64str, unsigned char *outBuffer, size_t *lengthptr)
{
	char			decodedChar;
	unsigned long	base64Length;
	unsigned char	*eightBitByte;
	unsigned char	sixBitEncoding[4];
	unsigned short	encodingIndex;
	int				endOfData;
	const char		*equalPtr;
	const char		*base64CharPtr;
	const char		*base64EndPtr;
	
	/* Determine the length of the base64 input string.
	 * This also catches illegal '=' characters within a base64 string.
	 */
	
	base64Length = 0;
	
	/* is there an '=' character? */
	equalPtr = strchr(base64str, '=');
	if ( equalPtr != NULL )
	{
		/* yes -- then it must be the last character of an octet, or
		 * it must be the next to last character of an octet followed
		 * by another '=' character */
		switch ( (equalPtr - base64str) % 4 )
		{
			case 0:
			case 1:
				/* invalid encoding */
				goto error_exit;
				break;
				
			case 2:
				if ( equalPtr[1] != '=' )
				{
					/* invalid encoding */
					goto error_exit;
				}
				base64Length = (equalPtr - base64str) + 2;
				*lengthptr += 2;	/* adjust for padding */
				break;
				
			case 3:
				base64Length = (equalPtr - base64str) + 1;
				*lengthptr += 1;	/* adjust for padding */
				break;
		}
	}
	else
	{
		base64Length = strlen(base64str);
	}
	
	/* Make sure outBuffer is big enough */
	if ( *lengthptr < ((base64Length / 4) * 3) )
	{
		/* outBuffer is too small */
		goto error_exit;
	}
	
	/* Make sure length is a multiple of 4 */
	if ( (base64Length % 4) != 0 )
	{
		/* invalid encoding */
		goto error_exit;
	}
	
	/* OK -- */
	eightBitByte = outBuffer;
	encodingIndex = 0;
	endOfData = FALSE;
	base64EndPtr = (char *)((unsigned long)base64str + base64Length);
	base64CharPtr = base64str;
	while ( base64CharPtr < base64EndPtr )
	{
		decodedChar = *base64CharPtr++;
		
		if ( (decodedChar >= 'A') && (decodedChar <= 'Z') )
		{
			decodedChar = decodedChar - 'A';
		}
		else if ( (decodedChar >= 'a') && (decodedChar <= 'z') )
		{
			decodedChar = decodedChar - 'a' + 26;
		}
		else if ( (decodedChar >= '0') && (decodedChar <= '9') )
		{
			decodedChar = decodedChar - '0' + 52;
		}
		else if ( decodedChar == '+' )
		{
			decodedChar = 62;
		}
		else if ( decodedChar == '/' )
		{
			decodedChar = 63;
		}
		else if ( decodedChar == '=' ) /* end of base64 encoding */
		{
			endOfData = TRUE;
		}
		else
		{
			/* invalid character */
			goto error_exit;
		}
		
		if ( endOfData )
		{
			/* make sure there's no more looping */
			base64CharPtr = base64EndPtr;
		}
		else
		{
			sixBitEncoding[encodingIndex] = (unsigned char)decodedChar;
			++encodingIndex;
		}
		
		if ( (encodingIndex == 4) || endOfData)
		{
			/* convert four 6-bit characters into three 8-bit bytes */
			
			/* always get first byte */
			*eightBitByte++ =
			(sixBitEncoding[0] << 2) | ((sixBitEncoding[1] & 0x30) >> 4);
			if ( encodingIndex >= 3 )
			{
				/* get second byte only if encodingIndex is 3 or 4 */
				*eightBitByte++ =
				((sixBitEncoding[1] & 0x0F) << 4) | ((sixBitEncoding[2] & 0x3C) >> 2);
				if ( encodingIndex == 4 )
				{
					/* get third byte only if encodingIndex is 4 */
					*eightBitByte++ =
					((sixBitEncoding[2] & 0x03) << 6) | (sixBitEncoding[3] & 0x3F);
				}
			}
			
			/* reset encodingIndex */
			encodingIndex = 0;
		}
	}
	
	/* return the number of bytes in outBuffer and no error */
	*lengthptr = eightBitByte - outBuffer;
	return ( 0 );
	
error_exit:
	/* return 0 bytes in outBuffer and an error */
	*lengthptr = 0;
	return ( -1 );
}
/*****************************************************************************/

void parse_stat_end(void *ctx,
					const xmlChar *localname,
					const xmlChar *prefix,
					const xmlChar *URI)
{
	#pragma unused(localname,prefix,URI)
	struct webdav_stat_attr* text_ptr = (struct webdav_stat_attr*)ctx;
	text_ptr->start = false;
}
/*****************************************************************************/

void parser_file_count_end(void *ctx,
						   const xmlChar *localname,
						   const xmlChar *prefix,
						   const xmlChar *URI)
{
	#pragma unused(ctx,localname,prefix,URI)
}
/*****************************************************************************/

void parser_statfs_end(void *ctx,
					   const xmlChar *localname,
					   const xmlChar *prefix,
					   const xmlChar *URI)
{
	#pragma unused(localname,prefix,URI)
	struct webdav_quotas* text_ptr = (struct webdav_quotas*)ctx;
	text_ptr->start = false;
}
/*****************************************************************************/

void parser_lock_end(void *ctx,
					 const xmlChar *localname,
					 const xmlChar *prefix,
					 const xmlChar *URI)
{
	#pragma unused(localname,prefix,URI)
	webdav_parse_lock_struct_t *lock_struct = (webdav_parse_lock_struct_t *)ctx;
	lock_struct->start = false;
}
/*****************************************************************************/

void parser_opendir_end(void *ctx,
						const xmlChar *localname,
						const xmlChar *prefix,
						const xmlChar *URI)
{
	#pragma unused(localname,prefix,URI)
	webdav_parse_opendir_struct_t * struct_ptr = (webdav_parse_opendir_struct_t *)ctx;
	struct_ptr->start = false;
}
/*****************************************************************************/

void parser_cachevalidators_end(void *ctx,
								const xmlChar *localname,
								const xmlChar *prefix,
								const xmlChar *URI)
{
	#pragma unused(localname,prefix,URI)
	struct webdav_parse_cachevalidators_struct *cachevalidators_struct = (struct webdav_parse_cachevalidators_struct *)ctx;
	cachevalidators_struct->start = false;
}
/*****************************************************************************/

void parser_multistatus_end(void *ctx,
							const xmlChar *localname,
							const xmlChar *prefix,
							const xmlChar *URI)
{
	#pragma unused(localname,prefix,URI)
	webdav_parse_multistatus_list_t * struct_ptr = (webdav_parse_multistatus_list_t *)ctx;
	struct_ptr->start = false;
}
/*****************************************************************************/
static webdav_parse_opendir_element_t *create_opendir_element(void)
{
	webdav_parse_opendir_element_t *element_ptr;
	
	element_ptr = malloc(sizeof(webdav_parse_opendir_element_t));
	if (!element_ptr)
		return (NULL);
	
	bzero(element_ptr, sizeof(webdav_parse_opendir_element_t));
	element_ptr->dir_data.d_type = DT_REG;
	element_ptr->seen_href = FALSE;
	element_ptr->seen_response_end = FALSE;
	element_ptr->dir_data.d_namlen = 0;
	element_ptr->dir_data.d_name_URI_length = 0;
	element_ptr->dir_data.d_reclen = sizeof(struct webdav_dirent);
	element_ptr->next = NULL;
	return (element_ptr);
}
/*****************************************************************************/
void parser_opendir_create (void *ctx,
							const xmlChar *localname,
							const xmlChar *prefix,
							const xmlChar *URI,
							int nb_namespaces,
							const xmlChar **namespaces,
							int nb_attributes,
							int nb_defaulted,
							const xmlChar **attributes)
{
	#pragma unused(prefix,URI,nb_namespaces,namespaces,nb_attributes,nb_defaulted,attributes)
	webdav_parse_opendir_element_t * element_ptr = NULL;
	webdav_parse_opendir_element_t * list_ptr;
	webdav_parse_opendir_struct_t * struct_ptr = (webdav_parse_opendir_struct_t *)ctx;
	CFStringRef nodeString;
	struct_ptr->start=true;
	nodeString =  CFStringCreateWithCString (kCFAllocatorDefault,
											 (const char *)localname,
											 kCFStringEncodingUTF8
											 );
	/* See if this is the resource type.  If it is, malloc a webdav_parse_opendir_element_t element and add it to the list.*/
	
	if (((CFStringCompare(nodeString, CFSTR("href"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		element_ptr = struct_ptr->tail;
		
		if ( (element_ptr != NULL) && (element_ptr->seen_href == FALSE))
		{
			// <rdar://problem/4173444>
			// This is a placeholder (<D:propstat> & children came before the <D:href>)
			element_ptr->seen_href = TRUE;
			struct_ptr->id = WEBDAV_OPENDIR_ELEMENT;
			struct_ptr->data_ptr = (void *)element_ptr;
		}
		else
		{
			// Create the new href element
			element_ptr = create_opendir_element();
			require_action(element_ptr != NULL, malloc_element_ptr, struct_ptr->error = ENOMEM);
			
			element_ptr->seen_href = TRUE;
			
			if (struct_ptr->head == NULL)
			{
				struct_ptr->head = element_ptr;
			}
			else
			{
				list_ptr = struct_ptr->tail;
				list_ptr->next = element_ptr;
			}
			struct_ptr->tail = element_ptr;
			
			struct_ptr->id = WEBDAV_OPENDIR_ELEMENT;
			struct_ptr->data_ptr = (void *)element_ptr;
		}
	}	/* end if href */
	else if (((CFStringCompare(nodeString, CFSTR("collection"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		/* If we have a collection property, we should normally have an
		 * element ptr in the context already. But this is not always the
		 * case (see <rdar://problem/4173444>).
		 */
		element_ptr = struct_ptr->tail;
		
		// ignore the last <D:href> if we've already seen <D:/response>
		if ( (element_ptr != NULL) && (element_ptr->seen_response_end == TRUE))
			element_ptr = NULL;
		
		if (element_ptr == NULL)
		{
			// <rdar://problem/4173444> WebDAV filesystem bug parsing PROPFIND payload
			//
			// The <D:href> element might appear after the <D:propstat>. To handle this
			// case we simply create a placeholder opendir element.
			element_ptr = create_opendir_element();
			require_action(element_ptr != NULL, malloc_element_ptr, struct_ptr->error = ENOMEM);
			
			if (struct_ptr->head == NULL)
			{
				struct_ptr->head = element_ptr;
			}
			else
			{
				list_ptr = struct_ptr->tail;
				list_ptr->next = element_ptr;
			}
			struct_ptr->tail = element_ptr;
		}
		
		element_ptr->dir_data.d_type = DT_DIR;
		
		/* Not interested in child of collection element. We can
		 * and should free the return_ptr in this case.
		 */
		struct_ptr->id = WEBDAV_OPENDIR_IGNORE;
		struct_ptr->data_ptr = NULL;
	}	/* end if collection */
	else if (((CFStringCompare(nodeString, CFSTR("getcontentlength"), kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		/* If we have size then mark up the element pointer so that the
		 * child will know to parse the upcoming text size and store it.
		 */
		element_ptr = struct_ptr->tail;
		
		// ignore the last <D:href> if we've already seen <D:/response>
		if ( (element_ptr != NULL) && (element_ptr->seen_response_end == TRUE))
			element_ptr = NULL;
		
		if (element_ptr == NULL)
		{
			// <rdar://problem/4173444> WebDAV filesystem bug parsing PROPFIND payload
			//
			// The <D:href> element might appear after the <D:propstat>. To handle this
			// case we simply create a placeholder opendir element.
			element_ptr = create_opendir_element();
			require_action(element_ptr != NULL, malloc_element_ptr, struct_ptr->error = ENOMEM);
			
			if (struct_ptr->head == NULL)
			{
				struct_ptr->head = element_ptr;
			}
			else
			{
				list_ptr = struct_ptr->tail;
				list_ptr->next = element_ptr;
			}
			struct_ptr->tail = element_ptr;
		}
		
		struct_ptr->id = WEBDAV_OPENDIR_ELEMENT_LENGTH;
		struct_ptr->data_ptr = (void *)element_ptr;
	}	/* end if length */
	else if (((CFStringCompare(nodeString, CFSTR("getlastmodified"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		/* If we have size then mark up the element pointer so that the
		 * child will know to parse the upcoming text size and store it.
		 */
		element_ptr = struct_ptr->tail;
		
		// ignore the last <D:href> if we've already seen <D:/response>
		if ( (element_ptr != NULL) && (element_ptr->seen_response_end == TRUE))
			element_ptr = NULL;
		
		if (element_ptr == NULL)
		{
			// <rdar://problem/4173444> WebDAV filesystem bug parsing PROPFIND payload
			//
			// The <D:href> element might appear after the <D:propstat>. To handle this
			// case we simply create a placeholder opendir element.
			element_ptr = create_opendir_element();
			require_action(element_ptr != NULL, malloc_element_ptr, struct_ptr->error = ENOMEM);
			
			if (struct_ptr->head == NULL)
			{
				struct_ptr->head = element_ptr;
			}
			else
			{
				list_ptr = struct_ptr->tail;
				list_ptr->next = element_ptr;
			}
			struct_ptr->tail = element_ptr;
		}
		
		struct_ptr->id = WEBDAV_OPENDIR_ELEMENT_MODDATE;
		struct_ptr->data_ptr = (void *)element_ptr;
	}	/* end if modified */
	else if (((CFStringCompare(nodeString, CFSTR("creationdate"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		/* If we have size then mark up the element pointer so that the
		 * child will know to parse the upcoming text size and store it.
		 */
		element_ptr = struct_ptr->tail;
		
		// ignore the last <D:href> if we've already seen <D:/response>
		if ( (element_ptr != NULL) && (element_ptr->seen_response_end == TRUE))
			element_ptr = NULL;
		
		if (element_ptr == NULL)
		{
			// <rdar://problem/4173444> WebDAV filesystem bug parsing PROPFIND payload
			//
			// The <D:href> element might appear after the <D:propstat>. To handle this
			// case we simply create a placeholder opendir element.
			element_ptr = create_opendir_element();
			require_action(element_ptr != NULL, malloc_element_ptr, struct_ptr->error = ENOMEM);
			
			if (struct_ptr->head == NULL)
			{
				struct_ptr->head = element_ptr;
			}
			else
			{
				list_ptr = struct_ptr->tail;
				list_ptr->next = element_ptr;
			}
			struct_ptr->tail = element_ptr;
		}
		
		struct_ptr->id = WEBDAV_OPENDIR_ELEMENT_CREATEDATE;
		struct_ptr->data_ptr = (void *)element_ptr;
	}	/* end if createdate */
	else if (((CFStringCompare(nodeString, CFSTR("appledoubleheader"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		/* If we have size then mark up the element pointer so that the
		 * child will know to parse the upcoming text size and store it.
		 */
		syslog(LOG_ERR, "in the create opendir appledoubleheaderfield\n");
		element_ptr = struct_ptr->tail;
		
		// ignore the last <D:href> if we've already seen <D:/response>
		if ( (element_ptr != NULL) && (element_ptr->seen_response_end == TRUE))
			element_ptr = NULL;
		
		if (element_ptr == NULL)
		{
			// <rdar://problem/4173444> WebDAV filesystem bug parsing PROPFIND payload
			//
			// The <D:href> element might appear after the <D:propstat>. To handle this
			// case we simply create a placeholder opendir element.
			element_ptr = create_opendir_element();
			require_action(element_ptr != NULL, malloc_element_ptr, struct_ptr->error = ENOMEM);
			
			if (struct_ptr->head == NULL)
			{
				struct_ptr->head = element_ptr;
			}
			else
			{
				list_ptr = struct_ptr->tail;
				list_ptr->next = element_ptr;
			}
			struct_ptr->tail = element_ptr;
		}
		
		struct_ptr->id = WEBDAV_OPENDIR_APPLEDOUBLEHEADER;
		struct_ptr->data_ptr = (void *)element_ptr;
	}	/* end if appledoubleheader */
	else if (((CFStringCompare(nodeString, CFSTR("response"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		struct_ptr->id = WEBDAV_OPENDIR_ELEMENT_RESPONSE;
		struct_ptr->data_ptr = (void *)NULL;
	}
	else {
		struct_ptr->id = WEBDAV_OPENDIR_IGNORE;
		struct_ptr->data_ptr = (void *)NULL;
	}
	
malloc_element_ptr:
	syslog(LOG_DEBUG,"malloc failed\n");
	
	
}
/*****************************************************************************/
static webdav_parse_multistatus_element_t *
create_multistatus_element(void)
{
	webdav_parse_multistatus_element_t *element_ptr;
	
	element_ptr = malloc(sizeof(webdav_parse_multistatus_element_t));
	if (!element_ptr)
		return (NULL);
	
	bzero(element_ptr, sizeof(webdav_parse_multistatus_element_t));
	element_ptr->statusCode = WEBDAV_MULTISTATUS_INVALID_STATUS;
	element_ptr->name_len = 0;
	element_ptr->name[0] = 0;
	element_ptr->seen_href = FALSE;
	element_ptr->seen_response_end = FALSE;
	element_ptr->next = NULL;
	return (element_ptr);
}
/*****************************************************************************/

static void parser_file_count_create(void *ctx,
									 const xmlChar *localname,
									 const xmlChar *prefix,
									 const xmlChar *URI,
									 int nb_namespaces,
									 const xmlChar **namespaces,
									 int nb_attributes,
									 int nb_defaulted,
									 const xmlChar **attributes)
{
	#pragma unused(prefix,URI,nb_namespaces,namespaces,nb_attributes,nb_defaulted,attributes)
	CFStringRef nodeString;
	nodeString =  CFStringCreateWithCString (kCFAllocatorDefault,
											 (const char *)localname,
											 kCFStringEncodingUTF8
											 );
	if (((CFStringCompare(nodeString, CFSTR("href"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		++(*((int *)ctx));
	}
	if(nodeString)
		CFRelease(nodeString);
}
/*****************************************************************************/

static void parser_stat_create(void *ctx,
							   const xmlChar *localname,
							   const xmlChar *prefix,
							   const xmlChar *URI,
							   int nb_namespaces,
							   const xmlChar **namespaces,
							   int nb_attributes,
							   int nb_defaulted,
							   const xmlChar **attributes)
{
	#pragma unused(prefix,URI,nb_namespaces,namespaces,nb_attributes,nb_defaulted,attributes)
	struct webdav_stat_attr* text_ptr = (struct webdav_stat_attr*)ctx;
	text_ptr->data = (void *)WEBDAV_STATFS_IGNORE;
	text_ptr->start = true;
	CFStringRef nodeString;
	nodeString =  CFStringCreateWithCString (kCFAllocatorDefault,
											 (const char *)localname,
											 kCFStringEncodingUTF8
											 );
	/* See if this is a type we are interested in  If it is, we'll return
	 the appropriate constant */
	
	if (((CFStringCompare(nodeString, CFSTR("getcontentlength"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		text_ptr->data = (void *)WEBDAV_STAT_LENGTH;
	}
	else
	{
		if (((CFStringCompare(nodeString, CFSTR("getlastmodified"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
		{
			text_ptr->data = (void *)WEBDAV_STAT_MODDATE;
		}
		else if (((CFStringCompare(nodeString, CFSTR("creationdate"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
		{
			text_ptr->data = (void *)WEBDAV_STAT_CREATEDATE;
		}
		else
		{
			if (((CFStringCompare(nodeString, CFSTR("collection"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
			{
				/* It's a collection so set the type as VDIR */
				((struct stat *)ctx)->st_mode = S_IFDIR;
			}	/* end if collection */
		}	/* end of if-else mod date */
	}	/* end if-else length*/
	if(nodeString)
		CFRelease(nodeString);
}
/*****************************************************************************/

static void parser_statfs_create(void *ctx,
								 const xmlChar *localname,
								 const xmlChar *prefix,
								 const xmlChar *URI,
								 int nb_namespaces,
								 const xmlChar **namespaces,
								 int nb_attributes,
								 int nb_defaulted,
								 const xmlChar **attributes)
{
	#pragma unused(prefix,URI,nb_namespaces,namespaces,nb_attributes,nb_defaulted,attributes)
	struct webdav_quotas* text_ptr = (struct webdav_quotas*)ctx;
	text_ptr->data = (void *)WEBDAV_STATFS_IGNORE;
	text_ptr->start = true;
	CFStringRef nodeString;
	nodeString =  CFStringCreateWithCString (kCFAllocatorDefault,
											 (const char *)localname,
											 kCFStringEncodingUTF8
											 );
	/* See if this is a type we are interested in  If it is, we'll return
	 the appropriate constant */
	
	/* handle the "quota-available-bytes" and "quota-used-bytes" properties in the "DAV:" namespace */
	if (((CFStringCompare(nodeString, CFSTR("quota-available-bytes"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		text_ptr->data = (void *)WEBDAV_STATFS_QUOTA_AVAILABLE_BYTES;
	}
	else if (((CFStringCompare(nodeString, CFSTR("quota-used-bytes"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		text_ptr->data = (void *)WEBDAV_STATFS_QUOTA_USED_BYTES;
	}
	/* handle the deprecated "quota" and "quotaused" properties in the "DAV:" namespace */
	else if (((CFStringCompare(nodeString, CFSTR("quota"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		text_ptr->data = (void *)WEBDAV_STATFS_QUOTA;
	}
	else if (((CFStringCompare(nodeString, CFSTR("quotaused"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		text_ptr->data = (void *)WEBDAV_STATFS_QUOTAUSED;
	}
	
	if(nodeString)
		CFRelease(nodeString);
}

/*****************************************************************************/

static void parser_lock_create(void *ctx,
							   const xmlChar *localname,
							   const xmlChar *prefix,
							   const xmlChar *URI,
							   int nb_namespaces,
							   const xmlChar **namespaces,
							   int nb_attributes,
							   int nb_defaulted,
							   const xmlChar **attributes)
{
	#pragma unused(prefix,URI,nb_namespaces,namespaces,nb_attributes,nb_defaulted,attributes)
	webdav_parse_lock_struct_t *lock_struct = (webdav_parse_lock_struct_t *)ctx;
	CFStringRef nodeString = NULL;
	nodeString =  CFStringCreateWithCString (kCFAllocatorDefault,
											 (const char *)localname,
											 kCFStringEncodingUTF8
											 );
	/* See if this is a type we are interested in  If it is, we'll return
	 the appropriate constant */
	if (((CFStringCompare(nodeString, CFSTR("locktoken"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		lock_struct->context = WEBDAV_LOCK_TOKEN;
	}
	else
	{
		if (((CFStringCompare(nodeString, CFSTR("href"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
		{
			if (lock_struct->context == WEBDAV_LOCK_TOKEN)
			{
				lock_struct->context = WEBDAV_LOCK_HREF;
			}
			else
			{
				lock_struct->context = 0;
			}
		}
	}	/* end if-else locktoken*/
	if(nodeString)
		CFRelease(nodeString);
}
/*****************************************************************************/
static void parser_cachevalidators_create(void *ctx,
										  const xmlChar *localname,
										  const xmlChar *prefix,
										  const xmlChar *URI,
										  int nb_namespaces,
										  const xmlChar **namespaces,
										  int nb_attributes,
										  int nb_defaulted,
										  const xmlChar **attributes)
{
	#pragma unused(prefix,URI,nb_namespaces,namespaces,nb_attributes,nb_defaulted,attributes)
	struct webdav_parse_cachevalidators_struct* text_ptr = (struct webdav_parse_cachevalidators_struct*)ctx;
	CFStringRef nodeString = NULL;
	text_ptr->start = true;
	nodeString =  CFStringCreateWithCString (kCFAllocatorDefault,
											 (const char *)localname,
											 kCFStringEncodingUTF8
											 );
	/* See if this is a type we are interested in  If it is, we'll return
	 the appropriate constant */
	if (((CFStringCompare(nodeString, CFSTR("getlastmodified"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		text_ptr->data = (void *)WEBDAV_CACHEVALIDATORS_MODDATE;
	}
	else if (((CFStringCompare(nodeString, CFSTR("getetag"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		text_ptr->data = (void *)WEBDAV_CACHEVALIDATORS_ETAG;
	}
	if(nodeString)
		CFRelease(nodeString);
}

/*****************************************************************************/

static void parser_multistatus_create(void *ctx,
									  const xmlChar *localname,
									  const xmlChar *prefix,
									  const xmlChar *URI,
									  int nb_namespaces,
									  const xmlChar **namespaces,
									  int nb_attributes,
									  int nb_defaulted,
									  const xmlChar **attributes)
{
	#pragma unused(prefix,URI,nb_namespaces,namespaces,nb_attributes,nb_defaulted,attributes)
	webdav_parse_multistatus_element_t * element_ptr = NULL;
	webdav_parse_multistatus_element_t * list_ptr = NULL;
	webdav_parse_multistatus_list_t * struct_ptr = (webdav_parse_multistatus_list_t *)ctx;
	CFStringRef nodeString = NULL;
	struct_ptr->start = true;
	nodeString =  CFStringCreateWithCString (kCFAllocatorDefault,
											 (const char *)localname,
											 kCFStringEncodingUTF8
											 );
	/* See if this is the resource type.  If it is, malloc a webdav_parse_opendir_element_t element and
	 add it to the list. */
	if (((CFStringCompare(nodeString, CFSTR("href"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		element_ptr = struct_ptr->tail;
		
		if ( (element_ptr != NULL) && (element_ptr->seen_href == FALSE))
		{
			// <rdar://problem/4173444>
			// This is a placeholder (<D:propstat> & children came before the <D:href>)
			element_ptr->seen_href = TRUE;
			struct_ptr->id = WEBDAV_MULTISTATUS_ELEMENT;
			struct_ptr->data_ptr = (void *)element_ptr;
		}
		else
		{
			// Create the new href element
			element_ptr = create_multistatus_element();
			require_action(element_ptr != NULL, malloc_element_ptr, struct_ptr->error = ENOMEM);
			
			element_ptr->seen_href = TRUE;
			
			if (struct_ptr->head == NULL)
			{
				struct_ptr->head = element_ptr;
			}
			else
			{
				list_ptr = struct_ptr->tail;
				list_ptr->next = element_ptr;
			}
			struct_ptr->tail = element_ptr;
			
			struct_ptr->id = WEBDAV_MULTISTATUS_ELEMENT;
			struct_ptr->data_ptr = (void *)element_ptr;
		}
	}	/* end if href */
	else if (((CFStringCompare(nodeString, CFSTR("status"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		/* If we have status then mark up the element pointer so that the
		 * add callback will know to parse the upcoming text size and store it.
		 */
		element_ptr = struct_ptr->tail;
		
		// ignore the last <D:href> if we've already seen <D:/response>
		if ( (element_ptr != NULL) && (element_ptr->seen_response_end == TRUE))
			element_ptr = NULL;
		
		if (element_ptr == NULL)
		{
			// <rdar://problem/4173444> WebDAV filesystem bug parsing PROPFIND payload
			//
			// The <D:href> element might appear after the <D:propstat>. To handle this
			// case we simply create a placeholder opendir element.
			element_ptr = create_multistatus_element();
			require_action(element_ptr != NULL, malloc_element_ptr, struct_ptr->error = ENOMEM);
			
			if (struct_ptr->head == NULL)
			{
				struct_ptr->head = element_ptr;
			}
			else
			{
				list_ptr = struct_ptr->tail;
				list_ptr->next = element_ptr;
			}
			struct_ptr->tail = element_ptr;
		}
		
		struct_ptr->id = WEBDAV_MULTISTATUS_STATUS;
		struct_ptr->data_ptr = (void *)element_ptr;
	}	/* end if length */
	else if (((CFStringCompare(nodeString, CFSTR("response"),kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
	{
		struct_ptr->id = WEBDAV_MULTISTATUS_RESPONSE;
		struct_ptr->data_ptr = (void *)NULL;
	}
	else {
		struct_ptr->id = WEBDAV_MULTISTATUS_IGNORE;
		struct_ptr->data_ptr = (void *)NULL;
		
	}
	
malloc_element_ptr:
	syslog(LOG_INFO,"malloc failed\n");
	
	if(nodeString)
		CFRelease(nodeString);
}
/*****************************************************************************/
void parser_opendir_add(void *ctx, const xmlChar *localname, int length)
{
	
	webdav_parse_opendir_element_t * element_ptr;
	webdav_parse_opendir_text_t * text_ptr = NULL;
	webdav_parse_opendir_struct_t * parent_ptr = (webdav_parse_opendir_struct_t *)ctx;
	char * ampPointer = NULL;
	char* str_ptr = NULL;
	char *ep;
	
	text_ptr = malloc(sizeof(webdav_parse_opendir_text_t));
	bzero(text_ptr,sizeof(webdav_parse_opendir_text_t));
	text_ptr->size = (CFIndex)length;
	memcpy(text_ptr->name,localname,length);
	/* If the parent is one of our returned directory elements, and if this is a
	 * text element, than copy the text into the name buffer provided we have room */
	if(parent_ptr->start == true)
	{
		switch (parent_ptr->id)
		{
			case WEBDAV_OPENDIR_ELEMENT:
				element_ptr = (webdav_parse_opendir_element_t *)parent_ptr->data_ptr;
				/* TO Handle a special case for ampersand */
				ampPointer = strstr((const char*) localname,"amp;");
				if(ampPointer) {
					char * literalPtr = strchr((const char*) localname,'<');
					int totalLength = (int)(literalPtr - (char*)localname);
					if(totalLength >= (length+5)) {
						str_ptr = (char*)malloc(totalLength+1);
						memset(str_ptr,0,totalLength+1);
						memcpy(str_ptr,localname,totalLength);
						ampPointer = strstr((const char*) str_ptr,"amp;");
						/* To handle multiple ampersands*/
						while(ampPointer) {
							ampPointer+=4;
							memcpy(&text_ptr->name[length],"&",1);
							memcpy(&text_ptr->name[length+1],ampPointer,totalLength-length-4-1);
							text_ptr->size = totalLength-4;
							ampPointer = strstr((const char*)text_ptr->name,"amp;");
							length++;
						}
						free(str_ptr);
					}
				}
				/* make sure the complete name will fit in the structure */
				if ((element_ptr->dir_data.d_name_URI_length + text_ptr->size) <=
					((unsigned int)sizeof(element_ptr->dir_data.d_name) - 1))
				{
					bcopy(text_ptr->name,
						  &element_ptr->dir_data.d_name[element_ptr->dir_data.d_name_URI_length],
						  text_ptr->size);
					element_ptr->dir_data.d_name_URI_length += (uint32_t)text_ptr->size;
				}
				else
				{
					debug_string("URI too long");
					parent_ptr->error = ENAMETOOLONG;
				}
				break;
				
			case WEBDAV_OPENDIR_ELEMENT_LENGTH:
				element_ptr = (webdav_parse_opendir_element_t *)parent_ptr->data_ptr;
				element_ptr->statsize = strtoq((const char *)text_ptr->name, &ep, 10);
				break;
				
			case WEBDAV_OPENDIR_ELEMENT_MODDATE:
				element_ptr = (webdav_parse_opendir_element_t *)parent_ptr->data_ptr;
				element_ptr->stattime.tv_sec = DateBytesToTime(text_ptr->name, strlen((const char *)text_ptr->name));
				if (element_ptr->stattime.tv_sec == -1)
				{
					element_ptr->stattime.tv_sec = 0;
				}
				element_ptr->stattime.tv_nsec = 0;
				break;
				
			case WEBDAV_OPENDIR_ELEMENT_CREATEDATE:
				// First try ISO8601
				element_ptr = (webdav_parse_opendir_element_t *)parent_ptr->data_ptr;
				element_ptr->createtime.tv_sec = ISO8601ToTime(text_ptr->name, strlen((const char *)text_ptr->name));
				
				if (element_ptr->createtime.tv_sec == -1) {
					// Try RFC 850, RFC 1123
					element_ptr->createtime.tv_sec = DateBytesToTime(text_ptr->name, strlen((const char *)text_ptr->name));
					
				}
				
				if (element_ptr->createtime.tv_sec == -1)
				{
					element_ptr->createtime.tv_sec = 0;
				}
				element_ptr->createtime.tv_nsec = 0;
				break;
				
			case WEBDAV_OPENDIR_APPLEDOUBLEHEADER:
			{
				size_t	len = APPLEDOUBLEHEADER_LENGTH;
				
				element_ptr = (webdav_parse_opendir_element_t *)parent_ptr->data_ptr;
				from_base64((const char *)text_ptr->name, (unsigned char *)element_ptr->appledoubleheader, &len);
				if (len == APPLEDOUBLEHEADER_LENGTH)
				{
					element_ptr->appledoubleheadervalid = TRUE;
				}
			}
				break;
				
			default:
				break;
		}	/* end of switch statement */
		parent_ptr->start = false;
	}/* end of if it is our text element */
	free(text_ptr);
}

/*****************************************************************************/

static void parser_lock_add(void *ctx, const xmlChar *localname, int length)
{
	webdav_parse_lock_struct_t *lock_struct = (webdav_parse_lock_struct_t *)ctx;
	UInt8 *text_ptr = malloc(length+1);
	bzero(text_ptr,length+1);
	memcpy(text_ptr,localname,length);
	UInt8* ch = NULL;
	
	if (lock_struct->context == WEBDAV_LOCK_HREF)
	{
		lock_struct->context = 0; /* clear the context so the next time through it's not in WEBDAV_LOCK_HREF */
		
		/* Since the context is set to WEBDAV_LOCK_HREF, we have
		 * found the token and we have found the href indicating
		 * that the locktoken is coming so squirrel it away.
		 */
		
		// Trim trailing whitespace
		ch = &text_ptr[length-1];
		while (ch > text_ptr) {
			if (isspace(*ch))
				*ch-- = '\0';
			else
				break;
		}
		
		lock_struct->locktoken = (char *)text_ptr;
		
	}
}
/*****************************************************************************/

static void parser_stat_add(void *ctx, const xmlChar *localname, int length)
{
	UInt8 *text_ptr = (UInt8*) malloc(length);
	bzero(text_ptr,length);
	memcpy(text_ptr,localname,length);
	struct webdav_stat_attr *statbuf = (struct webdav_stat_attr *)ctx;
	struct webdav_stat_attr*parent = (struct webdav_stat_attr*)ctx;
	char *ep;
	/*
	 * If the context reflects one of our properties then the localname must
	 * be the text pointer with the data we want
	 */
	if(parent->start==true)
	{
		switch ((uintptr_t)parent->data)
		{
			case WEBDAV_STAT_LENGTH:
				/* the text pointer is the length in bytes so put it in the stat buffer */
				if (text_ptr && (text_ptr != (UInt8 *)WEBDAV_STAT_IGNORE))
				{
					statbuf->attr_stat.st_size = strtoq((const char *)text_ptr, &ep, 10);
				}
				else
				{
					/* if we got a string set to NULL we could not fit the value in our buffer, return invalid value */
					statbuf->attr_stat.st_size = -1LL;
				}
				break;
				
			case WEBDAV_STAT_MODDATE:
				/* the text pointer is the date so translate it and put it into the stat buffer */
				
				if (text_ptr && (text_ptr != (UInt8 *)WEBDAV_STAT_IGNORE))
				{
					statbuf->attr_stat.st_mtimespec.tv_sec = DateBytesToTime(text_ptr, length);
					if (statbuf->attr_stat.st_mtimespec.tv_sec == -1)
					{
						statbuf->attr_stat.st_mtimespec.tv_sec = 0;
					}
					statbuf->attr_stat.st_mtimespec.tv_nsec = 0;
				}
				else
				{
					/* if we got a string set to NULL we could not fit the value in our buffer, do nothing */
				}
				break;
				
			case WEBDAV_STAT_CREATEDATE:
				/* the text pointer is the date so translate it and put it into the stat buffer */
				
				if (text_ptr && (text_ptr != (UInt8 *)WEBDAV_STAT_IGNORE))
				{
					// First try ISO8601
					statbuf->attr_create_time.tv_sec = ISO8601ToTime(text_ptr, length);
					
					if (statbuf->attr_create_time.tv_sec == -1) {
						// Try RFC 850, RFC 1123
						statbuf->attr_create_time.tv_sec = DateBytesToTime(text_ptr, length);
					}
					
					if (statbuf->attr_create_time.tv_sec == -1)
					{
						statbuf->attr_create_time.tv_sec = 0;
					}
					statbuf->attr_create_time.tv_nsec = 0;
				}
				else
				{
					/* if we got a string set to NULL we could not fit the value in our buffer, do nothing */
				}
				break;
				
			default:
				break;
		}
		parent->start = false;
	}
	free(text_ptr);
}

/*****************************************************************************/

static void parser_statfs_add(void *ctx, const xmlChar *localname, int length)
{
	char *text_ptr = (char*) malloc(length);
	bzero(text_ptr,length);
	memcpy(text_ptr,(char*)localname,length);
	
	struct webdav_quotas *quotas = (struct webdav_quotas *)ctx;
	struct webdav_quotas* parent = (struct webdav_quotas*)ctx;
	char *ep;
	/*
	 * If the parent reflects one of our properties than the localname must
	 * be the text pointer with the data we want
	 */
	if(parent->start == true)
	{
		switch ((uintptr_t)parent->data)
		{
			case WEBDAV_STATFS_QUOTA_AVAILABLE_BYTES:
				/* the text pointer is the data so put it in the quotas buffer */
				if (text_ptr && (text_ptr != (char *)WEBDAV_STATFS_IGNORE))
				{
					quotas->quota_available_bytes = strtouq(text_ptr, &ep, 10);
					quotas->use_bytes_values = TRUE;
				}
				else
				{
					/* if we got a string set to NULL we could not fit the value in our buffer, do nothing */
				}
				break;
				
			case WEBDAV_STATFS_QUOTA_USED_BYTES:
				/* the text pointer is the data so put it in the quotas buffer */
				if (text_ptr && (text_ptr != (char *)WEBDAV_STATFS_IGNORE))
				{
					quotas->quota_used_bytes = strtouq(text_ptr, &ep, 10);
				}
				else
				{
					/* if we got a string set to NULL we could not fit the value in our buffer, do nothing */
				}
				break;
				
			case WEBDAV_STATFS_QUOTA:
				/* the text pointer is the data so put it in the quotas buffer */
				if (text_ptr && (text_ptr != (char *)WEBDAV_STATFS_IGNORE))
				{
					quotas->quota = strtouq(text_ptr, &ep, 10);
				}
				else
				{
					/* if we got a string set to NULL we could not fit the value in our buffer, do nothing */
				}
				break;
				
			case WEBDAV_STATFS_QUOTAUSED:
				/* the text pointer is the data so put it in the quotas buffer */
				if (text_ptr && (text_ptr != (char *)WEBDAV_STATFS_IGNORE))
				{
					quotas->quotaused = strtouq(text_ptr, &ep, 10);
				}
				else
				{
					/* if we got a string set to NULL we could not fit the value in our buffer, do nothing */
				}
				break;
				
			default:
				break;
		}
		parent->start = false;
	}
	free(text_ptr);
}

/*****************************************************************************/

static void parser_cachevalidators_add(void *ctx, const xmlChar *localname, int length)
{
	struct webdav_parse_cachevalidators_struct *cachevalidators_struct = (struct webdav_parse_cachevalidators_struct *)ctx;
	
	/*
	 * If the parent reflects one of our properties than the child must
	 * be the text pointer with the data we want
	 */if(cachevalidators_struct->start == true)
	 {
		 switch ((uintptr_t)cachevalidators_struct->data)
		 {
			 case WEBDAV_CACHEVALIDATORS_MODDATE:
				 /* the text pointer is the date so translate it and get it into last_modified */
				 if (localname && (localname != (UInt8 *)WEBDAV_CACHEVALIDATORS_IGNORE))
				 {
					 cachevalidators_struct->last_modified = DateBytesToTime(localname, length);
				 }
				 else
				 {
					 /* if we got a string set to NULL we could not fit the value in our buffer, do nothing */
				 }
				 break;
				 
			 case WEBDAV_CACHEVALIDATORS_ETAG:
				 /* the text pointer is the etag so copy it into last_modified */
				 if (localname && (localname != (UInt8 *)WEBDAV_CACHEVALIDATORS_IGNORE))
				 {
					 size_t	len = length + 1;
					 
					 cachevalidators_struct->entity_tag = calloc(1,len);
					 if ( cachevalidators_struct->entity_tag != NULL )
					 {
						 strlcpy(cachevalidators_struct->entity_tag, (const char *)localname, len);
					 }
				 }
				 else
				 {
					 /* if we got a string set to NULL we could not fit the value in our buffer, do nothing */
				 }
				 break;
				 
			 default:
				 break;
		 }
		 cachevalidators_struct->start = false;
	 }

}

/*****************************************************************************/

static void parser_multistatus_add(void *ctx, const xmlChar *localname, int length)
{
	webdav_parse_multistatus_element_t * element_ptr;
	webdav_parse_multistatus_list_t * parent_ptr = (webdav_parse_multistatus_list_t *)ctx;
	webdav_parse_multistatus_text_t * text_ptr = NULL;
	webdav_parse_multistatus_list_t * struct_ptr = (webdav_parse_multistatus_list_t *)ctx;
	char *ep, *ch, *endPtr;
	int errnum;
	/* If the parent is one of our returned directory elements, and if this is a
	 * text element, than copy the text into the name buffer provided we have room */
	text_ptr = malloc(sizeof(webdav_parse_multistatus_text_t));
	bzero(text_ptr,sizeof(webdav_parse_multistatus_text_t));
	text_ptr->size = (CFIndex)length;
	memcpy(text_ptr->name,localname,length);
	
	if(parent_ptr->start == true)
	{
		switch (parent_ptr->id)
		{
			case WEBDAV_MULTISTATUS_ELEMENT:
				element_ptr = (webdav_parse_multistatus_element_t *)parent_ptr->data_ptr;
				
				/* make sure the complete name will fit in the structure */
				if ( (text_ptr->size) < (CFIndex)(sizeof(element_ptr->name)))
				{
					bcopy(text_ptr->name, &element_ptr->name, text_ptr->size);
					element_ptr->name_len += (uint32_t)text_ptr->size;
					element_ptr->name[element_ptr->name_len] = 0;
				}
				else
				{
					debug_string("URI too long");
					struct_ptr->error = ENAMETOOLONG;
				}
				break;
				
			case WEBDAV_MULTISTATUS_STATUS:
				/* the text pointer is status code so put it in the element statusCode field */
				element_ptr = (webdav_parse_multistatus_element_t *)parent_ptr->data_ptr;
				ch = (char *)text_ptr->name;
				endPtr = ch + text_ptr->size;
				
				// Find the 'H' of "HTTP/1.1 XXX Description"
				while (ch < endPtr) {
					if (*ch == 'H')
						break;
					ch++;
				}
				
				// Did we overshoot to the end?
				if (ch >= endPtr) {
					debug_string("Cannot parse Status line, HTTP string not found");
					struct_ptr->error = EIO;
					break;
				}
				
				// Now find the space inbetween "HTTP/1.1" and the numeric status code
				while (ch < endPtr) {
					if (*ch == ' ') {
						ch++;
						break;
					}
					ch++;
				}
				
				// Did we overshoot to the end?
				if (ch >= endPtr) {
					debug_string("Cannot parse Status line, numeric status code missing");
					struct_ptr->error = EIO;
					break;
				}
				
				// Now convert status code string to a ulong
				errno = 0;
				element_ptr->statusCode = (UInt32)strtoul((const char *)ch, &ep, 10);
				errnum = errno;
				
				// See if the conversion was successful
				if ((errnum != 0) || (ep == ch)) {
					// nothing was converted
					debug_string("Cannot parse Status line, could not convert numeric status code");
					struct_ptr->error = EIO;
				}
				break;
				
			default:
				break;
		}	/* end of switch statement */
		parent_ptr->start = false;
	}/* end of if it is our text element */
	free(text_ptr);
}

/*****************************************************************************/

/*
 * GetNormalizedPathLength returns the length of the absolute path (with percent
 * escapes removed) portion of a CFURLRef.
 */
static CFIndex GetNormalizedPathLength(CFURLRef anURL)
{
	CFURLRef absoluteURL;
	CFStringRef escapedPath;
	CFStringRef unescapedPath;
	CFIndex result;
	
	result = 0;
	absoluteURL = CFURLCopyAbsoluteURL(anURL);
	require(absoluteURL != NULL, CFURLCopyAbsoluteURL);
	
	escapedPath = CFURLCopyPath(absoluteURL);
	require(escapedPath != NULL, CFURLCopyPath);
	
	unescapedPath = CFURLCreateStringByReplacingPercentEscapes(kCFAllocatorDefault, escapedPath, CFSTR(""));
	require_string(unescapedPath != NULL, CFURLCreateStringByReplacingPercentEscapes, "name was not legal UTF8");
	
	result = CFStringGetLength(unescapedPath);
	
	CFRelease(unescapedPath);
	
CFURLCreateStringByReplacingPercentEscapes:
	
	CFRelease(escapedPath);
	
CFURLCopyPath:
	
	CFRelease(absoluteURL);
	
CFURLCopyAbsoluteURL:
	
	return ( result );
}

/*****************************************************************************/

/*
 * GetComponentName determines if the URI combined with the parent URL is a
 * child of the parent or is the parent itself, and if it is a child, extracts
 * the child's component name.
 *
 * GetComponentName returns TRUE if the URI is for a child, or returns FALSE if
 * the URI is the parent itself. The componentName buffer will contain the child's
 * component name if the result is TRUE.
 *
 * The parentPathLength parameter allows this routine to determine child/parent
 * status without comparing the path strings.
 */
static Boolean GetComponentName(	/* <- TRUE if http URI was not parent and component name was returned */
								CFURLRef urlRef,				/* -> the parent directory's URL  */
								CFIndex parentPathLength,		/* -> the parent directory's percent decoded path length */
								char *uri,						/* -> the http URI from the WebDAV server */
								char *componentName)			/* <-> point to buffer of MAXNAMLEN + 1 bytes where URI's LastPathComponent is returned if result it TRUE */
{
	Boolean result;
	CFStringRef uriString;				/* URI as CFString */
	CFURLRef uriURL;					/* URI converted to full URL */
	CFStringRef uriName;				/* URI's LastPathComponent as CFString */
	
	result = FALSE;
	
	/* create a CFString from the c-string containing URI */
	uriString = CFStringCreateWithCString(kCFAllocatorDefault, uri, kCFStringEncodingUTF8);
	require(uriString != NULL, CFStringCreateWithCString);
	
	/* create a CFURL from the URI CFString and the parent URL */
	uriURL = CFURLCreateWithString(kCFAllocatorDefault, uriString, urlRef);
	if (uriURL != NULL) {
		/* CFURLCreateWithString worked fine, so release uriString and keep going */
		CFRelease(uriString);
	}
	else {
		/* Fix for Windows servers.  They tend to send over names that have a space in them instead of the correct %20.  This causes
		 CFURLCreateWithString to fail.  Since it might be a partially escaped URL string, try to unescape the string, then re-escape it */
		CFStringRef unEscapedString;		/* URI as CFString with un-escaped chars */
		CFStringRef reEscapedString;		/* URI as CFString with re-escaped chars */
		
		unEscapedString = CFURLCreateStringByReplacingPercentEscapesUsingEncoding (kCFAllocatorDefault, uriString, NULL, kCFStringEncodingUTF8);
		CFRelease(uriString);
		require(unEscapedString != NULL, CFURLCreateWithString);
		
		reEscapedString = CFURLCreateStringByAddingPercentEscapes (kCFAllocatorDefault, unEscapedString, NULL, NULL, kCFStringEncodingUTF8);
		CFRelease(unEscapedString);
		require(reEscapedString != NULL, CFURLCreateWithString);
		
		/* try again to create a CFURL from the reEscapedString and the parent URL */
		uriURL = CFURLCreateWithString(kCFAllocatorDefault, reEscapedString, urlRef);
		CFRelease(reEscapedString);
		require(uriURL != NULL, CFURLCreateWithString);
	}
	
	/* see if this is the parent or a child */
	if ( GetNormalizedPathLength(uriURL) > parentPathLength ) {
		/* this is a child */
		
		/* get the child's name */
		uriName = CFURLCopyLastPathComponent(uriURL);
		require_string(uriName != NULL, CFURLCopyLastPathComponent, "name was not legal UTF8");
		
		if ( CFStringGetCString(uriName, componentName, MAXNAMLEN + 1, kCFStringEncodingUTF8) ) {
			/* we have the child name */
			result = TRUE;
		}
		else {
			debug_string("could not get child name (too long?)");
		}
		CFRelease(uriName);
	}
	else {
		/* this is the parent, skip it */
	}
	
CFURLCopyLastPathComponent:
	
	CFRelease(uriURL);
	
CFURLCreateWithString:
CFStringCreateWithCString:
	
	return ( result );
}

/*****************************************************************************/

int parse_opendir(UInt8 *xmlp,					/* -> xml data returned by PROPFIND with depth of 1 */
				  CFIndex xmlp_len,				/* -> length of xml data */
				  CFURLRef urlRef,				/* -> the CFURL to the parent directory */
				  uid_t uid,						/* -> uid of the user making the request */
				  struct node_entry *parent_node)	/* -> pointer to the parent directory's node_entry */
{
	int error = 0;
	ssize_t size = 0;
	struct webdav_dirent dir_data[2];
	CFIndex parentPathLength;
	webdav_parse_opendir_struct_t opendir_struct;
	webdav_parse_opendir_element_t *element_ptr, *prev_element_ptr;
	opendir_struct.head = opendir_struct.tail = NULL;
	opendir_struct.error = 0;
	
	xmlSAXHandler sh;
    memset(&sh,0,sizeof(sh));
    sh.startElementNs = parser_opendir_create;
    sh.characters = parser_opendir_add;
	sh.endElementNs = parser_opendir_end;
    sh.initialized = XML_SAX2_MAGIC;
	
	/* truncate the file, and reset the file pointer to 0 */
	require(ftruncate(parent_node->file_fd, 0) == 0, ftruncate);
	require(lseek(parent_node->file_fd, 0, SEEK_SET) == 0, lseek);
	
	if(xmlp != NULL)
	{
		int result = xmlSAXUserParseMemory( &sh,&opendir_struct,(char*)xmlp,(int)xmlp_len);
		require(result == 0, ParserCreate);
		/* parse the XML -- exit now if error during parse */
	}
	
	/* if the directory is not deleted, write "." and ".."  */
	if ( !NODE_IS_DELETED(parent_node) )
	{
		bzero(dir_data, sizeof(dir_data));
		
		dir_data[0].d_ino = parent_node->fileid;
		dir_data[0].d_reclen = sizeof(struct webdav_dirent);
		dir_data[0].d_type = DT_DIR;
		dir_data[0].d_namlen = 1;
		dir_data[0].d_name[0] = '.';
		
		dir_data[1].d_ino =
		(dir_data[0].d_ino == WEBDAV_ROOTFILEID) ? WEBDAV_ROOTPARENTFILEID : parent_node->parent->fileid;
		dir_data[1].d_reclen = sizeof(struct webdav_dirent);
		dir_data[1].d_type = DT_DIR;
		dir_data[1].d_namlen = 2;
		dir_data[1].d_name[0] = '.';
		dir_data[1].d_name[1] = '.';
		
		size = write(parent_node->file_fd, dir_data, sizeof(struct webdav_dirent) * 2);
		require(size == (sizeof(struct webdav_dirent) * 2), write_dot_dotdot);
	}
	
	/*
	 * Important: the xml we get back from the server includes the info
	 * on the parent directory as well as all of its children.
	 *
	 * The elements returned by PROPFIND contain http URI. So, give a parent URL of
	 * http://host/parent/, the responses could be:
	 *		absolute URL:	http://host/parent/child
	 *		absolute path:	/parent/child
	 *		relative path:	child
	 * So, if all URLs are normalized to an absolute path with percent escapes
	 * removed, then the children will always be longer than the parent.
	 */
	
	/* get the parent directory's path length */
	parentPathLength = GetNormalizedPathLength(urlRef);
	
	/* invalidate any children nodes -- they'll be marked valid by nodecache_get_node */
	(void) nodecache_invalidate_directory_node_time(parent_node);
	
	/* look at the list of elements */
	for (element_ptr = opendir_struct.head; element_ptr != NULL; element_ptr = element_ptr->next)
	{
		char namebuffer[MAXNAMLEN + 1];
		struct webdav_stat_attr statbuf;
		
		// Skip any placeholder that never saw a matching <D:href> element
		if (element_ptr->seen_href == FALSE)
			continue;
		
		/* make element_ptr->dir_data.d_name a cstring */
		element_ptr->dir_data.d_name[element_ptr->dir_data.d_name_URI_length] = '\0';
		//syslog(LOG_ERR,"element_ptr->dir_data.d_name is %s\n",element_ptr->dir_data.d_name);
		/* get the component name if this element is not the parent */
		if ( GetComponentName(urlRef, parentPathLength, element_ptr->dir_data.d_name, namebuffer) )
		{
			/* this is a child */
			struct node_entry *element_node;
			size_t name_len;
			
			name_len = strlen(namebuffer);
			//syslog(LOG_ERR,"namebuffer is %s\n",namebuffer);
			/* get (or create) a cache node for this element */
			error = nodecache_get_node(parent_node, name_len, namebuffer, TRUE, FALSE,
									   element_ptr->dir_data.d_type == DT_DIR ? WEBDAV_DIR_TYPE : WEBDAV_FILE_TYPE, &element_node);
			if (error)
			{
				debug_string("nodecache_get_node failed");
				continue;
			}
			/* move just the element name over element_ptr->dir_data.d_name */
			bcopy(element_node->name, element_ptr->dir_data.d_name, element_node->name_length);
			
			element_ptr->dir_data.d_name[element_node->name_length] = '\0';
			element_ptr->dir_data.d_namlen = element_node->name_length;
			
			/* set the file number */
			element_ptr->dir_data.d_ino = element_node->fileid;
			//syslog(LOG_ERR,"element_node->fileid : %d\n",element_node->fileid);
			/*
			 * Prepare to cache this element's attributes, since it's
			 * highly likely a stat will follow reading the directory.
			 */
			
			bzero(&statbuf, sizeof(struct webdav_stat_attr));
			
			/* the first thing to do is fill in the fields we cannot get from the server. */
			statbuf.attr_stat.st_dev = 0;
			/* Why 1 for st_nlink?
			 * Getting the real link count for directories is expensive.
			 * Setting it to 1 lets FTS(3) (and other utilities that assume
			 * 1 means a file system doesn't support link counts) work.
			 */
			statbuf.attr_stat.st_nlink = 1;
			statbuf.attr_stat.st_uid = UNKNOWNUID;
			statbuf.attr_stat.st_gid = UNKNOWNUID;
			statbuf.attr_stat.st_rdev = 0;
			statbuf.attr_stat.st_blksize = WEBDAV_IOSIZE;
			statbuf.attr_stat.st_flags = 0;
			statbuf.attr_stat.st_gen = 0;
			
			/* set all times to the last modified time since we cannot get the other times */
			statbuf.attr_stat.st_atimespec = statbuf.attr_stat.st_mtimespec = statbuf.attr_stat.st_ctimespec = element_ptr->stattime;
			
			/* set create time if we have it */
			if (element_ptr->createtime.tv_sec)
				statbuf.attr_create_time = element_ptr->createtime;
			//syslog(LOG_ERR,"element_ptr->dir_data.d_type : %d\n",element_ptr->dir_data.d_type);
			if (element_ptr->dir_data.d_type == DT_DIR)
			{
				statbuf.attr_stat.st_mode = S_IFDIR | S_IRWXU;
				statbuf.attr_stat.st_size = WEBDAV_DIR_SIZE;
				/* appledoubleheadervalid is never valid for directories */
				element_ptr->appledoubleheadervalid = FALSE;
			}
			else
			{
				statbuf.attr_stat.st_mode = S_IFREG | S_IRWXU;
				statbuf.attr_stat.st_size = element_ptr->statsize;
				/* appledoubleheadervalid is valid for files only if the server
				 * returned the appledoubleheader property and file size is
				 * the size of the appledoubleheader (APPLEDOUBLEHEADER_LENGTH bytes).
				 */
				element_ptr->appledoubleheadervalid =
				(element_ptr->appledoubleheadervalid && (element_ptr->statsize == APPLEDOUBLEHEADER_LENGTH));
				//syslog(LOG_ERR, "element_ptr->appledoubleheadervalid %d",element_ptr->appledoubleheadervalid);
			}
			
			/* calculate number of S_BLKSIZE blocks */
			statbuf.attr_stat.st_blocks = ((statbuf.attr_stat.st_size + S_BLKSIZE - 1) / S_BLKSIZE);
			
			/* set the fileid in statbuf*/
			statbuf.attr_stat.st_ino = element_node->fileid;
			
			/* Now cache the stat structure (ignoring errors) */
			(void) nodecache_add_attributes(element_node, uid, &statbuf,
											element_ptr->appledoubleheadervalid ? element_ptr->appledoubleheader : NULL);
			
			/* Complete the task of getting the regular name into the dirent */
			
			size = write(parent_node->file_fd, (void *)&element_ptr->dir_data, element_ptr->dir_data.d_reclen);
			require(size == element_ptr->dir_data.d_reclen, write_element);
		}
		else
		{
			struct node_entry *temp_node;
			/* it was the parent */
			
			/* we are reading this directory, so mark it "recent" */
			(void) nodecache_get_node(parent_node, 0, NULL, TRUE, TRUE, WEBDAV_DIR_TYPE, &temp_node);
			
			/*
			 * Prepare to cache this element's attributes, since it's
			 * highly likely a stat will follow reading the directory.
			 */
			
			bzero(&statbuf, sizeof(struct webdav_stat_attr));
			
			/* the first thing to do is fill in the fields we cannot get from the server. */
			statbuf.attr_stat.st_dev = 0;
			/* Why 1 for st_nlink?
			 * Getting the real link count for directories is expensive.
			 * Setting it to 1 lets FTS(3) (and other utilities that assume
			 * 1 means a file system doesn't support link counts) work.
			 */
			statbuf.attr_stat.st_nlink = 1;
			statbuf.attr_stat.st_uid = UNKNOWNUID;
			statbuf.attr_stat.st_gid = UNKNOWNUID;
			statbuf.attr_stat.st_rdev = 0;
			statbuf.attr_stat.st_blksize = WEBDAV_IOSIZE;
			statbuf.attr_stat.st_flags = 0;
			statbuf.attr_stat.st_gen = 0;
			
			/* set all times to the last modified time since we cannot get the other times */
			statbuf.attr_stat.st_atimespec = statbuf.attr_stat.st_mtimespec = statbuf.attr_stat.st_ctimespec = element_ptr->stattime;
			
			/* set create time if we have it */
			if (element_ptr->createtime.tv_sec)
				statbuf.attr_create_time = element_ptr->createtime;
			
			statbuf.attr_stat.st_mode = S_IFDIR | S_IRWXU;
			statbuf.attr_stat.st_size = WEBDAV_DIR_SIZE;
			
			/* calculate number of S_BLKSIZE blocks */
			statbuf.attr_stat.st_blocks = ((statbuf.attr_stat.st_size + S_BLKSIZE - 1) / S_BLKSIZE);
			
			/* set the fileid in statbuf*/
			statbuf.attr_stat.st_ino = parent_node->fileid;
			
			/* Now cache the stat structure (ignoring errors) */
			(void) nodecache_add_attributes(parent_node, uid, &statbuf, NULL);
		}
	}	/* for element_ptr */
	
	/* delete any children nodes that are still invalid */
	(void) nodecache_delete_invalid_directory_nodes(parent_node);
	
	/* free any elements allocated */
	element_ptr = opendir_struct.head;
	while (element_ptr)
	{
		prev_element_ptr = element_ptr;
		element_ptr = element_ptr->next;
		free(prev_element_ptr);
	}
	
	return ( 0 );
	
	/**********************/
	
write_element:
	/* free any elements allocated */
	element_ptr = opendir_struct.head;
	while (element_ptr)
	{
		prev_element_ptr = element_ptr;
		element_ptr = element_ptr->next;
		free(prev_element_ptr);
	}
write_dot_dotdot:
	/* directory is in unknown condition - erase whatever is there */
	(void) ftruncate(parent_node->file_fd, 0);
ParserCreate:
lseek:
ftruncate:
	return ( EIO );
}

/*****************************************************************************/
webdav_parse_multistatus_list_t *
parse_multi_status(
				   UInt8 *xmlp,					/* -> xml data returned by PROPFIND with depth of 1 */
				   CFIndex xmlp_len)				/* -> length of xml data */
{
	webdav_parse_multistatus_list_t *multistatus_list;
	
	multistatus_list = malloc(sizeof(webdav_parse_multistatus_list_t));
	require(multistatus_list != NULL, malloc_list);
	
	multistatus_list->error = 0;
	multistatus_list->head = NULL;
	multistatus_list->tail = NULL;
	
	xmlSAXHandler sh;
    memset(&sh,0,sizeof(sh));
    sh.startElementNs = parser_multistatus_create;
    sh.characters = parser_multistatus_add;
	sh.endElementNs = parser_multistatus_end;
    sh.initialized = XML_SAX2_MAGIC;
	
	if(xmlp != NULL)
	{
		int result = xmlSAXUserParseMemory( &sh,multistatus_list,(char*)xmlp,(int)xmlp_len);
		require(result == 0, ParserCreate);
	}

ParserCreate:
malloc_list:
	return (multistatus_list);
	
	return ( 0 );
}


/*****************************************************************************/

int parse_file_count(const UInt8 *xmlp, CFIndex xmlp_len, int *file_count)
{
	xmlSAXHandler sh;
    memset(&sh,0,sizeof(sh));
    sh.startElementNs = parser_file_count_create;
	sh.endElementNs = parser_file_count_end;
    sh.initialized = XML_SAX2_MAGIC;
	*file_count = 0;
	
	if(xmlp != NULL)
	{
		xmlSAXUserParseMemory( &sh,file_count,(char*)xmlp,(int)xmlp_len);
	}
	
	return ( 0 );
}

/*****************************************************************************/

int parse_stat(const UInt8 *xmlp, CFIndex xmlp_len, struct webdav_stat_attr *statbuf)
{
	xmlSAXHandler sh;
    memset(&sh,0,sizeof(sh));
    sh.startElementNs = parser_stat_create;
	sh.endElementNs = parse_stat_end;
    sh.characters = parser_stat_add;
    sh.initialized = XML_SAX2_MAGIC;
	bzero((void *)statbuf, sizeof(struct webdav_stat_attr));
	
	if(xmlp != NULL)
	{
		xmlSAXUserParseMemory( &sh,statbuf,(char*)xmlp,(int)xmlp_len);
	}
	/* Coming back from the parser:
	 *   statbuf->attr_stat_info.attr_stat.st_mode will be 0 or will have S_IFDIR set if the object is a directory.
	 *   statbuf->attr_stat_info.attr_stat.st_mtimespec will be 0 or will have the last modified time.
	 *   statbuf->attr_stat_info.attr_create_time will be 0 or the file creation time
	 *   statbuf->attr_stat_info.attr_stat.st_size will be set to the resource size if the object is a file.
	 *
	 * So, the first thing to do is fill in the fields we cannot get from the server.
	 */
	
	statbuf->attr_stat.st_dev = 0;
	/* Why 1 for st_nlink?
	 * Getting the real link count for directories is expensive.
	 * Setting it to 1 lets FTS(3) (and other utilities that assume
	 * 1 means a file system doesn't support link counts) work.
	 */
	statbuf->attr_stat.st_nlink = 1;
	statbuf->attr_stat.st_uid = UNKNOWNUID;
	statbuf->attr_stat.st_gid = UNKNOWNUID;
	statbuf->attr_stat.st_rdev = 0;
	statbuf->attr_stat.st_blksize = WEBDAV_IOSIZE;
	statbuf->attr_stat.st_flags = 0;
	statbuf->attr_stat.st_gen = 0;
	
	/* set last accessed and changed times to last modified time since we cannot get them */
	statbuf->attr_stat.st_atimespec = statbuf->attr_stat.st_ctimespec = statbuf->attr_stat.st_mtimespec;
	
	/* was this a directory? */
	if ( S_ISDIR(statbuf->attr_stat.st_mode) )
	{
		/* yes - add the directory access permissions */
		statbuf->attr_stat.st_mode |= S_IRWXU;
		/* fake up the directory size */
		statbuf->attr_stat.st_size = WEBDAV_DIR_SIZE;
	}
	else
	{
		/* no - mark it as a regular file and set the file access permissions
		 * (for now, everything is either a file or a directory)
		 */
		statbuf->attr_stat.st_mode = S_IFREG | S_IRWXU;
	}
	
	/* calculate number of S_BLKSIZE blocks */
	statbuf->attr_stat.st_blocks = ((statbuf->attr_stat.st_size + S_BLKSIZE - 1) / S_BLKSIZE);
	
	return ( 0 );
}

/*****************************************************************************/

int parse_statfs(const UInt8 *xmlp, CFIndex xmlp_len, struct statfs *statfsbuf)
{
	xmlSAXHandler sh;
    memset(&sh,0,sizeof(sh));
    sh.startElementNs = parser_statfs_create;
    sh.characters = parser_statfs_add;
	sh.endElementNs = parser_statfs_end;
    sh.initialized = XML_SAX2_MAGIC;
	
	struct webdav_quotas quotas;
	bzero((void *)statfsbuf, sizeof(struct statfs));
	bzero((void *)&quotas, sizeof(struct webdav_quotas));
	
	if(xmlp != NULL)
	{
		xmlSAXUserParseMemory( &sh,&quotas,(char*)xmlp,(int)xmlp_len);
	}
	/* were the IETF quota properties returned? */
	if ( quotas.use_bytes_values )
	{
		uint64_t total_bytes;
		uint64_t total_blocks;
		uint32_t bsize;
		
		/* calculate the total bytes (available + used) */
		total_bytes = quotas.quota_available_bytes + quotas.quota_used_bytes;
		if ( (total_bytes >= quotas.quota_available_bytes) && (total_bytes >= quotas.quota_used_bytes) )
		{
			/*
			 * calculate the smallest file system block size (bsize) that's a
			 * multiple of S_BLKSIZE, is >= S_BLKSIZE, and is <= LONG_MAX
			 */
			bsize = S_BLKSIZE / 2;
			do
			{
				bsize *= 2;
				total_blocks = ((total_bytes + bsize - 1) / bsize);
			} while ( total_blocks > LONG_MAX );
			
			/* stuff the results into statfsbuf */
			statfsbuf->f_bsize = bsize;
#ifdef __LP64__
			statfsbuf->f_blocks = total_blocks;
			statfsbuf->f_bavail = statfsbuf->f_bfree = quotas.quota_available_bytes / bsize;
#else
			statfsbuf->f_blocks = (long)total_blocks;
			statfsbuf->f_bavail = statfsbuf->f_bfree = (long)(quotas.quota_available_bytes / bsize);
#endif
		}
		/* else we were handed values we cannot represent so leave statfsbuf zeroed (no quota support for this file system) */
	}
	else
	{
		/* use the deprecated quota and quotaused if they were returned */
		if ( (quotas.quota != 0) && (quotas.quota > quotas.quotaused) )
		{
#ifdef __LP64__
			statfsbuf->f_bavail = statfsbuf->f_bfree = (quotas.quota - quotas.quotaused);
#else
			statfsbuf->f_bavail = statfsbuf->f_bfree = (long)(quotas.quota - quotas.quotaused);
#endif
		}
		else
		{
			statfsbuf->f_bavail = statfsbuf->f_bfree = 0;
		}
#ifdef __LP64__
		statfsbuf->f_blocks = quotas.quota;
#else
		statfsbuf->f_blocks = (long)quotas.quota;
#endif
		statfsbuf->f_bsize = S_BLKSIZE;
	}
	
	statfsbuf->f_iosize = WEBDAV_IOSIZE;
	
	return ( 0 );
}

/*****************************************************************************/

int parse_lock(const UInt8 *xmlp, CFIndex xmlp_len, char **locktoken)
{
	xmlSAXHandler sh;
    memset(&sh,0,sizeof(sh));
    sh.startElementNs = parser_lock_create;
	sh.characters = parser_lock_add;
	sh.endElementNs = parser_lock_end;
    sh.initialized = XML_SAX2_MAGIC;
	webdav_parse_lock_struct_t lock_struct;
	lock_struct.context = 0;
	lock_struct.locktoken = NULL;	/* NULL coming into this function */
	
	if(xmlp != NULL)
	{
		xmlSAXUserParseMemory( &sh,&lock_struct,(char*)xmlp,(int)xmlp_len);
	}
	
	*locktoken = (char *)lock_struct.locktoken;
	if (*locktoken == NULL)
	{
		debug_string("error parsing lock token");
	}
	
	return ( 0 );
}

/*****************************************************************************/

int parse_cachevalidators(const UInt8 *xmlp, CFIndex xmlp_len, time_t *last_modified, char **entity_tag)
{
	xmlSAXHandler sh;
    memset(&sh,0,sizeof(sh));
    sh.startElementNs = parser_cachevalidators_create;
	sh.characters = parser_cachevalidators_add;
	sh.endElementNs = parser_cachevalidators_end;
    sh.initialized = XML_SAX2_MAGIC;
	struct webdav_parse_cachevalidators_struct cachevalidators_struct;
	/* results if parser fails or values are not found */
	cachevalidators_struct.last_modified = 0;
	cachevalidators_struct.entity_tag = NULL;
	
	if(xmlp != NULL)
	{
		xmlSAXUserParseMemory( &sh,&cachevalidators_struct,(char*)xmlp,(int)xmlp_len);
	}
	
	if ( cachevalidators_struct.last_modified != 0 )
	{
		*last_modified = cachevalidators_struct.last_modified;
	}
	else
	{
		time(last_modified);
	}
	*entity_tag = cachevalidators_struct.entity_tag;
	
	return ( 0 );
}

/*****************************************************************************/
