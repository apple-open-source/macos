/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include <architecture/ppc/asm_help.h>

#define BRANCH(X, Y)	\
TEXT @ \
LABEL(X) @ \
BRANCH_EXTERN(Y)

BRANCH(_asprintf$LDBL64, _asprintf)
BRANCH(_err$LDBL64, _err)
BRANCH(_errc$LDBL64, _errc)
BRANCH(_errx$LDBL64, _errx)
BRANCH(_fprintf$LDBL64, _fprintf)
BRANCH(_fscanf$LDBL64, _fscanf)
BRANCH(_fwprintf$LDBL64, _fwprintf)
BRANCH(_fwscanf$LDBL64, _fwscanf)
BRANCH(_printf$LDBL64, _printf)
BRANCH(_scanf$LDBL64, _scanf)
BRANCH(_snprintf$LDBL64, _snprintf)
BRANCH(_sprintf$LDBL64, _sprintf)
BRANCH(_sscanf$LDBL64, _sscanf)
BRANCH(_strtold$LDBL64, _strtold)
BRANCH(_swprintf$LDBL64, _swprintf)
BRANCH(_swscanf$LDBL64, _swscanf)
BRANCH(_syslog$LDBL64, _syslog)
BRANCH(_vasprintf$LDBL64, _vasprintf)
BRANCH(_verr$LDBL64, _verr)
BRANCH(_verrc$LDBL64, _verrc)
BRANCH(_verrx$LDBL64, _verrx)
BRANCH(_vfprintf$LDBL64, _vfprintf)
BRANCH(_vfscanf$LDBL64, _vfscanf)
BRANCH(_vfwprintf$LDBL64, _vfwprintf)
BRANCH(_vfwscanf$LDBL64, _vfwscanf)
BRANCH(_vprintf$LDBL64, _vprintf)
BRANCH(_vscanf$LDBL64, _vscanf)
BRANCH(_vsnprintf$LDBL64, _vsnprintf)
BRANCH(_vsprintf$LDBL64, _vsprintf)
BRANCH(_vsscanf$LDBL64, _vsscanf)
BRANCH(_vswprintf$LDBL64, _vswprintf)
BRANCH(_vswscanf$LDBL64, _vswscanf)
BRANCH(_vsyslog$LDBL64, _vsyslog)
BRANCH(_vwarn$LDBL64, _vwarn)
BRANCH(_vwarnc$LDBL64, _vwarnc)
BRANCH(_vwarnx$LDBL64, _vwarnx)
BRANCH(_vwprintf$LDBL64, _vwprintf)
BRANCH(_vwscanf$LDBL64, _vwscanf)
BRANCH(_warn$LDBL64, _warn)
BRANCH(_warnc$LDBL64, _warnc)
BRANCH(_warnx$LDBL64, _warnx)
BRANCH(_wcstold$LDBL64, _wcstold)
BRANCH(_wprintf$LDBL64, _wprintf)
BRANCH(_wscanf$LDBL64, _wscanf)
