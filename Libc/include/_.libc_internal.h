/*
 * Copyright (c) 2006, 2007 Apple Inc. All rights reserved.
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

/***********************************************************************
 * Not to be installed in /usr/include
 ***********************************************************************/

#ifndef __LIBC_INTERNAL_H_
#define __LIBC_INTERNAL_H_

#define LIBC_STRING(str)	#str

#if defined(VARIANT_LEGACY)
#  define LIBC_ALIAS(sym)	/* nothing */
#  define LIBC_ALIAS_C(sym)	/* nothing */
#  define LIBC_ALIAS_I(sym)	/* nothing */
#  define LIBC_INODE64(sym)	/* nothing */
#  define LIBC_1050(sym)	/* nothing */
#else /* !VARIANT_LEGACY */
#  if defined(__LP64__)
#    define LIBC_ALIAS(sym)	/* nothing */
#    if defined(VARIANT_CANCELABLE)
#      define LIBC_ALIAS_C(sym)	/* nothing */
#    else /* !VARIANT_CANCELABLE */
#      define LIBC_ALIAS_C(sym)	_asm("_" LIBC_STRING(sym) "$NOCANCEL")
#    endif /* VARIANT_CANCELABLE */
#    if defined(VARIANT_INODE32)
#      define LIBC_ALIAS_I(sym)	/* nothing */
#    else /* !VARIANT_INODE32 */
#      define LIBC_ALIAS_I(sym)	_asm("_" LIBC_STRING(sym) "$INODE64")
#    endif /* VARIANT_INODE32 */
#  else /* !__LP64__ */
#    define LIBC_ALIAS(sym)	_asm("_" LIBC_STRING(sym) "$UNIX2003")
#    if defined(VARIANT_CANCELABLE)
#      define LIBC_ALIAS_C(sym)	_asm("_" LIBC_STRING(sym) "$UNIX2003")
#    else /* !VARIANT_CANCELABLE */
#      define LIBC_ALIAS_C(sym)	_asm("_" LIBC_STRING(sym) "$NOCANCEL$UNIX2003")
#    endif /* VARIANT_CANCELABLE */
#    if defined(VARIANT_INODE32)
#      define LIBC_ALIAS_I(sym)	_asm("_" LIBC_STRING(sym) "$UNIX2003")
#    else /* !VARIANT_INODE32 */
#      define LIBC_ALIAS_I(sym)	_asm("_" LIBC_STRING(sym) "$INODE64$UNIX2003")
#    endif /* VARIANT_INODE32 */
#  endif /* __LP64__ */
#  if defined(VARIANT_INODE32)
#    define LIBC_INODE64(sym)	/* nothing */
#  else /* !VARIANT_INODE32 */
#    define LIBC_INODE64(sym)	_asm("_" LIBC_STRING(sym) "$INODE64")
#  endif /* VARIANT_INODE32 */
#  if defined(VARIANT_PRE1050)
#    define LIBC_1050(sym)	/* nothing */
#  else /* !VARIANT_PRE1050 */
#    define LIBC_1050(sym)	_asm("_" LIBC_STRING(sym) "$1050")
#  endif /* VARIANT_PRE1050 */
#endif /* VARIANT_LEGACY */

#define LIBC_EXTSN(sym)		_asm("_" LIBC_STRING(sym) "$DARWIN_EXTSN")
#if defined(VARIANT_CANCELABLE)
#  define LIBC_EXTSN_C(sym)	_asm("_" LIBC_STRING(sym) "$DARWIN_EXTSN")
#else /* !VARIANT_CANCELABLE */
#  define LIBC_EXTSN_C(sym)	_asm("_" LIBC_STRING(sym) "$DARWIN_EXTSN$NOCANCEL")
#endif /* !VARIANT_CANCELABLE */

/* 5243343 - define PR_5243343 temporarily */
#define PR_5243343

#endif /* __LIBC_INTERNAL_H_ */
