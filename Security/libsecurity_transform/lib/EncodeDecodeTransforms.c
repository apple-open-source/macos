/*
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
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

#include "SecEncodeTransform.h"
#include "SecDecodeTransform.h"
#include "SecCustomTransform.h"
#include "CoreFoundation/CoreFoundation.h"
#include "misc.h"
#include "Utilities.h"
#include <zlib.h>
#include <malloc/malloc.h>

const static CFStringRef DecodeName = CFSTR("com.apple.security.Decoder");
const static CFStringRef EncodeName = CFSTR("com.apple.security.Encoder");
// base32 & base64 are as per RFC 4648
const CFStringRef kSecBase64Encoding = CFSTR("base64");
const CFStringRef kSecBase32Encoding = CFSTR("base32");
// kSecBase32FDEEncoding is SPI (8436055), it avoids I and O, and uses 8 and 9.
// Not good for number form dislexics, but avoids the appearance of a conflict
// between 0 and O or 1 and I (note: 0 and 1 are not used anyway, so there is
// no conflict).
const CFStringRef kSecBase32FDEEncoding = CFSTR("base32FDE");
const CFStringRef kSecZLibEncoding = CFSTR("zlib");
const CFStringRef kSecEncodeTypeAttribute = CFSTR("EncodeType");
const CFStringRef kSecDecodeTypeAttribute = CFSTR("DecodeType");
const CFStringRef kSecEncodeLineLengthAttribute = CFSTR("LineLength");
const CFStringRef kSecCompressionRatio = CFSTR("CompressionRatio");

// There is no way to initialize a const CFNumberRef, so these
// either need to be non-const, or they need to be a CF type
// with a const constructor (CFStringRef).
const CFStringRef kSecLineLength64 = CFSTR("64");
const CFStringRef kSecLineLength76 = CFSTR("76");

static unsigned char Base64Vals[] = 
{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f, 
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 
	0x3c, 0x3d, 0xff, 0xff, 0xff, 0x40, 0xff, 0xff, 
	0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 
	0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 
	0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 
	0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 
	0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static char Base64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789"
"+/=";

static unsigned char Base32Vals[] = {0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02, 0x03,
	0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	
static unsigned char Base32FDEVals[] = {0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x08, 0x12, 
	0xff, 0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02, 0x03, 
	0x04, 0x05, 0x06, 0x07, 0xff, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 
	0x0f, 0x10, 0x11, 0xff, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/* --------------------------------------------------------------------------
	function:		DecodeTransform
	description:	This function returns a block that implements the 
					Decode Transfrom
   -------------------------------------------------------------------------- */
static SecTransformInstanceBlock DecodeTransform(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		SecTransformCustomSetAttribute(ref, kSecDecodeTypeAttribute,
			kSecTransformMetaAttributeRequired, kCFBooleanTrue);
		
		SecTransformSetAttributeAction(ref, 
			kSecTransformActionAttributeNotification,
			kSecDecodeTypeAttribute,
			^(SecTransformStringOrAttributeRef attribute, CFTypeRef value)
			{
				if (NULL == value || CFGetTypeID(value) != CFStringGetTypeID())
				{
					CFErrorRef errorResult = fancy_error(kSecTransformErrorDomain, 
						kSecTransformErrorInvalidInput, 
						CFSTR("Decode type was not a CFStringRef"));
					return (CFTypeRef)errorResult;
				}
				// value is a CFStringRef
				if (kCFCompareEqualTo == CFStringCompare(value, kSecBase64Encoding, 0)) 
				{
					__block struct { unsigned char a[4]; } leftover;
					static const short int in_chunk_size = 4;
					static const short int  out_chunk_size = 3;
					__block int leftover_cnt = 0;

					SecTransformSetDataAction(ref, kSecTransformActionProcessData, 
					^(CFTypeRef value) 
					{
						CFDataRef d = value;
						CFIndex enc_cnt = d ? CFDataGetLength(d) : 0;
						const unsigned char *enc = d ? CFDataGetBytePtr(d) : NULL;
						const unsigned char *enc_end = enc + enc_cnt;
						int n_chunks = (leftover_cnt + enc_cnt) / out_chunk_size + 1;

						unsigned char *out_base = malloc(n_chunks * out_chunk_size);
						if (!out_base) {
							return (CFTypeRef) GetNoMemoryError();
						}
						unsigned char *out_end = out_base + n_chunks * out_chunk_size;
						unsigned char *out = out_base;
						int chunk_i = leftover_cnt;

						for(; enc < enc_end || !enc; chunk_i++) {
							unsigned char ch, b;
							if (enc) {
								ch = *enc++;
							} else {
								ch = '=';
							}
							if (ch == ' ' || ch == '\n' || ch == '\r') {
								chunk_i -= 1;
								continue;
							}

							b = Base64Vals[ch];
							if (b != 0xff) {
								leftover.a[chunk_i] = b;
							}

							if (chunk_i == in_chunk_size-1 || ch == '=') {
								*out = (leftover.a[0] & 0x3f) << 2;
								*out++ |= ((leftover.a[1] & 0x3f) >> 4);
								*out = (leftover.a[1] & 0x0f) << 4;
								*out++ |= (leftover.a[2] & 0x3f) >> 2;
								*out = (leftover.a[2] & 0x03) << 6;
								*out++ |= (leftover.a[3] & 0x3f);

								out -= 3 - chunk_i;
								if (ch == '=') {
									if (chunk_i != 0) {
										out--;
									}
									chunk_i = -1;
									break;
								}
								chunk_i = -1;
							}
						}
						leftover_cnt = (chunk_i > 0) ? chunk_i : 0;
						if (out > out_end) {
							// We really shouldn't get here, but if we do we just smashed something.
							abort();
						}

						CFDataRef ret = CFDataCreateWithBytesNoCopy(NULL, out_base, out - out_base, kCFAllocatorMalloc);
						if (!d) {
							SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, 
								kSecTransformMetaAttributeValue, ret);
                            CFRelease(ret);
							ret = NULL;
						}
						return (CFTypeRef)ret;
					});
				} 
				else if (kCFCompareEqualTo == CFStringCompare(value, kSecBase32Encoding, 0) || kCFCompareEqualTo == CFStringCompare(value, kSecBase32FDEEncoding, 0)) 
				{
					__block struct { uint64_t a[2]; } accumulator = { .a = {0, 0}};
					__block short int bits_accumulated = 0;
					//static const short int in_chunk_size = 5, out_chunk_size = 8;
					static const short int out_chunk_size = 8;
					const short int full_accumulator = 80;
					unsigned char *base32values = NULL;
					
					if (kCFCompareEqualTo == CFStringCompare(value, kSecBase32Encoding, 0)) {
						base32values = Base32Vals;
					} else if (kCFCompareEqualTo == CFStringCompare(value, kSecBase32FDEEncoding, 0)) {
						base32values = Base32FDEVals;
					}
					
					if (NULL == base32values) {
						// There is only one supported type, so we don't want to mention it in an error message
						CFErrorRef bad_type = CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, "Unknown base32 type '%@'", value);
						
						SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, bad_type);
						
						return (CFTypeRef)bad_type;
					}
					
					SecTransformSetDataAction(ref, kSecTransformActionProcessData, 
						^(CFTypeRef value) 
						{
							CFDataRef d = value;
							CFIndex enc_cnt = d ? CFDataGetLength(d) : 0;
							const unsigned char *enc = d ? CFDataGetBytePtr(d) : NULL;
							const unsigned char *enc_end = enc + enc_cnt;
							int n_chunks = (bits_accumulated/8 + enc_cnt) / out_chunk_size + 1;

							unsigned char *out_base = malloc(n_chunks * out_chunk_size);
							if (!out_base) {
								return (CFTypeRef)GetNoMemoryError();
							}
							unsigned char *out_end = out_base + n_chunks * out_chunk_size;
							unsigned char *out = out_base;

							for(; enc < enc_end || !d;) {
								unsigned char ch, b;
								if (enc) {
									ch = *enc++;
								} else {
									ch = '=';
								}

								b = base32values[ch];
								if (b == 0xff) {
									continue;
								}

								if (ch != '=') {
									// 5 new low order bits
									accumulator.a[1] = accumulator.a[1] << 5 | (0x1f & (accumulator.a[0] >> (64 -5)));
									accumulator.a[0] = accumulator.a[0] << 5 | b;
									bits_accumulated += 5;
								}
								if (bits_accumulated == full_accumulator || ch == '=') {
									short shifted = 0;
									for(; shifted + bits_accumulated < full_accumulator; shifted += 5) {
										accumulator.a[1] = accumulator.a[1] << 5 | (0x1f & accumulator.a[0] >> (64 -5));
										accumulator.a[0] = accumulator.a[0] << 5;
									}
									for(; bits_accumulated >= 8; bits_accumulated -= 8) {
										// Get 8 high bits
										*out++ = accumulator.a[1] >> (80 - 64 - 8);
										accumulator.a[1] = (accumulator.a[1] << 8 | accumulator.a[0] >> (64 - 8)) & 0xffff;
										accumulator.a[0] = accumulator.a[0] << 8;
									}
									bits_accumulated = 0;
									if (ch == '=') {
										break;
									}
								}
							}
							if (out > out_end) {
								// We really shouldn't get here, but if we do we just smashed something.
								abort();
							}

							CFDataRef ret = CFDataCreateWithBytesNoCopy(NULL, out_base, out - out_base, kCFAllocatorMalloc);
							if (!d) {
								SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, 
									kSecTransformMetaAttributeValue, ret);
                                CFRelease(ret);
								ret = NULL;
							}
							return (CFTypeRef)ret;
						});
				} 
				else if (kCFCompareEqualTo == CFStringCompare(value, kSecZLibEncoding, 0)) 
				{
					__block z_stream zs;
					__block Boolean started = FALSE; 
					
					CFBooleanRef hasRatio = (CFBooleanRef)SecTranformCustomGetAttribute(ref, 
												kSecCompressionRatio, kSecTransformMetaAttributeHasOutboundConnections);
					Boolean ratio_connected = (kCFBooleanTrue == hasRatio);
								
					bzero(&zs, sizeof(zs));
					
					SecTransformSetDataAction(ref, kSecTransformActionProcessData, 
						^(CFTypeRef value) 
						{
							CFDataRef d = value;
							if (!started) {
								if (!d) {
									return (CFTypeRef)NULL;
								}
								started = TRUE;
								inflateInit(&zs);
							}

							if (d) {
								zs.next_in = (UInt8 *)(CFDataGetBytePtr(d)); // we know that zlib will not 'futz' with the data
								zs.avail_in = CFDataGetLength(d);
							} else {
								zs.next_in = NULL;
								zs.avail_in = 0;
							}

							int rc = Z_OK;

							CFIndex buf_sz = malloc_good_size(zs.avail_in ? zs.avail_in : 1024 * 4);

							while ((d && zs.avail_in) || (d == NULL && rc != Z_STREAM_END)) {
								unsigned char *buf = malloc(buf_sz);
								if (!buf) {
									return (CFTypeRef)GetNoMemoryError();
								}

								zs.next_out = buf;
								zs.avail_out = buf_sz;

								rc = inflate(&zs, d ? Z_NO_FLUSH : Z_FINISH);

								CFIndex buf_used = buf_sz - zs.avail_out;
#ifdef DEBUG_ZLIB_MEMORY_USE
								// It might be useful to look at these and tweak things like when we should use DataCreate vs. DataCreateWithBytesNoCopy
								CFfprintf(stderr, ">>zavail_in %d buf_sz %d; d %p; ", zs.avail_in, buf_sz, d);
								CFfprintf(stderr, "rc=%d %s", rc, (rc == Z_OK) ? "Z_OK" : (rc == Z_STREAM_END) ? "Z_STREAM_END" : (rc == Z_BUF_ERROR) ? "Z_BUF_ERROR" : "?");
								CFfprintf(stderr, " (output used %d, input left %d)\n", buf_used, zs.avail_in);
#endif
								if (rc == Z_OK || rc == Z_STREAM_END) {
									CFDataRef d;
									if ((4 * buf_used) / buf_sz <= 1) {
										// we would waste 25%+ of the buffer, make a smaller copy and release the original
										d = CFDataCreate(NULL, buf, buf_used);
										free(buf);
									} else {
										d = CFDataCreateWithBytesNoCopy(NULL, buf, buf_used, kCFAllocatorMalloc);
									}
									SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, 
											kSecTransformMetaAttributeValue, d);
									CFRelease(d);
								} else if (rc == Z_BUF_ERROR) {
									free(buf);
									if ((int)buf_sz > (1 << Z_BEST_COMPRESSION) && 0 == zs.avail_in) {
										// zlib has an odd convention about EOF and Z_BUF_ERROR, see http://www.zlib.net/zlib_how.html
										// Z_BUF_ERROR can mean "you don't have a big enough output buffer, please enlarge", or "the input buffer is
										// empty, please get more data".    So if  we get Z_BUF_ERROR, and there are 0 bytes of input, and the output
										// buffer is larger the the maximum number of bytes a single symbol can decode to (2^compression level, which
										// is at most Z_BEST_COMPRESSION) we KNOW the complaint isn't about the output buffer, but the input
										// buffer and we are free to go.    NOTE: we will only hit this if we are at the end of the stream, and the prior
										// data chunk was already entirely decoded.
										rc = Z_STREAM_END;
									}									
									buf_sz = malloc_good_size(buf_sz * 2);
								} else {
									free(buf);
									CFStringRef emsg = CFStringCreateWithFormat(NULL, NULL, CFSTR("Zlib error#%d"), rc);
									CFErrorRef err = fancy_error(kSecTransformErrorDomain, kSecTransformErrorInvalidInput, emsg);
									CFRelease(emsg);
									return (CFTypeRef)err;
								}
							}

							if (ratio_connected && zs.total_in && zs.total_out) {
								float r = (float)zs.total_in / zs.total_out;
								CFNumberRef ratio = CFNumberCreate(NULL, kCFNumberFloatType, &r);
								SecTransformCustomSetAttribute(ref, kSecCompressionRatio, 
									kSecTransformMetaAttributeValue, ratio);
								CFRelease(ratio);
							}

							if (rc == Z_OK) {
								return (CFTypeRef)SecTransformNoData();
							} else if (rc == Z_STREAM_END) {
								inflateEnd(&zs);
								started = FALSE;
								return (CFTypeRef)NULL;
							}
							CFStringRef emsg = CFStringCreateWithFormat(NULL, NULL, CFSTR("Zlib error#%d"), rc);
							CFErrorRef err = fancy_error(kSecTransformErrorDomain, kSecTransformErrorInvalidInput, emsg);
							CFRelease(emsg);
							return (CFTypeRef)err;
						});					
				}
				else
				{
					CFErrorRef bad_type = CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, "Unsupported decode type '%@', supported types are kSecBase64Encoding, kSecBase32Encoding, and kSecGZipEncoding", value);
					
					SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, bad_type);
					
					return (CFTypeRef)bad_type;
				}
				return value;
			});
		
		return result;
	};
	
	return Block_copy(instanceBlock);
}
	

SecTransformRef SecDecodeTransformCreate(CFTypeRef DecodeType, CFErrorRef* error) {
	
	static dispatch_once_t once;
	__block Boolean ok = TRUE;
	CFErrorRef localError = NULL;
				
	dispatch_block_t aBlock = ^
	{
		ok = SecTransformRegister(DecodeName, &DecodeTransform, (CFErrorRef*)&localError);
	};
	
	dispatch_once(&once, aBlock);

	if (!ok || NULL != localError) 
	{
		if (NULL != error)
		{
			*error = localError;
		}
		return NULL;
	}
		
	SecTransformRef tr = SecTransformCreate(DecodeName, &localError);
	if (!tr || NULL != localError) 
	{
		// There might be a leak if tr is returned but localError is 
		// not NULL, but that should not happen
		if (NULL != error)
		{
			*error = localError;
		}
		return NULL;
	}
	
	SecTransformSetAttribute(tr, kSecDecodeTypeAttribute, DecodeType, &localError);
	if (NULL != localError)
	{
		CFRelease(tr);
		tr = NULL;
		if (NULL != error)
		{
			*error = localError;
		}
	}
	
	return tr;
}

unsigned char *encode_base64(const unsigned char *bin, unsigned char *base64, int bin_cnt) {	
	for(; bin_cnt > 0; bin_cnt -= 3, base64 += 4, bin += 3) {
		switch (bin_cnt)
		{
			default:
			case 3:
				base64[0] = Base64Chars[((bin[0] >> 2) & 0x3f)];
				base64[1] = Base64Chars[((bin[0] & 0x03) << 4) | 
										((bin[1] >> 4) & 0x0f)];
				base64[2] = Base64Chars[((bin[1] & 0x0f) << 2) | 
										((bin[2] >> 6) & 0x03)];
				base64[3] = Base64Chars[(bin[2] & 0x3f)];
				break;
				
			case 2:
				base64[3] = '=';
				base64[0] = Base64Chars[((bin[0] >> 2) & 0x3f)];
				base64[1] = Base64Chars[((bin[0] & 0x03) << 4) | 
										((bin[1] >> 4) & 0x0f)];
				base64[2] = Base64Chars[((bin[1] & 0x0f) << 2)];
				break;
				
			case 1:
				base64[3] = base64[2] = '=';
				base64[0] = Base64Chars[((bin[0] >> 2) & 0x3f)];
				base64[1] = Base64Chars[((bin[0] & 0x03) << 4)];
				break;

			case 0:
				base64[0] = base64[1] = base64[2] = base64[3] = '=';
				break;
		}
	}
	
	return base64;
}


/* --------------------------------------------------------------------------
	function:		DecodeTransform
	description:	This function returns a block that implements the 
					Decode Transfrom
   -------------------------------------------------------------------------- */
static SecTransformInstanceBlock EncodeTransform(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
							
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		SecTransformCustomSetAttribute(ref, kSecEncodeTypeAttribute, 
			kSecTransformMetaAttributeRequired, kCFBooleanTrue);

		__block int line_length = 0, target_line_length = 0;

		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, 
			kSecEncodeLineLengthAttribute, 
			^(SecTransformStringOrAttributeRef attribute, CFTypeRef value) 
			{
				SecTransformPushbackAttribute(ref, attribute, value);
				return value;
			});		

		CFTypeRef (^new_line_length)(int out_chunk_size, CFTypeRef value) = ^(int out_chunk_size, CFTypeRef value) 
		{
			if (CFGetTypeID(value) == CFNumberGetTypeID()) {
				CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &target_line_length);
			} else if (CFGetTypeID(value) == CFStringGetTypeID()) {
				int requested_length = CFStringGetIntValue(value);
				if (requested_length == 0 && CFStringCompare(CFSTR("0"), value, kCFCompareAnchored)) {
					SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, "Could not convert '%@' to a number, please set %@ to a numeric value", kSecEncodeLineLengthAttribute, value));
				} else {
					target_line_length = requested_length;
				}
			} else {
				CFStringRef valueType = CFCopyTypeIDDescription(CFGetTypeID(value));
				SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, CreateSecTransformErrorRef(kSecTransformErrorInvalidType, "%@ requires a CFNumber, but was set to a %@ (%@)", kSecEncodeLineLengthAttribute, valueType, value));
				CFRelease(valueType);
			}
			target_line_length -= target_line_length % out_chunk_size;

			if (target_line_length < 0) {
				target_line_length = 0;
			}

			return value;
		};

		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, 
			kSecEncodeTypeAttribute, 
			^(SecTransformStringOrAttributeRef attribute, CFTypeRef value) 
			{
				if (NULL == value || CFGetTypeID(value) != CFStringGetTypeID())
				{
					CFErrorRef errorResult = fancy_error(kSecTransformErrorDomain, 
						kSecTransformErrorInvalidInput, 
						CFSTR("Encode type was not a CFStringRef"));
					return (CFTypeRef)errorResult;
				}
				
				if (kCFCompareEqualTo == CFStringCompare(value, kSecBase64Encoding, 0)) 
				{
					__block struct { unsigned char a[3]; } leftover;
					static const short int in_chunk_size = 3, out_chunk_size = 4;
					__block int leftover_cnt = 0;

					SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, 
						kSecEncodeLineLengthAttribute, 
						^(SecTransformStringOrAttributeRef attribute, CFTypeRef value) 
						{
							return new_line_length(out_chunk_size, value);
						});

					SecTransformSetDataAction(ref, kSecTransformActionProcessData, 
						^(CFTypeRef value) 
						{
							CFDataRef d = value;
							CFIndex in_len = d ? CFDataGetLength(d) : 0;
							const unsigned char *in = d ? CFDataGetBytePtr(d) : NULL;
							int n_chunks = in_len / in_chunk_size + 3;
							int buf_len = n_chunks * out_chunk_size;
							if (target_line_length) 
							{
								buf_len += (n_chunks * out_chunk_size) / target_line_length;
							}
							unsigned char *out = malloc(buf_len);
							unsigned char *out_end = out + buf_len, *out_base = out;
							if (!out) 
							{
								return (CFTypeRef)GetNoMemoryError();
							}
							if (leftover_cnt) 
							{
								CFIndex copy_len = in_chunk_size - leftover_cnt;
								copy_len = (copy_len > in_len) ? in_len : copy_len;
								memcpy(leftover.a + leftover_cnt, in, copy_len);

								if (copy_len + leftover_cnt == in_chunk_size || d == NULL) 
								{
									out = encode_base64(leftover.a, out, copy_len + leftover_cnt);
									if (in) 
									{
										in += copy_len;
										in_len -= copy_len;
									}
								} 
								else 
								{
									free(out);
									leftover_cnt += copy_len;
									return (CFTypeRef)SecTransformNoData();
								}
							}

							CFIndex chunked_in_len;
							while (in_len >= in_chunk_size) 
							{
								chunked_in_len = in_len - (in_len % in_chunk_size);
								if (target_line_length) 
								{
									if (target_line_length <= line_length + out_chunk_size) 
									{
										*out++ = '\n';
										line_length = 0;
									}
									int max_process = (((target_line_length - line_length) / out_chunk_size) * in_chunk_size);
									chunked_in_len = (chunked_in_len < max_process) ? chunked_in_len : max_process;
								}
								unsigned char *old_out = out;
								out = encode_base64(in, out, chunked_in_len);
								line_length += out - old_out;
								in += chunked_in_len;
								in_len -= chunked_in_len;
							}
							leftover_cnt = in_len;
							if (leftover_cnt) 
							{
								memcpy(leftover.a, in, leftover_cnt);
							}

							if (out > out_end) 
							{
								// we should never hit this, but if we do there is no recovery: we smashed past a buffer into the heap
								abort();
							}

							CFTypeRef ret = CFDataCreateWithBytesNoCopy(NULL, out_base, out - out_base, kCFAllocatorMalloc);
							if (!d) 
							{
								SecTransformCustomSetAttribute(ref,kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, ret);
                              CFRelease(ret);
								ret = NULL;
							}
							return ret;
						});
				} 
				else if (kCFCompareEqualTo == CFStringCompare(value, kSecBase32Encoding, 0) || kCFCompareEqualTo == CFStringCompare(value, kSecBase32FDEEncoding, 0)) 
				{
					__block struct { uint64_t a[2]; } accumulator = { .a = {0, 0} };
					__block short int bits_accumulated = 0;
					static const short int in_chunk_size = 5;
					static const short int out_chunk_size = 8;
					char *base32alphabet = NULL;
					
					if (kCFCompareEqualTo == CFStringCompare(value, kSecBase32Encoding, 0)) {
						base32alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
					} else if (kCFCompareEqualTo == CFStringCompare(value, kSecBase32FDEEncoding, 0)) {
						base32alphabet = "ABCDEFGH8JKLMNOPQR9TUVWXYZ234567";
					}
					
					if (NULL == base32alphabet) {
						// There is only one supported type, so we don't want to mention it in an error message
						CFErrorRef bad_type = CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, "Unknown base32 type '%@'", value);
						
						SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, bad_type);
						
						return (CFTypeRef)bad_type;
					}
					
					SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, 
						kSecEncodeLineLengthAttribute, 
						^(SecTransformStringOrAttributeRef attribute, CFTypeRef value) 
						{
							return new_line_length(out_chunk_size, value);
						});					

					SecTransformSetDataAction(ref, kSecTransformActionProcessData, 
						^(CFTypeRef value) 
						{
							CFDataRef d = value;
							CFIndex in_len = d ? CFDataGetLength(d) : 0;
							const unsigned char *in = d ? CFDataGetBytePtr(d) : NULL;
							const unsigned char *in_end = in + in_len;
							int n_chunks = in_len / in_chunk_size + 3;
							int buf_len = n_chunks * out_chunk_size;
							if (target_line_length) 
							{
								buf_len += (n_chunks * out_chunk_size) / target_line_length;
							}
							__block unsigned char *out = malloc(buf_len);
							unsigned char *out_end = out + buf_len, *out_base = out;
							if (!out) {
								return (CFTypeRef)GetNoMemoryError();
							}

							void (^chunk)(void) = ^{
								// Grab the 5 bit (log(32)==5) values from the 80 bit accumulator.   Most signifigant bits first

								// (this could be done without the loop, which would save few cycles at the end of a stream)
								short int shift = 80 - bits_accumulated;
								for(; shift > 0; shift -= 8) {
									accumulator.a[1] = accumulator.a[1] << 8 | accumulator.a[0] >> (64 - 8);
									accumulator.a[0] = accumulator.a[0] << 8;
								}

								for(; bits_accumulated > 0; bits_accumulated -= 5) {
									*out++ = base32alphabet[(accumulator.a[1] >> 11) & 0x1f];
									accumulator.a[1] = 0xffff & (accumulator.a[1] << 5 | (accumulator.a[0] >> (64 - 5)));
									accumulator.a[0] = accumulator.a[0] << 5;
									if (++line_length >= target_line_length && target_line_length) {
										*out++ = '\n';
										line_length = 0;
									}
								}
								bits_accumulated = 0;
							};

							for (; in < in_end; in++) 
							{
								accumulator.a[1] = accumulator.a[1] << 8 | accumulator.a[0] >> (64 - 8);
								accumulator.a[0] = accumulator.a[0] << 8 | *in;
								bits_accumulated += 8;
								if (bits_accumulated == 8*in_chunk_size) 
								{
									chunk();
								}
							}

							if (!d && bits_accumulated) {
								short int padding = 0;
								switch(bits_accumulated) {
									case 8:
										padding = 6;
										break;
									case 16:
										padding = 4;
										break;
									case 24:
										padding = 3;
										break;
									case 32:
										padding = 1;
										break;
								}
								chunk();
								int i;
								for(i = 0; i < padding; i++) {
									*out++ = '=';
								}
							}

							if (out > out_end) {
								// we should never hit this, but if we do there is no recovery: we smashed past a buffer into the heap
								abort();
							}

							CFTypeRef ret = NULL;
							if (out - out_base) {
								ret = CFDataCreateWithBytesNoCopy(NULL, out_base, out - out_base, kCFAllocatorMalloc);
							} else {
								ret = SecTransformNoData();
							}
							if (!d) {
								if (ret != SecTransformNoData()) {
									SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, 
										kSecTransformMetaAttributeValue, ret);
                                    CFRelease(ret);
								}
								ret = NULL;
							}
							return ret;						
						});
				} 
				else if (kCFCompareEqualTo == CFStringCompare(value, kSecZLibEncoding, 0)) 
				{
					__block z_stream zs;
					bzero(&zs, sizeof(zs));
					__block int clevel = Z_DEFAULT_COMPRESSION;
					__block Boolean started = FALSE;
					
					CFBooleanRef hasRatio = (CFBooleanRef)SecTranformCustomGetAttribute(ref, kSecCompressionRatio, 
						kSecTransformMetaAttributeHasOutboundConnections);
						
					Boolean ratio_connected = (kCFBooleanTrue == hasRatio);

					SecTransformSetDataAction(ref, kSecTransformActionProcessData,
						^(CFTypeRef value) 
						{
							CFDataRef d = value;

							if (!started) {
								started = TRUE;
								deflateInit(&zs, clevel);
							}

							if (d) {
								zs.next_in = (UInt8 *)CFDataGetBytePtr(d); // We know that xLib will not 'Futz' with the data
								zs.avail_in = CFDataGetLength(d);
							} else {
								zs.next_in = NULL;
								zs.avail_in = 0;
							}

							int rc = Z_BUF_ERROR;

							CFIndex buf_sz = malloc_good_size(zs.avail_in ? zs.avail_in : 1024 * 4);

							while ((d && zs.avail_in) || (d == NULL && rc != Z_STREAM_END)) {
								unsigned char *buf = malloc(buf_sz);
								if (!buf) {
									return (CFTypeRef)GetNoMemoryError();
								}

								zs.next_out = buf;
								zs.avail_out = buf_sz;

								rc = deflate(&zs, d ? Z_NO_FLUSH : Z_FINISH);

								CFIndex buf_used = buf_sz - zs.avail_out;
		#ifdef DEBUG_ZLIB_MEMORY_USE
								// It might be useful to look at these and tweak things like when we should use DataCreate vs. DataCreateWithBytesNoCopy
								CFfprintf(stderr, "<<zavail_in %d buf_sz %d; d %p; ", zs.avail_in, buf_sz, d);
								CFfprintf(stderr, "rc=%d %s", rc, (rc == Z_OK) ? "Z_OK" : (rc == Z_STREAM_END) ? "Z_FINISH" : (rc == Z_BUF_ERROR) ? "Z_BUF_ERROR" : "?");
								CFfprintf(stderr, " (output used %d, input left %d)\n", buf_used, zs.avail_in);
		#endif
								if (rc == Z_OK || rc == Z_STREAM_END) {
									CFDataRef d;
									if ((4 * buf_used) / buf_sz <= 1) {
										// we would waste 25%+ of the buffer, make a smaller copy and release the original
										d = CFDataCreate(NULL, buf, buf_used);
										free(buf);
									} else {
										d = CFDataCreateWithBytesNoCopy(NULL, buf, buf_used, kCFAllocatorMalloc);
									}
									SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, 
										kSecTransformMetaAttributeValue, d);
									CFRelease(d);
								} else if (rc == Z_BUF_ERROR) {
									free(buf);
									buf_sz = malloc_good_size(buf_sz * 2);
								} else {
									free(buf);
									CFStringRef emsg = CFStringCreateWithFormat(NULL, NULL, CFSTR("Zlib error#%d"), rc);
									CFErrorRef err = fancy_error(kSecTransformErrorDomain, kSecTransformErrorInvalidInput, emsg);
									CFRelease(emsg);
									return (CFTypeRef)err;
								}
							}
							if (ratio_connected && zs.total_in && zs.total_out) {
								float r = (float)zs.total_out / zs.total_in;
								CFNumberRef ratio = CFNumberCreate(NULL, kCFNumberFloatType, &r);
								SecTransformCustomSetAttribute(ref, kSecCompressionRatio, 
									kSecTransformMetaAttributeValue, ratio);
								CFRelease(ratio);
							}
							if (d) {
								return (CFTypeRef)SecTransformNoData();
							} else {
								deflateEnd(&zs);
								started = FALSE;
								return (CFTypeRef)NULL;
							}
						});

					SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, 
						kSecEncodeLineLengthAttribute, ^(SecTransformStringOrAttributeRef attribute, CFTypeRef value) 
					{
						return value;
					});		
				} 
				else 
				{
					CFErrorRef bad_type = CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, "Unsupported encode type '%@', supported types are kSecBase64Encoding, kSecBase32Encoding, and kSecGZipEncoding", value);
					
					SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, bad_type);
					
					return (CFTypeRef)bad_type;
				}

				return (CFTypeRef)value;
			});

		return result;
	};
	
	return Block_copy(instanceBlock);	
}

SecTransformRef SecEncodeTransformCreate(CFTypeRef EncodeType, CFErrorRef* error) 
{
	
	static dispatch_once_t once;
	__block Boolean ok = TRUE;
	CFErrorRef localError = NULL;
				
	dispatch_block_t aBlock = ^
	{
		ok = SecTransformRegister(EncodeName, &EncodeTransform, (CFErrorRef*)&localError);
	};
	
	dispatch_once(&once, aBlock);

	if (!ok || NULL != localError) 
	{
		if (NULL != error)
		{
			*error = localError;
		}
		
		return NULL;
	}
	
	SecTransformRef tr = SecTransformCreate(EncodeName, &localError);
	if (!tr || NULL != localError) 
	{
		// There might be a leak if tr is returned but localError is 
		// not NULL, but that should not happen
		if (NULL != error)
		{
			*error = localError;
		}
		return NULL;
	}
	
	SecTransformSetAttribute(tr, kSecEncodeTypeAttribute, EncodeType, &localError);
	if (NULL != localError)
	{
		CFRelease(tr);
		tr = NULL;
		if (NULL != error)
		{
			*error = localError;
		}
	}
		
	return tr;
}
