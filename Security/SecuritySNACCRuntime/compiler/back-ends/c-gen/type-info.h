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
 * compiler/back-ends/c-gen/type-info.h  - fills in c type information
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/back-ends/c-gen/Attic/type-info.h,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: type-info.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:28  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:44  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 18:47:46  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/10/08  03:48:20  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.1  1994/08/28  09:48:43  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */


/*
typedef struct CNamedElmt
{
    struct CNamedElmt *next;
    int value;
    char *name;
} CNamedElmt;



typedef struct CTypeInfo
{
    CTypeId           cTypeId;
    char             *cFieldName;
    char             *cTypeName;
    int               isPtr;
    int               isEndCType;
    CNamedElmt       *cNamedElmts;
    int               choiceIdValue;
    char             *choiceIdSymbol;
    char             *choiceIdEnumName;
    char             *choiceIdEnumFieldName;
    char             *printRoutineName;
    char             *encodeRoutineName;
    char             *decodeRoutineName;
}  CTypeInfo;


*/

/*
 * allows upto 9999 unamed fields of the same type in a single structure
 * or 9999 values (diff asn1 scopes -> global c scope) with same name
 */

/*
#define MAX_C_FIELD_NAME_DIGITS    4
#define MAX_C_VALUE_NAME_DIGITS    4
#define MAX_C_TYPE_NAME_DIGITS     4
#define MAX_C_ROUTINE_NAME_DIGITS  4

*/

void PrintCTypeInfo PROTO ((FILE *f, Type *t));

void FillCTypeInfo PROTO ((CRules *r, ModuleList *m));
