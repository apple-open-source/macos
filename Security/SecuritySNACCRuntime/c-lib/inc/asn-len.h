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
 * asn_len.h
 *
 * Warning: many of these routines are MACROs for performance reasons
 *          - be carful where you use them.  Don't use more than one per
 *          assignment statement -
 *          (eg itemLen += BEncEoc (b) + BEncFoo (b) ..; this
 *           will break the code)
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
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c-lib/inc/asn-len.h,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: asn-len.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:22  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:20  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.2  1995/07/27 08:42:40  rj
 * cpp macro TBL changed to TTBL since some type table code uses TBL as a type name.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.1  1994/08/28  09:21:29  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#ifndef _asn_len_h_
#define _asn_len_h_

typedef unsigned long int AsnLen;

/* max unsigned value  - used for internal rep of indef len */
#define INDEFINITE_LEN		~0L


#ifdef USE_INDEF_LEN

#define BEncEocIfNec( b)	BEncEoc (b)

/*
 * include len for EOC  (2 must be first due to BEncIndefLen
 * - ack! ugly macros!)
 */
#define BEncConsLen( b, len)	2 + BEncIndefLen(b)


#else  /* use definite length - faster?/smaller encodings */


/* do nothing since only using definite lens */
#define BEncEocIfNec( b)

#define BEncConsLen( b, len)	BEncDefLen(b, len)


#endif



/*
 * writes indefinite length byte to buffer. 'returns' encoded len (1)
 */
#define BEncIndefLen( b)\
    1;\
    BufPutByteRvs (b, 0x80);


#define BEncEoc( b)\
    2;\
    BufPutByteRvs (b, 0);\
    BufPutByteRvs (b, 0);


/*
 * use if you know the encoded length will be 0 >= len <= 127
 * Eg for booleans, nulls, any resonable integers and reals
 *
 * NOTE: this particular Encode Routine does NOT return the length
 * encoded (1).
 */
#define BEncDefLenTo127( b, len)\
    BufPutByteRvs (b, (unsigned char) len)

#define BDEC_2ND_EOC_OCTET( b, bytesDecoded, env)\
{\
    if ((BufGetByte (b) != 0) || BufReadError (b)) {\
        Asn1Error ("ERROR - second octet of EOC not zero\n");\
        longjmp (env, -28);}\
     (*bytesDecoded)++;\
}


AsnLen BEncDefLen PROTO ((BUF_TYPE  b, AsnLen len));

AsnLen BDecLen PROTO ((BUF_TYPE b, AsnLen  *bytesDecoded, ENV_TYPE env));

/*
AsnLen BEncEoc PROTO ((BUF_TYPE b));
*/
void BDecEoc PROTO ((BUF_TYPE b, AsnLen *bytesDecoded, ENV_TYPE env));

#if TTBL
int PeekEoc PROTO ((BUF_TYPE b));
#endif

#endif /* conditional include */
