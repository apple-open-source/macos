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
 * asn_any.h
 *
 * MS 92
 * Copyright (C) 1992 Michael Sample and the University of British Columbia
 *
 * This library is free software; you can redistribute it and/or
 * modify it provided that this copyright/license information is retained
 * in original form.
 *
 * If you modify this file, you must clearly indicate your changes.
 *
 * This source code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/c-lib/inc/Attic/asn-any.h,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: asn-any.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:22  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:19  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1997/02/28 13:39:49  wan
 * Modifications collected for new version 1.3: Bug fixes, tk4.2.
 *
 * Revision 1.2  1995/07/24 21:01:07  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.1  1994/08/28  09:21:22  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#ifndef _asn_any_h_
#define _asn_any_h_

#include "hash.h"

/*
 * 1 hash table for integer keys
 * 1 hash table for oid keys
 */
extern Table *anyOidHashTblG;
extern Table *anyIntHashTblG;

typedef AsnLen (*EncodeFcn) PROTO ((BUF_TYPE b, void *value));
typedef void (*DecodeFcn) PROTO ((BUF_TYPE b, void *value, AsnLen *bytesDecoded, ENV_TYPE env));
typedef void (*FreeFcn)   PROTO ((void *v));
typedef void (*PrintFcn)  PROTO ((FILE *f, void *v));

/*
 * this is put into the hash table with the
 * int or oid as the key
 */
typedef struct AnyInfo
{
  int		anyId;  /* will be a value from the AnyId enum */
  AsnOid	oid;    /* will be zero len/null if intId is valid */
  AsnInt	intId;
  unsigned int	size;  /* size of the C data type (ie as ret'd by sizeof) */
  EncodeFcn	Encode;
  DecodeFcn	Decode;
  FreeFcn	Free;
  PrintFcn	Print;
} AnyInfo;


typedef struct AsnAny
{
  AnyInfo	*ai; /* point to entry in hash tbl that has routine ptrs */
  void		*value; /* points to the value */
} AsnAny;

/*
 * Returns anyId value for the given ANY type.
 * Use this to determine to the type of an ANY after decoding
 * it. Returns -1 if the ANY info is not available
 */
#define GetAsnAnyId( a)		(((a)->ai)? (a)->ai->anyId: -1)

/*
 * used before encoding or decoding a type so the proper
 * encode or decode routine is used.
 */
void SetAnyTypeByInt PROTO ((AsnAny *v, AsnInt id));
void SetAnyTypeByOid PROTO ((AsnAny *v, AsnOid *id));


/*
 * used to initialize the hash table (s)
 */
void InstallAnyByInt PROTO ((int anyId, AsnInt intId, unsigned int size, EncodeFcn encode, DecodeFcn decode, FreeFcn free, PrintFcn print));

void InstallAnyByOid PROTO ((int anyId, AsnOid *oid, unsigned int size, EncodeFcn encode, DecodeFcn decode, FreeFcn free, PrintFcn print));


/*
 * Standard enc, dec, free, & print routines
 * for the AsnAny type.
 * These call the routines referenced from the
 * given value's hash table entry.
 */
void FreeAsnAny PROTO ((AsnAny *v));

AsnLen BEncAsnAny PROTO ((BUF_TYPE b, AsnAny *v));

void BDecAsnAny PROTO ((BUF_TYPE b, AsnAny *result, AsnLen *bytesDecoded, ENV_TYPE env));

void PrintAsnAny PROTO ((FILE *f, AsnAny *v, unsigned short indent));



/* AnyDefinedBy is currently the same as AsnAny */

typedef AsnAny			AsnAnyDefinedBy;

#define FreeAsnAnyDefinedBy	FreeAsnAny

#define BEncAsnAnyDefinedBy	BEncAsnAny

#define BDecAsnAnyDefinedBy	BDecAsnAny

#define PrintAsnAnyDefinedBy	PrintAsnAny


#endif /* conditional include */
