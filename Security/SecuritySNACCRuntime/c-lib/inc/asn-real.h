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
 * asn_real.h
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
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/c-lib/inc/Attic/asn-real.h,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: asn-real.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:23  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:20  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.2  1995/07/24 21:01:18  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.1  1994/08/28  09:21:35  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */


#ifndef _asn_real_h_
#define _asn_real_h_

typedef double AsnReal;

extern AsnReal PLUS_INFINITY;
extern AsnReal MINUS_INFINITY;


void InitAsnInfinity();

AsnLen BEncAsnReal PROTO ((BUF_TYPE b, AsnReal *data));

void BDecAsnReal PROTO ((BUF_TYPE b, AsnReal *result, AsnLen *bytesDecoded, ENV_TYPE env));

AsnLen BEncAsnRealContent PROTO ((BUF_TYPE b, AsnReal *data));

void BDecAsnRealContent PROTO ((BUF_TYPE b, AsnTag tag, AsnLen len, AsnReal *result, AsnLen *bytesDecoded, ENV_TYPE env));

/* do nothing */
#define FreeAsnReal( v)

void PrintAsnReal PROTO ((FILE *f, AsnReal *b, unsigned short int indent));

#endif
