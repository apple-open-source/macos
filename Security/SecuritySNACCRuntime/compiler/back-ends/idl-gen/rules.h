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
 * compiler/back_ends/idl_gen/rules.h
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *           and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/compiler/back-ends/idl-gen/rules.h,v 1.1.1.1 2001/05/18 23:14:09 mb Exp $
 * $Log: rules.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:09  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:29  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:45  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.1  1997/01/01 20:25:39  rj
 * first draft
 *
 * Revision 1.2  1994/10/08  03:47:50  rj
 */

/* see ../../core/asn1module.h for IDLTDI (C++ type def info) */

typedef struct IDLRules
{
  int		maxDigitsToAppend;

  char		*typePrefix,
		*typeSuffix;

  char		*choiceEnumSuffix;
  char		*choiceUnionFieldName;	/* what the name of the choice's union is */
  char		*choiceUnionName;	/* name (tag) for choice union def name */

  int		capitalizeNamedElmts;

  IDLTDI	typeConvTbl[BASICTYPE_MACRODEF + 1];
} IDLRules;

extern IDLRules idlRulesG;
