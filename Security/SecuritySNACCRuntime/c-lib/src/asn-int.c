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
 * asn_int.c - BER encode, decode, print and free routines for the
 *             ASN.1 INTEGER type
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
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c-lib/src/asn-int.c,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: asn-int.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:25  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:30  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/24 21:04:51  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:05:05  rj
 * reduce the risk of unwanted surprises with macro expansion by properly separating the C tokens.
 *
 * Revision 1.1  1994/08/28  09:45:53  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include "asn-config.h"
#include "asn-len.h"
#include "asn-tag.h"
#include "asn-int.h"

/*
 * encodes universal TAG LENGTH and Contents of and ASN.1 INTEGER
 */
AsnLen
BEncAsnInt PARAMS ((b, data),
    BUF_TYPE     b _AND_
    AsnInt *data)
{
    AsnLen len;

    len =  BEncAsnIntContent (b, data);
    len += BEncDefLen (b, len);
    len += BEncTag1 (b, UNIV, PRIM, INTEGER_TAG_CODE);
    return len;
}  /* BEncAsnInt */


/*
 * decodes universal TAG LENGTH and Contents of and ASN.1 INTEGER
 */
void
BDecAsnInt PARAMS ((b, result, bytesDecoded, env),
    BUF_TYPE   b _AND_
    AsnInt    *result _AND_
    AsnLen *bytesDecoded _AND_
    jmp_buf env)
{
    AsnTag tag;
    AsnLen elmtLen;

    if ((tag = BDecTag (b, bytesDecoded, env)) != MAKE_TAG_ID (UNIV, PRIM, INTEGER_TAG_CODE))
    {
         Asn1Error ("BDecAsnInt: ERROR wrong tag on INTEGER.\n");
         longjmp (env, -40);
    }

    elmtLen = BDecLen (b, bytesDecoded, env);
    BDecAsnIntContent (b, tag, elmtLen, result, bytesDecoded, env);

}  /* BDecAsnInt */


/*
 * encodes signed long integer's contents
 */
AsnLen
BEncAsnIntContent PARAMS ((b, data),
    BUF_TYPE     b _AND_
    AsnInt *data)
{
    int             len;
    int             i;
    unsigned long int   mask;
    unsigned long int   dataCpy;

#define INT_MASK (0x7f80 << ((sizeof(AsnInt) - 2) * 8))

    dataCpy = *data;

    /*
     * calculate encoded length of the integer (content)
     */
    mask = INT_MASK;
    if ((long int)dataCpy < 0)
        for (len = sizeof (AsnInt); len > 1; --len)
        {
            if ((dataCpy & mask) == mask)
                mask >>= 8;
            else
                break;
        }
    else
        for (len = sizeof (AsnInt); len > 1; --len)
        {
            if ((dataCpy & mask) == 0)
                mask >>= 8;
            else
                break;
        }

    /*
     * write the BER integer
     */
    for (i = 0; i < len; i++)
    {
        BufPutByteRvs (b, dataCpy);
        dataCpy >>= 8;
    }

    return len;

}  /* BEncAsnIntContent */


/*
 * Decodes content of BER a INTEGER value.  The given tag is ignored.
 */
void
BDecAsnIntContent PARAMS ((b, tagId, len, result, bytesDecoded, env),
    BUF_TYPE   b _AND_
    AsnTag     tagId _AND_
    AsnLen     len _AND_
    AsnInt    *result _AND_
    AsnLen *bytesDecoded _AND_
    jmp_buf env)
{
    int   i;
    long int   retVal;
    unsigned long int  byte;


    if (len > sizeof (AsnInt))
    {
        Asn1Error ("BDecAsnIntContent: ERROR - integer to big to decode.\n");
        longjmp (env, -7);
    }

    /*
     * look at integer value
     */
    byte = (unsigned long int) BufGetByte (b);

    if (byte & 0x80)   /* top bit of first byte is sign bit */
        retVal = (-1 << 8) | byte;
    else
        retVal = byte;

    /*
     * write from buffer into long int
     */
    for (i = 1; i < len; i++)
        retVal = (retVal << 8) | (unsigned long int)(BufGetByte (b));

    if (BufReadError (b))
    {
        Asn1Error ("BDecAsnIntContent: ERROR - decoded past end of data \n");
        longjmp (env, -8);
    }
    (*bytesDecoded) += len;

    *result = retVal;

}  /* BDecAsnIntContent */


/*
 * Prints the given integer to the given FILE * in Value Notation.
 * indent is ignored.
 */
void
PrintAsnInt PARAMS ((f, v, indent),
    FILE *f _AND_
    AsnInt *v _AND_
    unsigned short int indent)
{
    fprintf (f,"%d", *v);
}


/*
 * The following deal with UNSIGNED long ints.
 * They do the same as the above routines for unsigned values.
 *
 * The compiler generated code does not call them. (It should
 * based on subtype info but it does not).
 */


/*
 * encodes universal TAG LENGTH and Contents of and ASN.1 INTEGER
 */
AsnLen
BEncUAsnInt PARAMS ((b, data),
    BUF_TYPE b _AND_
    UAsnInt *data)
{
    AsnLen len;

    len =  BEncUAsnIntContent (b, data);
    len += BEncDefLen (b, len);
    len += BEncTag1 (b, UNIV, PRIM, INTEGER_TAG_CODE);
    return len;
}  /* BEncUAsnInt */


/*
 * decodes universal TAG LENGTH and Contents of and ASN.1 INTEGER
 */
void
BDecUAsnInt PARAMS ((b, result, bytesDecoded, env),
    BUF_TYPE b _AND_
    UAsnInt *result _AND_
    AsnLen *bytesDecoded _AND_
    jmp_buf env)
{
    AsnTag tag;
    AsnLen elmtLen;

    if ((tag = BDecTag (b, bytesDecoded, env)) != MAKE_TAG_ID (UNIV, PRIM, INTEGER_TAG_CODE))
    {
         Asn1Error ("BDecAsnInt: ERROR wrong tag on INTGER.\n");
         longjmp (env, -40);
    }

    elmtLen = BDecLen (b, bytesDecoded, env);
    BDecUAsnIntContent (b, tag, elmtLen, result, bytesDecoded, env);

}  /* BDecUAsnInt */


/*
 * encodes unsigned long integer.  This allows you to correctly
 * handle unsiged values that used the most significant (sign) bit.
 */
AsnLen
BEncUAsnIntContent PARAMS ((b, data),
    BUF_TYPE b _AND_
    UAsnInt *data)
{
    int             len;
    int             i;
    unsigned long int   mask;
    unsigned long int   dataCpy;

    dataCpy = *data;

    /*
     * calculate encoded length of the integer (content)
     */
    mask = INT_MASK;
    if ((long int)dataCpy < 0)
    {
        /*write integer as normal (remember writing in reverse) */
        for (i = 0; i < sizeof (UAsnInt); i++)
        {
            BufPutByteRvs (b, dataCpy);
            dataCpy >>= 8;
        }
        /*
         * write zero byte at beginning of int, since high bit
         * is set and need to differentiate between sign
         * bit and high bit in unsigned case.
         * (this code follows the prev for loop since writing
         *  in reverse)
         */
        BufPutByteRvs (b, 0);

        return sizeof (UAsnInt)+1;
    }
    else
    {
        for (len = sizeof (UAsnInt); len > 1; --len)
        {
            if ((dataCpy & mask) == 0)
                mask >>= 8;
            else
                break;
        }

        /* write the BER integer */
        for (i = 0; i < len; i++)
        {
            BufPutByteRvs (b, dataCpy);
            dataCpy >>= 8;
        }
        return len;
    }

}  /* BEncUAsnIntContent */


/*
 * decode integer portion - no tag or length expected or decoded
 * assumes unsigned integer - This routine is useful for
 * integer subtyped to > 0 eg Guage ::= INTEGER (0..4294967295)
 */
void
BDecUAsnIntContent PARAMS ((b, tag, len, result, bytesDecoded, env),
    BUF_TYPE b _AND_
    AsnTag   tag _AND_
    AsnLen   len _AND_
    UAsnInt *result _AND_
    AsnLen  *bytesDecoded _AND_
    jmp_buf  env)
{
    int   i;
    unsigned long int retVal;

    retVal =  (unsigned long int) BufGetByte (b);

    if (len > (sizeof (UAsnInt)+1))
    {
        Asn1Error ("BDecUAsnIntContent: ERROR - integer to big to decode.\n");
        longjmp (env, -9);
    }
    else if (retVal & 0x80)   /* top bit of first byte is sign bit */
    {
        Asn1Error ("BDecUAsnIntContent: ERROR - integer is negative.\n");
        longjmp (env, -10);
    }
    else if ((len == (sizeof (UAsnInt)+1)) && (retVal != 0))
    {
        /*
         * first octet must be zero 5 octets long - extra 0 octet
         * at beginning is only used for value > 0 that need the
         * high bit
         */
        Asn1Error ("BDecUAsnIntContent: ERROR - integer is negative.\n");
        longjmp (env, -11);
    }

    /*
     * write from buffer into long int
     */
    for (i = 1; i < len; i++)
        retVal = (retVal << 8) | (unsigned long int)(BufGetByte (b));

    if (BufReadError (b))
    {
        Asn1Error ("BDecUIntegerContent: ERROR - decoded past end of data\n");
        longjmp (env, -12);
    }
    (*bytesDecoded) += len;

    *result = retVal;

}  /* BDecUAsnIntContent */


void
PrintUAsnInt PARAMS ((f, v, indent),
    FILE *f _AND_
    UAsnInt *v _AND_
    unsigned short int indent)
{
    fprintf (f, "%u", *v);
}
