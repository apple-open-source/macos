/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * XDRSerializer.m
 *
 * XDR serializer for lookupd property lists
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <NetInfo/config.h>
#import <NetInfo/system_log.h>
#import "XDRSerializer.h"
#import <netinfo/lookup_types.h>
#import "_lu_types.h"
#import "LUGlobal.h"
#import <stdlib.h>
#import <string.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <NetInfo/dsutil.h>

#define MAX_INPUT_STRING 8192

extern struct ether_addr *ether_aton(char *);
extern char *ether_ntoa(struct ether_addr *);
extern char *nettoa(unsigned long net);

@implementation XDRSerializer

- (void)encodeString:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
	maxLength:(unsigned int)maxLen
{
	char *value;

	value = [item valueForKey:key];
	if (value == NULL) [self encodeString:"" intoXdr:xdrs maxLength:maxLen];
	else [self encodeString:value intoXdr:xdrs maxLength:maxLen];
}

- (void)encodeString:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
{
	[self encodeString:key from:item intoXdr:xdrs maxLength:_LU_MAXLUSTRLEN];
}

- (void)encodeInt:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
	default:(int)def
{
	char *value;
	int i;

	value = [item valueForKey:key];
	if (value == NULL)
	{
		i = def;
		[self encodeInt:i intoXdr:xdrs];
		return;
	}

	i = atoi(value);
	if (!xdr_int(xdrs, &i))
	{
		system_log(LOG_WARNING, "xdr_int failed");
	}
}

- (void)encodeInt:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
{
	[self encodeInt:key from:item intoXdr:xdrs default:0];
}

- (void)encodeIPAddr:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
{
	char *value;
	unsigned long i;

	value = [item valueForKey:key];
	if (value == NULL)
	{
		i = 0;
		[self encodeUnsignedLong:i intoXdr:xdrs];
		return;
	}

	i = inet_addr((char *)value);
	if (!xdr_u_long(xdrs, &i))
	{
		system_log(LOG_WARNING, "xdr_u_long failed");
	}
}

- (void)encodeNetAddr:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
{
	char *value;
	unsigned long i;

	value = [item valueForKey:key];
	if (value == NULL)
	{
		i = 0;
		[self encodeUnsignedLong:i intoXdr:xdrs];
		return;
	}

	i = inet_network((char *)value);
	if (!xdr_u_long(xdrs, &i))
	{
		system_log(LOG_WARNING, "xdr_u_long failed");
	}
}

- (void)encodeENAddr:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
{
	char *value;
	struct ether_addr *ether;

	ether = NULL;
	value = [item valueForKey:key];
	if (value != NULL)
	{
		ether = ether_aton((char *)value);
	}

	if (ether == NULL) ether = ether_aton("0:0:0:0:0:0");
	if (!xdr_opaque(xdrs, (caddr_t)ether, 6))
	{
		system_log(LOG_WARNING, "xdr_opaque failed");
	}
}

- (void)encodeStrings:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
	maxCount:(unsigned int)maxCount
	maxLength:(unsigned int)maxLen
{
	unsigned long i, len, truncated, lpos;
	char **values;
	char *s;

	truncated = 0;
	values = [item valuesForKey:key];
	len = [item countForKey:key];
	if (len == IndexNull) len = 0;

	if (len > maxCount)
	{
		system_log(LOG_ERR, "truncating list at \"%s\" (maximum %d of %d values)", values[maxCount], maxCount, len);
		len = maxCount;
	}
		
	lpos = xdr_getpos(xdrs);
	[self encodeUnsignedLong:len intoXdr:xdrs];

	for (i = 0; i < len; i++)
	{
		s = values[i];
		if (!xdr_string(xdrs, &s, maxLen))
		{
			system_log(LOG_ERR, "truncated list before \"%s\" (%d of %d values)", values[i], i, len);
			truncated = i;
			break;
		}
	}

	if (truncated != 0)
	{
		len = truncated - 1;
		xdr_setpos(xdrs, lpos);
		[self encodeUnsignedLong:len intoXdr:xdrs];

		for (i = 0; i < len; i++)
		{
			s = values[i];
			if (!xdr_string(xdrs, &s, maxLen))
			{
				system_log(LOG_ERR, "xdr_string failed at size %lu (%d of %d strings)", xdr_getpos(xdrs), i, len);
				break;
			}
		}
	}
}

- (void)encodeStrings:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
	max:(unsigned int)maxCount
{
	[self encodeStrings:key from:item intoXdr:xdrs maxCount:maxCount maxLength:_LU_MAXLUSTRLEN];
}

- (void)encodeIPAddrs:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
	max:(int)maxCount
{
	unsigned long i, len;
	char **values;
	unsigned long ip;

	values = [item valuesForKey:key];
	len = [item countForKey:key];
	if (len == IndexNull) len = 0;

	if (len > maxCount)
	{
		system_log(LOG_ERR, "truncating at %d values", maxCount);
		len = maxCount;
	}

	[self encodeUnsignedLong:len intoXdr:xdrs];

	for (i = 0; i < len; i++)
	{
		ip = inet_addr((char *)values[i]);
		[self encodeUnsignedLong:ip intoXdr:xdrs];
	}
}

- (void)encodeString:(char *)aString intoXdr:(XDR *)xdrs maxLength:(unsigned int)maxLen
{
	if (!xdr_string(xdrs, &aString, maxLen))
	{
		system_log(LOG_WARNING, "xdr_string failed");
	}
}

- (void)encodeString:(char *)aString intoXdr:(XDR *)xdrs
{
	[self encodeString:aString intoXdr:xdrs maxLength:_LU_MAXLUSTRLEN];
}

- (void)encodeInt:(int)i intoXdr:(XDR *)xdrs
{
	if (!xdr_int(xdrs, &i))
	{
		system_log(LOG_WARNING, "xdr_int failed");
	}
}

- (void)encodeBool:(BOOL)i intoXdr:(XDR *)xdrs
{
	bool_t b;

	b = i;
	if (!xdr_bool(xdrs, &b))
	{
		system_log(LOG_WARNING, "xdr_bool failed");
	}
}

- (void)encodeUnsignedLong:(unsigned long)i intoXdr:(XDR *)xdrs
{
	if (!xdr_u_long(xdrs, &i))
	{
		system_log(LOG_WARNING, "xdr_u_long failed");
	}
}

/* 
 * decode routines
 */
- (char *)decodeString:(char *)buf length:(int)len;
{
	char *str;
	XDR inxdr;

	if (buf == NULL) return NULL;
	if (len == 0) return NULL;

	xdrmem_create(&inxdr, buf, len, XDR_DECODE);
	str = NULL;
	xdr__lu_string(&inxdr, &str);
	xdr_destroy(&inxdr);

	return str;
}

- (char *)decodeInt:(char *)buf length:(int)len
{
	char *str;
	int i;
	XDR inxdr;

	xdrmem_create(&inxdr, buf, len, XDR_DECODE);
	if (!xdr_int(&inxdr, &i))
	{
		xdr_destroy(&inxdr);
		return NULL;
	}
	xdr_destroy(&inxdr);

	str = malloc(16);
	sprintf(str, "%d", i);

	return str;
}

- (char *)decodeIPAddr:(char *)buf length:(int)len
{
	struct in_addr ip;
	char *str;
	int i;
	XDR inxdr;

	xdrmem_create(&inxdr, buf, len, XDR_DECODE);
	if (!xdr_int(&inxdr, &i))
	{
		xdr_destroy(&inxdr);
		return NULL;
	}
	xdr_destroy(&inxdr);

	ip.s_addr = i;
	str = malloc(16);
	sprintf(str, "%s", inet_ntoa(ip));

	return str;
}

- (char *)decodeIPNet:(char *)buf length:(int)len
{
	char *str;
	int i;
	XDR inxdr;

	xdrmem_create(&inxdr, buf, len, XDR_DECODE);
	if (!xdr_int(&inxdr, &i))
	{
		xdr_destroy(&inxdr);
		return NULL;
	}
	xdr_destroy(&inxdr);

	str = malloc(16);
	sprintf(str, "%s", nettoa(i));

	return str;
}

- (char *)decodeENAddr:(char *)buf length:(int)len
{
	char *str;
	struct ether_addr en;
	XDR inxdr;

	bzero((char *)&en, sizeof(struct ether_addr));
	xdrmem_create(&inxdr, buf, len, XDR_DECODE);
	if (!xdr_opaque(&inxdr, (caddr_t)&en, sizeof(struct ether_addr)))
	{
		xdr_destroy(&inxdr);
		return NULL;
	}
	xdr_destroy(&inxdr);

	str = malloc(20);
	sprintf(str, "%s", ether_ntoa(&en));

	return str;
}

- (char **)twoStringsFromBuffer:(char *)buf length:(int)len
{
	char *str1, *str2;
	char **l = NULL;
	XDR inxdr;

	if (buf == NULL) return NULL;
	if (len == 0) return NULL;

	xdrmem_create(&inxdr, buf, len, XDR_DECODE);
	str1 = NULL;
	str2 = NULL;
	if (!xdr__lu_string(&inxdr, &str1) ||
	    !xdr__lu_string(&inxdr, &str2))
	{
		xdr_destroy(&inxdr);
		return NULL;
	}
	xdr_destroy(&inxdr);

	l = appendString(str1, l);
	l = appendString(str2, l);

	free(str1);
	free(str2);

	return l;
}

- (char **)threeStringsFromBuffer:(char *)buf length:(int)len
{
	char *str1, *str2, *str3;
	char **l = NULL;
	XDR inxdr;

	if (buf == NULL) return NULL;
	if (len == 0) return NULL;

	xdrmem_create(&inxdr, buf, len, XDR_DECODE);
	str1 = NULL;
	str2 = NULL;
	str3 = NULL;
	if (!xdr__lu_string(&inxdr, &str1) ||
	    !xdr__lu_string(&inxdr, &str2) ||
		!xdr__lu_string(&inxdr, &str3))
	{
		xdr_destroy(&inxdr);
		return NULL;
	}
	xdr_destroy(&inxdr);

	l = appendString(str1, l);
	l = appendString(str2, l);
	l = appendString(str3, l);

	free(str1);
	free(str2);
	free(str3);

	return l;
}

- (int)intFromBuffer:(char *)buf length:(int)len
{
	int i;
	XDR inxdr;

	xdrmem_create(&inxdr, buf, len, XDR_DECODE);
	if (!xdr_int(&inxdr, &i)) i = 0;
	xdr_destroy(&inxdr);

	return i;
}

- (LUDictionary *)dictionaryFromBuffer:(char *)buf length:(int)len
{
	LUDictionary *item;
	char *key, *val, **l;
	int i, j, count, n;
	XDR inxdr;

	if (buf == NULL) return NULL;
	if (len == 0) return NULL;

	xdrmem_create(&inxdr, buf, len, XDR_DECODE);

	if (!xdr_int(&inxdr, &count))
	{
		xdr_destroy(&inxdr);
		return NULL;
	}

	l = NULL;

	item = [[LUDictionary alloc] init];
	for (i = 0; i < count; i++)
	{
		key = NULL;
		if (!xdr_string(&inxdr, &key, MAX_INPUT_STRING))
		{
			break;
		}

		if (!xdr_int(&inxdr, &n))
		{
			break;
		}

		l = NULL;
		for (j = 0; j < n; j++)
		{
			val = NULL;
			if (!xdr_string(&inxdr, &val, MAX_INPUT_STRING))
			{
				break;
			}

			l = appendString(val, l);
			free(val);
		}

		if (j != n)
		{
			break;
		}

		[item setValues:l forKey:key count:n];
		free(key);
		key = NULL;
		freeList(l);
		l = NULL;
	}

	if (key != NULL) free(key);
	if (l != NULL) freeList(l);

	xdr_destroy(&inxdr);
	if (i != count)
	{
fprintf(stderr, "expected %d keys but got %d\n", count, i);
		[item release];
		return NULL;
	}

	return item;	
}

- (char **)intAndStringFromBuffer:(char *)buf length:(int)len
{
	int i;
	char *str;
	char **l = NULL;
	XDR inxdr;
	char num[64];

	if (buf == NULL) return NULL;
	if (len == 0) return NULL;

	xdrmem_create(&inxdr, buf, len, XDR_DECODE);
	str = NULL;
	if (!xdr_int(&inxdr, &i) ||
	    !xdr__lu_string(&inxdr, &str))
	{
		xdr_destroy(&inxdr);
		return NULL;
	}
	xdr_destroy(&inxdr);

	sprintf(num, "%d", i);
	l = appendString(num, l);
	l = appendString(str, l);
	free(str);
	return l;
}

- (char **)inNetgroupArgsFromBuffer:(char *)buf length:(int)len
{
	_lu_innetgr_args args;
	XDR inxdr;
	char **l = NULL;

	bzero(&args, sizeof(args));
	xdrmem_create(&inxdr, buf, len, XDR_DECODE);
	if (!xdr__lu_innetgr_args(&inxdr, &args))
	{
		xdr_destroy(&inxdr);
		return NULL;
	}
	xdr_destroy(&inxdr);

	l = appendString(args.group, l);

	if (args.host != NULL) l = appendString(*args.host, l);
	else l = appendString("", l);
	if (args.user != NULL) l = appendString(*args.user, l);
	else l = appendString("", l);
	if (args.domain != NULL) l = appendString(*args.domain, l);
	else l = appendString("", l);

	xdr_free(xdr__lu_innetgr_args, &args);

	return l;
}

@end
