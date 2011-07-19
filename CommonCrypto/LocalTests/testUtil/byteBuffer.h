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
    uint8_t  *bytes;
} byteBufferStruct, *byteBuffer;

void printByteBuffer(byteBuffer bb, char *name);

void printBytes(uint8_t *buff, size_t len, char *name);

byteBuffer
mallocByteBuffer(size_t len);

byteBuffer
hexStringToBytes(char *inhex);

byteBuffer
bytesToBytes(void *bytes, size_t len);

int
bytesAreEqual(byteBuffer b1, byteBuffer b2);

char
*bytesToHexString(byteBuffer bytes);

#endif _BYTEBUFFER_H_
