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
 * compiler/back_ends/c++_gen/gen_vals.h
 *
 * MS 92
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *           and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/compiler/back-ends/c++-gen/gen-vals.h,v 1.3 2001/06/25 21:51:10 dmitch Exp $
 * $Log: gen-vals.h,v $
 * Revision 1.3  2001/06/25 21:51:10  dmitch
 * Avoid instantiating AsnInt constants; use #define instead. Partial fix for Radar 2664258.
 *
 * Revision 1.2  2001/06/20 21:30:32  dmitch
 * Per SNACC_OIDS_AS_DEFINES #define, optionally define OIDs as #defines in the header rather than as statically initialized objects in the .cpp file.
 *
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:27  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:40  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 18:23:19  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/10/08  03:47:58  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.1  1994/08/28  09:48:06  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

/*
 * When true, compile OIDs as #defines in the header, not as 
 * statically declared constants.
 */
#define SNACC_OIDS_AS_DEFINES	1

/*
 * When true, compile AsnInts as #defines in the header, not as 
 * statically declared constants.
 */
#define SNACC_INTS_AS_DEFINES	1


void PrintCxxValueDef PROTO ((FILE *src, CxxRules *r, ValueDef *v));

void PrintCxxValueExtern PROTO ((FILE *hdr, CxxRules *r, ValueDef *v));

void PrintCxxValuesClass PROTO ((FILE *f, CxxRules *r, Value *v));

void PrintCxxValueInstatiation PROTO ((FILE *f, CxxRules *r, Value *v));

void PrintCxxOidValue PROTO ((FILE *f, CxxRules *r, AsnOid *oid));

void PrintCxxIntValue PROTO ((FILE *f, CxxRules *r, AsnInt oid));
