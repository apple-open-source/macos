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
 * compiler/back_ends/c_gen/str_util.h
 *
 * Mike Sample
 * 91/08/12
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/compiler/back-ends/str-util.h,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: str-util.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:27  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:39  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 18:13:55  rj
 * by default, snacc now derives output file names from the .asn1 input file name instead of the module name.
 * the global keepbaseG variable switches between the two behaviours.
 *
 * additional filename generator for idl backend.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/10/08  03:48:18  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.1  1994/08/28  09:48:38  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

char *Asn1TypeName2CTypeName PROTO ((char *aName));

char *Asn1FieldName2CFieldName PROTO ((char *aName));

char *Asn1ValueName2CValueName PROTO ((char *aName));

char *GetCaps PROTO ((char *str));

char *GetCapsAndDigits PROTO ((char *str));

void  Str2UCase PROTO ((char *str, int len));

void  Str2LCase PROTO ((char *str, int len));

void  Dash2Underscore PROTO ((char *str, int len));

void  AppendDigit PROTO ((char *str, int digit));

void  MakeCStrUnique PROTO ((DefinedObj *nameList, char *str, int maxDigits, int startingDigit));

void  MakeCxxStrUnique PROTO ((DefinedObj *nameList, char *str, int maxDigits, int startingDigit));

extern int	keepbaseG;

char *MakeBaseFileName PROTO ((const char *moduleName));
char *MakeFileName PROTO ((const char *moduleName, const char *suffix));
char *MakeCHdrFileName PROTO ((const char *moduleName));
char *MakeCSrcFileName PROTO ((const char *moduleName));
char *MakeCxxHdrFileName PROTO ((const char *moduleName));
char *MakeCxxSrcFileName PROTO ((const char *moduleName));
#ifdef _IBM_ENC_
char *MakedbHdrFileName PROTO ((const char *moduleName)); /* 19.8.93 IBM-ENC */
char *MakedbSrcFileName PROTO ((const char *moduleName)); /* 19.8.93 IBM-ENC */
#endif /* _IBM_ENC_ */
#if IDL
char *MakeIDLFileName PROTO ((const char *moduleName));
#endif
