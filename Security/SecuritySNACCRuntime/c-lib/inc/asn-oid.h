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
 * asn_oid.h
 *
 *  this file depends on asn_octs.h
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
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/c-lib/inc/asn-oid.h,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: asn-oid.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:23  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:20  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.2  1995/07/27 10:24:00  rj
 * minor change to merge with type table code.
 *
 * Revision 1.1  1994/08/28  09:21:34  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */


#ifndef _asn_oid_h_
#define _asn_oid_h_

#include "asn-octs.h"

typedef AsnOcts AsnOid;  /* standard oid type  */


#define ASNOID_PRESENT( aoid)	ASNOCTS_PRESENT (aoid)

AsnLen BEncAsnOid PROTO ((BUF_TYPE b, AsnOid *data));

void BDecAsnOid PROTO ((BUF_TYPE b, AsnOid *result, AsnLen *bytesDecoded, ENV_TYPE env));

#define BEncAsnOidContent( b, oid)   BEncAsnOctsContent (b, oid)


void BDecAsnOidContent PROTO ((BUF_TYPE b, AsnTag tag, AsnLen len, AsnOid  *result, AsnLen *bytesDecoded, ENV_TYPE env));


#define FreeAsnOid	FreeAsnOcts

void PrintAsnOid PROTO ((FILE *f, AsnOid *b, unsigned short int indent));

#define AsnOidsEquiv( o1, o2) AsnOctsEquiv (o1, o2)

/* linked oid type that may be easier to use in some circumstances */
#define NULL_OID_ARCNUM	-1
typedef struct OID
{
  struct OID	*next;
  long int	arcNum;
#if COMPILER || TTBL
  struct Value	*valueRef;
#endif
} OID;

AsnLen EncodedOidLen PROTO ((OID *oid));

void BuildEncodedOid PROTO ((OID *oid, AsnOid *result));

void UnbuildEncodedOid PROTO ((AsnOid *eoid, OID **result));

#endif /* conditional include */
