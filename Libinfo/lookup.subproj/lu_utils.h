/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Useful macros and other stuff for generic lookups
 * Copyright (C) 1989 by NeXT, Inc.
 */

#import <netinfo/lookup_types.h>
#include <netinfo/ni.h>
#include <stdarg.h>

extern mach_port_t _lu_port;
extern unit *_lookup_buf;
extern int _lu_running(void);

int lookupd_query(ni_proplist *l, ni_proplist ***out);
ni_proplist *lookupd_make_query(char *cat, char *fmt, ...);
void ni_property_merge(ni_property *a, ni_property *b);
void ni_proplist_merge(ni_proplist *a, ni_proplist *b);

typedef enum lookup_state {
	LOOKUP_CACHE,
	LOOKUP_FILE,
} lookup_state;

#define SETSTATE(_lu_set, _old_set, state, stayopen) \
{ \
	if (_lu_running()) { \
		_lu_set(stayopen); \
		*state = LOOKUP_CACHE; \
	} else { \
		_old_set(stayopen); \
		*state = LOOKUP_FILE; \
	} \
} 

#define SETSTATEVOID(_lu_set, _old_set, state) \
{ \
	if (_lu_running()) { \
		_lu_set(); \
		*state = LOOKUP_CACHE; \
	} else { \
		_old_set(); \
		*state = LOOKUP_FILE; \
	} \
} 

#define INTSETSTATEVOID(_lu_set, _old_set, state) \
{ \
	int result; \
	if (_lu_running()) { \
		result = _lu_set(); \
		*state = LOOKUP_CACHE; \
	} else { \
		result = _old_set(); \
		*state = LOOKUP_FILE; \
	} \
	return result; \
} 

#define UNSETSTATE(_lu_unset, _old_unset, state) \
{ \
	if (_lu_running()) { \
		_lu_unset(); \
	} else { \
		_old_unset(); \
	} \
	*state = LOOKUP_CACHE; \
}

#define GETENT(_lu_get, _old_get, state, res_type) \
{ \
	res_type *res; \
\
	if (_lu_running()) { \
		if (*state == LOOKUP_CACHE) { \
			res = _lu_get(); \
		} else { \
			res = _old_get(); \
		} \
	} else { \
		res = _old_get(); \
	} \
	return (res); \
}

#define LOOKUP1(_lu_lookup, _old_lookup, arg, res_type) \
{ \
	res_type *res; \
 \
	if (_lu_running()) { \
		res = _lu_lookup(arg); \
	} else { \
		res = _old_lookup(arg); \
	} \
	return (res); \
}

#define LOOKUP2(_lu_lookup, _old_lookup, arg1, arg2, res_type) \
{ \
	res_type *res; \
 \
	if (_lu_running()) { \
		res = _lu_lookup(arg1, arg2); \
	} else { \
		res = _old_lookup(arg1, arg2); \
	} \
	return (res); \
}
