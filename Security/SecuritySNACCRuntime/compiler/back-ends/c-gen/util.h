/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * compiler/back-ends/c-gen/util.c - C encoder/decode related utility routines
 *
 *  MS 91/11/04
 *
 * Copyright (C) 1992 Michael Sample and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/back-ends/c-gen/Attic/util.h,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: util.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:28  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:44  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 18:48:39  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/10/08  03:48:21  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.1  1994/08/28  09:48:45  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */


#define FIRST_LEVEL	1	/* must be 1 or greater */
#define MAX_VAR_REF	512	/* max chars for ref'ing a var eg v->foo->bar.x->v*/

void   MakeVarPtrRef PROTO ((CRules *r, TypeDef *td, Type *parent, Type *fieldType, char *parentVarName, char *newVarName));

void   MakeVarValueRef PROTO ((CRules *r, TypeDef *td, Type *parent, Type *fieldType, char *parentVarName, char *newVarName));

void   MakeChoiceIdValueRef PROTO ((CRules *r, TypeDef *td, Type *parent, Type *fieldType, char *parentVarName, char *newVarName));

void   PrintElmtAllocCode PROTO ((FILE *f, Type *type, char *varPtrRefName));

void   PrintEocDecoders PROTO ((FILE *f, int maxLenLevel, int minLenLevel, char *lenBaseVarName, int totalLevel, char *totalBaseVarName));
