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


// file: .../c++-lib/src/asn-len.C - ASN.1 Length manipluation routines
//
// MS 92/06/18
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
// $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-lib/c++/asn-len.cpp,v 1.1.1.1 2001/05/18 23:14:05 mb Exp $
// $Log: asn-len.cpp,v $
// Revision 1.1.1.1  2001/05/18 23:14:05  mb
// Move from private repository to open source repository
//
// Revision 1.2  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.1  2000/06/15 18:44:57  dmitch
// These snacc-generated source files are now checked in to allow cross-platform build.
//
// Revision 1.2  2000/06/08 20:05:35  dmitch
// Mods for X port. These files are actually machine generated and probably don't need to be in CVS....
//
// Revision 1.1.1.1  2000/03/09 01:00:06  rmurphy
// Base Fortissimo Tree
//
// Revision 1.1  1999/02/25 05:21:51  mb
// Added snacc c++ library
//
// Revision 1.5  1997/02/16 20:26:04  rj
// check-in of a few cosmetic changes
//
// Revision 1.4  1995/07/24  20:33:15  rj
// changed `_' to `-' in file names.
//
// Revision 1.3  1994/10/08  04:18:24  rj
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
// Revision 1.2  1994/08/28  10:01:13  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:21:00  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#include "asn-config.h"
#include "asn-len.h"


/*
 * Encodes the given length to the given buffer.
 * returns the number of octets written to the buffer.
 */
AsnLen
BEncDefLen (BUF_TYPE  b, AsnLen len)
{
    /*
     * unrolled for efficiency
     * (check each possibitlity of the 4 byte integer)
     */
    if (len < 128)
    {
        b.PutByteRvs (len);
        return 1;
    }
    else if (len < 256)
    {
        b.PutByteRvs (len);
        b.PutByteRvs (0x81);
        return 2;
    }
    else if (len < 65536)
    {
        b.PutByteRvs (len);
        b.PutByteRvs (len >> 8);
        b.PutByteRvs (0x82);
        return 3;
    }
    else if (len < 16777126)
    {
        b.PutByteRvs (len);
        b.PutByteRvs (len >> 8);
        b.PutByteRvs (len >> 16);
        b.PutByteRvs (0x83);
        return 4;
    }
    else
    {
        b.PutByteRvs (len);
        b.PutByteRvs (len >> 8);
        b.PutByteRvs (len >> 16);
        b.PutByteRvs (len >> 24);
        b.PutByteRvs (0x84);
        return 5;
    }
} /*  EncodeDefLen */

/*
 * Decode a BER length from the given buffer. Increments bytesDecoded
 * by the number of octets of the encoded length.  Flags an
 * error if the length is too large or a read error occurs
 */
AsnLen
BDecLen (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env)
{
    AsnLen  len;
    unsigned char  byte;
    int  lenBytes;

    byte = b.GetByte();

    if (b.ReadError())
    {
        Asn1Error << "BDecLen: decoded past end of data" << endl;
        longjmp (env, -9);
    }

    bytesDecoded++;
    if (byte < 128)   /* short length */
        return byte;

    else if (byte == (unsigned char) 0x080)  /* indef len indicator */
        return INDEFINITE_LEN;

    else  /* long len form */
    {
        /*
         * strip high bit to get # bytes left in len
         */
        lenBytes = byte & (unsigned char) 0x7f;

        if (lenBytes > sizeof (long int))
        {
            Asn1Error << "BDecLen: ERROR - length overflow" << endl;
            longjmp (env, -10);
        }

        bytesDecoded += lenBytes;

        for (len = 0; lenBytes > 0; lenBytes--)
            len = (len << 8) | (unsigned long int) b.GetByte();


        if (b.ReadError())
        {
            Asn1Error << "BDecLen: decoded past end of data" << endl;
            longjmp (env, -11);
        }

        return len;
    }
    /* not reached */
}


/*
 * Encodes an End of Contents (EOC) to the given buffer.
 * Returns the encoded length.
 */
AsnLen
BEncEoc (BUF_TYPE b)
{

    b.PutByteRvs (0);
    b.PutByteRvs (0);
    return 2;
}  /* BEncEoc */

/*
 * Decodes an EOC from the given buffer.  flags an error if the
 * octets are non-zero or if read error occured.
 */
void
BDecEoc (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env)
{

    if ((b.GetByte() != 0) || (b.GetByte() != 0) || b.ReadError())
    {
        Asn1Error << "BDecEoc: ERROR - non zero byte in EOC or end of data reached" << endl;
        longjmp (env, -12);
    }
    bytesDecoded += 2;
}  /* BDecEoc */
