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
 * dsdata
 *
 * This is the basic data type used for directory service information.
 * The dsdata structure contains a type, a length, and a pointer to a
 * memory area of that length (in bytes).  Big-endian encoding is used.
 *
 * UTF-8 strings (DataTypeUTF8Str) that can be reduced without loss to
 * ASCII strings (DataTypeCStr) can be passed to dsdata_to_cstring().
 * If utf8string_to_dsdata() is passed a UTF-8 string that can be
 * reduced to an ASCII string, then a dsdata of type DataTypeCStr
 * will be returned.
 *
 */
 
#ifndef __DSDATA_H__
#define __DSDATA_H__

#ifndef IndexNull
#define IndexNull (u_int32_t)-1
#endif

#include <stdio.h>
#include <NetInfo/config.h>
#include <NetInfo/dsstatus.h>

#define DataTypeNil           0
#define DataTypeBlob          1
#define DataTypeBool          2
#define DataTypeInt           3
#define DataTypeUInt          4
#define DataTypeFloat         5
#define DataTypeCStr          6
#define DataTypeUTF8Str       7
/* 8-63 reserved for simple types */
#define DataTypeInt8Array    64
#define DataTypeUInt8Array   65
#define DataTypeInt16Array   66
#define DataTypeUInt16Array  67
#define DataTypeInt32Array   68
#define DataTypeUInt32Array  69
#define DataTypeInt64Array   70
#define DataTypeUInt64Array  71
/* 72-251 reserved  */
#define DataTypeDirectoryID	253
#define DataTypeDSAttribute	254
#define DataTypeAny			255
#define DataTypeDSRecord		256

#define IsStringDataType(type) ((type == DataTypeCStr) || (type == DataTypeUTF8Str))

/* Size of type + length */
#define DSDATA_STORAGE_HEADER_SIZE 8

typedef struct
{
	u_int32_t type;
	u_int32_t length;
	char *data;

	u_int32_t retain;
} dsdata;


dsdata *dsdata_alloc(void);
dsdata *dsdata_new(u_int32_t, u_int32_t, char *);
dsdata *dsdata_copy(dsdata *);

dsdata *dsdata_insert(dsdata *a, dsdata *b, u_int32_t where, u_int32_t len);

dsdata *dsdata_retain(dsdata *);
void dsdata_release(dsdata *);

u_int32_t dsdata_size(dsdata *);

dsdata *dsdata_read(char *);
dsdata *dsdata_fread(FILE *);

dsstatus dsdata_write(dsdata *, char *);
dsstatus dsdata_fwrite(dsdata *, FILE *);

int32_t dsdata_equal(dsdata *, dsdata *);
int32_t dsdata_compare(dsdata *, dsdata *);
int32_t dsdata_compare_sub(dsdata *, dsdata *, u_int32_t, u_int32_t);

dsdata *cstring_to_dsdata(char *);
dsdata *utf8string_to_dsdata(char *);
dsdata *int8_to_dsdata(int8_t);
dsdata *uint8_to_dsdata(u_int8_t);
dsdata *int16_to_dsdata(int16_t);
dsdata *uint16_to_dsdata(u_int16_t);
dsdata *int32_to_dsdata(int32_t);
dsdata *uint32_to_dsdata(u_int32_t);
dsdata *int64_to_dsdata(int64_t);
dsdata *uint64_to_dsdata(u_int64_t);
dsdata *dsid_to_dsdata(u_int32_t i);

/*
 * NB - the following two routines do not allocate memory.
 * They simply return a pointer to the memory in the dsdata.
 * This works because dsdata includes a NULL character to
 * terminate strings.  Do not free the returned pointers!
 */
char *dsdata_to_cstring(dsdata *);
char *dsdata_to_utf8string(dsdata *);

int8_t dsdata_to_int8(dsdata *);
u_int8_t dsdata_to_uint8(dsdata *);
int16_t dsdata_to_int16(dsdata *);
u_int16_t dsdata_to_uint16(dsdata *);
int32_t dsdata_to_int32(dsdata *);
u_int32_t dsdata_to_uint32(dsdata *);
int64_t dsdata_to_int64(dsdata *);
u_int64_t dsdata_to_uint64(dsdata *);
u_int32_t dsdata_to_dsid(dsdata *data);

dsdata *int8_array_to_dsdata(int8_t *, u_int32_t);
dsdata *uint8_array_to_dsdata(u_int8_t *, u_int32_t);
dsdata *int16_array_to_dsdata(int16_t *, u_int32_t);
dsdata *uint16_array_to_dsdata(u_int16_t *, u_int32_t);
dsdata *int32_array_to_dsdata(int32_t *, u_int32_t);
dsdata *uint32_array_to_dsdata(u_int32_t *, u_int32_t);
dsdata *int64_array_to_dsdata(int64_t *, u_int32_t);
dsdata *uint64_array_to_dsdata(u_int64_t *, u_int32_t);

int8_t dsdata_int8_at_index(dsdata *, u_int32_t);
u_int8_t dsdata_uint8_at_index(dsdata *, u_int32_t);
int16_t dsdata_int16_at_index(dsdata *, u_int32_t);
u_int16_t dsdata_uint16_at_index(dsdata *, u_int32_t);
int32_t dsdata_int32_at_index(dsdata *, u_int32_t);
u_int32_t dsdata_uint32_at_index(dsdata *, u_int32_t);
int64_t dsdata_int64_at_index(dsdata *, u_int32_t);
u_int64_t dsdata_uint64_at_index(dsdata *, u_int32_t);

void dsdata_print(dsdata *, FILE *);

#endif __DSDATA_H__
