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
#include "webdav_parse.h"
#include "webdav_cache.h"
#include "webdav_network.h"

extern long long
	 strtoq(const char *, char **, int);

/*****************************************************************************/

/* local function prototypes */

static int from_base64(const char *base64str, unsigned char *outBuffer, size_t *lengthptr);
static void *parser_opendir_create(CFXMLParserRef parser, CFXMLNodeRef node, void *context);
static void *parser_file_count_create(CFXMLParserRef parser, CFXMLNodeRef node, void *context);
static void *parser_stat_create(CFXMLParserRef parser, CFXMLNodeRef node, void *context);
static void *parser_statfs_create(CFXMLParserRef parser, CFXMLNodeRef node, void *context);
static void *parser_lock_create(CFXMLParserRef parser, CFXMLNodeRef node, void *context);
static void parser_add(CFXMLParserRef parser, void *parent, void *child, void *context);
static void parser_opendir_add(CFXMLParserRef parser, void *parent, void *child, void *context);
static void parser_stat_add(CFXMLParserRef parser, void *parent, void *child, void *context);
static void parser_statfs_add(CFXMLParserRef parser, void *parent, void *child, void *context);
static void parser_end(CFXMLParserRef parser, void *xml_type, void *context);
static void parser_opendir_end(CFXMLParserRef parser, void *my_element, void *context);
static CFDataRef parser_resolve(CFXMLParserRef parser, CFXMLExternalID *extID, void *context);

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

#define SUBSTITUTE_PHYSICAL_ENTITY(node, text_ptr, size) \
	{ \
		if (CFXMLNodeGetTypeCode(node) == kCFXMLNodeTypeEntityReference) \
		{ \
			if (!strcmp(text_ptr, "amp")) \
				strcpy(text_ptr, "&"); \
			else if (!strcmp(text_ptr, "lt")) \
				strcpy(text_ptr, "<"); \
			else if (!strcmp(text_ptr, "gt")) \
				strcpy(text_ptr, ">"); \
			else if (!strcmp(text_ptr, "apos")) \
				strcpy(text_ptr, "'"); \
			else if (!strcmp(text_ptr, "quot")) \
				strcpy(text_ptr, """"); \
			size = 1; \
		} \
	}

static void *parser_opendir_create(CFXMLParserRef parser, CFXMLNodeRef node, void *context)
{
	#pragma unused(parser)
	webdav_parse_opendir_element_t * element_ptr = NULL;
	webdav_parse_opendir_element_t * list_ptr;
	webdav_parse_opendir_struct_t * struct_ptr = (webdav_parse_opendir_struct_t *)context;
	webdav_parse_opendir_return_t * return_ptr;
	webdav_parse_opendir_text_t * text_ptr;
	void *return_value;
	CFRange comparison_range;
	CFStringRef nodeString = CFXMLNodeGetString(node);

	/* set up our return */
	return_ptr = malloc(sizeof(webdav_parse_opendir_return_t));
	require_action(return_ptr != NULL, malloc_return_ptr, struct_ptr->error = ENOMEM);

	return_value = (void *)return_ptr;
	return_ptr->id = WEBDAV_OPENDIR_IGNORE;
	return_ptr->data_ptr = (void *)NULL;

	/* See if this is the resource type.  If it is, malloc a webdav_parse_opendir_element_t element and
	  add it to the list.	If not, return something but keep parsing */
	switch (CFXMLNodeGetTypeCode(node))
	{
		case kCFXMLNodeTypeElement:
			comparison_range = CFStringFind(nodeString, CFSTR(":"), 0);
			comparison_range.location++;
			comparison_range.length = CFStringGetLength(nodeString) - comparison_range.location;
			if (((CFStringCompareWithOptions(nodeString, CFSTR("href"), comparison_range,
				kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
			{
				element_ptr = malloc(sizeof(webdav_parse_opendir_element_t));
				require_action(element_ptr != NULL, malloc_element_ptr, struct_ptr->error = ENOMEM);
				
				bzero(element_ptr, sizeof(webdav_parse_opendir_element_t));
				return_ptr->id = WEBDAV_OPENDIR_ELEMENT;
				return_ptr->data_ptr = (void *)element_ptr;
				element_ptr->dir_data.d_type = DT_REG;
				element_ptr->dir_data.d_namlen = 0;
				element_ptr->dir_data.d_name_URI_length = 0;
				element_ptr->dir_data.d_reclen = sizeof(struct dirent);
				element_ptr->next = NULL;

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
				element_ptr->next = NULL;
			}	/* end if href */
			else if (((CFStringCompareWithOptions(nodeString, CFSTR("collection"),
					comparison_range, kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
			{
				/* If we have a collection property, we should have an
					* element ptr in the context already.	If not, its
					* an error and we should barf. Since we are parsing an
					* xml tree, we can rest assured that the last element
					* found (href) is the one have just seen a resource type
					* on and hence the one where collection is found and
					* trapped here */

				element_ptr = struct_ptr->tail;
				if (element_ptr)
				{
					element_ptr->dir_data.d_type = DT_DIR;

					/* not interested in child of collection element. We can 
						* can and should free the return_ptr in this case.*/

					return_value = NULL;
					free(return_ptr);
				}
			}	/* end if collection */
			else if (((CFStringCompareWithOptions(nodeString, CFSTR("getcontentlength"),
					comparison_range, kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
			{
				/* If we have size then mark up the element pointer so that the
					* child will know to parse the upcoming text size and store it.
					*/
				element_ptr = struct_ptr->tail;
				if (element_ptr)
				{
					return_ptr->id = WEBDAV_OPENDIR_ELEMENT_LENGTH;
					return_ptr->data_ptr = (void *)element_ptr;
				}
			}	/* end if length */
			else if (((CFStringCompareWithOptions(nodeString, CFSTR("getlastmodified"),
					comparison_range, kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
			{
				/* If we have size then mark up the element pointer so that the
					* child will know to parse the upcoming text size and store it.
					*/
				element_ptr = struct_ptr->tail;
				if (element_ptr)
				{
					return_ptr->id = WEBDAV_OPENDIR_ELEMENT_MODDATE;
					return_ptr->data_ptr = (void *)element_ptr;
				}
			}	/* end if modified */
			else if (((CFStringCompareWithOptions(nodeString, CFSTR("appledoubleheader"),
					comparison_range, kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
			{
				/* If we have size then mark up the element pointer so that the
					* child will know to parse the upcoming text size and store it.
					*/
				element_ptr = struct_ptr->tail;
				if (element_ptr)
				{
					return_ptr->id = WEBDAV_OPENDIR_APPLEDOUBLEHEADER;
					return_ptr->data_ptr = (void *)element_ptr;
				}
			}	/* end if appledoubleheader */
			break;
			
		case kCFXMLNodeTypeEntityReference:
		case kCFXMLNodeTypeText:
		case kCFXMLNodeTypeCDATASection:
			/* If it is a text element, create a structure and pass it back 
			 * we have no way of knowing if this is the text of an href in the
			 * xml (what we are looking for) or some other random text.	 Set it
			 * up and then addchild will figure it out by checking the type of
			 * the child to add and the type of the parent.
			 */
			text_ptr = malloc(sizeof(webdav_parse_opendir_text_t));
			require_action(text_ptr != NULL, malloc_text_ptr, struct_ptr->error = ENOMEM);

			/* Get the bytes out in UTF8 form */
			require_action(CFStringGetBytes(nodeString, CFRangeMake(0, CFStringGetLength(nodeString)),
				kCFStringEncodingUTF8, /*stop on loss */ 0,/*not ext*/  0, text_ptr->name,
				sizeof(text_ptr->name) - 1, &text_ptr->size) == CFStringGetLength(nodeString),
				name_too_long, free(text_ptr); struct_ptr->error = ENAMETOOLONG);

			text_ptr->name[text_ptr->size] = '\0';

			/* Workaround for lack of support for kCFXMLParserReplacePhysicalEntities option *** */
			SUBSTITUTE_PHYSICAL_ENTITY(node, text_ptr->name, text_ptr->size);

			return_ptr->id = WEBDAV_OPENDIR_TEXT;
			return_ptr->data_ptr = text_ptr;
			break;

		default:
			break;

	}	/* end switch */

	return (return_value);

name_too_long:
malloc_text_ptr:
malloc_element_ptr:
	free(return_ptr);
malloc_return_ptr:
	return (NULL);
}

/*****************************************************************************/

static void *parser_file_count_create(CFXMLParserRef parser, CFXMLNodeRef node, void *context)
{
	#pragma unused(parser)
	CFRange comparison_range;

	/* Look for hrefs & count them. */
	switch (CFXMLNodeGetTypeCode(node))
	{
		case kCFXMLNodeTypeElement:
			{
				CFStringRef str = CFXMLNodeGetString(node);
				comparison_range = CFStringFind(str, CFSTR(":"), 0);
				comparison_range.location++;
				comparison_range.length = CFStringGetLength(str) - comparison_range.location;
				if (((CFStringCompareWithOptions(str, CFSTR("href"), comparison_range,
					kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
				{
					*((int *)context) += 1;
				}
				/* end if href */

				break;
			}
			
		default:
			break;
	}											/* end switch */

	return ((void *)1);							/* have to return something or the parser will stop parsing */
}

/*****************************************************************************/

static void *parser_stat_create(CFXMLParserRef parser, CFXMLNodeRef node, void *context)
{
	#pragma unused(parser)
	void *return_val = (void *)WEBDAV_STAT_IGNORE;
	char *text_ptr;
	size_t size;
	CFRange comparison_range;
	CFStringRef nodeString = CFXMLNodeGetString(node);
	/* See if this is a type we are interested in  If it is, we'll return
	  the appropriate constant */

	switch (CFXMLNodeGetTypeCode(node))
	{
		case kCFXMLNodeTypeElement:
			comparison_range = CFStringFind(nodeString, CFSTR(":"), 0);
			comparison_range.location++;
			comparison_range.length = CFStringGetLength(nodeString) - comparison_range.location;

			if (((CFStringCompareWithOptions(nodeString, CFSTR("getcontentlength"),
				comparison_range, kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
			{
				return_val = (void *)WEBDAV_STAT_LENGTH;
			}
			else
			{
				if (((CFStringCompareWithOptions(nodeString, CFSTR("getlastmodified"),
					comparison_range, kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
				{
					return_val = (void *)WEBDAV_STAT_MODDATE;
				}
				else
				{
					if (((CFStringCompareWithOptions(nodeString, CFSTR("collection"),
						comparison_range, kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
					{
						/* It's a collection so set the type as VDIR */
						((struct stat *)context)->st_mode = S_IFDIR;
					}	/* end if collection */
				}	/* end of if-else mod date */
			}	/* end if-else length*/
			break;

		case kCFXMLNodeTypeEntityReference:
		case kCFXMLNodeTypeText:
		case kCFXMLNodeTypeCDATASection:
			text_ptr = malloc(WEBDAV_MAX_STAT_SIZE);
			require_action(text_ptr != NULL, malloc_text_ptr, return_val = NULL);
			
			/* Get the bytes out in UTF8 form */
			require_action(CFStringGetBytes(nodeString, CFRangeMake(0, CFStringGetLength(nodeString)),
				kCFStringEncodingUTF8, /*stop on loss */ 0,/*not ext*/  0, text_ptr,
				WEBDAV_MAX_STAT_SIZE - 1, &size) == CFStringGetLength(nodeString),
				name_too_long, free(text_ptr); return_val = NULL);
			
			/* null terminate the string */
			text_ptr[size] = '\0';

			/* Workaround for lack of support for kCFXMLParserReplacePhysicalEntities option *** */
			SUBSTITUTE_PHYSICAL_ENTITY(node, text_ptr, size);

			return_val = (void *)text_ptr;
			break;

		default:
			break;
	}	/* end switch */

name_too_long:
malloc_text_ptr:

	return (return_val);
}

/*****************************************************************************/

static void *parser_statfs_create(CFXMLParserRef parser, CFXMLNodeRef node, void *context)
{
	#pragma unused(parser, context)
	void *return_val = (void *)WEBDAV_STATFS_IGNORE;
	char *text_ptr;
	size_t size;
	CFRange comparison_range;
	CFStringRef nodeString = CFXMLNodeGetString(node);
	/* See if this is a type we are interested in  If it is, we'll return
	  the appropriate constant */

	switch (CFXMLNodeGetTypeCode(node))
	{
		case kCFXMLNodeTypeElement:
			comparison_range = CFStringFind(nodeString, CFSTR(":"), 0);
			comparison_range.location++;
			comparison_range.length = CFStringGetLength(nodeString) - comparison_range.location;

			if (((CFStringCompareWithOptions(nodeString, CFSTR("quota"), comparison_range,
				kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
			{
				return_val = (void *)WEBDAV_STATFS_QUOTA;
			}
			else
			{
				if (((CFStringCompareWithOptions(nodeString, CFSTR("quotaused"),
					comparison_range, kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
				{
					return_val = (void *)WEBDAV_STATFS_QUOTAUSED;
				}
			}
			break;

		case kCFXMLNodeTypeEntityReference:
		case kCFXMLNodeTypeText:
		case kCFXMLNodeTypeCDATASection:
			text_ptr = malloc(WEBDAV_MAX_STATFS_SIZE);
			require_action(text_ptr != NULL, malloc_text_ptr, return_val = NULL);
			
			/* Get the bytes out in UTF8 form */
			require_action(CFStringGetBytes(nodeString, CFRangeMake(0, CFStringGetLength(nodeString)),
				kCFStringEncodingUTF8, /*stop on loss */ 0,/*not ext*/  0, text_ptr,
				WEBDAV_MAX_STATFS_SIZE - 1, &size) == CFStringGetLength(nodeString),
				name_too_long, free(text_ptr); return_val = NULL);
			
			/* null terminate the string */
			text_ptr[size] = '\0';

			/* Workaround for lack of support for kCFXMLParserReplacePhysicalEntities option *** */
			SUBSTITUTE_PHYSICAL_ENTITY(node, text_ptr, size);

			return_val = (void *)text_ptr;
			break;

		default:
			break;
	}	/* end switch */

name_too_long:
malloc_text_ptr:

	return (return_val);
}

/*****************************************************************************/

static void *parser_lock_create(CFXMLParserRef parser, CFXMLNodeRef node, void *context)
{
	#pragma unused(parser)
	void *return_val = (void *)WEBDAV_LOCK_CONTINUE;
	char *text_ptr;
	size_t text_size, string_size;
	CFRange comparison_range;
	CFStringRef nodeString = CFXMLNodeGetString(node);
	webdav_parse_lock_struct_t *lock_struct = (webdav_parse_lock_struct_t *)context;

	/* See if this is a type we are interested in  If it is, we'll return
	  the appropriate constant */

	switch (CFXMLNodeGetTypeCode(node))
	{
		case kCFXMLNodeTypeElement:
			comparison_range = CFStringFind(nodeString, CFSTR(":"), 0);
			comparison_range.location++;
			comparison_range.length = CFStringGetLength(nodeString) - comparison_range.location;

			if (((CFStringCompareWithOptions(nodeString, CFSTR("locktoken"), comparison_range,
				kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
			{
				lock_struct->context = WEBDAV_LOCK_TOKEN;
			}
			else
			{
				if (((CFStringCompareWithOptions(nodeString, CFSTR("href"), comparison_range,
					kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
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
			break;

		case kCFXMLNodeTypeEntityReference:
		case kCFXMLNodeTypeText:
		case kCFXMLNodeTypeCDATASection:
			if (lock_struct->context == WEBDAV_LOCK_HREF)
			{
				lock_struct->context = 0; /* clear the context so the next time through it's not in WEBDAV_LOCK_HREF */
				
				/* Since the context is set to WEBDAV_LOCK_HREF, we have
				 * found the token and we have found the href indicating
				 * that the locktoken is coming so squirrel it away.
				 */
				text_size = CFStringGetLength(nodeString);
				text_ptr = malloc(text_size + 1);
				require_action(text_ptr != NULL, malloc_text_ptr, return_val = NULL);
				
				/* Get the bytes out in UTF8 form */
				require_action(((size_t)CFStringGetBytes(nodeString, CFRangeMake(0, text_size), kCFStringEncodingUTF8,
					/*stop on loss */ 0,/*not ext*/  0, text_ptr, WEBDAV_MAX_STAT_SIZE - 1, &string_size) == text_size),
					name_too_long, free(text_ptr); return_val = NULL);
				
				/* null terminate the string */
				text_ptr[string_size] = '\0';

				/* Workaround for lack of support for kCFXMLParserReplacePhysicalEntities option *** */
				SUBSTITUTE_PHYSICAL_ENTITY(node, text_ptr, string_size);

				lock_struct->locktoken = text_ptr;

				/* Since we found what we are looking for, return NULL
				 * to stop the parse.
				 */
				return_val = NULL;
			}
			break;

		default:
			break;
	}											/* end switch */

name_too_long:
malloc_text_ptr:

	return (return_val);
}

/*****************************************************************************/

static void *parser_cachevalidators_create(CFXMLParserRef parser, CFXMLNodeRef node, void *context)
{
	#pragma unused(parser, context)
	void *return_val = (void *)WEBDAV_CACHEVALIDATORS_IGNORE;
	char *text_ptr;
	size_t text_size;
	size_t size;
	CFRange comparison_range;
	CFStringRef nodeString = CFXMLNodeGetString(node);
	
	/* See if this is a type we are interested in  If it is, we'll return
	  the appropriate constant */
	switch (CFXMLNodeGetTypeCode(node))
	{
		case kCFXMLNodeTypeElement:
			comparison_range = CFStringFind(nodeString, CFSTR(":"), 0);
			comparison_range.location++;
			comparison_range.length = CFStringGetLength(nodeString) - comparison_range.location;

			if (((CFStringCompareWithOptions(nodeString, CFSTR("getlastmodified"),
				comparison_range, kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
			{
				return_val = (void *)WEBDAV_CACHEVALIDATORS_MODDATE;
			}
			else if (((CFStringCompareWithOptions(nodeString, CFSTR("getetag"),
				comparison_range, kCFCompareCaseInsensitive)) == kCFCompareEqualTo))
			{
				return_val = (void *)WEBDAV_CACHEVALIDATORS_ETAG;
			}
			break;

		case kCFXMLNodeTypeEntityReference:
		case kCFXMLNodeTypeText:
		case kCFXMLNodeTypeCDATASection:
			text_size = CFStringGetLength(nodeString);
			text_ptr = malloc(text_size + 1);
			require_action(text_ptr != NULL, malloc_text_ptr, return_val = NULL);
			
			/* Get the bytes out in UTF8 form */
			require_action((size_t)CFStringGetBytes(nodeString, CFRangeMake(0, text_size),
				kCFStringEncodingUTF8, /*stop on loss */ 0,/*not ext*/  0, text_ptr,
				text_size, &size) == text_size,
				name_too_long, free(text_ptr); return_val = NULL);
			
			/* null terminate the string */
			text_ptr[size] = '\0';

			/* Workaround for lack of support for kCFXMLParserReplacePhysicalEntities option *** */
			SUBSTITUTE_PHYSICAL_ENTITY(node, text_ptr, size);

			return_val = (void *)text_ptr;
			break;

		default:
			break;
	}	/* end switch */

name_too_long:
malloc_text_ptr:

	return (return_val);
}

/*****************************************************************************/

static void parser_add(CFXMLParserRef parser, void *parent, void *child, void *context)
{
	#pragma unused(parser, parent, child, context)
	/* Add nothing and do nothing.	We are not actually creating the tree */
	return;
}

/*****************************************************************************/

static void parser_opendir_add(CFXMLParserRef parser, void *parent, void *child, void *context)
{
	#pragma unused(parser)
	webdav_parse_opendir_element_t * element_ptr;
	webdav_parse_opendir_return_t * parent_ptr = (webdav_parse_opendir_return_t *)parent;
	webdav_parse_opendir_return_t * child_ptr = (webdav_parse_opendir_return_t *)child;
	webdav_parse_opendir_text_t * text_ptr;
	webdav_parse_opendir_struct_t * struct_ptr = (webdav_parse_opendir_struct_t *)context;
	char *ep;

	/* If the parent is one of our returned directory elements, and if this is a
	 * text element, than copy the text into the name buffer provided we have room */

	if (child_ptr->id == WEBDAV_OPENDIR_TEXT)
	{
		text_ptr = (webdav_parse_opendir_text_t *)child_ptr->data_ptr;

		switch (parent_ptr->id)
		{
			case WEBDAV_OPENDIR_ELEMENT:
				element_ptr = (webdav_parse_opendir_element_t *)parent_ptr->data_ptr;

				/* make sure the complete name will fit in the structure */
				if ((element_ptr->dir_data.d_name_URI_length + text_ptr->size) <=
					(sizeof(element_ptr->dir_data.d_name) - 1))
				{
					bcopy(text_ptr->name,
						&element_ptr->dir_data.d_name[element_ptr->dir_data.d_name_URI_length],
						text_ptr->size);
					element_ptr->dir_data.d_name_URI_length += text_ptr->size;
				}
				else
				{
					debug_string("URI too long");
					struct_ptr->error = ENAMETOOLONG;
				}
				break;

			case WEBDAV_OPENDIR_ELEMENT_LENGTH:
				element_ptr = (webdav_parse_opendir_element_t *)parent_ptr->data_ptr;
				element_ptr->statsize = strtoq(text_ptr->name, &ep, 10);
				break;

			case WEBDAV_OPENDIR_ELEMENT_MODDATE:
				element_ptr = (webdav_parse_opendir_element_t *)parent_ptr->data_ptr;
				element_ptr->stattime.tv_sec = DateBytesToTime(text_ptr->name, strlen(text_ptr->name));
				if (element_ptr->stattime.tv_sec == -1)
				{
					element_ptr->stattime.tv_sec = 0;
				}
				element_ptr->stattime.tv_nsec = 0;
				break;

			case WEBDAV_OPENDIR_APPLEDOUBLEHEADER:
				{
					size_t	len = APPLEDOUBLEHEADER_LENGTH;

					element_ptr = (webdav_parse_opendir_element_t *)parent_ptr->data_ptr;
					from_base64(text_ptr->name, element_ptr->appledoubleheader, &len);
					if (len == APPLEDOUBLEHEADER_LENGTH)
					{
						element_ptr->appledoubleheadervalid = TRUE;
					}
				}
				break;

			default:
				break;
		}	/* end of switch statement */
		
		/*
		 * The text pointer is not an element that persists in the context and is
		 * thus not freed by parse_opendir like element pointers are.  Since we
		 * allocated it in the create routine, we must free it here
		 */
		free(text_ptr);
	}	/* end of if it is our text element */
}

/*****************************************************************************/

static void parser_stat_add(CFXMLParserRef parser, void *parent, void *child, void *context)
{
	#pragma unused(parser)
	char *text_ptr = (char *)child;
	struct stat *statbuf = (struct stat *)context;
	char *ep;

	/*
	 * If the parent reflects one of our properties than the child must
	 * be the text pointer with the data we want
	 */
	switch ((int)parent)
	{
		case WEBDAV_STAT_LENGTH:
			/* the text pointer is the length in bytes so put it in the stat buffer */
			if (text_ptr && (text_ptr != (char *)WEBDAV_STAT_IGNORE))
			{
				statbuf->st_size = strtoq(text_ptr, &ep, 10);
				free(text_ptr);
			}
			else
			{
				/* if we got a string set to NULL we could not fit the value in our buffer, return invalid value */
				statbuf->st_size = -1LL;
			}
			break;

		case WEBDAV_STAT_MODDATE:
			/* the text pointer is the date so translate it and put it into the stat buffer */

			if (text_ptr && (text_ptr != (char *)WEBDAV_STAT_IGNORE))
			{
				statbuf->st_mtimespec.tv_sec = DateBytesToTime(text_ptr, strlen(text_ptr));
				if (statbuf->st_mtimespec.tv_sec == -1)
				{
					statbuf->st_mtimespec.tv_sec = 0;
				}
				statbuf->st_mtimespec.tv_nsec = 0;
				free(text_ptr);
			}
			else
			{
				/* if we got a string set to NULL we could not fit the value in our buffer, do nothing */
			}
			break;

		default:
			/*
			 * if it is ignore or something else, check the child. If it is not
			 * a return value and isn't null, it's a text ptr so free it
			 */
			if (text_ptr &&
				text_ptr != (char *)WEBDAV_STAT_IGNORE &&
				text_ptr != (char *)WEBDAV_STAT_LENGTH &&
				text_ptr != (char *)WEBDAV_STAT_MODDATE)
			{
				free(text_ptr);
			}
			break;
	}

	return;
}

/*****************************************************************************/

static void parser_statfs_add(CFXMLParserRef parser, void *parent, void *child, void *context)
{
	#pragma unused(parser)
	char *text_ptr = (char *)child;
	struct statfs *statfsbuf = (struct statfs *)context;
	char *ep;

	/*
	 * If the parent reflects one of our properties than the child must
	 * be the text pointer with the data we want
	 */
	switch ((int)parent)
	{
		case WEBDAV_STATFS_QUOTA:
			/* the text pointer is the length in blocks so put it in the statfs buffer */
			if (text_ptr && (text_ptr != (char *)WEBDAV_STATFS_IGNORE))
			{
				statfsbuf->f_blocks = strtoq(text_ptr, &ep, 10);
				free(text_ptr);
			}
			else
			{
				/* if we got a string set to NULL we could not fit the value in our buffer, do nothing */
			}
			break;

		case WEBDAV_STATFS_QUOTAUSED:
			/* the text pointer is the data so translate it so put it in the statfs buffer */
			if (text_ptr && (text_ptr != (char *)WEBDAV_STATFS_IGNORE))
			{
				statfsbuf->f_bavail = statfsbuf->f_bfree = strtoq(text_ptr, &ep, 10);
				free(text_ptr);
			}
			else
			{
				/* if we got a string set to NULL we could not fit the value in our buffer, do nothing */
			}
			break;

		default:
			/*
			 * if it is ignore or something else, check the child. If it is not
			 * a return value and isn't null, it's a text ptr so free it
			 */
			if (text_ptr &&
				text_ptr != (char *)WEBDAV_STATFS_IGNORE &&
				text_ptr != (char *)WEBDAV_STATFS_QUOTA && 
				text_ptr != (char *)WEBDAV_STATFS_QUOTAUSED)
			{
				free(text_ptr);
			}
			break;
	}

	return;
}

/*****************************************************************************/

static void parser_cachevalidators_add(CFXMLParserRef parser, void *parent, void *child, void *context)
{
	#pragma unused(parser)
	char *text_ptr = (char *)child;
	struct webdav_parse_cachevalidators_struct *cachevalidators_struct = (struct webdav_parse_cachevalidators_struct *)context;

	/*
	 * If the parent reflects one of our properties than the child must
	 * be the text pointer with the data we want
	 */
	switch ((int)parent)
	{
		case WEBDAV_CACHEVALIDATORS_MODDATE:
			/* the text pointer is the date so translate it and get it into last_modified */
			if (text_ptr && (text_ptr != (char *)WEBDAV_CACHEVALIDATORS_IGNORE))
			{
				cachevalidators_struct->last_modified = DateBytesToTime(text_ptr, strlen(text_ptr));
				free(text_ptr);
			}
			else
			{
				/* if we got a string set to NULL we could not fit the value in our buffer, do nothing */
			}
			break;

		case WEBDAV_CACHEVALIDATORS_ETAG:
			/* the text pointer is the etag so copy it into last_modified */
			if (text_ptr && (text_ptr != (char *)WEBDAV_CACHEVALIDATORS_IGNORE))
			{
				cachevalidators_struct->entity_tag = malloc(strlen(text_ptr) + 1);
				if ( cachevalidators_struct->entity_tag != NULL )
				{
					strcpy(cachevalidators_struct->entity_tag, text_ptr);
				}
				free(text_ptr);
			}
			else
			{
				/* if we got a string set to NULL we could not fit the value in our buffer, do nothing */
			}
			break;

		default:
			/*
			 * if it is ignore or something else, check the child. If it is not
			 * a return value and isn't null, it's a text ptr so free it
			 */
			if (text_ptr &&
				text_ptr != (char *)WEBDAV_CACHEVALIDATORS_IGNORE &&
				text_ptr != (char *)WEBDAV_CACHEVALIDATORS_MODDATE &&
				text_ptr != (char *)WEBDAV_CACHEVALIDATORS_ETAG)
			{
				free(text_ptr);
			}
			break;
	}

	return;
}

/*****************************************************************************/

static void parser_end(CFXMLParserRef parser, void *xml_type, void *context)
{
	#pragma unused(parser, xml_type, context)
	return;	/* deallocate the tree ? */
}

/*****************************************************************************/

static void parser_opendir_end(CFXMLParserRef parser, void *my_element, void *context)
{
	#pragma unused(parser, context)
	if (my_element)
	{
		free(my_element);	/* free up the space we reserved for the return_ptr */
	}
	return;
}

/*****************************************************************************/

static CFDataRef parser_resolve(CFXMLParserRef parser, CFXMLExternalID *extID, void *context)
{
	#pragma unused(parser, extID, context)
	return (NULL);
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
	CFStringRef uriString;	/* URI as CFString */
	CFURLRef uriURL;		/* URI converted to full URL */
	CFStringRef uriName;	/* URI's LastPathComponent as CFString */
	
	result = FALSE;
	
	/* create a CFString from the c-string containing URI */
	uriString = CFStringCreateWithCString(kCFAllocatorDefault, uri, kCFStringEncodingUTF8);
	require(uriString != NULL, CFStringCreateWithCString);
	
	/* create a CFURL from the URI CFString and the parent URL */
	uriURL = CFURLCreateWithString(kCFAllocatorDefault, uriString, urlRef);
	CFRelease(uriString);
	require(uriURL != NULL, CFURLCreateWithString);
	
	/* see if this is the parent or a child */
	if ( GetNormalizedPathLength(uriURL) > parentPathLength )
	{
		/* this is a child */
		
		/* get the child's name */
		uriName = CFURLCopyLastPathComponent(uriURL);
		require_string(uriName != NULL, CFURLCopyLastPathComponent, "name was not legal UTF8");
		
		if ( CFStringGetCString(uriName, componentName, MAXNAMLEN + 1, kCFStringEncodingUTF8) )
		{
			/* we have the child name */
			result = TRUE;
		}
		else
		{
			debug_string("could not get child name (too long?)");
		}
		CFRelease(uriName);
	}
	else
	{
		/* this is the parent, skip it */
	}

CFURLCopyLastPathComponent:

	CFRelease(uriURL);

CFURLCreateWithString:
CFStringCreateWithCString:
	
	return ( result );
}

/*****************************************************************************/

int parse_opendir(
	UInt8 *xmlp,					/* -> xml data returned by PROPFIND with depth of 1 */
	CFIndex xmlp_len,				/* -> length of xml data */
	CFURLRef urlRef,				/* -> the CFURL to the parent directory */
	uid_t uid,						/* -> uid of the user making the request */ 
	struct node_entry *parent_node)	/* -> pointer to the parent directory's node_entry */
{
	CFDataRef xml_dataref;
	CFXMLParserCallBacks callbacks =
	{
		0, parser_opendir_create, parser_opendir_add, parser_opendir_end, parser_resolve, NULL
	};
	webdav_parse_opendir_struct_t opendir_struct;
	webdav_parse_opendir_element_t *element_ptr, *prev_element_ptr;
	CFXMLParserContext context =
	{
		0,	&opendir_struct, NULL, NULL, NULL
	};
	CFXMLParserRef parser;
	int error;
	ssize_t size;
	struct dirent dir_data[2];
	CFIndex parentPathLength;
	
	error = 0;
	opendir_struct.head = opendir_struct.tail = NULL;
	opendir_struct.error = 0;

	/* truncate the file, and reset the file pointer to 0 */
	require(ftruncate(parent_node->file_fd, 0) == 0, ftruncate);
	require(lseek(parent_node->file_fd, 0, SEEK_SET) == 0, lseek);
	
	/* get the xml data into a form Core Foundation can understand */
	xml_dataref = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, xmlp, xmlp_len, kCFAllocatorNull);
	require(xml_dataref != NULL, CFDataCreateWithBytesNoCopy);

	/* create the Parser */
	/* Note: if supported, could use kCFXMLParserReplacePhysicalEntities for options */
	parser = CFXMLParserCreate(kCFAllocatorDefault, xml_dataref, NULL, kCFXMLParserNoOptions, kCFXMLNodeCurrentVersion, &callbacks, &context);
	require(parser != NULL, CFXMLParserCreate);
	
	/* parse the XML -- exit now if error during parse */
	require((CFXMLParserParse(parser) == true) && (opendir_struct.error == 0), CFXMLParserParse);

	/* if the directory is not deleted, write "." and ".."  */
	if ( !NODE_IS_DELETED(parent_node) )
	{
		bzero(dir_data, sizeof(dir_data));

		dir_data[0].d_ino = parent_node->fileid;
		dir_data[0].d_reclen = sizeof(struct dirent);
		dir_data[0].d_type = DT_DIR;
		dir_data[0].d_namlen = 1;
		dir_data[0].d_name[0] = '.';

		dir_data[1].d_ino =
			(dir_data[0].d_ino == WEBDAV_ROOTFILEID) ? WEBDAV_ROOTPARENTFILEID : parent_node->parent->fileid;
		dir_data[1].d_reclen = sizeof(struct dirent);
		dir_data[1].d_type = DT_DIR;
		dir_data[1].d_namlen = 2;
		dir_data[1].d_name[0] = '.';
		dir_data[1].d_name[1] = '.';

		size = write(parent_node->file_fd, dir_data, sizeof(struct dirent) * 2);
		require(size == (sizeof(struct dirent) * 2), write_dot_dotdot);
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
	
	/* look at the list of elements */
	for (element_ptr = opendir_struct.head; element_ptr != NULL; element_ptr = element_ptr->next)
	{
		char namebuffer[MAXNAMLEN + 1];
		
		/* make element_ptr->dir_data.d_name a cstring */
		element_ptr->dir_data.d_name[element_ptr->dir_data.d_name_URI_length] = '\0';

		/* get the component name if this element is not the parent */
		if ( GetComponentName(urlRef, parentPathLength, element_ptr->dir_data.d_name, namebuffer) )
		{
			/* this is a child */
			struct node_entry *element_node;
			int name_len;
			struct stat statbuf;
			
			name_len = strlen(namebuffer);
			
			/* get (or create) a cache node for this element */
			error = nodecache_get_node(parent_node, name_len, namebuffer, TRUE,
				element_ptr->dir_data.d_type == DT_DIR ? WEBDAV_DIR_TYPE : WEBDAV_FILE_TYPE, &element_node);
			if (error)
			{
				debug_string("nodecache_get_node failed");
				continue;
			}
			
			/* move just the element name over element_ptr->dir_data.d_name */
			bcopy(namebuffer, element_ptr->dir_data.d_name, (size_t)name_len);
			element_ptr->dir_data.d_name[name_len] = '\0';
			element_ptr->dir_data.d_namlen = name_len;
			
			/* set the file number */
			element_ptr->dir_data.d_ino = element_node->fileid;
			
			/*
			 * Prepare to cache this element's attributes, since it's
			 * highly likely a stat will follow reading the directory.
			 */

			bzero(&statbuf, sizeof(struct stat));
			
			/* the first thing to do is fill in the fields we cannot get from the server. */
			statbuf.st_dev = 0;
			/* Why 1 for st_nlink?
			 * Getting the real link count for directories is expensive.
			 * Setting it to 1 lets FTS(3) (and other utilities that assume
			 * 1 means a file system doesn't support link counts) work.
			 */
			statbuf.st_nlink = 1;
			statbuf.st_uid = UNKNOWNUID;
			statbuf.st_gid = UNKNOWNUID;
			statbuf.st_rdev = 0;
			statbuf.st_blksize = WEBDAV_IOSIZE;
			statbuf.st_flags = 0;
			statbuf.st_gen = 0;
			
			/* set all times to the last modified time since we cannot get the other times */
			statbuf.st_atimespec = statbuf.st_mtimespec = statbuf.st_ctimespec = element_ptr->stattime;
			
			if (element_ptr->dir_data.d_type == DT_DIR)
			{
				statbuf.st_mode = S_IFDIR | ACCESSPERMS;
				statbuf.st_size = WEBDAV_DIR_SIZE;
				/* appledoubleheadervalid is never valid for directories */
				element_ptr->appledoubleheadervalid = FALSE;
			}
			else
			{
				statbuf.st_mode = S_IFREG | ACCESSPERMS;
				statbuf.st_size = element_ptr->statsize;
				/* appledoubleheadervalid is valid for files only if the server
				 * returned the appledoubleheader property and file size is
				 * the size of the appledoubleheader (APPLEDOUBLEHEADER_LENGTH bytes).
				 */
				element_ptr->appledoubleheadervalid =
					(element_ptr->appledoubleheadervalid && (element_ptr->statsize == APPLEDOUBLEHEADER_LENGTH));
			}

			/* calculate number of S_BLKSIZE blocks */
			statbuf.st_blocks = ((statbuf.st_size + S_BLKSIZE - 1) / S_BLKSIZE);

			/* set the fileid in statbuf*/
			statbuf.st_ino = element_node->fileid;
			
			/* Now cache the stat structure (ignoring errors) */
			(void) nodecache_add_attributes(element_node, uid, &statbuf,
				element_ptr->appledoubleheadervalid ? element_ptr->appledoubleheader : NULL);
			
			/* Complete the task of getting the regular name into the dirent */

			size = write(parent_node->file_fd, (void *)&element_ptr->dir_data, element_ptr->dir_data.d_reclen);
			require(size == element_ptr->dir_data.d_reclen, write_element);
		}
	}	/* for element_ptr */
		
	/* free any elements allocated */
	element_ptr = opendir_struct.head;
	while (element_ptr)
	{
		prev_element_ptr = element_ptr;
		element_ptr = element_ptr->next;
		free(prev_element_ptr);
	}
	
	CFRelease(parser);
	CFRelease(xml_dataref);
	
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
CFXMLParserParse:
	CFRelease(parser);
CFXMLParserCreate:
	CFRelease(xml_dataref);
CFDataCreateWithBytesNoCopy:
lseek:
ftruncate:
	return ( EIO );
}

/*****************************************************************************/

int parse_file_count(char *xmlp, int xmlp_len, int *file_count)
{
	CFXMLParserCallBacks callbacks =
	{
		0, parser_file_count_create, parser_add, parser_end, parser_resolve, NULL
	};
	CFXMLParserContext context =
	{
		0, file_count, NULL, NULL, NULL
	};
	CFDataRef xml_dataref;
	CFXMLParserRef parser;
	
	*file_count = 0;

	/* get the xml data into a form Core Foundation can understand */
	xml_dataref = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, xmlp,
		(CFIndex)xmlp_len, kCFAllocatorNull);
	if ( xml_dataref != NULL )
	{
		/* create the Parser */
		/* Note: if supported, could use kCFXMLParserReplacePhysicalEntities for options */
		parser = CFXMLParserCreate(kCFAllocatorDefault, xml_dataref, NULL, kCFXMLParserNoOptions, 
			kCFXMLNodeCurrentVersion, &callbacks, &context);
		if ( parser != NULL )
		{
			CFXMLParserParse(parser);
			CFRelease(parser);
		}
		CFRelease(xml_dataref);
	}

	return ( 0 );
}

/*****************************************************************************/

int parse_stat(char *xmlp, int xmlp_len, struct stat *statbuf)
{
	CFXMLParserCallBacks callbacks =
	{
		0, parser_stat_create, parser_stat_add, parser_end, parser_resolve, NULL
	};
	CFXMLParserContext context =
	{
		0, statbuf, NULL, NULL, NULL
	};
	CFDataRef xml_dataref;
	CFXMLParserRef parser;

	bzero((void *)statbuf, sizeof(struct stat));
	
	/* get the xml data into a form Core Foundation can understand */
	xml_dataref = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, xmlp, 
		(CFIndex)xmlp_len, kCFAllocatorNull);
	if ( xml_dataref != NULL )
	{
		/* create the Parser */
		/* Note: if supported, could use kCFXMLParserReplacePhysicalEntities for options */
		parser = CFXMLParserCreate(kCFAllocatorDefault, xml_dataref, NULL, kCFXMLParserNoOptions, 
			kCFXMLNodeCurrentVersion, &callbacks, &context);
		if ( parser != NULL )
		{
			CFXMLParserParse(parser);
			CFRelease(parser);
		}
		CFRelease(xml_dataref);
	}

	/* Coming back from the parser:
	 *   statbuf->st_mode will be 0 or will have S_IFDIR set if the object is a directory.
	 *   statbuf->st_mtimespec will be 0 or will have the last modified time.
	 *   statbuf->st_size will be set to the resource size if the object is a file.
	 *
	 * So, the first thing to do is fill in the fields we cannot get from the server.
	 */
	
	statbuf->st_dev = 0;
	/* Why 1 for st_nlink?
	 * Getting the real link count for directories is expensive.
	 * Setting it to 1 lets FTS(3) (and other utilities that assume
	 * 1 means a file system doesn't support link counts) work.
	 */
	statbuf->st_nlink = 1;
	statbuf->st_uid = UNKNOWNUID;
	statbuf->st_gid = UNKNOWNUID;
	statbuf->st_rdev = 0;
	statbuf->st_blksize = WEBDAV_IOSIZE;
	statbuf->st_flags = 0;
	statbuf->st_gen = 0;
	
	/* set all times to the last modified time since we cannot get the other times */
	statbuf->st_atimespec = statbuf->st_ctimespec = statbuf->st_mtimespec;
	
	/* was this a directory? */
	if ( S_ISDIR(statbuf->st_mode) )
	{
		/* yes - add the directory access permissions */
		statbuf->st_mode |= ACCESSPERMS;
		/* fake up the directory size */
		statbuf->st_size = WEBDAV_DIR_SIZE;
	}
	else
	{
		/* no - mark it as a regular file and set the file access permissions
		 * (for now, everything is either a file or a directory)
		 */
		statbuf->st_mode = S_IFREG | ACCESSPERMS;
	}

	/* calculate number of S_BLKSIZE blocks */
	statbuf->st_blocks = ((statbuf->st_size + S_BLKSIZE - 1) / S_BLKSIZE);
	
	return ( 0 );
}

/*****************************************************************************/

int parse_statfs(char *xmlp, int xmlp_len, struct statfs *statfsbuf)
{
	CFXMLParserCallBacks callbacks =
	{
		0, parser_statfs_create, parser_statfs_add, parser_end, parser_resolve, NULL
	};
	CFXMLParserContext context =
	{
		0, statfsbuf, NULL, NULL, NULL
	};
	CFDataRef xml_dataref;
	CFXMLParserRef parser;

	bzero((void *)statfsbuf, sizeof(struct statfs));
	
	/* get the xml data into a form Core Foundation can understand */
	xml_dataref = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, xmlp,
		(CFIndex)xmlp_len, kCFAllocatorNull);
	if ( xml_dataref != NULL )
	{
		/* create the Parser */
		/* Note: if supported, could use kCFXMLParserReplacePhysicalEntities for options */
		parser = CFXMLParserCreate(kCFAllocatorDefault, xml_dataref, NULL, kCFXMLParserNoOptions, 
			kCFXMLNodeCurrentVersion, &callbacks, &context);
		if ( parser != NULL )
		{
			CFXMLParserParse(parser);
			CFRelease(parser);
		}
		CFRelease(xml_dataref);
	}

	/*
	 * now turn quota-used (which we temporarily stored in f_bfree) into 
	 * f_bavail with a little subtraction
	 */
	if (statfsbuf->f_blocks && (statfsbuf->f_blocks > statfsbuf->f_bfree))
	{
		statfsbuf->f_bavail = statfsbuf->f_bfree = (statfsbuf->f_blocks - statfsbuf->f_bfree);
	}
	else
	{
		statfsbuf->f_bavail = statfsbuf->f_bfree = 0;
	}
	
	statfsbuf->f_iosize = WEBDAV_IOSIZE;

	return ( 0 );
}

/*****************************************************************************/

int parse_lock(char *xmlp, int xmlp_len, char **locktoken)
{
	CFXMLParserCallBacks callbacks =
	{
		0, parser_lock_create, parser_add, parser_end, parser_resolve, NULL
	};
	webdav_parse_lock_struct_t lock_struct;
	CFXMLParserContext context =
	{
		0, &lock_struct, NULL, NULL, NULL
	};
	CFDataRef xml_dataref;
	CFXMLParserRef parser;
	
	lock_struct.context = 0;
	lock_struct.locktoken = NULL;	/* NULL coming into this function */

	/* get the xml data into a form Core Foundation can understand */
	xml_dataref = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, xmlp,
		(CFIndex)xmlp_len, kCFAllocatorNull);
	if ( xml_dataref != NULL )
	{
		/* create the Parser */
		/* Note: if supported, could use kCFXMLParserReplacePhysicalEntities for options */
		parser = CFXMLParserCreate(kCFAllocatorDefault, xml_dataref, NULL, kCFXMLParserNoOptions, 
			kCFXMLNodeCurrentVersion, &callbacks, &context);
		if ( parser != NULL )
		{
			CFXMLParserParse(parser);
			CFRelease(parser);
		}
		CFRelease(xml_dataref);
	}

	*locktoken = lock_struct.locktoken;
	if (*locktoken == NULL)
	{
		debug_string("error parsing lock token");
	}

	return ( 0 );
}

/*****************************************************************************/

int parse_cachevalidators(char *xmlp, int xmlp_len, time_t *last_modified, char **entity_tag)
{
	CFXMLParserCallBacks callbacks =
	{
		0, parser_cachevalidators_create, parser_cachevalidators_add, parser_end, parser_resolve, NULL
	};
	struct webdav_parse_cachevalidators_struct cachevalidators_struct;
	CFXMLParserContext context =
	{
		0, &cachevalidators_struct, NULL, NULL, NULL
	};
	CFDataRef xml_dataref;
	CFXMLParserRef parser;
	
	/* results if parser fails or values are not found */
	cachevalidators_struct.last_modified = 0;
	cachevalidators_struct.entity_tag = NULL;
	
	/* get the xml data into a form Core Foundation can understand */
	xml_dataref = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, xmlp, 
		(CFIndex)xmlp_len, kCFAllocatorNull);
	if ( xml_dataref != NULL )
	{
		/* create the Parser */
		/* Note: if supported, could use kCFXMLParserReplacePhysicalEntities for options */
		parser = CFXMLParserCreate(kCFAllocatorDefault, xml_dataref, NULL, kCFXMLParserNoOptions, 
			kCFXMLNodeCurrentVersion, &callbacks, &context);
		if ( parser != NULL )
		{
			/* parse the XML  */
			CFXMLParserParse(parser);
			CFRelease(parser);
		}
		CFRelease(xml_dataref);
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
