/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#import "AMString.h"
#import <pthread.h>
#import <string.h>
#import <stdlib.h>
#import <stdio.h>
#import <stdarg.h>

static pthread_mutex_t _stringarray_mutex = PTHREAD_MUTEX_INITIALIZER;
static String **_strings;
static int _nstrings;

@implementation String

+ (String *)uniqueString:(char *)s
{
	unsigned int top, bot, mid, range;
	int comp, i;
	String *result;

	if (s == NULL) return NULL;

	pthread_mutex_lock(&_stringarray_mutex);
	
	if (_nstrings == 0)
	{
		_strings = (String **)malloc(sizeof(String *));
		_strings[_nstrings] = [[String alloc] initWithChars:s];
		_nstrings++;
		result = _strings[_nstrings - 1];
		goto Std_return;
	}

	top = _nstrings - 1;
	bot = 0;
	mid = top / 2;

	range = top - bot;
	while (range > 1)
	{
		comp = strcmp(s, [_strings[mid] value]);
		if (comp == 0) {
			result = [_strings[mid] retain];
			goto Std_return;
		};
		if (comp < 0) top = mid;
		else bot = mid;

		range = top - bot;
		mid = bot + (range / 2);
	}

	if (strcmp(s, [_strings[top] value]) == 0) {
		result = [_strings[top] retain];
		goto Std_return;
	} else if (strcmp(s, [_strings[bot] value]) == 0) {
		result = [_strings[bot] retain];
		goto Std_return;
	};

	if (strcmp(s, [_strings[bot] value]) < 0) mid = bot;
	else if (strcmp(s, [_strings[top] value]) > 0) mid = top + 1;
	else mid = top;

	_nstrings++;
	_strings = (String **)realloc(_strings, sizeof(String *) * _nstrings);
	for (i = _nstrings - 1; i > mid; i--) _strings[i] = _strings[i - 1];
	_strings[mid] = [[String alloc] initWithChars:s];
	result = _strings[mid];

Std_return:
	pthread_mutex_unlock(&_stringarray_mutex);
	return result;
}

- (String *)initWithChars:(char *)s
{
	[super init];

	len = strlen(s);
	val = malloc(len + 1);
	bcopy(s, val, len);
	val[len] = '\0';

	return self;
}

- (String *)initWithInteger:(int)i
{
	char s[64];

	[super init];

	sprintf(s, "%d", i);
	len = strlen(s);
	val = malloc(len + 1);
	bcopy(s, val, len);
	val[len] = '\0';

	return self;
}

+ (String *)stringWithInteger:(int)n
{
	char s[64];

	sprintf(s, "%d", n);
	return [String uniqueString:s];
}

/* 'uniqueStringIndex' must be called with _stringarray_mutex already locked... */
static unsigned int uniqueStringIndex(char *s)
{
	unsigned int top, bot, mid, range;
	int comp;

	if (_nstrings == 0) return IndexNull;
	top = _nstrings - 1;
	bot = 0;
	mid = top / 2;

	range = top - bot;
	while (range > 1)
	{
		comp = strcmp(s, [_strings[mid] value]);
		if (comp == 0) return mid;
		else if (comp < 0) top = mid;
		else bot = mid;

		range = top - bot;
		mid = bot + (range / 2);
	}

	if (strcmp(s, [_strings[top] value]) == 0) return top;
	if (strcmp(s, [_strings[bot] value]) == 0) return bot;
	return IndexNull;
}

- (void)dealloc
{
	unsigned int i, where;

	pthread_mutex_lock(&_stringarray_mutex);

	where = uniqueStringIndex(val);
	free(val);

	if (where == IndexNull) {
		pthread_mutex_unlock(&_stringarray_mutex);
		return;
	};

	for (i = where + 1; i < _nstrings; i++) _strings[i-1] = _strings[i];
	_nstrings--;
	if (_nstrings == 0)
	{
		free(_strings);
		_strings = NULL;
	}
	else
	{
		_strings = (String **)realloc(_strings, sizeof(String *) * _nstrings);
	}

	pthread_mutex_unlock(&_stringarray_mutex);
	
	[super dealloc];
}

- (String *)prefix:(char)c
{
	int i;
	char *t;
	String *x;

	for (i = 0; ((val[i] != '\0') && (val[i] != c)); i++);
	if (i == 0) return nil;
	if (val[i] == '\0')
	{
		x = [String uniqueString:val];
		return x;
	}

	t = malloc(i + 1);
	bcopy(val, t, i);
	t[i] = '\0';
	x = [String uniqueString:t];
	free(t);
	return x;
}

- (String *)postfix:(char)c
{
	int i, l;
	char *t;
	String *x;

	for (i = 0; ((val[i] != '\0') && (val[i] != c)); i++);
	if (val[i] == '\0') return nil;
	l = strlen(val) - i;
	if (l == 1) return nil;

	t = malloc(l);
	l--;
	bcopy((val + i + 1), t, l);
	t[l] = '\0';
	x = [String uniqueString:t];
	free(t);
	return x;
}
	
- (String *)presuffix:(char)c
{
	int i;
	char *t;
	String *x;

	for (i = len - 1; ((i >= 0) && (val[i] != c)); i--);
	if (i == 0) return nil;
	if (val[0] == '\0') return nil;

	t = malloc(i + 1);
	bcopy(val, t, i);
	t[i] = '\0';
	x = [String uniqueString:t];
	free(t);
	return x;
}

- (String *)suffix:(char)c
{
	int i, l;
	char *t;
	String *x;

	l = len;

	for (i = len - 1; ((i >= 0) && (val[i] != c)); i--);
	if (i == 0) return nil;
	l -= i;
	if (l == 1) return nil;
	t = malloc(l);
	l--;
	bcopy((val + i + 1), t, l);
	t[l] = '\0';
	x = [String uniqueString:t];
	free(t);
	return x;
}

- (String *)lowerCase
{
	int i;
	char *t;
	String *x;

	t = malloc(len + 1);

	for (i = 0; val[i] != '\0'; i++) 
	{
		if ((val[i] >= 'A') && (val[i] <= 'Z'))
			t[i] = val[i] + 32;
		else
			t[i] = val[i];
	}
	t[i] = '\0';
	x = [String uniqueString:t];
	free(t);
	return x;
}

- (String *)upperCase
{
	int i;
	char *t;
	String *x;

	t = malloc(len + 1);

	for (i = 0; val[i] != '\0'; i++) 
	{
		if ((val[i] >= 'a') && (val[i] <= 'z'))
			t[i] = val[i] - 32;
		else
			t[i] = val[i];
	}
	t[i] = '\0';
	x = [String uniqueString:t];
	free(t);
	return x;
}

- (Array *)explode:(char)c
{
	Array *l;
	String *x;
	unsigned int p;

	l = [[Array alloc] init];
	
	p = 0;
	x = [self explode:c pos:&p];
	while (x != nil)
	{
		[l addObject:x];
		[x release];
		x = [self explode:c pos:&p];
	}

	return l;
}

- (String *)explode:(char)c pos:(unsigned int *)where
{
	String *x;
	char *p, *t;
	int i, n;

	if ((*where) >= len) return nil;

	p = val + (*where);
	while (p[0] != '\0')
	{
		for (i = 0; ((p[i] != '\0') && p[i] != c); i++);
		n = i;
		t = malloc(n + 1);
		for (i = 0; i < n; i++) t[i] = p[i];
		t[n] = '\0';

		x = [String uniqueString:t];
		free(t);
		*where += n;
		if (p[i] == c) *where += 1;
		return x;
	}
	return nil;
}

- (char *)value
{
	return val;
}

- (int)intValue
{
	return atoi(val);
}

- (char *)copyValue
{
	char *t;

	t = malloc(len + 1);
	bcopy(val, t, len);
	return t;
}

+ (String *)concatStrings:(String *)s :(String *)t
{
	int l;
	char *n;
	String *x;

	if (s == nil) return t;
	if (t == nil) return s;

	l = [s length] + [t length] + 1;
	n = malloc(l);
	sprintf(n, "%s%s", [s value], [t value]);
	x = [String uniqueString:n];
	free(n);
	return x;
}

/* compare self with s */
- (int)compare:(String *)s
{
	/* define nil as the smallest value string */
	if (s == nil) return 1;
	if (self == s) return 0;
	return strcmp(val, [s value]);
}

/* compare self with s */
- (BOOL)equal:(String *)s
{
	if (s == nil) return NO;
	if (self == s) return YES;
	if (strcmp(val, [s value]) == 0) return YES;
	return NO;
}

- (unsigned int)length
{
	return len;
}

/*
 * returns pointer to next instance of char following position p
 */
- (char *)scan:(char)c pos:(int *)p
{
	if (*p >= len) return NULL;
	(*p)++;
	while ((*p < len) && (val[*p] != c)) (*p)++;
	if (*p >= len)
	{
		*p = len;
		return NULL;
	}
	return val + (*p);
}

@end
