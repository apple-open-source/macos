/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*	NXCType.h */

/* Copyright (c) 1990 NeXT, Inc. - 8/21/90 RM */
/* patterned after ctype.h ; for 8-bit encoding, Europe only */

#ifndef _NXCTYPE_H
#define _NXCTYPE_H

#include <ctype.h>

extern int NXIsAlNum(unsigned c);
extern int NXIsAlpha(unsigned c);
extern int NXIsCntrl(unsigned c);
extern int NXIsDigit(unsigned c);
extern int NXIsGraph(unsigned c);
extern int NXIsLower(unsigned c);
extern int NXIsPrint(unsigned c);
extern int NXIsPunct(unsigned c);
extern int NXIsSpace(unsigned c);
extern int NXIsUpper(unsigned c);
extern int NXIsXDigit(unsigned c);
extern int _NXToLower(unsigned c);
extern int _NXToUpper(unsigned c);
extern int NXToLower(unsigned c);
extern int NXToUpper(unsigned c);
extern int NXIsAscii(unsigned c);
extern unsigned char *NXToAscii(unsigned c);

/*
 * Data structures used by the internationized NX... versions of the
 * ctype(3) routines.  These structures are private to the above routines
 * and should NOT be referenced by the application.
 */
extern const unsigned int _NX_CTypeTable_[];	/* char types */
extern const unsigned char _NX_ULTable_[]; 	/* case conversion table */

#endif /* _NXCTYPE_H */
