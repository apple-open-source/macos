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
 * nibble_alloc.h - handles buffer allocation
 * MS 91
 *
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
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/c-lib/inc/Attic/nibble-alloc.h,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: nibble-alloc.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:23  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:21  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.2  1995/07/24 21:01:22  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.1  1994/08/28  09:21:43  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#ifndef _nibble_alloc_h_
#define _nibble_alloc_h_

typedef struct NibbleBuf
{
    char *start;
    char *end;
    char *curr;
    struct NibbleBuf *next;
} NibbleBuf;


typedef struct NibbleMem
{
    NibbleBuf *firstNibbleBuf;
    NibbleBuf *currNibbleBuf;
    unsigned long int incrementSize;
} NibbleMem;



void InitNibbleMem PROTO ((unsigned long int initialSize, unsigned long int incrementSize));

void ShutdownNibbleMem();

void ServiceNibbleFault PROTO ((unsigned long int size));

void *NibbleAlloc PROTO ((unsigned long int size));

void ResetNibbleMem();


#endif /* conditional include */
