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
 * XDRSerializer.h
 *
 * XDR serializer for property lists
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "Root.h"
#import <sys/types.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <net/if.h>
#import <net/if_arp.h>
#import <net/etherdefs.h>
#ifndef bool_t
#define bool_t  int
#endif
#import <rpc/types.h>
#import <rpc/xdr.h>
#import "LUDictionary.h"
#import "LUArray.h"

@interface XDRSerializer : Root
{
}

- (void)encodeString:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
	maxLength:(unsigned int)maxLen;

- (void)encodeString:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs;

- (void)encodeInt:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs;

- (void)encodeStrings:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
	maxCount:(unsigned int)maxCount
	maxLength:(unsigned int)maxLen;

- (void)encodeStrings:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
	max:(unsigned int)maxCount;

- (void)encodeIPAddrs:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs
	max:(int)maxCount;

- (void)encodeIPAddr:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs;

- (void)encodeNetAddr:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs;

- (void)encodeENAddr:(char *)key
	from:(LUDictionary *)item
	intoXdr:(XDR *)xdrs;

- (void)encodeString:(char *)aString
	intoXdr:(XDR *)xdrs
	maxLength:(unsigned int)maxLen;

- (void)encodeString:(char *)aString
	intoXdr:(XDR *)xdrs;

- (void)encodeInt:(int)i intoXdr:(XDR *)xdrs;
- (void)encodeBool:(BOOL)i intoXdr:(XDR *)xdrs;
- (void)encodeUnsignedLong:(unsigned long)i intoXdr:(XDR *)xdrs;

- (char *)decodeString:(char *)buf length:(int)len;
- (char *)decodeInt:(char *)buf length:(int)len;
- (char *)decodeIPAddr:(char *)buf length:(int)len;
- (char *)decodeIPNet:(char *)buf length:(int)len;
- (char *)decodeENAddr:(char *)buf length:(int)len;

- (int)intFromBuffer:(char *)buf length:(int)len;
- (char **)twoStringsFromBuffer:(char *)buf length:(int)len;
- (char **)threeStringsFromBuffer:(char *)buf length:(int)len;
- (char **)intAndStringFromBuffer:(char *)buf length:(int)len;
- (char **)inNetgroupArgsFromBuffer:(char *)buf length:(int)len;

- (LUDictionary *)dictionaryFromBuffer:(char *)buf length:(int)len;

@end
