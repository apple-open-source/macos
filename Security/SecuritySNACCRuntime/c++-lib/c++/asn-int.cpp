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


// file: .../c++-lib/src/asn-int.C - methods for AsnInt (ASN.1 INTEGER) class
//
// MS 92/06/16
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
// $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-lib/c++/asn-int.cpp,v 1.2 2001/06/27 23:09:14 dmitch Exp $
// $Log: asn-int.cpp,v $
// Revision 1.2  2001/06/27 23:09:14  dmitch
// Pusuant to Radar 2664258, avoid all cerr-based output in NDEBUG configuration.
//
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
// Revision 1.2  1999/03/21 02:07:36  mb
// Added Copy to every AsnType.
//
// Revision 1.1  1999/02/25 05:21:51  mb
// Added snacc c++ library
//
// Revision 1.7  1997/02/28 13:39:45  wan
// Modifications collected for new version 1.3: Bug fixes, tk4.2.
//
// Revision 1.6  1995/09/07 18:55:50  rj
// (unsigned) long int replaced by newly introduced Asn(U)IntType at a lot of places.
// they shall provide 32 bit integer types on all platforms.
//
// Revision 1.5  1995/07/24  20:17:32  rj
// #if TCL ... #endif wrapped into #if META ... #endif
//
// call constructor with additional pdu and create arguments.
//
// changed `_' to `-' in file names.
//
// Revision 1.4  1995/02/18  16:48:05  rj
// denote a long if we want a long
//
// Revision 1.3  1994/10/08  04:18:23  rj
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
// Revision 1.2  1994/08/28  10:01:12  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:59  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#include "asn-config.h"
#include "asn-len.h"
#include "asn-tag.h"
#include "asn-type.h"
#include "asn-int.h"

AsnType *AsnInt::Clone() const
{
  return new AsnInt;
}

AsnType *AsnInt::Copy() const
{
  return new AsnInt (*this);
}

// Encodes BER content of this AsnInt to the given buffer.
// Returns the number of octets written to the buffer.
AsnLen AsnInt::BEncContent (BUF_TYPE b)
{
    AsnLen		len;
    int			i;
    AsnUIntType	mask;
    AsnUIntType	dataCpy;

#define MASK	(0x7f80L << ((sizeof (AsnIntType) - 2) * 8))

    dataCpy = value;

    /*
     * calculate encoded length of the integer (content)
     */
    mask = MASK;
    if ((AsnIntType)dataCpy < 0)
        for (len = sizeof (AsnIntType); len > 1; --len)
        {
            if ((dataCpy & mask) == mask)
                mask >>= 8;
            else
                break;
        }
    else
        for (len = sizeof (AsnIntType); len > 1; --len)
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
        b.PutByteRvs (dataCpy);
        dataCpy >>= 8;
    }

    return len;
}

// Decodes the content of a BER INTEGER from the given buffer.
// The value is placed in this object.  tagId is ignored.
// bytesDecoded is incremented by the number of bytes read for this
// integer value.
void AsnInt::BDecContent (BUF_TYPE b, AsnTag tagId, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env)
{
    int   i;
    AsnUIntType byte;

    if (elmtLen > sizeof (AsnIntType))
    {
        Asn1Error << "AsnInt::BDecContent: ERROR - integer is too big to decode." << endl;
        longjmp (env, -7);
    }

    /*
     * look at integer value
     */
    byte =  (AsnUIntType) b.GetByte();

    if (byte & 0x80)   /* top bit of first byte is sign bit */
        value = (-1 << 8) | byte;
    else
        value = byte;

    /*
     * write from buffer into AsnIntType
     */
    for (i = 1; i < elmtLen; i++)
        value = (value << 8) | (AsnUIntType)(b.GetByte());

    bytesDecoded += elmtLen;

    if (b.ReadError())
    {
        Asn1Error << "AsnInt::BDecContent: ERROR - decoded past end of data." << endl;
        longjmp (env, -8);
    }


}  /* AsnInt::BDecContent */

AsnLen AsnInt::BEnc (BUF_TYPE b)
{
    AsnLen l;
    l = BEncContent (b);
    BEncDefLenTo127 (b, l);
    l++;
    l += BEncTag1 (b, UNIV, PRIM, INTEGER_TAG_CODE);
    return l;
}

void AsnInt::BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env)
{
    AsnLen elmtLen;
    if (BDecTag (b, bytesDecoded, env) != MAKE_TAG_ID (UNIV, PRIM, INTEGER_TAG_CODE))
    {
	Asn1Error << "AsnInt::BDec: ERROR tag on INTEGER is wrong." << endl;
	longjmp (env,-53);
    }

    elmtLen = BDecLen (b, bytesDecoded, env);
    BDecContent (b, MAKE_TAG_ID (UNIV, PRIM, INTEGER_TAG_CODE), elmtLen, bytesDecoded, env);
}

void AsnInt::Print (ostream &os) const
{
#ifndef	NDEBUG
    os << value;
#endif
}

#if META

const AsnIntTypeDesc AsnInt::_desc (NULL, NULL, false, AsnTypeDesc::INTEGER, NULL, NULL);

const AsnTypeDesc *AsnInt::_getdesc() const
{
  return &_desc;
}

#if TCL

#define RETURN_NAME_INSTEAD_OF_VALUE	0

int AsnInt::TclGetVal (Tcl_Interp *interp) const
{
#if RETURN_NAME_INSTEAD_OF_VALUE
  const AsnNameDesc *n = _getdesc()->getnames();
  if (n)
    for (; n->name; n++)
      if (n->value == value)
      {
	Tcl_SetResult (interp, n->name, TCL_STATIC);
	return TCL_OK;
      }
#endif

  char buf[32];
  sprintf (buf, "%d", value);
  Tcl_SetResult (interp, buf, TCL_VOLATILE);
  return TCL_OK;
}

int AsnInt::TclSetVal (Tcl_Interp *interp, const char *valstr)
{
  const AsnNameDesc *n = _getdesc()->getnames();
  if (n)
    for (; n->name; n++)
      if (!strcmp (n->name, valstr))
      {
	value = n->value;
	return TCL_OK;
      }

  int valval;
  if (Tcl_GetInt (interp, (char*)valstr, &valval) != TCL_OK)
    return TCL_ERROR;
  value = valval;
  return TCL_OK;
}

#endif /* TCL */
#endif /* META */
