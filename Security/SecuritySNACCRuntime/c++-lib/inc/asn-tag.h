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


// file: .../c++-lib/inc/asn-tag.h - stuff for dealing with tags
//
// MS 92
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
//
// ------------------------------------------------------------------------
// -  J.G. Van Dyke & Associates, Inc. Modification History of SNACC 1.3  -
// ------------------------------------------------------------------------
// 
// All modification are relative to the v1.3 of SNACC.  We used SunOS 4.1.3's 
// SCCS.  The revision #'s start at 1.1,  which is the original version from 
// SNACC 1.3.
// 
// 
// ../SCCS/s.asn-tag.h:
// 
// D 1.2	98/04/24 22:39:52 pleonber	2 1	00012/00000/00204
// added INSERT_VDA_COMMENT for script that adds SCCS history
// 
// D 1.1	98/04/17 10:30:00 pleonber	1 0	00204/00000/00000
// date and time created 98/04/17 10:30:00 by pleonber
// 
// ----------------------- End of VDA Modifications ---------------------------
// 
// 
// 
// $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-lib/inc/asn-tag.h,v 1.1.1.1 2001/05/18 23:14:06 mb Exp $
// $Log: asn-tag.h,v $
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.4  2001/05/05 00:59:18  rmurphy
// Adding darwin license headers
//
// Revision 1.3  2000/12/22 00:20:55  dmitch
// Added UTF8STRING_TAG_CODE.
//
// Revision 1.2  2000/06/15 18:48:24  dmitch
// Snacc-generated source files, now part of CVS tree to allow for cross-platform build of snaccRuntime.
//
// Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
// Base Fortissimo Tree
//
// Revision 1.1  1999/02/25 05:21:45  mb
// Added snacc c++ library
//
// Revision 1.6  1997/02/16 20:25:45  rj
// check-in of a few cosmetic changes
//
// Revision 1.5  1995/07/24  17:35:52  rj
// ``#error "..."'' instead of ``#error ...''.
//
// changed `_' to `-' in file names.
//
// Revision 1.4  1995/02/18  16:45:53  rj
// let cpp choose a 32 bit integer type.
//
// Revision 1.3  1994/10/08  04:18:09  rj
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
// Revision 1.2  1994/08/28  10:00:56  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:43  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#ifndef _asn_tag_h_
#define _asn_tag_h_

#if SIZEOF_INT == 4
  #define UL	unsigned int
#elif SIZEOF_LONG == 4
  #define UL	unsigned long
#elif SIZEOF_SHORT == 4
  #define UL	unsigned short
#else
  #error "can't find integer type which is 4 bytes in size"
#endif
typedef UL	AsnTag;

// Tag Id's byte len
#define TB	sizeof (AsnTag)

// The MAKE_TAG_ID macro generates the TAG_ID rep for the
// the given class/form/code (rep'd in long integer form)
// if the class/form/code are constants the compiler (should)
// calculate the tag completely --> zero runtime overhead.
// This is good for efficiently comparing tags in switch statements
// (decoding) etc.  because run-time bit fiddling (eliminated) minimized
#ifndef _IBM_ENC_
#define MAKE_TAG_ID( cl, fm, cd)\
	((((UL)(cl)) << ((TB -1) * 8)) | (((UL)(fm)) << ((TB -1) * 8)) | (MAKE_TAG_ID_CODE (((UL)(cd)))))
#else
#define MAKE_TAG_ID( cl, fm, cd)\
	((MAKE_TAG_ID_CODE (cd)) | (cl << ((TB -1) * 8)) | (fm << ((TB -1) * 8)))
#endif /* _IBM_ENC_ */

#define MAKE_TAG_ID_CODE( cd)\
( (cd < 31) ?  (MAKE_TAG_ID_CODE1 (cd)):\
      ((cd < 128)?  (MAKE_TAG_ID_CODE2 (cd)):\
	 ((cd < 16384)?  (MAKE_TAG_ID_CODE3 (cd)):\
	   (MAKE_TAG_ID_CODE4 (cd)))))

#define MAKE_TAG_ID_CODE1( cd) ((long int)cd << ((TB -1) * 8))
#define MAKE_TAG_ID_CODE2( cd) ((31l << ((TB -1) * 8)) | (cd << ((TB-2) * 8)))
#define MAKE_TAG_ID_CODE3( cd) ((31l << ((TB -1) * 8))\
				| ((cd & 0x3f80) << 9)\
				| ( 0x0080 << ((TB-2) * 8))\
				| ((cd & 0x007F) << ((TB-3)* 8)))

#define MAKE_TAG_ID_CODE4( cd) ((31l << ((TB -1) * 8))\
				| ((cd & 0x1fc000) << 2)\
				| ( 0x0080 << ((TB-2) * 8))\
				| ((cd & 0x3f80) << 1)\
				| ( 0x0080 << ((TB-3) * 8))\
				| ((cd & 0x007F) << ((TB-4)*8)))


typedef enum BER_CLASS
{
    ANY_CLASS = -2,
    NULL_CLASS = -1,
    UNIV = 0,
    APPL = (1 << 6),
    CNTX = (2 << 6),
    PRIV = (3 << 6)
} BER_CLASS;

typedef enum BER_FORM
{
    ANY_FORM = -2,
    NULL_FORM = -1,
    PRIM = 0,
    CONS = (1 << 5)
} BER_FORM;


typedef enum BER_UNIV_CODE
{
    NO_TAG_CODE = 0,
    BOOLEAN_TAG_CODE = 1,
    INTEGER_TAG_CODE,
    BITSTRING_TAG_CODE,
    OCTETSTRING_TAG_CODE,
    NULLTYPE_TAG_CODE,
    OID_TAG_CODE,
    OD_TAG_CODE,
    EXTERNAL_TAG_CODE,
    REAL_TAG_CODE,
    ENUM_TAG_CODE,
	UTF8STRING_TAG_CODE = 12,
    SEQ_TAG_CODE =  16,
    SET_TAG_CODE,
    NUMERICSTRING_TAG_CODE,
    PRINTABLESTRING_TAG_CODE,
    TELETEXSTRING_TAG_CODE,
    VIDEOTEXSTRING_TAG_CODE,
    IA5STRING_TAG_CODE,
    UTCTIME_TAG_CODE,
    GENERALIZEDTIME_TAG_CODE,
    GRAPHICSTRING_TAG_CODE,
    VISIBLESTRING_TAG_CODE,

#ifndef VDADER_RULES

    GENERALSTRING_TAG_CODE

#else
    GENERALSTRING_TAG_CODE,
    UNIVERSALSTRING_TAG_CODE = 28,
    BMPSTRING_TAG_CODE = 30
#endif

} BER_UNIV_CODE;

#define TT61STRING_TAG_CODE TELETEXSTRING_TAG_CODE
#define ISO646STRING_TAG_CODE VISIBLESTRING_TAG_CODE

/*
 * the TAG_ID_[CLASS/FORM/CODE] macros are not
 * super fast - try not to use during encoding/decoding
 */
#define TAG_ID_CLASS( tid) ( (tid & (0xC0 << ((TB-1) *8))) >> ((TB -1) * 8))
#define TAG_ID_FORM( tid)  ( (tid & (0x20 << ((TB-1) *8))) >> ((TB -1) * 8))

/*
 * TAG_IS_CONS evaluates to true if the given AsnTag type
 * tag has the constructed bit set.
 */
#define TAG_IS_CONS(tag) ((tag) & (CONS << ((TB-1) *8)))


#define EOC_TAG_ID 0



/*
 * tag encoders.  given constant exprs for class form & code in the
 * source, these can be optimized by the compiler (eg
 * do the shifts and bitwise ors etc)
 */

#define BEncTag1( b, class, form, code)\
    1;\
    b.PutByteRvs ((class) | (form) | (code))

#define BEncTag2( b, class, form, code)\
    2;\
    b.PutByteRvs (code);\
    b.PutByteRvs ((class) | (form) | 31)

#define BEncTag3( b, class, form, code)\
    3;\
    b.PutByteRvs ((code) & 0x7F);\
    b.PutByteRvs (0x80 | ((code) >> 7));\
    b.PutByteRvs ((class) | (form) | 31)

#define BEncTag4( b, class, form, code)\
    4;\
    b.PutByteRvs ((code) & 0x7F);\
    b.PutByteRvs (0x80 | ((code) >> 7));\
    b.PutByteRvs (0x80 | ((code) >> 14));\
    b.PutByteRvs ((class) | (form) | 31)

#define BEncTag5( b, class, form, code)\
    5;\
    b.PutByteRvs ((code) & 0x7F);\
    b.PutByteRvs (0x80 | ((code) >> 7));\
    b.PutByteRvs (0x80 | ((code) >> 14));\
    b.PutByteRvs (0x80 | ((code) >> 21));\
    b.PutByteRvs ((class) | (form) | 31)


AsnTag BDecTag (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);

#endif /* conditional include */
