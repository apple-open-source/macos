//
//  DataConversion.m
//  FIPSCAVS
//
//  Created by James Murphy on 04/26/13.
//  Copyright 2013 Apple. All rights reserved.
//
// 	This is confidential and proprietary code of Apple Inc.  It may not be 
//	used, copied or modified in any way without Apple's expressed 
//	written permission in each case. No redistribution of this 
//	Apple Confidential code is allowed.
//

#import "DataConversion.h"
#import <dispatch/dispatch.h>

/* ==========================================================================
	Static data used to convert between binary and HEX data
   ========================================================================== */

static char byteMap[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
static int byteMapLen = sizeof(byteMap);

/* ==========================================================================
	Static functions to lookup the appropriate hex value from binary data ot
	binary data from a hex value
   ========================================================================== */

static uint8_t nibbleFromChar(char c)
{
    if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	return 255;
}

static char nibbleToChar(uint8_t nibble)
{
	if(nibble < byteMapLen) return byteMap[nibble];
	return '*';
}

/* ==========================================================================
	Extend the NSData class to convert a binary value into a hex string
   ========================================================================== */
@implementation NSData (DataConversion)

- (NSString *)toHexString
{
	NSString* result = nil;
    
    unsigned char* data_bytes = (unsigned char*)[self bytes];
    int data_len = (int)[self length];
    int len = ((data_len * 2) + 1);
    
    unsigned char buffer[len];
    int iCnt;
    for (iCnt = 0; iCnt < data_len; iCnt++)
    {
        buffer[iCnt * 2] = nibbleToChar(data_bytes[iCnt] >> 4);
        buffer[iCnt * 2 + 1] = nibbleToChar(data_bytes[iCnt] & 0x0F);
    }

    buffer[data_len * 2] = 0;
    result = [NSString stringWithUTF8String:(const char *)buffer];
    return result;
}

@end

/* ==========================================================================
	Extend the NSString class to convert a hex string into a binary value
   ========================================================================== */

@implementation NSString (DataConversion)

- (NSData *)hexStringToData
{
	NSData* result = nil;
    
    const char* utf8_str = [self UTF8String];
    int len = (int)(strlen(utf8_str) / 2);
    uint8_t buffer[len];
    uint8_t* p = NULL;
    
    memset(buffer, 0, len);
    int iCnt;
    for (iCnt = 0, (p = (uint8_t*)utf8_str); iCnt < len; iCnt++)
    {
        buffer[iCnt] = (nibbleFromChar(*p) << 4) | nibbleFromChar(*(p+1));
        p += 2;
    }
    buffer[len] = 0;
    result = [NSData dataWithBytes:buffer length:len];
    return result;
}

@end





