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
 * str_stk.h  - maintains a stack of the components of a bit string
 * or octet string so they can be copied into a single chunk
 *
 *
 *  CONSTRUCTED BIT AND OCTET STRINGS SUCK. They should be
 *  specified in the application's ASN.1 spec as SEQUENCE OF OCTET STRING
 *
 * this stack stuff is for decoding constructed bit/octet strings
 * so the user gets a single contiguous bit/octet str instead of
 * irritating little pieces.  This does not cost a lot more than
 * a linked octet/bit string type since we're copying from the
 * buffer anyway, not referencing it directly (even in simple case).
 * It will cost more if the string stk overflows and
 * needs to be enlarged via realloc - set the values of
 * initialStkSizeG, and stkGrowSize carefully for your application.
 * Once the StkSize grows, it doesn't shrink back ever.
 *
 * Only three routine use/deal with this stack garbage
 *  BDecConsAsnOcts
 *  BDecConsAsnBits
 *  SetupConsBitsOctsStringStk
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
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/c-lib/inc/str-stk.h,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: str-stk.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:23  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:22  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.2  1995/07/24 21:01:24  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.1  1994/08/28  09:45:41  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

typedef struct StrStkElmt
{
    char *str;
    unsigned long int len;
} StrStkElmt;

typedef struct StrStk
{
    StrStkElmt *stk; /* ptr to array of SSElmts with 'size' elmts */
    unsigned long int initialNumElmts;
    unsigned long int numElmts;  /* total # of elements in str stk */
    unsigned long int growElmts; /* # elmts to increase size by when nec */
    unsigned long int nextFreeElmt; /* index of next free element */
    unsigned long int totalByteLen; /* octet len of string stored in stk */
} StrStk;


extern StrStk strStkG;

/*
 * initializes stk (Allocates if nec.)
 * once stk is enlarged, it doesn't shrink
 */
#define RESET_STR_STK()\
{\
    strStkG.nextFreeElmt = 0;\
    strStkG.totalByteLen = 0;\
    if (strStkG.stk == NULL){\
       strStkG.stk = (StrStkElmt*) malloc ((strStkG.initialNumElmts) *sizeof (StrStkElmt));\
       strStkG.numElmts = strStkG.initialNumElmts;}\
}


/*
 * add a char*,len pair to top of stack.
 * grows stack if necessary using realloc (!)
 */
#define PUSH_STR(strPtr, strsLen, env)\
{\
    if (strStkG.nextFreeElmt >= strStkG.numElmts)\
    {\
       strStkG.stk = (StrStkElmt*) realloc (strStkG.stk, (strStkG.numElmts + strStkG.growElmts) *sizeof (StrStkElmt));\
       strStkG.numElmts += strStkG.growElmts;\
    }\
    strStkG.totalByteLen += strsLen;\
    strStkG.stk[strStkG.nextFreeElmt].str = strPtr;\
    strStkG.stk[strStkG.nextFreeElmt].len = strsLen;\
    strStkG.nextFreeElmt++;\
}


/*
 * Set up size values for the stack that is used for merging constructed
 * octet or bit string into single strings.
 * ****  Call this before decoding anything. *****
 * Note: you don't have to call this if the default values
 * for initialStkSizeG and stkGrowSizeG are acceptable
 */
#define SetupConsBitsOctsStringStk (initialNumberOfElmts, numberOfElmtsToGrowBy)\
{\
    strStkG.initialNumElmts = initialNumberOfElmts; \
    strStkG.growElmts = numberOfElmtsToGrowBy;\
}
