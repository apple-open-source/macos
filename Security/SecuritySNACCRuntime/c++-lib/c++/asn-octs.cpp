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


// file: .../c++-lib/src/asn-octs.C - ASN.1 OCTET STRING methods for AsnOcts Class
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
// $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-lib/c++/asn-octs.cpp,v 1.2 2001/06/27 23:09:15 dmitch Exp $
// $Log: asn-octs.cpp,v $
// Revision 1.2  2001/06/27 23:09:15  dmitch
// Pusuant to Radar 2664258, avoid all cerr-based output in NDEBUG configuration.
//
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.3  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.2  2000/12/07 22:16:14  dmitch
// Thread-safe mods: added a local StrStk strStkG to the routines which need it.
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
// Revision 1.4  1999/04/08 21:21:25  aram
// Fixed AsnOcts destructor.
//
// Revision 1.3  1999/03/21 02:07:36  mb
// Added Copy to every AsnType.
//
// Revision 1.2  1999/02/26 00:23:40  mb
// Fixed for Mac OS 8
//
// Revision 1.1  1999/02/25 05:21:53  mb
// Added snacc c++ library
//
// Revision 1.6  1997/02/28 13:39:45  wan
// Modifications collected for new version 1.3: Bug fixes, tk4.2.
//
// Revision 1.5  1995/08/17 15:33:33  rj
// augment binify()'s error message
//
// Revision 1.4  1995/07/24  20:23:32  rj
// use memcmpeq that is defined in .../snacc.h to use either memcmp or bcmp.
//
// code extracted from AsnOcts::TclGetVal and AsnOcts::TclSetVal into ::debinify and ::binify in meta.C
//
// #if TCL ... #endif wrapped into #if META ... #endif
//
// call constructor with additional pdu and create arguments.
//
// changed `_' to `-' in file names.
//
// Revision 1.3  1994/10/08  04:18:27  rj
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
// Revision 1.2  1994/08/28  10:01:16  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:21:05  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#include "asn-config.h"
#include "asn-len.h"
#include "asn-tag.h"
#include "asn-type.h"
#include "asn-bits.h" /* for TO_HEX */
#include "asn-octs.h"
#include "str-stk.h"

#ifndef	__APPLE__
extern StrStk strStkG;
#endif

AsnOcts::~AsnOcts()
{
  Asn1Free( octs );
}

AsnType *AsnOcts::Clone() const
{
  return new AsnOcts;
}

AsnType *AsnOcts::Copy() const
{
  return new AsnOcts (*this);
}

// Initialize the AsnOcts with a char * and length.
// copies the string str.
void AsnOcts::Set (const char *str, size_t len)
{
    octetLen = len;
#ifndef _IBM_ENC_
    octs = new char[octetLen];
#else
    octs = (char *) mem_mgr_ptr->Get (octetLen+1);          /* Guido Grassel, 11.8.93 */
#endif /* _IBM_ENC_ */
    memcpy (octs, str, octetLen);
}

// Initialize the AsnOcts from another AsnOcts
// copies the string from o.
void AsnOcts::Set (const AsnOcts &o)
{
    if (&o != this) // avoid o = o;
    {
        octetLen = o.octetLen;
#ifndef _IBM_ENC_
        octs = new char[octetLen];
#else
	octs = (char *) mem_mgr_ptr->Get (octetLen+1);         /* Guido Grassel, 11.8.93 */
#endif /* _IBM_ENC_ */
        memcpy (octs, o.octs, octetLen);
    }
}

// Initialize the AsnOcts from a Null terminated string.
// copies the string str.
void AsnOcts::Set (const char *str)
{
    octetLen = strlen (str);
#ifndef _IBM_ENC_
    octs = new char[octetLen];
#else
    octs = (char *) mem_mgr_ptr->Get (octetLen+1);         /* Guido Grassel, 11.8.93 */
#endif /* _IBM_ENC_ */
    memcpy (octs, str, octetLen);
}


// free old octs value and tehn set the new
// octs and octetLen values with the given char *and length.
// copies the string str.
void AsnOcts::ReSet (const char *str, size_t len)
{
    if (str != octs)
    {
#ifndef _IBM_ENC_
        delete octs;
        Set (str, len);
#else
      mem_mgr_ptr->Put ((void *)octs);                /* Guido Grassel, 11.8.93 */
      Set (str, len);
#endif /* _IBM_ENC_ */
    }
}

// Free old octs value and then set the new
// octs and octetLen from the given AsnOcts
// copies the string in o.
void AsnOcts::ReSet (const AsnOcts &o)
{
    if (&o != this) // avoid s = s;
    {
#ifndef _IBM_ENC_
        delete octs;
        Set (o);
#else
      mem_mgr_ptr->Put ((void *)octs);               /* Guido Grassel, 11.8.93 */
      Set (o);
#endif /* _IBM_ENC_ */
    }
}

// Free old octs value and then set the new
// octs and octetLen values from a null terminated string.
// copies the string str.
void AsnOcts::ReSet (const char *str)
{
    if (str != octs)
    {
#ifndef _IBM_ENC_
        delete octs;
        Set (str);
#else
      mem_mgr_ptr->Put ((void *)octs);                /* Guido Grassel, 11.8.93 */
      Set (str);
#endif /* _IBM_ENC_ */
    }
}


// Prints the AsnOcts to the given ostream in Value Notation.
void AsnOcts::Print (ostream &os) const
{
#ifndef	NDEBUG
    int i;
    os << "'";
    for (i = 0; i < octetLen; i++)
        os << TO_HEX (octs[i] >> 4) << (TO_HEX (octs[i]));

    os << "'H  -- \"";

    /* put printable parts in ASN.1 comment */
    for (i = 0; i < octetLen; i++)
    {
        if (isspace (octs[i]))
            os << " ";  /* newlines->space (so don't screw up ASN.1 comment) */
        else if (isprint (octs[i]))
            os <<octs[i];
        else
            os << ".";
    }
    os << "\" --";
#endif	/* NDEBUG */
} /* AsnOcts::Print */


AsnLen AsnOcts::BEncContent (BUF_TYPE b)
{
    b.PutSegRvs (octs, octetLen);
    return octetLen;
}

// Decodes a BER OCTET STRING value and puts it in this object.
// Constructed OCTET STRINGs are always concatenated into primitive ones.
void AsnOcts::BDecContent (BUF_TYPE b, AsnTag tagId, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env)
{
    /*
     * tagId is encoded tag shifted into long int.
     * if CONS bit is set then constructed octet string
     */
    if (tagId & 0x20000000)
        BDecConsOcts (b, elmtLen, bytesDecoded, env);

    else /* primitive octet string */
    {
        octetLen = elmtLen;
#ifndef _IBM_ENC_
        octs =  Asn1Alloc (elmtLen+1);
#else
      octs = (char *) mem_mgr_ptr->Get (elmtLen+1);                /* Guido Grassel, 11.8.93 */
#endif /* _IBM_ENC_ */
        b.CopyOut (octs, elmtLen);

        if (b.ReadError())
        {
            Asn1Error << "BDecOctetString: ERROR - decoded past end of data" << endl;
            longjmp (env, -14);
        }

        /* add null terminator - this is not included in the str's len */
        octs[elmtLen] = '\0';
        bytesDecoded += elmtLen;
    }

} /* AsnOcts::BDecContent */

AsnLen AsnOcts::BEnc (BUF_TYPE b)
{
    AsnLen l;
    l =  BEncContent (b);
    l += BEncDefLen (b, l);
    l += BEncTag1 (b, UNIV, PRIM, OCTETSTRING_TAG_CODE);
    return l;
}

void AsnOcts::BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env)
{
    AsnLen elmtLen;
    AsnTag tag;

    tag = BDecTag (b, bytesDecoded, env);
    if ((tag != MAKE_TAG_ID (UNIV, PRIM, OCTETSTRING_TAG_CODE)) &&
	(tag != MAKE_TAG_ID (UNIV, CONS, OCTETSTRING_TAG_CODE)))
    {
	Asn1Error << "AsnOcts::BDec: ERROR tag on OCTET STRING is wrong." << endl;
	longjmp (env,-56);
    }
    elmtLen = BDecLen (b, bytesDecoded, env);
    BDecContent (b, tag, elmtLen, bytesDecoded, env);
}



/*
 * Used for concatenating constructed OCTET STRING values when decoding
 *
 * fills string stack with references to the pieces of a
 * construced octet string
 */
#ifdef __APPLE__
static void
FillOctetStringStk (BUF_TYPE b, AsnLen elmtLen0, AsnLen &bytesDecoded, ENV_TYPE env, StrStk &strStkG)
#else
static void
FillOctetStringStk (BUF_TYPE b, AsnLen elmtLen0, AsnLen &bytesDecoded, ENV_TYPE env)
#endif
{
    size_t refdLen;
    size_t totalRefdLen;
    char *strPtr;
    AsnLen totalElmtsLen1 = 0;
    unsigned long int tagId1;
    AsnLen elmtLen1;

    for (; (totalElmtsLen1 < elmtLen0) || (elmtLen0 == INDEFINITE_LEN);)
    {
        tagId1 = BDecTag (b, totalElmtsLen1, env);

        if ((tagId1 == EOC_TAG_ID) && (elmtLen0 == INDEFINITE_LEN))
        {
            BDEC_2ND_EOC_OCTET (b, totalElmtsLen1, env);
            break;
        }

        elmtLen1 = BDecLen (b, totalElmtsLen1, env);
        if (tagId1 == MAKE_TAG_ID (UNIV, PRIM, OCTETSTRING_TAG_CODE))
        {
            /*
             * primitive part of string, put references to piece (s) in
             * str stack
             */
            totalRefdLen = 0;
            refdLen = elmtLen1;
            while (1)
            {
                strPtr = b.GetSeg (&refdLen);

                strStkG.Push (strPtr, refdLen);
                totalRefdLen += refdLen;
                if (totalRefdLen == elmtLen1)
                    break; /* exit this while loop */

                if (refdLen == 0) /* end of data */
                {
                    Asn1Error << "BDecConsOctetString: ERROR - attempt to decode past end of data" << endl;
                    longjmp (env, -15);
                }
                refdLen = elmtLen1 - totalRefdLen;
            }
            totalElmtsLen1 += elmtLen1;
        }


        else if (tagId1 == MAKE_TAG_ID (UNIV, CONS, OCTETSTRING_TAG_CODE))
        {
            /*
             * constructed octets string embedding in this constructed
             * octet string. decode it.
             */
			#ifdef	__APPLE__
            FillOctetStringStk (b, elmtLen1, totalElmtsLen1, env,
				strStkG);
			#else
            FillOctetStringStk (b, elmtLen1, totalElmtsLen1, env);
			#endif
        }
        else  /* wrong tag */
        {
            Asn1Error << "BDecConsOctetString: ERROR - decoded non-OCTET STRING tag inside a constructed OCTET STRING" << endl;
            longjmp (env, -16);
        }
    } /* end of for */

    bytesDecoded += totalElmtsLen1;

}  /* AsnOcts::FillOctetStringStk */



/*
 * decodes a seq of universally tagged octets until either EOC is
 * encountered or the given len decoded.  Return them in a
 * single concatenated octet string
 */
void AsnOcts::BDecConsOcts (BUF_TYPE b, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env)
{
	#ifdef	__APPLE__
	StrStk strStkG(128, 64);
	#endif
    strStkG.Reset();

    /*
     * decode each piece of the octet string, puting
     * an entry in the octet/bit string stack for each
     */
	#ifdef	__APPLE__
    FillOctetStringStk (b, elmtLen, bytesDecoded, env, strStkG);
	#else
    FillOctetStringStk (b, elmtLen, bytesDecoded, env);
	#endif
	
    /* alloc single str long enough for combined octetstring */
    octetLen = strStkG.totalByteLen;

#ifndef _IBM_ENC_
    octs = Asn1Alloc (octetLen);
#else
    octs = (char *) mem_mgr_ptr->Get (octetLen);                /* Guido Grassel, 11.8.93 */
#endif /* _IBM_ENC_ */

    strStkG.CopyOut (octs);

}  /* BDecConsOcts */

int AsnOcts::OctsEquiv (const AsnOcts &o) const
{
    return o.octetLen == octetLen && !memcmpeq (o.octs, octs, octetLen);
}

#if META

const AsnOctsTypeDesc AsnOcts::_desc (NULL, NULL, false, AsnTypeDesc::OCTET_STRING, NULL);

const AsnTypeDesc *AsnOcts::_getdesc() const
{
  return &_desc;
}

#if TCL

int AsnOcts::TclGetVal (Tcl_Interp *interp) const
{
  return debinify (interp, octs, octetLen);
}

int AsnOcts::TclSetVal (Tcl_Interp *interp, const char *valstr)
{
  char*		buf;
  size_t	len;
  int		result = TCL_OK;

#ifndef _IBM_ENC_
    buf = new char[strlen (valstr)];
#else
    buf = (char *) mem_mgr_ptr->Get (strlen (valstr)+1);
#endif /* _IBM_ENC_ */

  if (!buf)
  {
    Tcl_AppendResult (interp, "Malloc failed for type ", _getdesc()->getmodule()->name, ".", _getdesc()->getname(), NULL);
    return TCL_ERROR;
  }
  if (binify (interp, valstr, buf, &len) != TCL_OK)
  {
    Tcl_AppendResult (interp, " for type ", _getdesc()->getmodule()->name, ".", _getdesc()->getname(), NULL);
    result = TCL_ERROR;
  }
  else
    ReSet (buf, len);

#ifndef _IBM_ENC_
  delete buf;
#else
  mem_mgr_ptr->Put ((void*) buf);
#endif /* _IBM_ENC_ */

  return result;
}

#endif /* TCL */
#endif /* META */
