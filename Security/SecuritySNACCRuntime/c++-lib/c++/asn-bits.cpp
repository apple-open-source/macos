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


// file: .../c++-lib/src/asn-bits.C - AsnBits (ASN.1 BIT STRING) Type
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
// $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-lib/c++/asn-bits.cpp,v 1.3 2001/06/28 23:36:11 dmitch Exp $
// $Log: asn-bits.cpp,v $
// Revision 1.3  2001/06/28 23:36:11  dmitch
// Removed SccsId statics. numToHexCharTblG table now const. Radar 2705410.
//
// Revision 1.2  2001/06/27 23:09:14  dmitch
// Pusuant to Radar 2664258, avoid all cerr-based output in NDEBUG configuration.
//
// Revision 1.1.1.1  2001/05/18 23:14:05  mb
// Move from private repository to open source repository
//
// Revision 1.3  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.2  2000/12/07 22:15:49  dmitch
// Thread-safe mods: added a local StrStk strStkG to the routines which need it.
//
// Revision 1.1  2000/06/15 18:44:57  dmitch
// These snacc-generated source files are now checked in to allow cross-platform build.
//
// Revision 1.2  2000/06/08 20:05:34  dmitch
// Mods for X port. These files are actually machine generated and probably don't need to be in CVS....
//
// Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
// Base Fortissimo Tree
//
// Revision 1.3  1999/03/21 02:07:35  mb
// Added Copy to every AsnType.
//
// Revision 1.2  1999/02/26 00:23:39  mb
// Fixed for Mac OS 8
//
// Revision 1.1  1999/02/25 05:21:50  mb
// Added snacc c++ library
//
// Revision 1.9  1997/08/27 15:55:15  wan
// GetBit now returns 0 or 1, not 0 or <#bit>, even if bool type is emulated.
//
// Revision 1.8  1997/02/28 13:39:44  wan
// Modifications collected for new version 1.3: Bug fixes, tk4.2.
//
// Revision 1.7  1997/01/01 20:19:01  rj
// dereferencing pointer to member function is neither necessary nor allowed
//
// Revision 1.6  1995/08/17  15:37:49  rj
// set Tcl's errorCode variable
//
// Revision 1.5  1995/07/24  20:09:07  rj
// use memzero that is defined in .../snacc.h to use either memset or bzero.
// use memcmpeq that is defined in .../snacc.h to use either memcmp or bcmp.
//
// call constructor with additional pdu and create arguments.
//
// #if TCL ... #endif wrapped into #if META ... #endif
//
// changed `_' to `-' in file names.
//
// Revision 1.4  1994/10/08  04:18:21  rj
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
// Revision 1.3  1994/08/31  23:37:57  rj
// TRUE turned into true
//
// Revision 1.2  1994/08/28  10:01:11  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:57  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#include "asn-config.h"
#include "asn-len.h"
#include "asn-tag.h"
#include "asn-type.h"
#include "asn-bits.h"
#include "str-stk.h"

#ifndef	__APPLE__

/* OS X - thread safe - the only routine which uses this allocates
 * it on the stack */
extern StrStk strStkG;
unsigned short int  strStkUnusedBitsG;
#endif	/* __APPLE__ */

const
char numToHexCharTblG[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

AsnBits::~AsnBits()
{
  delete bits;
}

AsnType *AsnBits::Clone() const
{
  return new AsnBits;
}

AsnType *AsnBits::Copy() const
{
  return new AsnBits (*this);
}

// Initializes the bits string with a bit string numBits in length.
// All bits are zeroed.
void AsnBits::Set (size_t numBits)
{
  bitLen = numBits;
  size_t octetLen = (bitLen+7)/8;

#ifndef _IBM_ENC_
  bits = Asn1Alloc (octetLen);
#else
  bits = (char *) mem_mgr_ptr->Get (octetLen);       // Guido Grassel, 11.8.93
#endif /* _IBM_ENC_ */
  memzero (bits, octetLen); // init to zeros
}

// initializes a BIT STRING with the given string and bit length
// Copies the bits from bitsOcts.
void AsnBits::Set (const char *bitOcts, size_t numBits)
{
    if (bitOcts != bits)
    {
        bitLen = numBits;
	size_t octetLen = (bitLen+7)/8;
#ifndef _IBM_ENC_
        bits = new char[octetLen];
#else
	bits = (char *) mem_mgr_ptr->Get (octetLen);       // Guido Grassel, 11.8.93
#endif /* _IBM_ENC_ */
        memcpy (bits, bitOcts, octetLen);
    }
}

// initializes a BIT STRING by copying another BIT STRING's bits
void AsnBits::Set (const AsnBits &b)
{
  if (&b != this)
  {
    bitLen = b.bitLen;
    size_t octetLen = (bitLen+7)/8;
#ifndef _IBM_ENC_
    bits = new char[octetLen];
#else
    bits = (char *) mem_mgr_ptr->Get (octetLen);       // Guido Grassel, 11.8.93
#endif /* _IBM_ENC_ */
    memcpy (bits, b.bits, octetLen);
  }
}

// Initializes the bits string with a bit string numBits in length.
// All bits are zeroed.
void AsnBits::ReSet (size_t numBits)
{
#ifndef _IBM_ENC_
  delete bits;
  Set (numBits);
#else
  mem_mgr_ptr->Put ((void *) bits);       // Guido Grassel, 11.8.93
  Set (numBits);
#endif /* _IBM_ENC_ */
}

// frees old bits value and then re-initializes the
// BIT STRING with the given string and bit length
// Copies the bitOcts into bits.
void AsnBits::ReSet (const char *bitOcts, size_t numBits)
{
  if (bitOcts != bits)
  {
#ifndef _IBM_ENC_
    delete bits;
    Set (bitOcts, numBits);
#else
    mem_mgr_ptr->Put ((void *) bits);       // Guido Grassel, 11.8.93
    Set (bitOcts, numBits);
#endif /* _IBM_ENC_ */
  }
}

// frees old bits value and then re-initializes the
// BIT STRING by copying another BIT STRING's bits
void AsnBits::ReSet (const AsnBits &b)
{
  if (&b != this)  // avoid b = b; probs
  {
#ifndef _IBM_ENC_
    delete bits;
    Set (b);
#else
    mem_mgr_ptr->Put ((void *) bits);       // Guido Grassel, 11.8.93
    Set (b);
#endif /* _IBM_ENC_ */
  }
}


// Returns true if the given BIT STRING is the same as this one
bool AsnBits::BitsEquiv (const AsnBits &ab) const
{
  size_t octetsLessOne = (bitLen-1)/8;
  size_t octetBits = 7 - (bitLen % 8);

  if (!bitLen && !ab.bitLen)
    return true;

  // trailing bits may not be significant
  return bitLen == ab.bitLen
    && !memcmpeq (bits, ab.bits, octetsLessOne)
    && (bits[octetsLessOne] & (0xFF << octetBits)) == (ab.bits[octetsLessOne] & (0xFF << octetBits));
}  /* AsnBits::BitsEquiv */


// set given bit to 1. Most signif. bit is bit 0, least signif bit is bitLen-1
void AsnBits::SetBit (size_t bit)
{
  if (bit < bitLen)
  {
    size_t octet = bit/8;
    size_t octetsBit = 7 - (bit % 8);	// bit zero is first/most sig bit in octet
    bits[octet] |= 1 << octetsBit;
  }
#ifdef DEBUG
  else
    Asn1Errror << "AsnBits::SetBit: ERROR - bit larger than bit string" << endl;
#endif
} /* AsnBits::SetBit */

// Clr bit. Most signif. bit is bit 0, least signif bit is bitLen-1
void AsnBits::ClrBit (size_t bit)
{
  if (bit < bitLen)
  {
    size_t octet = bit/8;
    size_t octetsBit = 7 - (bit % 8);	// bit zero is first/most sig bit in octet
    bits[octet] &= ~(1 << octetsBit);
  }
#ifdef DEBUG
  else
    Asn1Errror << "AsnBits::ClrBit: ERROR - bit larger than bit string" << endl;
#endif
} /* AsnBits::ClrBit */

// returns given bit. Most signif. bit is bit 0, least signif bit is bitLen-1.
// Returns false if the givnen bit index is out of range.
bool AsnBits::GetBit (size_t bit) const
{
  if (bit < bitLen)
  {
    size_t octet = bit/8;
    size_t octetsBit = 7 - (bit % 8);	// bit zero is first/most sig bit in octet
    return !!(bits[octet] & (1 << octetsBit));
  }
#ifdef DEBUG
  else
    Asn1Errror << "AsnBits::GetBit: ERROR - bit larger than bit string" << endl;
#endif

  return false;
}  /* AsnBits::GetBit */


// Encoded the content (included unused bits octet) of the BIT STRING
// to the given buffer.
AsnLen AsnBits::BEncContent (BUF_TYPE b)
{
    size_t byteLen = (bitLen+7)/8;
    b.PutSegRvs (bits, byteLen);

    size_t unusedBits = (bitLen % 8);
    if (unusedBits != 0)
        unusedBits = 8 - unusedBits;
    b.PutByteRvs (unusedBits);

    return byteLen + 1;

} /* AsnBits::BEncContent */


// Decodes a BER BIT STRING from the given buffer and stores
// the value in this object.
void AsnBits::BDecContent (BUF_TYPE b, AsnTag tagId, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env)
{
//    char *tmp;

    /*
     * tagId is encoded tag shifted into long int.
     * if CONS bit is set then constructed bit string
     */
    if (tagId & 0x20000000)
        BDecConsBits (b, elmtLen, bytesDecoded, env);

    else /* primitive octet string */
    {
        bytesDecoded += elmtLen;
        elmtLen--;
        bitLen = (elmtLen * 8) - (unsigned int)b.GetByte();
#ifndef _IBM_ENC_
        bits =  Asn1Alloc (elmtLen);
#else
	bits = (char *) mem_mgr_ptr->Get (elmtLen);       // Guido Grassel, 11.8.93
#endif /* _IBM_ENC_ */
        b.CopyOut (bits, elmtLen);
        if (b.ReadError())
        {
            Asn1Error << "BDecBitString: ERROR - decoded past end of data" << endl;
            longjmp (env, -1);
        }
    }

} /* AsnBits::BDecContent */

AsnLen AsnBits::BEnc (BUF_TYPE b)
{
    AsnLen l;
    l =  BEncContent (b);
    l += BEncDefLen (b, l);
    l += BEncTag1 (b, UNIV, PRIM, BITSTRING_TAG_CODE);
    return l;
}

void AsnBits::BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env)
{
    AsnLen elmtLen;
    AsnTag tag;

    tag = BDecTag (b, bytesDecoded, env);
    if ((tag != MAKE_TAG_ID (UNIV, PRIM, BITSTRING_TAG_CODE))
      && (tag != MAKE_TAG_ID (UNIV, CONS, BITSTRING_TAG_CODE)))
    {
	Asn1Error << "AsnBits::BDec: ERROR tag on BIT STRING is wrong." << endl;
	longjmp (env,-50);
    }
    elmtLen = BDecLen (b, bytesDecoded, env);
    BDecContent (b, tag, elmtLen, bytesDecoded, env);
}

/*
 * Used to concatentate constructed bit strings when decoding.
 *
 * fills string stack with references to the pieces of a
 * construced bit string. sets strStkUnusedBitsG appropriately.
 * and strStkTotalByteLenG to bytelen needed to hold the bitstring
 */
#ifdef	__APPLE__
void AsnBits::FillBitStringStk (BUF_TYPE b, AsnLen elmtLen0, 
	AsnLen &bytesDecoded, ENV_TYPE env,
	StrStk &strStkG,
	unsigned short int  &strStkUnusedBitsG)
#else
void AsnBits::FillBitStringStk (BUF_TYPE b, AsnLen elmtLen0, 
	AsnLen &bytesDecoded, ENV_TYPE env)
#endif
{
    size_t refdLen;
    size_t totalRefdLen;
    char *strPtr;
    AsnLen totalElmtsLen1 = 0;
    unsigned long int tagId1;
    AsnLen elmtLen1;
    size_t lenToRef;
//    size_t unusedBits;

    for (; (totalElmtsLen1 < elmtLen0) || (elmtLen0 == INDEFINITE_LEN); )
    {
        tagId1 = BDecTag (b, totalElmtsLen1, env);

        if ((tagId1 == EOC_TAG_ID) && (elmtLen0 == INDEFINITE_LEN))
        {
            BDEC_2ND_EOC_OCTET (b, totalElmtsLen1, env);
            break;
        }

        elmtLen1 = BDecLen (b, totalElmtsLen1, env);
        if (tagId1 == MAKE_TAG_ID (UNIV, PRIM, BITSTRING_TAG_CODE))
        {
            /*
             * primitive part of string, put references to piece (s) in
             * str stack
             */

            /*
             * get unused bits octet
             */
            if (strStkUnusedBitsG != 0)
            {
                /*
                 *  whoa - only allowed non-octed aligned bits on
                 *  on last piece of bits string
                 */
                Asn1Error << "BDecConsBitString: ERROR - a component of a constructed BIT STRING that is not the last has non-zero unused bits" << endl;
                longjmp (env, -2);
            }

            if (elmtLen1 != 0)
                strStkUnusedBitsG = b.GetByte();

            totalRefdLen = 0;
            lenToRef =elmtLen1-1; /* remove one octet for the unused bits oct*/
            refdLen = lenToRef;
            while (1)
            {
                strPtr = b.GetSeg (&refdLen);

                strStkG.Push (strPtr, refdLen);
                totalRefdLen += refdLen;
                if (totalRefdLen == lenToRef)
                    break; /* exit this while loop */

                if (refdLen == 0) /* end of data */
                {
                    Asn1Error << "BDecConsOctetString: ERROR - expecting more data" << endl;
                    longjmp (env, -3);
                }
                refdLen = lenToRef - totalRefdLen;
            }
            totalElmtsLen1 += elmtLen1;
        }


        else if (tagId1 == MAKE_TAG_ID (UNIV, CONS, BITSTRING_TAG_CODE))
        {
            /*
             * constructed octets string embedding in this constructed
             * octet string. decode it.
             */
            FillBitStringStk (b, elmtLen1, totalElmtsLen1, env
				#ifdef	__APPLE__
				, strStkG, strStkUnusedBitsG
				#endif
				);
       }
        else  /* wrong tag */
        {
            Asn1Error << "BDecConsBitString: ERROR - decoded non-BIT STRING tag inside a constructed BIT STRING" << endl;
            longjmp (env, -4);
        }
    } /* end of for */

    bytesDecoded += totalElmtsLen1;
}  /* FillBitStringStk */


/*
 * decodes a seq of universally tagged bits until either EOC is
 * encountered or the given len decoded.  Return them in a
 * single concatenated bit string
 */
void AsnBits::BDecConsBits (BUF_TYPE b, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env)
{
	#ifdef	__APPLE__
	StrStk strStkG(128, 64);
	unsigned short int  strStkUnusedBitsG;
	#endif
    strStkG.Reset();
    strStkUnusedBitsG = 0;

    /*
     * decode each piece of the octet string, puting
     * an entry in the octet/bit string stack for each
     */
    FillBitStringStk (b, elmtLen, bytesDecoded, env, strStkG, 
		strStkUnusedBitsG);

    /* alloc single str long enough for combined bitstring */
    bitLen = strStkG.totalByteLen*8 - strStkUnusedBitsG;

#ifndef _IBM_ENC_
    bits = Asn1Alloc (strStkG.totalByteLen);
#else
    bits = (char *) mem_mgr_ptr->Get (strStkG.totalByteLen);       // Guido Grassel, 11.8.93
#endif /* _IBM_ENC_ */

    strStkG.CopyOut (bits);

}  /* BDecConsBits */

// prints the BIT STRING to the given ostream.
void AsnBits::Print (ostream &os) const
{
#ifndef	NDEBUG
    size_t octetLen = (bitLen+7)/8;

    os << "'";
    for (int i = 0; i < octetLen; i++)
        os << TO_HEX (bits[i] >> 4) << (TO_HEX (bits[i]));
    os << "'H  -- BIT STRING bitlen = " << bitLen << " --";
#endif	/* NDEBUG */
}

#if META

const AsnBitsTypeDesc AsnBits::_desc (NULL, NULL, false, AsnTypeDesc::BIT_STRING, NULL, NULL);

const AsnTypeDesc *AsnBits::_getdesc() const
{
  return &_desc;
}

#if TCL

int AsnBits::TclGetVal (Tcl_Interp *interp) const
{
  Tcl_ResetResult(interp);
  for (int i=0; i<bitLen; i++)
    Tcl_AppendResult(interp,GetBit (i) ? "1" : "0",NULL);
  return TCL_OK;
}

int AsnBits::TclSetVal (Tcl_Interp *interp, const char *valstr)
{
  int		i;
  const char	*p;

  for (i=0, p=valstr; *p; i++, p++)
    switch (*p)
    {
      case '0':
      case '1':
	break;
      default:
	const char c[2] = { *p, '\0' };
	Tcl_AppendResult (interp, "illegal character '", c, "' for bit in type ", _getdesc()->getmodule()->name, ".", _getdesc()->getname(), NULL);
	Tcl_SetErrorCode (interp, "SNACC", "ILLBIT", NULL);
	return TCL_ERROR;
    }

  ReSet (i);

  for (i=0, p=valstr; i<bitLen; i++, p++)
    *p == '0' ? ClrBit(i) : SetBit(i);

  return TCL_OK;
}

#endif /* TCL */
#endif /* META */
