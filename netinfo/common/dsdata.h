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
#define DataTypeCaseCStr      8
#define DataTypeCaseUTF8Str   9
/* 8-63 reserved for simple types */
#define DataTypeInt8Array    64
#define DataTypeUInt8Array   65
#define DataTypeInt16Array   66
#define DataTypeUInt16Array  67
#define DataTypeInt32Array   68
#define DataTypeUInt32Array  69
#define DataTypeInt64Array   70
#define DataTypeUInt64Array  71
/* 72-250 reserved  */
#define DataTypeDSReference 251
#define DataTypeCPtr        252
#define DataTypeDirectoryID	253
#define DataTypeDSAttribute	254
#define DataTypeAny			255
#define DataTypeDSRecord		256

#define IsStringDataType(type) (((type) == DataTypeCStr) || \
	((type) == DataTypeCaseCStr) || \
	((type) == DataTypeUTF8Str) || \
	((type) == DataTypeCaseUTF8Str))

#define IsCaseStringDataType(type) (((type) == DataTypeCaseCStr) || \
	((type) == DataTypeCaseUTF8Str))

#define IsUTF8DataType(type) (((type) == DataTypeUTF8Str) || \
	((type) == DataTypeCaseUTF8Str))

#define StringDataTypes(t1, t2) (IsStringDataType(t1) && IsStringDataType(t2))

/* can t2 be compared against t1? */
#define ComparableDataTypes(t1, t2) (((t2) == DataTypeAny) ? 1 : \
	(StringDataTypes(t1, t2) || ((t1) == (t2))))

/* Size of type + length */
#define DSDATA_STORAGE_HEADER_SIZE 8

typedef struct
{
	u_int32_t type;
	u_int32_t length;
	char *data;

	u_int32_t retain;
} dsdata;


dsdata *dsdata_alloc(u_int32_t size);
dsdata *dsdata_new(u_int32_t type, u_int32_t len, char *buf);
dsdata *dsdata_copy(dsdata *d);

/*
 * Inserts data from b into a.
 * Returns a NEW pointer for a - don't use the old one!
 */
dsdata *dsdata_insert(dsdata *a, dsdata *b, u_int32_t where, u_int32_t len);

dsdata *dsdata_retain(dsdata *d);
void dsdata_release(dsdata *d);

u_int32_t dsdata_size(dsdata *d);

dsdata *dsdata_read(char *filename);
dsdata *dsdata_fread(FILE *file);

dsstatus dsdata_write(dsdata *d, char *filename);
dsstatus dsdata_fwrite(dsdata *d, FILE *file);

int32_t dsdata_equal(dsdata *a, dsdata *b);
int32_t dsdata_compare(dsdata *a, dsdata *b);
int32_t dsdata_compare_sub(dsdata *a, dsdata *b, u_int32_t start, u_int32_t len);

dsdata *cstring_to_dsdata(char *s);
dsdata *casecstring_to_dsdata(char *s);
dsdata *utf8string_to_dsdata(char *s);
dsdata *caseutf8string_to_dsdata(char *);
dsdata *int8_to_dsdata(int8_t n);
dsdata *uint8_to_dsdata(u_int8_t n);
dsdata *int16_to_dsdata(int16_t n);
dsdata *uint16_to_dsdata(u_int16_t n);
dsdata *int32_to_dsdata(int32_t n);
dsdata *uint32_to_dsdata(u_int32_t n);
dsdata *int64_to_dsdata(int64_t n);
dsdata *uint64_to_dsdata(u_int64_t n);
dsdata *dsid_to_dsdata(u_int32_t n);

/*
 * NB - the following two routines do not allocate memory.
 * They simply return a pointer to the memory in the dsdata.
 * This works because dsdata includes a NULL character to
 * terminate strings.  Do not free the returned pointers!
 */
char *dsdata_to_cstring(dsdata *d);
char *dsdata_to_utf8string(dsdata *d);

int8_t dsdata_to_int8(dsdata *d);
u_int8_t dsdata_to_uint8(dsdata *d);
int16_t dsdata_to_int16(dsdata *d);
u_int16_t dsdata_to_uint16(dsdata *d);
int32_t dsdata_to_int32(dsdata *d);
u_int32_t dsdata_to_uint32(dsdata *d);
int64_t dsdata_to_int64(dsdata *d);
u_int64_t dsdata_to_uint64(dsdata *d);
u_int32_t dsdata_to_dsid(dsdata *d);

dsdata *int8_array_to_dsdata(int8_t *, u_int32_t);
dsdata *uint8_array_to_dsdata(u_int8_t *, u_int32_t);
dsdata *int16_array_to_dsdata(int16_t *, u_int32_t);
dsdata *uint16_array_to_dsdata(u_int16_t *, u_int32_t);
dsdata *int32_array_to_dsdata(int32_t *, u_int32_t);
dsdata *uint32_array_to_dsdata(u_int32_t *, u_int32_t);
dsdata *int64_array_to_dsdata(int64_t *, u_int32_t);
dsdata *uint64_array_to_dsdata(u_int64_t *, u_int32_t);

int8_t dsdata_int8_at_index(dsdata *d, u_int32_t i);
u_int8_t dsdata_uint8_at_index(dsdata *d, u_int32_t i);
int16_t dsdata_int16_at_index(dsdata *d, u_int32_t i);
u_int16_t dsdata_uint16_at_index(dsdata *d, u_int32_t i);
int32_t dsdata_int32_at_index(dsdata *d, u_int32_t i);
u_int32_t dsdata_uint32_at_index(dsdata *d, u_int32_t i);
int64_t dsdata_int64_at_index(dsdata *d, u_int32_t i);
u_int64_t dsdata_uint64_at_index(dsdata *d, u_int32_t i);

void dsdata_print(dsdata *d, FILE *f);

#endif __DSDATA_H__
