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
 * asn_tag.h
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
 * INSERT_VDA_COMMENTS
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/c-lib/inc/asn-tag.h,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: asn-tag.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:23  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:21  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/27 08:44:15  rj
 * cpp macro TBL changed to TTBL since some type table code uses TBL as a type name.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1995/02/18  16:22:23  rj
 * let cpp choose a 32 bit integer type.
 *
 * Revision 1.1  1994/08/28  09:21:37  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#ifndef _asn_tag_h_
#define _asn_tag_h_

#if SIZEOF_INT == 4
#  define UL		unsigned int
#else
#  if SIZEOF_LONG == 4
#    define UL		unsigned long
#  else
#    if SIZEOF_SHORT == 4
#      define UL	unsigned short
#    endif
#  endif
#endif
#ifndef UL
  #error "can't find integer type which is 4 bytes in size"
#endif
typedef UL	AsnTag;

/* Tag Id's byte length */
#define TB	sizeof (AsnTag)

/*
 * The MAKE_TAG_ID macro generates the TAG_ID rep for the
 * the given class/form/code (rep'd in long integer form)
 * if the class/form/code are constants the compiler (should)
 * calculate the tag completely --> zero runtime overhead.
 * This is good for efficiently comparing tags in switch statements
 * (decoding) etc.  because run-time bit fiddling (eliminated) minimized
 */
#ifndef _IBM_ENC_
#define MAKE_TAG_ID( cl, fm, cd)\
	((((UL)(cl)) << ((TB -1) * 8)) | (((UL)(fm)) << ((TB -1) * 8)) | (MAKE_TAG_ID_CODE (((UL)(cd)))))
#else
#define MAKE_TAG_ID( cl, fm, cd)\
	((MAKE_TAG_ID_CODE (cd)) | (cl << ((TB -1) * 8)) | (fm << ((TB -1) * 8)))
#endif /* _IBM_ENC_ */

#define MAKE_TAG_ID_CODE(cd)\
( (cd < 31) ?  (MAKE_TAG_ID_CODE1 (cd)):\
      ((cd < 128)?  (MAKE_TAG_ID_CODE2 (cd)):\
         ((cd < 16384)?  (MAKE_TAG_ID_CODE3 (cd)):\
           (MAKE_TAG_ID_CODE4 (cd)))))

#define MAKE_TAG_ID_CODE1(cd)  (cd << ((TB -1) * 8))
#define MAKE_TAG_ID_CODE2(cd)  ((31 << ((TB -1) * 8)) | (cd << ((TB-2) * 8)))
#define MAKE_TAG_ID_CODE3(cd)  ((31 << ((TB -1) * 8))\
                                | ((cd & 0x3f80) << 9)\
                                | ( 0x0080 << ((TB-2) * 8))\
                                | ((cd & 0x007F) << ((TB-3)* 8)))

#define MAKE_TAG_ID_CODE4(cd)  ((31 << ((TB -1) * 8))\
                                | ((cd & 0x1fc000) << 2)\
                                | ( 0x0080 << ((TB-2) * 8))\
                                | ((cd & 0x3f80) << 1)\
                                | ( 0x0080 << ((TB-3) * 8))\
                                | ((cd & 0x007F) << ((TB-4)*8)))



typedef enum
{
    ANY_CLASS = -2,
    NULL_CLASS = -1,
    UNIV = 0,
    APPL = (1 << 6),
    CNTX = (2 << 6),
    PRIV = (3 << 6)
} BER_CLASS;

typedef enum
{
    ANY_FORM = -2,
    NULL_FORM = -1,
    PRIM = 0,
    CONS = (1 << 5)
} BER_FORM;


typedef enum
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

#define TT61STRING_TAG_CODE	TELETEXSTRING_TAG_CODE
#define ISO646STRING_TAG_CODE	VISIBLESTRING_TAG_CODE


/*
 * the TAG_ID_[CLASS/FORM/CODE] macros are not
 * super fast - try not to use during encoding/decoding
 */
#define TAG_ID_CLASS( tid)	((tid & (0xC0 << ((TB-1) *8))) >> ((TB -1) * 8))
#define TAG_ID_FORM( tid)	((tid & (0x20 << ((TB-1) *8))) >> ((TB -1) * 8))

/*
 * TAG_IS_CONS evaluates to true if the given AsnTag type
 * tag has the constructed bit set.
 */
#define TAG_IS_CONS( tag)	((tag) & (CONS << ((TB-1) *8)))
#define CONSIFY( tag)		(tag | (CONS << ((TB-1) *8)))
#define DECONSIFY( tag)		(tag &  ~(CONS << ((TB-1) *8)))


/* not a valid tag - usually the first EOC octet */
#define EOC_TAG_ID		0



/*
 * tag encoders.  given constant values for class form &
 * code in the  source, these can be optimized by the compiler
 * (e.g.  do the shifts and bitwise ands & ors etc)
 *
 * This is the prototype that the following BEncTag routines
 * would use if they were routines.  They return the number of
 * octets written to the buffer.
 *
 *
 *AsnLen BEncTag PROTO ((BUF_TYPE b, BER_CLASS class, BER_FORM form, int code));
 *
 * WARNING: these are FRAGILE macros (What people will do for performance!)
 *          Be careful of situations like:
 *            if (foo)
 *                  encLen += BEncTag1 (...);
 *          Use {}'s to enclose any ASN.1 related routine that you are
 *          treating as a single statement in your code.
 */
#define BEncTag1( b, class, form, code)\
    1;\
    BufPutByteRvs (b, (class) | (form) | (code));

#define BEncTag2( b, class, form, code)\
    2;\
    BufPutByteRvs (b, code);\
    BufPutByteRvs (b, (class) | (form) | 31);

#define BEncTag3( b, class, form, code)\
    3;\
    BufPutByteRvs (b, (code) & 0x7F);\
    BufPutByteRvs (b, 0x80 | ((code) >> 7));\
    BufPutByteRvs (b, (class) | (form) | 31);

#define BEncTag4( b, class, form, code)\
    4;\
    BufPutByteRvs (b, (code) & 0x7F);\
    BufPutByteRvs (b, 0x80 | ((code) >> 7));\
    BufPutByteRvs (b, 0x80 | ((code) >> 14));\
    BufPutByteRvs (b, (class) | (form) | 31);

#define BEncTag5( b, class, form, code)\
    5;\
    BufPutByteRvs (b, (code) & 0x7F);\
    BufPutByteRvs (b, 0x80 | ((code) >> 7));\
    BufPutByteRvs (b, 0x80 | ((code) >> 14));\
    BufPutByteRvs (b, 0x80 | ((code) >> 21));\
    BufPutByteRvs (b, (class) | (form) | 31);


/* the following are protos for routines ins asn_tag.c */


AsnTag BDecTag PROTO ((BUF_TYPE  b, AsnLen *bytesDecoded, ENV_TYPE env));
#if TTBL
AsnTag PeekTag PROTO ((BUF_TYPE b, ENV_TYPE env));
#endif

#endif /* conditional include */
