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
 * print.c - library routines for printing ASN.1 values.
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
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c-lib/src/print.c,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: print.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:25  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:32  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1997/03/13 09:15:19  wan
 * Improved dependency generation for stupid makedepends.
 * Corrected PeekTag to peek into buffer only as far as necessary.
 * Added installable error handler.
 * Fixed small glitch in idl-code generator (Markku Savela <msa@msa.tte.vtt.fi>).
 *
 * Revision 1.2  1995/07/24 21:04:55  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.1  1994/08/28  09:46:08  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include "asn-config.h"
#include "print.h"

unsigned short int stdIndentG = 4;


void
Indent PARAMS ((f, i),
    FILE *f _AND_
    unsigned short int i)
{
    for (; i > 0; i--)
        fputc (' ', f);  /* this may be slow */
}

void Asn1DefaultErrorHandler PARAMS ((str, severity),
    char* str _AND_
    int severity)
{
    fprintf(stderr,"%s",str);
}

static Asn1ErrorHandler asn1CurrentErrorHandler = Asn1DefaultErrorHandler;

void
Asn1Error PARAMS ((str),
    char* str)
{
    (*asn1CurrentErrorHandler)(str,1);
}

void
Asn1Warning PARAMS ((str),
    char* str)
{
    (*asn1CurrentErrorHandler)(str,0);
}

Asn1ErrorHandler
Asn1InstallErrorHandler PARAMS ((handler),
    Asn1ErrorHandler handler)
{
    Asn1ErrorHandler former = asn1CurrentErrorHandler;
    asn1CurrentErrorHandler = handler;
    return former;
}

