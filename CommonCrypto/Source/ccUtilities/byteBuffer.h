/*                                                                              
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
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
 *  printByteBuffer.h
 *  byteutils
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef _BYTEBUFFER_H_
#define _BYTEBUFFER_H_

typedef struct byte_buf {
    size_t  len;
    size_t  size;
    uint8_t  *bytes;
} byteBufferStruct, *byteBuffer;

void printByteBuffer(byteBuffer bb, char *name);

void printBytes(uint8_t *buff, size_t len, char *name);

byteBuffer
mallocByteBuffer(size_t len);

void
freeByteBuffer(byteBuffer b);

byteBuffer
hexStringToBytes(char *inhex);

byteBuffer
bytesToBytes(void *bytes, size_t len);

int
bytesAreEqual(byteBuffer b1, byteBuffer b2);

char
*bytesToHexString(byteBuffer bytes);

#endif /* _BYTEBUFFER_H_ */
