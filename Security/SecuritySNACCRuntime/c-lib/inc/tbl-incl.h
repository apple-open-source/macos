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
 * tbl-incl.h  - wraps all nec tbl stuff in one file
 *
 * Copyright (C) 1993 Michael Sample
 *            and the University of British Columbia
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
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c-lib/inc/tbl-incl.h,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: tbl-incl.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:23  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:22  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.2  1997/05/07 15:18:34  wan
 * Added (limited) size constraints, bitstring and enumeration names to tables
 *
 * Revision 1.1  1995/07/27 08:55:57  rj
 * first check-in after being merged into .../c-lib/.
 *
 */

#define TTBL	3

#include "asn-incl.h"
#include "tbl.h"

typedef void AVal;

typedef AVal *AStructVal;  /* an array of AVal ptrs */

typedef struct AChoiceVal
{
    enum { achoiceval_notused } choiceId;
    AVal *val;
} AChoiceVal;


#include "tbl-util.h"
#include "tbl-enc.h"
#include "tbl-dec.h"
#include "tbl-print.h"
#include "tbl-free.h"

/*
 * TblError (char *str) - configure error handler
 */
#define TblError( str)		fprintf (stderr, "%s", str)
