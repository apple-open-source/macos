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

#include <NetInfo/dsdata.h>
#include <NetInfo/utf-8.h>
#include <string.h>
#include <stdlib.h>

//#define _ALLOC_DEBUG_
//#define _DEALLOC_DEBUG_

#ifdef DSDATA_UNIQUE
dsdata **_unique_cstring_data = NULL;
u_int32_t _unique_cstring_count = 0;

void
dsdata_release_unique(void)
{
	u_int32_t i;

	for (i = 0; i < _unique_cstring_count; i++)
		dsdata_release(_unique_cstring_data[i]);

	if (_unique_cstring_data != NULL) free(_unique_cstring_data);

	_unique_cstring_data = NULL;
	_unique_cstring_count = 0;
}

dsdata *
dsdata_unique(dsdata *d)
{
	unsigned int top, bot, mid, range;
	int comp, i;

	if (d == NULL) return NULL;
	if (d->type != DataTypeCStr) return d;

	if (_unique_cstring_count == 0)
	{
		_unique_cstring_data = (dsdata **)malloc(sizeof(dsdata *));
		_unique_cstring_data[0] = dsdata_retain(d);
		_unique_cstring_count++;
		return d;
	}

	top = _unique_cstring_count - 1;
	bot = 0;
	mid = top / 2;

	range = top - bot;
	while (range > 1)
	{
		comp = dsdata_compare(d, _unique_cstring_data[mid]);
		if (comp == 0)
		{
			dsdata_retain(_unique_cstring_data[mid]);
			dsdata_release(d);
			return _unique_cstring_data[mid];
		}
		else if (comp < 0) top = mid;
		else bot = mid;

		range = top - bot;
		mid = bot + (range / 2);
	}

	if (dsdata_equal(d, _unique_cstring_data[top]))
	{
		dsdata_retain(_unique_cstring_data[top]);
		dsdata_release(d);
		return _unique_cstring_data[top];
	}

	if (dsdata_equal(d, _unique_cstring_data[bot]))
	{
		dsdata_retain(_unique_cstring_data[bot]);
		dsdata_release(d);
		return _unique_cstring_data[bot];
	}

	if (dsdata_compare(d, _unique_cstring_data[bot]) < 0) mid = bot;
	else if (dsdata_compare(d, _unique_cstring_data[top]) > 0) mid = top + 1;
	else mid = top;

	_unique_cstring_count++;
	_unique_cstring_data = (dsdata **)realloc(_unique_cstring_data, sizeof(dsdata *) * _unique_cstring_count);
	for (i = _unique_cstring_count - 1; i > mid; i--)
		_unique_cstring_data[i] = _unique_cstring_data[i - 1];

	_unique_cstring_data[mid] = dsdata_retain(d);
	return d;
}
#endif DSDATA_UNIQUE

static u_int64_t
htonq(u_int64_t n)
{
#ifdef __BIG_ENDIAN__
	return n;
#else
	u_int32_t t;
	union
	{
		u_int64_t q;
		u_int32_t l[2];
	} x;

	x.q = n;
	t = x.l[0];
	x.l[0] = htonl(x.l[1]);
	x.l[1] = htonl(t);

	return x.q;
#endif
}

static u_int64_t
ntohq(u_int64_t n)
{
#ifdef __BIG_ENDIAN__
	return n;
#else
	u_int32_t t;
	union
	{
		u_int64_t q;
		u_int32_t l[2];
	} x;

	x.q = n;
	t = x.l[0];
	x.l[0] = ntohl(x.l[1]);
	x.l[1] = ntohl(t);

	return x.q;
#endif
}

void
dsdata_print(dsdata *d, FILE *f)
{
	u_int32_t i, v;
	float fv;
#ifdef _UNIX_BSD_43_
	int32_t iv;
	u_int32_t uv;
#else
	int64_t iv;
	u_int64_t uv;
#endif

	if (d == NULL)
	{
		fprintf(f, "-nil-");
		return;
	}

	switch (d->type)
	{
		case DataTypeNil:
			fprintf(f, "(nil)");
			return;
		case DataTypeBlob:
			fprintf(f, "(blob)");
			for (i = 0; i < d->length; i++)
			{
				v = d->data[i];
				fprintf(f, " 0x%x", v);
			}
			return;
		case DataTypeBool:
			if (d->data[0] == 0) fprintf(f, "(bool) NO");
			else fprintf(f, "(bool) YES");
			return;
		case DataTypeInt:
			iv = 0;
			if (d->length == 1) iv = dsdata_to_int8(d);
			else if (d->length == 2) iv = dsdata_to_int16(d);
			else if (d->length == 4) iv = dsdata_to_int32(d);
#ifdef _UNIX_BSD_43_
			fprintf(f, "(int) %d", iv);
#else
			else if (d->length == 8) iv = dsdata_to_int64(d);
			fprintf(f, "(int) %qd", iv);
#endif
			return;
		case DataTypeUInt:
			uv = 0;
			if (d->length == 1) uv = dsdata_to_uint8(d);
			else if (d->length == 2) uv = dsdata_to_uint16(d);
			else if (d->length == 4) uv = dsdata_to_uint32(d);
#ifdef _UNIX_BSD_43_
			fprintf(f, "(uint) %u", uv);
#else
			else if (d->length == 8) uv = dsdata_to_uint64(d);
			fprintf(f, "(uint) %qu", uv);
#endif
			return;
		case DataTypeFloat:
			memmove(&fv, d->data, 4);
			fprintf(f, "(float) %f", fv);
			return;
		case DataTypeCStr:
		case DataTypeCaseCStr:
#ifdef PRINT_STRING_TYPE_NAME
			fprintf(f, "(string) %s", d->data);
#else
			fprintf(f, "%s", d->data);
#endif
			return;
		case DataTypeUTF8Str:
		case DataTypeCaseUTF8Str:
			/* try first to print as a string */
			if (dsdata_to_cstring(d) != NULL)
			{
				/* can reduce to a cstring */
#ifdef PRINT_STRING_TYPE_NAME
				fprintf(f, "(string) %s", d->data);
#else
				fprintf(f, "%s", d->data);
#endif
				return;
			}
			/* fall through ... */
		default:
			fprintf(f, "(%d)", d->type);
			for (i = 0; i < d->length; i++)
			{
				v = d->data[i];
				fprintf(f, " 0x%x", v);
			}
			return;
	}
}

dsdata *
dsdata_alloc(u_int32_t len)
{
	dsdata *x;
	u_int32_t size;

	size = sizeof(dsdata) + len;
	x = (dsdata *)calloc(1, size);
	if (x == NULL) return NULL;

	x->length = len;
	x->data = (char *)x + sizeof(dsdata);

#ifdef _ALLOC_DEBUG_
	fprintf(stderr, "dsdata_alloc   0x%08x\n", (unsigned int)x);
#endif

	return x;
}

dsdata *
dsdata_realloc(dsdata *d, u_int32_t len)
{
	dsdata *x;

	x = dsdata_alloc(len);
	x->type = d->type;
	x->retain = d->retain;

	if (d->length > len)
	{
		memmove(x->data, d->data, len);
	}
	else
	{
		memmove(x->data, d->data, d->length);
	}

	dsdata_release(d);
	return x;

}

static void
dsdata_dealloc(dsdata *x)
{
#ifdef _DEALLOC_DEBUG_
	fprintf(stderr, "dsdata_delloc   0x%08x\n", (unsigned int)x);
#endif
	free(x);
}

dsdata *
dsdata_new(u_int32_t type, u_int32_t len, char *buf)
{
	dsdata *x;

	x = dsdata_alloc(len);
	x->type = type;
	if (len > 0) memmove(x->data, buf, x->length);
	
	x->retain = 1;

	return x;
}

dsdata *
dsdata_copy(dsdata *d)
{
	dsdata *x;

	if (d == NULL) return NULL;

	x = dsdata_alloc(d->length);
	x->type = d->type;
	if (d->length > 0) memmove(x->data, d->data, x->length);

	x->retain = 1;

	return x;
}

dsdata *
dsdata_insert(dsdata *a, dsdata *b, u_int32_t where, u_int32_t len)
{
	dsdata *x;
	int n, l;

	if (b == NULL) return a;

	if (a == NULL)
	{
		a = dsdata_copy(b);
		return a;
	}
	
	if (b->length == 0) return a;

	l = len;
	if (l > b->length) l = b->length;
	x = dsdata_alloc(a->length + l);
	
	n = where;
	if (n > a->length) n = a->length;
	if (n > 0) memmove(x->data, a->data, n);
	memmove(x->data + n, b->data, l);
	if (n < a->length) memmove(x->data + n + l, a->data + n, a->length - n);

	dsdata_release(a);
	return x;
}

u_int32_t
dsdata_size(dsdata *d)
{
	if (d == NULL) return 0;
	return d->length + DSDATA_STORAGE_HEADER_SIZE;
}

dsdata *
dsdata_retain(dsdata *d)
{
	if (d == NULL) return NULL;
	d->retain++;
	return d;
}	

void
dsdata_release(dsdata *d)
{
	if (d == NULL) return;

	d->retain--;
	if (d->retain > 0) return;

	dsdata_dealloc(d);
}

dsdata *
dsdata_fread(FILE *f)
{
	dsdata *d;
	int n;
	u_int32_t len, type, x;

	if (f == NULL) return NULL;

	n = fread(&x, sizeof(u_int32_t), 1, f);
	if (n != 1) return NULL;
	type = ntohl(x);
	
	n = fread(&x, sizeof(u_int32_t), 1, f);
	if (n != 1) return NULL;
	len = ntohl(x);

	d = dsdata_alloc(len);
	d->type = type;

	if (len > 0)
	{
		n = fread(d->data, d->length, 1, f);
		if (n != 1)
		{
			dsdata_dealloc(d);
			return NULL;
		}
	}

	d->retain = 1;
	
	return d;
}

dsdata *
dsdata_read(char *filename)
{
	dsdata *d;
	FILE *f;

	if (filename == NULL) return NULL;

	f = fopen(filename, "r");
	if (f == NULL) return NULL;

	d = dsdata_fread(f);
	fclose(f);
	return d;
}

dsstatus
dsdata_fwrite(dsdata *d, FILE *f)
{
	int n;
	u_int32_t x;
	
	if (d == NULL) return DSStatusNoData;
	if (f == NULL) return DSStatusNoFile;

	x = htonl(d->type);
	n = fwrite(&x, sizeof(u_int32_t), 1, f);
	if (n != 1) return DSStatusWriteFailed;

	x = htonl(d->length);
	n = fwrite(&x, sizeof(u_int32_t), 1, f);
	if (n != 1) return DSStatusWriteFailed;

	n = fwrite(d->data, d->length, 1, f);
	if (n != 1) return DSStatusWriteFailed;

	return DSStatusOK;
}

dsstatus
dsdata_write(dsdata *d, char *filename)
{
	FILE *f;
	dsstatus ds;

	if (d == NULL) return DSStatusNoData;
	if (filename == NULL) return DSStatusNoFile;

	f = fopen(filename, "w");
	if (f == NULL) return DSStatusWriteFailed;

	ds = dsdata_fwrite(d, f);
	fclose(f);
	return ds;
}

int32_t
dsdata_equal(dsdata *a, dsdata *b)
{
	u_int32_t casefold;

	if (a == b) return 1;

	if (a == NULL)
	{
		if (b == NULL) return 1;
		return 0;
	}

	if (a->length != b->length) return 0;

	if (IsStringDataType(a->type) && IsStringDataType(b->type))
	{
		casefold = (IsCaseStringDataType(a->type) || IsCaseStringDataType(b->type));

		if (IsUTF8DataType(a->type) || IsUTF8DataType(b->type))
		{
			return (dsutil_utf8_compare(a, b, casefold) == 0);
		}
		else
		{
			if (casefold == 0) return (strncmp(a->data, b->data, a->length - 1) == 0);
			return (strncasecmp(a->data, b->data, a->length - 1) == 0);
		}
	}

	if (a->type != b->type) return 0;

	return (memcmp(a->data, b->data, a->length) == 0);
}

int32_t
dsdata_compare(dsdata *a, dsdata *b)
{
	u_int32_t len;
	int32_t c;
	u_int32_t casefold;

	if (a == b) return 0;

	/* NULL is smaller than anything */
	if (a == NULL) return -1;	
	if (b == NULL) return 1;

	if (a->length == 0)
	{
		if (b->length == 0) return 0;
		return -1;
	}
	if (b->length == 0) return 1;

	len = a->length;
	if (b->length < len) len = b->length;

	c = -1;

	if (IsStringDataType(a->type) && IsStringDataType(b->type))
	{
		/* len includes terminating NUL */
		len--;

		casefold = (IsCaseStringDataType(a->type) || IsCaseStringDataType(b->type));
		if (IsUTF8DataType(a->type) || IsUTF8DataType(b->type))
		{
			c = dsutil_utf8_compare(a, b, casefold);
		}
		else
		{
			if (casefold == 0) c = strncmp(a->data, b->data, len);
			else c = strncasecmp(a->data, b->data, len);
		}
	}
	else
	{
		c = memcmp(a->data, b->data, len);
	}

	if (c != 0) return c;

	if (a->length == b->length) return 0;
	if (a->length < b->length) return -1;
	return 1;
}

/* compare a+start...a+start+len to b */
int32_t
dsdata_compare_sub(dsdata *a, dsdata *b, u_int32_t start, u_int32_t len)
{
	int32_t c, sublen;
	u_int32_t casefold;
	dsdata ax, bx;

	/* NULL is smaller than anything */
	if (a == NULL) return -1;	
	if (b == NULL) return 1;
	
	if (a->length == 0)
	{
		if (b->length == 0) return 0;
		return -1;
	}
	if (b->length == 0) return 1;

	sublen = a->length - start;
	if (sublen < len) len = sublen;
	if (b->length < len) len = b->length;

	c = -1;

	if (IsStringDataType(a->type) && IsStringDataType(b->type))
	{
		casefold = (IsCaseStringDataType(a->type) || IsCaseStringDataType(b->type));
		if (IsUTF8DataType(a->type) || IsUTF8DataType(b->type))
		{
			ax.retain = 1;
			ax.type = a->type;
			ax.data = a->data + start;
			ax.length = len;

			bx.retain = 1;
			bx.type = b->type;
			bx.data = b->data;
			bx.length = len;

			c = dsutil_utf8_compare(&ax, &bx, casefold);
		}
		else
		{
			/* NB: len includes terminating NULL */
			if (casefold == 0) c = strncmp(a->data + start, b->data, len - 1);
			else c = strncasecmp(a->data + start, b->data, len - 1);
		}
	}
	else
	{
		c = memcmp(a->data + start, b->data, len);
	}

	return c;
}

dsdata *cstring_to_dsdata(char *s)
{
	dsdata *x;
	
	if (s == NULL) return NULL;

	x = dsdata_alloc(strlen(s) + 1);
	x->type = DataTypeCStr;

	memmove(x->data, s, x->length);

	x->retain = 1;
	
	return x;
}

dsdata *casecstring_to_dsdata(char *s)
{
	dsdata *x;
	
	if (s == NULL) return NULL;

	x = dsdata_alloc(strlen(s) + 1);
	x->type = DataTypeCaseCStr;

	memmove(x->data, s, x->length);

	x->retain = 1;
	
	return x;
}

dsdata *utf8string_to_dsdata(char *s)
{
	dsdata *x;
	char *p;

	if (s == NULL) return NULL;

	x = dsdata_alloc(dsutil_utf8_bytes(s) + 1);
	x->type = DataTypeCStr;

	/* Do we need UTF-8 encoding? */
	for (p = s; *p != '\0'; DSUTIL_UTF8_INCR(p))
	{
		if (!DSUTIL_UTF8_ISASCII(p))
		{
			/* Yes, we do. */
			x->type = DataTypeUTF8Str;
			break;
		}
	}

	memmove(x->data, s, x->length);

	x->retain = 1;
	
	return x;
}

dsdata *caseutf8string_to_dsdata(char *s)
{
	dsdata *x;

	x = utf8string_to_dsdata(s);
	if (x != NULL)
	{
		if (x->type == DataTypeUTF8Str)
			x->type = DataTypeCaseUTF8Str;
		else
			x->type = DataTypeCaseCStr;
	}

	return x;
}

dsdata *int8_to_dsdata(int8_t i)
{
	dsdata *x;

	x = dsdata_alloc(1);
	x->type = DataTypeInt;

	x->data[0] = i;
	x->retain = 1;
	
	return x;
}
	
dsdata *uint8_to_dsdata(u_int8_t i)
{
	dsdata *x;
	
	x = dsdata_alloc(1);
	x->type = DataTypeUInt;

	x->data[0] = i;
	x->retain = 1;
	
	return x;
}
	
dsdata *int16_to_dsdata(int16_t i)
{
	dsdata *x;
	int16_t t;

	x = dsdata_alloc(2);
	x->type = DataTypeInt;

	t = htons(i);
	memmove(x->data, &t, x->length);
	x->retain = 1;
	
	return x;
}

dsdata *uint16_to_dsdata(u_int16_t i)
{
	dsdata *x;
	u_int16_t t;

	x = dsdata_alloc(2);
	x->type = DataTypeUInt;

	t = htons(i);
	memmove(x->data, &t, x->length);
	x->retain = 1;
	
	return x;
}

dsdata *int32_to_dsdata(int32_t i)
{
	dsdata *x;
	int32_t t;

	x = dsdata_alloc(4);
	x->type = DataTypeInt;

	t = htonl(i);
	memmove(x->data, &t, x->length);
	x->retain = 1;
	
	return x;
}

dsdata *uint32_to_dsdata(u_int32_t i)
{
	dsdata *x;
	u_int32_t t;

	x = dsdata_alloc(4);
	x->type = DataTypeUInt;

	t = htonl(i);
	memmove(x->data, &t, x->length);
	x->retain = 1;
	
	return x;
}

dsdata *int64_to_dsdata(int64_t i)
{
	dsdata *x;
	int64_t t;

	x = dsdata_alloc(8);
	x->type = DataTypeInt;

	t = htonq(i);
	memmove(x->data, &t, x->length);
	x->retain = 1;
	
	return x;
}

dsdata *uint64_to_dsdata(u_int64_t i)
{
	dsdata *x;
	u_int64_t t;

	x = dsdata_alloc(8);
	x->type = DataTypeUInt;

	t = htonq(i);
	memmove(x->data, &t, x->length);
	x->retain = 1;
	
	return x;
}

char *dsdata_to_cstring(dsdata *d)
{
	char *p;

	if (d == NULL) return NULL;

	/* Attempt to convert UTF-8 strings non-lossily. */
	switch (d->type)
	{
		case DataTypeUTF8Str:
		case DataTypeCaseUTF8Str:
			for (p = d->data; *p != '\0'; DSUTIL_UTF8_INCR(p))
			{
				if (!DSUTIL_UTF8_ISASCII(p)) return NULL;
			}
			/* fall through ... */
		case DataTypeCStr:
		case DataTypeCaseCStr:
			return d->data;
		default:
			break;
	}

	return NULL;
}

char *dsdata_to_utf8string(dsdata *d)
{
	if (d == NULL) return NULL;

	/* We can promote C strings to UTF8 strings */
	if (IsStringDataType(d->type))
	{
		return d->data;
	}

	return NULL;
}

int8_t dsdata_to_int8(dsdata *d)
{
	int8_t i;

	if (d == NULL) return 0;
	if (d->length < 1) return 0;

	i = d->data[0];
	return i;
}

u_int8_t dsdata_to_uint8(dsdata *d)
{
	u_int8_t i;

	if (d == NULL) return 0;
	if (d->length < 1) return 0;

	i = d->data[0];
	return i;
}

int16_t dsdata_to_int16(dsdata *d)
{
	int16_t i;

	if (d == NULL) return 0;
	if (d->length < 2) return 0;

	memmove(&i, d->data, 2);
	return ntohs(i);
}

u_int16_t dsdata_to_uint16(dsdata *d)
{
	u_int16_t i;

	if (d == NULL) return 0;
	if (d->length < 2) return 0;

	memmove(&i, d->data, 2);
	return ntohs(i);
}

int32_t dsdata_to_int32(dsdata *d)
{
	int32_t i;

	if (d == NULL) return 0;
	if (d->length < 4) return 0;

	memmove(&i, d->data, 4);
	return ntohl(i);
}

u_int32_t dsdata_to_uint32(dsdata *d)
{
	u_int32_t i;

	if (d == NULL) return 0;
	if (d->length < 4) return 0;

	memmove(&i, d->data, 4);
	return ntohl(i);
}

int64_t dsdata_to_int64(dsdata *d)
{
	int64_t i;

	if (d == NULL) return 0;
	if (d->length < 8) return 0;

	memmove(&i, d->data, 8);
	return ntohq(i);
}

u_int64_t dsdata_to_uint64(dsdata *d)
{
	u_int64_t i;

	if (d == NULL) return 0;
	if (d->length < 8) return 0;

	memmove(&i, d->data, 8);
	return ntohq(i);
}

dsdata *int8_array_to_dsdata(int8_t *a, u_int32_t n)
{
	dsdata *x;

	x = uint8_array_to_dsdata((u_int8_t *)a, n);
	x->type = DataTypeInt8Array;
	return x;
}

dsdata *uint8_array_to_dsdata(u_int8_t *a, u_int32_t n)
{
	dsdata *x;

	x = dsdata_alloc(n);
	x->type = DataTypeUInt8Array;
	if (n >  0) memmove(x->data, a, x->length);

	x->retain = 1;
	
	return x;
}

dsdata *int16_array_to_dsdata(int16_t *a, u_int32_t n)
{
	dsdata *x;

	x = uint16_array_to_dsdata((u_int16_t *)a, n);
	x->type = DataTypeInt16Array;
	return x;
}

dsdata *uint16_array_to_dsdata(u_int16_t *a, u_int32_t n)
{
	dsdata *x;
	void *p;
	u_int16_t t;
	u_int32_t i;

	x = dsdata_alloc(n * 2);
	x->type = DataTypeUInt16Array;

	if (n > 0) 
	{
		p = (void *)x->data;
		for (i = 0; i < n; i++)
		{
			t = htons(a[i]);
			memmove(p, &t, 2);
			p += 2;
		}
	}

	x->retain = 1;
	
	return x;
}

dsdata *int32_array_to_dsdata(int32_t *a, u_int32_t n)
{
	dsdata *x;

	x = uint32_array_to_dsdata((u_int32_t *)a, n);
	x->type = DataTypeInt32Array;
	return x;
}

dsdata *uint32_array_to_dsdata(u_int32_t *a, u_int32_t n)
{
	dsdata *x;
	void *p;
	u_int32_t t, i;

	x = dsdata_alloc(n * 4);
	x->type = DataTypeUInt32Array;

	if (n > 0)
	{
		p = (void *)x->data;
		for (i = 0; i < n; i++)
		{
			t = htonl(a[i]);
			memmove(p, &t, 4);
			p += 4;
		}
	}

	x->retain = 1;
	
	return x;
}

dsdata *int64_array_to_dsdata(int64_t *a, u_int32_t n)
{
	dsdata *x;

	x = uint64_array_to_dsdata((u_int64_t *)a, n);
	x->type = DataTypeInt64Array;
	return x;
}

dsdata *uint64_array_to_dsdata(u_int64_t *a, u_int32_t n)
{
	dsdata *x;
	void *p;
	u_int32_t i;
	u_int64_t t;

	x = dsdata_alloc(n * 8);
	x->type = DataTypeUInt64Array;

	if (n > 0)
	{
		p = (void *)x->data;
		for (i = 0; i < n; i++)
		{
			t = htonq(a[i]);
			memmove(p, &t, 8);
			p += 8;
		}
	}

	x->retain = 1;
	
	return x;
}

int8_t dsdata_int8_at_index(dsdata *d, u_int32_t n)
{
	return (int8_t)dsdata_uint8_at_index(d, n);
}

u_int8_t dsdata_uint8_at_index(dsdata *d, u_int32_t n)
{
	u_int8_t t;

	if (d == NULL) return 0;
	if (d->length < n) return 0;
	memmove(&t, d->data + n, 1);
	return t;
}

int16_t dsdata_int16_at_index(dsdata *d, u_int32_t n)
{
	return (int16_t)dsdata_uint16_at_index(d, n);
}

u_int16_t dsdata_uint16_at_index(dsdata *d, u_int32_t n)
{
	u_int16_t t;

	if (d == NULL) return 0;
	if ((d->length / 2) < n) return 0;
	memmove(&t, d->data + (n * 2), 2);
	return ntohs(t);
}

int32_t dsdata_int32_at_index(dsdata *d, u_int32_t n)
{
	return (int32_t)dsdata_uint32_at_index(d, n);
}

u_int32_t dsdata_uint32_at_index(dsdata *d, u_int32_t n)
{
	u_int32_t t;

	if (d == NULL) return 0;
	if ((d->length / 4) < n) return 0;
	memmove(&t, d->data + (n * 4), 4);
	return ntohl(t);
}

int64_t dsdata_int64_at_index(dsdata *d, u_int32_t n)
{
	return (int64_t)dsdata_uint64_at_index(d, n);
}

u_int64_t dsdata_uint64_at_index(dsdata *d, u_int32_t n)
{
	u_int64_t t;

	if (d == NULL) return 0;
	if ((d->length / 8) < n) return 0;
	memmove(&t, d->data + (n * 8), 8);
	return ntohq(t);
}

/* Directory IDs. */
dsdata *dsid_to_dsdata(u_int32_t i)
{
	dsdata *x;
	u_int32_t t;

	x = dsdata_alloc(4);
	x->type = DataTypeDirectoryID;

	t = htonl(i);
	memmove(x->data, &t, x->length);
	x->retain = 1;
	
	return x;
}

u_int32_t dsdata_to_dsid(dsdata *d)
{
	u_int32_t i;

	if (d == NULL) return 0;
	if (d->length < 4) return 0;

	memmove(&i, d->data, 4);
	return ntohl(i);
}
