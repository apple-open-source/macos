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


// file: .../c++-lib/src/asn-real.C - AsnReal (ASN.1 REAL) type
//
//  Mike Sample
//  92/07/02
// Copyright (C) 1992 Michael Sample and the University of British Columbia
//
// This library is free software; you can redistribute it and/or
// modify it provided that this copyright/license information is retained
// in original form.
//
// If you modify this file, you must clearly indicate your changes.
//
// This source code is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// $Header: /cvs/root/Security/SecuritySNACCRuntime/c++-lib/c++/Attic/asn-real.cpp,v 1.4 2002/03/21 05:38:45 dmitch Exp $
// $Log: asn-real.cpp,v $
// Revision 1.4  2002/03/21 05:38:45  dmitch
// Radar 2868524: no more setjmp/longjmp in SNACC-generated code.
//
// Revision 1.3.44.1  2002/03/20 00:36:50  dmitch
// Radar 2868524: SNACC-generated code now uses throw/catch instead of setjmp/longjmp.
//
// Revision 1.3  2001/06/27 23:09:15  dmitch
// Pusuant to Radar 2664258, avoid all cerr-based output in NDEBUG configuration.
//
// Revision 1.2  2001/06/21 21:57:00  dmitch
// Avoid global const PLUS_INFINITY, MINUS_INFINITY
//
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.3  2001/05/05 00:59:19  rmurphy
// Adding darwin license headers
//
// Revision 1.2  2000/06/08 20:05:36  dmitch
// Mods for X port. These files are actually machine generated and probably don't need to be in CVS....
//
// Revision 1.1.1.1  2000/03/09 01:00:06  rmurphy
// Base Fortissimo Tree
//
// Revision 1.3  1999/03/21 02:07:37  mb
// Added Copy to every AsnType.
//
// Revision 1.2  1999/02/26 00:23:40  mb
// Fixed for Mac OS 8
//
// Revision 1.1  1999/02/25 05:21:53  mb
// Added snacc c++ library
//
// Revision 1.7  1997/02/28 13:39:46  wan
// Modifications collected for new version 1.3: Bug fixes, tk4.2.
//
// Revision 1.6  1995/08/17 15:27:19  rj
// recognize and return "±inf" for PLUS-INFINITY/MINUS-INFINITY.
//
// Revision 1.5  1995/07/24  20:29:24  rj
// #if TCL ... #endif wrapped into #if META ... #endif
//
// call constructor with additional pdu and create arguments.
//
// changed `_' to `-' in file names.
//
// Revision 1.4  1995/02/18  17:01:49  rj
// denote a long if we want a long.
// make the code work on little endian CPUs.
// ported to work with CPU/compiler combinations providing 64 bit longs.
//
// Revision 1.3  1994/10/08  04:18:29  rj
// code for meta structures added (provides information about the generated code itself).
//
// code for Tcl interface added (makes use of the above mentioned meta code).
//
// virtual inline functions (the destructor, the Clone() function, BEnc(), BDec() and Print()) moved from inc/*.h to src/*.C because g++ turns every one of them into a static non-inline function in every file where the .h file gets included.
//
// made Print() const (and some other, mainly comparison functions).
//
// several `unsigned long int' turned into `size_t'.
//
// Revision 1.2  1994/08/28  10:01:18  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:21:07  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#include "asn-config.h"
#include "asn-len.h"
#include "asn-tag.h"
#include "asn-type.h"
#include "asn-real.h"

#ifndef IEEE_REAL_LIB
/* ieee functions (in case not in math.h)*/
extern "C" {
extern int iszero (double);
//extern int isinf (double);
//extern int signbit (double);
extern int ilogb (double);
//extern double scalbn (double, int);
}
#endif

double AsnPlusInfinity();
double AsnMinusInfinity();

#define ENC_PLUS_INFINITY	0x40
#define ENC_MINUS_INFINITY	0x41

#define REAL_BINARY		0x80
#define REAL_SIGN		0x40
#define REAL_EXPLEN_MASK	0x03
#define REAL_EXPLEN_1		0x00
#define REAL_EXPLEN_2		0x01
#define REAL_EXPLEN_3		0x02
#define REAL_EXPLEN_LONG	0x03
#define REAL_FACTOR_MASK	0x0c
#define REAL_BASE_MASK		0x30
#define REAL_BASE_2		0x00
#define REAL_BASE_8		0x10
#define REAL_BASE_16		0x20

AsnType *AsnReal::Clone() const
{
  return new AsnReal;
}

AsnType *AsnReal::Copy() const
{
  return new AsnReal (*this);
}

// Returns the smallest octet length needed to hold the given long int value
static unsigned int
SignedIntOctetLen (long int val)
{
    unsigned long int mask = (0x7f80L << ((sizeof (long int) - 2) * 8));
    unsigned int retVal = sizeof (long int);

    if (val < 0)
        val = val ^ (~0L);  /* XOR val with all 1's */

    while ((retVal > 1) && ((val & mask) == 0))
    {
        mask >>= 8;
        retVal--;
    }

    return retVal;

} /* SignedIntOctetLen */



#ifdef IEEE_REAL_FMT

// Returns the PLUS INFINITY in double format
// This assumes that a C++ double is an IEEE double.
// The bits for IEEE double PLUS INFINITY are
// 0x7ff0000000000000
double AsnPlusInfinity()
{
#ifndef _IBM_ENC_
    double d;
    unsigned char *c = (unsigned char *)&d;

#if WORDS_BIGENDIAN
    c[0] = 0x7f;
    c[1] = 0xf0;
    c[2] = 0x0;
    c[3] = 0x0;
    c[4] = 0x0;
    c[5] = 0x0;
    c[6] = 0x0;
    c[7] = 0x0;
#else
    c[7] = 0x7f;
    c[6] = 0xf0;
    c[5] = 0x0;
    c[4] = 0x0;
    c[3] = 0x0;
    c[2] = 0x0;
    c[1] = 0x0;
    c[0] = 0x0;
#endif

    return d;
#else
     return 1.7976931348623158e+308;
#endif /* _IBM_ENC_ */
} /* AsnPlusInfinity */

double AsnMinusInfinity()
{
    return -AsnPlusInfinity();
}

#if SIZEOF_DOUBLE != 8
  #error oops: doubles are expected to be 8 bytes in size!
#endif

/*
 * Use this routine if you system/compiler represents doubles in the IEEE format.
 */
AsnLen AsnReal::BEncContent (BUF_TYPE b)
{
    int	exponent;
    int isNeg;
#if SIZEOF_LONG == 8
    unsigned long mantissa, val, *p;
    int i;
#elif SIZEOF_LONG == 4
    unsigned char *dbl;
    unsigned long int *first4;
    unsigned long int *second4;
#else
  #error long neither 8 nor 4 bytes in size?
#endif

    /* no contents for 0.0 reals */
    if (value == 0.0) /* all bits zero, disregarding top/sign bit */
        return 0;

#if SIZEOF_LONG == 8
    /*
     * this part assumes that sizeof (long) == sizeof (double) == 8
     * It shouldn't be endian-dependent but I haven't verified that
     */

    p = (unsigned long*) &value;
    val = *p;

    isNeg = (val >> 63) & 1;
    /* special real values for +/- oo */
    if (!finite (value))
    {
        if (isNeg)
            b.PutByteRvs(ENC_MINUS_INFINITY);
        else
            b.PutByteRvs(ENC_PLUS_INFINITY);
        return 1;
    }
    else /* encode a binary real value */
    {
	exponent = (val >> 52) & 0x7ff;
	mantissa = (val & 0xfffffffffffffL) | 0x10000000000000L;

	for (i = 0; i < 7; i++)
	{
          b.PutByteRvs(mantissa & 0xff);
	  mantissa >>= 8;
        }
        exponent -= (1023 + 52);

#elif SIZEOF_LONG == 4
    /*
     * this part assumes that sizeof (long) == 4 and
     * that sizeof (double) == 8
     *
     * sign  exponent
     *     b 2-12 incl
     *  Sv-----------v----- rest is mantissa
     * -------------------------------------------
     * |         |
     * -------------------------------------------
     *  123456878 1234
     *
     * sign bit is 1 if real is < 0
     * exponent is an 11 bit unsigned value (subtract 1023 to get correct exp value)
     * decimal pt implied before mantissa (ie mantissa is all fractional)
     * and implicit 1 bit to left of decimal
     *
     * when given NaN (not a number - ie oo/oo) it encodes the wrong value
     * instead of checking for the error. If you want to check for it,
     *  a NaN is any sign bit with a max exponent (all bits a 1) followed
     *  by any non-zero mantissa. (a zero mantissa is used for infinity)
     *
     */

    first4 = (unsigned long int*) (dbl = (unsigned char*) &value);
    second4 = (unsigned long int *) (dbl + sizeof (long int));

    /* no contents for 0.0 reals */
    if (value == 0.0) /* all bits zero, disregarding top/sign bit */
        return 0;

    isNeg = dbl[0] & 0x80;

    /* special real values for +/- oo */
    if (((*first4 & 0x7fffffff) == 0x7ff00000) && (*second4 == 0))
    {
        if (isNeg)
            b.PutByteRvs (ENC_MINUS_INFINITY);
        else
            b.PutByteRvs (ENC_PLUS_INFINITY);

        return 1;
    }
    else  /* encode a binary real value */
    {
        exponent = (((*first4) >> 20) & 0x07ff);

        /* write the mantissa (N value) */
        b.PutSegRvs ((char*)(dbl+2), sizeof (double)-2);

        /*
         * The rightmost 4 bits of a double 2nd octet are the
         * most sig bits of the mantissa.
         * write the most signficant byte of the asn1 real manitssa,
         * adding implicit bit to 'left of decimal' if not de-normalized
         * (de normalized if exponent == 0)
         *
         * if the double is not in de-normalized form subtract 1023
         * from the exponent to get proper signed exponent.
         *
         * for both the normalized and de-norm forms
         * correct the exponent by subtracting 52 since:
         *   1. mantissa is 52 bits in the double (56 in ASN.1 REAL form)
         *   2. implicit decimal at the beginning of double's mantissa
         *   3. ASN.1 REAL's implicit decimal is after its mantissa
         * so converting the double mantissa to the ASN.1 form has the
         * effect of multiplying it by 2^52. Subtracting 52 from the
         * exponent corrects this.
         */
        if (exponent == 0) /* de-normalized - no implicit 1 to left of dec.*/
        {
            b.PutByteRvs (dbl[1] & 0x0f);
            exponent -= 52;
        }
        else
        {
            b.PutByteRvs ((dbl[1] & 0x0f) | 0x10); /* 0x10 adds implicit bit */
            exponent -= (1023 + 52);
        }

#else
  #error long neither 8 nor 4 bytes in size?
#endif

        /*  write the exponent  */
        b.PutByteRvs (exponent & 0xff);
        b.PutByteRvs (exponent >> 8);

        /* write format octet */
        /* bb is 00 since base is 2 so do nothing */
        /* ff is 00 since no other shifting is nec */
        if (isNeg)
            b.PutByteRvs (REAL_BINARY | REAL_EXPLEN_2 | REAL_SIGN);
        else
            b.PutByteRvs (REAL_BINARY | REAL_EXPLEN_2);

        return sizeof (double) + 2;
    }

    /* not reached */

}  /*  AsnReal::BEncContent */

#else  /* IEEE_REAL_FMT not def */

#ifdef IEEE_REAL_LIB

// Returns the PLUS INFINITY in double format
// this assumes you have the IEEE functions in
// the math lib
double AsnPlusInfinity()
{
    return infinity();
} /* AsnPlusInfinity */

double AsnMinusInfinity()
{
    return -AsnPlusInfinity();
}

// This routine uses the ieee library routines to encode
// this AsnReal's double value
AsnLen AsnReal::BEncContent (BUF_TYPE b)
{
    AsnLen encLen;
    double mantissa;
    double tmpMantissa;
    unsigned int truncatedMantissa;
    int exponent;
    unsigned int expLen;
    int sign;
    unsigned char buf[sizeof (double)];
    int i, mantissaLen;
    unsigned char firstOctet;

    /* no contents for 0.0 reals */
    if (iszero (value))
        return 0;

    /* special real values for +/- oo */
    if (isinf (value))
    {
        if (signbit (value)) /* neg */
            b.PutByteRvs (ENC_MINUS_INFINITY);
        else
            b.PutByteRvs (ENC_PLUS_INFINITY);

        encLen = 1;
    }
    else  /* encode a binary real value */
    {
        if (signbit (value))
            sign = -1;
        else
            sign = 1;

        exponent =  ilogb (value);

        /* get the absolute value of the mantissa (subtract 1 to make < 1) */
        mantissa = scalbn (fabs (value), -exponent-1);


        tmpMantissa = mantissa;

        /* convert mantissa into an unsigned integer */
        for (i = 0; i < sizeof (double); i++)
        {
            /* normalizied so shift 8 bits worth to the left of the decimal */
            tmpMantissa *= (1<<8);

            /* grab only (octet sized) the integer part */
            truncatedMantissa = (unsigned int) tmpMantissa;

            /* remove part to left of decimal now for next iteration */
            tmpMantissa -= truncatedMantissa;

            /* write into tmp buffer */
            buf[i] = truncatedMantissa;

            /* keep track of last non zero octet so can zap trailing zeros */
            if (truncatedMantissa)
                mantissaLen = i+1;
        }

        /*
         * write format octet  (first octet of content)
         *  field  1 S bb ff ee
         *  bit#   8 7 65 43 21
         *
         * 1 in bit#1 means binary rep
         * 1 in bit#2 means the mantissa is neg, 0 pos
         * bb is the base:    65  base
         *                    00    2
         *                    01    8
         *                    10    16
         *                    11    future ext.
         *
         * ff is the Value of F where  Mantissa = sign x N x 2^F
         *    FF can be one of 0 to 3 inclusive. (used to save re-alignment)
         *
         * ee is the length of the exponent:  21   length
         *                                    00     1
         *                                    01     2
         *                                    10     3
         *                                    11     long form
         *
         *
         * encoded binary real value looks like
         *
         *     fmt oct
         *   --------------------------------------------------------
         *   |1Sbbffee|  exponent (2's comp)  |   N (unsigned int)  |
         *   --------------------------------------------------------
         *    87654321
         */
        firstOctet = REAL_BINARY;
        if (signbit (value))
            firstOctet |= REAL_SIGN;

        /* bb is 00 since base is 2 so do nothing */
        /* ff is 00 since no other shifting is nec */

        /*
         * get exponent calculate its encoded length
         * Note that the process of converting the mantissa
         * double to an int shifted the decimal mantissaLen * 8
         * to the right - so correct that here
         */
        exponent++; /* compensate for trick to put mantissa < 1 */
        exponent -= (mantissaLen * 8);
        expLen = SignedIntOctetLen (exponent);

        switch (expLen)
        {
            case 1:
                firstOctet |= REAL_EXPLEN_1;
                break;
            case 2:
                firstOctet |= REAL_EXPLEN_2;
                break;
            case 3:
                firstOctet |= REAL_EXPLEN_3;
                break;
            default:
                firstOctet |= REAL_EXPLEN_LONG;
                break;
        }

        encLen = mantissaLen + expLen + 1;

        /* write the mantissa (N value) */
        b.PutSegRvs ((char*)buf, mantissaLen);

        /* write the exponent */
        for (i = expLen; i > 0; i--)
        {
            b.PutByteRvs (exponent);
            exponent >> 8;
        }

        /* write the exponents length if nec */
        if (expLen > 3)
        {
            encLen++;
            b.PutByteRvs (expLen);
        }

        /* write the format octet */
        b.PutByteRvs (firstOctet);

    }
    return encLen;

}  /*  AsnReal::BEncContent */

#else  /* neither IEEE_REAL_FMT or IEEE_REAL_LIB are def */


// Returns the PLUS INFINITY in double format
// This assumes that a C++ double is an IEEE double.
// The bits for IEEE double PLUS INFINITY are
// 0x7ff0000000000000
// NOTE: this is a guess - you should set this up for
// your architecture
double AsnPlusInfinity()
{
    double d;
    unsigned char *c;
    unsigned i;

    c = (unsigned char*)&d;
    c[0] = 0x7f;
    c[1] = 0xf0;
    for (i = 2; i < sizeof (double); i++)
        c[i] = 0;
    return d;
} /* AsnPlusInfinity */

double AsnMinusInfinity()
{
    return -AsnPlusInfinity();
}

/*
 * Encodes the content of an ASN.1 REAL value to the given buffer.
 * This version of the routine does not assume an IEEE double rep.
 * or the existence of the IEEE library routines.  Uses old style
 * UNIX frexp etc.
 */
AsnLen AsnReal::BEncContent (BUF_TYPE b)
{
    unsigned long int encLen;
    double mantissa;
    double tmpMantissa;
    unsigned int truncatedMantissa;
    int exponent;
    unsigned int expLen;
    int sign;
    unsigned char buf[sizeof (double)];
    unsigned i, mantissaLen;
    unsigned char firstOctet;

    /* no contents for 0.0 reals */
    if (value == 0.0)
        return 0;

    /* special real values for +/- oo */
    if (value == MINUS_INFINITY)
    {
        b.PutByteRvs (ENC_MINUS_INFINITY);
        encLen = 1;
    }
    else if (value == PLUS_INFINITY)
    {
        b.PutByteRvs (ENC_PLUS_INFINITY);
        encLen = 1;
    }
    else  /* encode a binary real value */
    {
        /*
         * this is what frexp gets from value
         * value == mantissa * 2^exponent
         * where 0.5 <= |manitissa| < 1.0
         */
        mantissa = frexp (value, &exponent);

        /* set sign and make mantissa = | mantissa | */
        if (mantissa < 0.0)
        {
            sign = -1;
            mantissa *= -1;
        }
        else
            sign = 1;


        tmpMantissa = mantissa;

        /* convert mantissa into an unsigned integer */
        for (i = 0; i < sizeof (double); i++)
        {
            /* normalizied so shift 8 bits worth to the left of the decimal */
            tmpMantissa *= (1<<8);

            /* grab only (octet sized) the integer part */
            truncatedMantissa = (unsigned int) tmpMantissa;

            /* remove part to left of decimal now for next iteration */
            tmpMantissa -= truncatedMantissa;

            /* write into tmp buffer */
            buf[i] = truncatedMantissa;

            /* keep track of last non zero octet so can zap trailing zeros */
            if (truncatedMantissa)
                mantissaLen = i+1;
        }

        /*
         * write format octet  (first octet of content)
         *  field  1 S bb ff ee
         *  bit#   8 7 65 43 21
         *
         * 1 in bit#1 means binary rep
         * 1 in bit#2 means the mantissa is neg, 0 pos
         * bb is the base:    65  base
         *                    00    2
         *                    01    8
         *                    10    16
         *                    11    future ext.
         *
         * ff is the Value of F where  Mantissa = sign x N x 2^F
         *    FF can be one of 0 to 3 inclusive. (used to save re-alignment)
         *
         * ee is the length of the exponent:  21   length
         *                                    00     1
         *                                    01     2
         *                                    10     3
         *                                    11     long form
         *
         *
         * encoded binary real value looks like
         *
         *     fmt oct
         *   --------------------------------------------------------
         *   |1Sbbffee|  exponent (2's comp)  |   N (unsigned int)  |
         *   --------------------------------------------------------
         *    87654321
         */
        firstOctet = REAL_BINARY;
        if (sign == -1)
            firstOctet |= REAL_SIGN;

        /* bb is 00 since base is 2 so do nothing */
        /* ff is 00 since no other shifting is nec */

        /*
         * get exponent calculate its encoded length
         * Note that the process of converting the mantissa
         * double to an int shifted the decimal mantissaLen * 8
         * to the right - so correct that here
         */
        exponent -= (mantissaLen * 8);
        expLen = SignedIntOctetLen (exponent);

        switch (expLen)
        {
            case 1:
                firstOctet |= REAL_EXPLEN_1;
                break;
            case 2:
                firstOctet |= REAL_EXPLEN_2;
                break;
            case 3:
                firstOctet |= REAL_EXPLEN_3;
                break;
            default:
                firstOctet |= REAL_EXPLEN_LONG;
                break;
        }

        encLen = mantissaLen + expLen + 1;

        /* write the mantissa (N value) */
        b.PutSegRvs ((char*)buf, mantissaLen);

        /* write the exponent */
        for (i = expLen; i > 0; i--)
        {
            b.PutByteRvs (exponent);
            exponent >>= 8;
        }

        /* write the exponents length if nec */
        if (expLen > 3)
        {
            encLen++;
            b.PutByteRvs (expLen);
        }

        /* write the format octet */
        b.PutByteRvs (firstOctet);

    }
    return encLen;

}  /*  AsnReal:BEncContent */



#endif
#endif


// Decode a REAL value's content from the given buffer.
// places the result in this object.
void AsnReal::BDecContent (BUF_TYPE b, AsnTag tagId, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env)
{
    unsigned char firstOctet;
    unsigned char firstExpOctet;
    unsigned i;
    unsigned int expLen;
    double mantissa;
    unsigned short base;
    long int exponent = 0;
    double tmpBase;
    double tmpExp;


    if (elmtLen == 0)
    {
        value = 0.0;
        return;
    }

    firstOctet = b.GetByte();
    if (elmtLen == 1)
    {
        bytesDecoded += 1;
        if (firstOctet == ENC_PLUS_INFINITY)
            value = PLUS_INFINITY;
        else if (firstOctet == ENC_MINUS_INFINITY)
            value = MINUS_INFINITY;
        else
        {
            Asn1Error << "AsnReal::BDecContent: ERROR - unrecognized 1 octet length real number" << endl;
			#if SNACC_EXCEPTION_ENABLE
			SnaccExcep::throwMe(-18);
			#else
            longjmp (env, -18);
			#endif
        }
    }
    else
    {
        if (firstOctet & REAL_BINARY)
        {
            firstExpOctet = b.GetByte();
            if (firstExpOctet & 0x80)
                exponent = -1;
            switch (firstOctet & REAL_EXPLEN_MASK)
            {
                case REAL_EXPLEN_1:
                    expLen = 1;
                    exponent =  (exponent << 8) | firstExpOctet;
                    break;

                case REAL_EXPLEN_2:
                    expLen = 2;
                    exponent =  (exponent << 16) | (((unsigned long int) firstExpOctet) << 8) | b.GetByte();
                    break;

                case REAL_EXPLEN_3:
                    expLen = 3;
                    exponent =  (exponent << 16) | (((unsigned long int) firstExpOctet) << 8) | b.GetByte();
                    exponent =  (exponent << 8) | b.GetByte();
                    break;

                default:  /* long form */
                    expLen = firstExpOctet +1;
                    i = firstExpOctet-1;
                    firstExpOctet =  b.GetByte();
                    if (firstExpOctet & 0x80)
                        exponent = (-1 <<8) | firstExpOctet;
                    else
                        exponent = firstExpOctet;
                    for (;i > 0; firstExpOctet--)
                        exponent = (exponent << 8) | b.GetByte();
                    break;
            }

            mantissa = 0.0;
            for (i = 1 + expLen; i < elmtLen; i++)
            {
                mantissa *= (1<<8);
                mantissa +=  b.GetByte();
            }

            /* adjust N by scaling factor */
            mantissa *= (1<<((firstOctet & REAL_FACTOR_MASK) >> 2));

            switch (firstOctet & REAL_BASE_MASK)
            {
                case REAL_BASE_2:
                    base = 2;
                    break;

                case REAL_BASE_8:
                    base = 8;
                    break;

                case REAL_BASE_16:
                    base = 16;
                    break;

                default:
                    Asn1Error << "AsnReal::BDecContent: ERROR - unsupported base for a binary real number." << endl;
					#if SNACC_EXCEPTION_ENABLE
					SnaccExcep::throwMe(-19);
					#else
                    longjmp (env, -19);
					#endif
                    break;

            }

            tmpBase = base;
            tmpExp = exponent;

            value =  mantissa * pow ((double)base, (double)exponent);

            if (firstOctet & REAL_SIGN)
                value = -value;

            bytesDecoded += elmtLen;
        }
        else /* decimal version */
        {
            Asn1Error << "AsnReal::BDecContent: ERROR - decimal REAL form is not currently supported" << endl;
			#if SNACC_EXCEPTION_ENABLE
			SnaccExcep::throwMe(-20);
			#else
            longjmp (env, -20);
			#endif
        }
    }

} /* AsnInt::BDecContent */

AsnLen AsnReal::BEnc (BUF_TYPE b)
{
    AsnLen l;
    l =  BEncContent (b);
    l += BEncDefLen (b, l);
    l += BEncTag1 (b, UNIV, PRIM, REAL_TAG_CODE);
    return l;
}

void AsnReal::BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env)
{
    AsnLen elmtLen;
    if (BDecTag (b, bytesDecoded, env) != MAKE_TAG_ID (UNIV, PRIM, REAL_TAG_CODE))
    {
	Asn1Error << "AsnReal::BDec: ERROR tag on REAL is wrong." << endl;
	#if SNACC_EXCEPTION_ENABLE
	SnaccExcep::throwMe(-58);
	#else
	longjmp (env,-58);
	#endif
    }
    elmtLen = BDecLen (b, bytesDecoded, env);

    BDecContent (b, MAKE_TAG_ID (UNIV, PRIM, REAL_TAG_CODE), elmtLen, bytesDecoded, env);
}

void AsnReal::Print (ostream &os) const
{
#ifndef	NDEBUG
  os << value;
#endif
}

#if META

const AsnRealTypeDesc AsnReal::_desc (NULL, NULL, false, AsnTypeDesc::REAL, NULL);

const AsnTypeDesc *AsnReal::_getdesc() const
{
  return &_desc;
}

#if TCL

int AsnReal::TclGetVal (Tcl_Interp *interp) const
{
  if (value == PLUS_INFINITY)
    strcpy (interp->result, "+inf");
  else if (value == MINUS_INFINITY)
    strcpy (interp->result, "-inf");
  else
    sprintf (interp->result, "%g", value);
  return TCL_OK;
}

int AsnReal::TclSetVal (Tcl_Interp *interp, const char *valstr)
{
  double valval;

  if (!strcmp (valstr, "+inf"))
    valval = PLUS_INFINITY;
  else if (!strcmp (valstr, "-inf"))
    valval = MINUS_INFINITY;
  else if (Tcl_GetDouble (interp, (char*)valstr, &valval) != TCL_OK)
    return TCL_ERROR;

  value = valval;

  return TCL_OK;
}

#endif /* TCL */
#endif /* META */
