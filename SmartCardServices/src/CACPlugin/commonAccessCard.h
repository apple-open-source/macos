/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 */

/******************************************************************
 
        MUSCLE SmartCard Development ( http://www.linuxnet.com )
            Title  : commonAccessCard.h
            Package: CACPlugin
            Author : David Corcoran
            Date   : 02/06/02
            License: Copyright (C) 2002 David Corcoran
                     <corcoran@linuxnet.com>
            Purpose: This abstracts the CAC Interface
 
 
********************************************************************/

#ifndef __commonAccessCard_h__
#define __commonAccessCard_h__

#ifdef __cplusplus
  extern "C" {
#endif

/* Some useful offsets in the buffer */
#define OFFSET_CLA	0x00
#define OFFSET_INS	0x01
#define OFFSET_P1	0x02
#define OFFSET_P2	0x03
#define OFFSET_P3	0x04
#define OFFSET_DATA	0x05


#define CAC_GID_CERTNAME  "C3"
#define CAC_MID_CERTNAME  "C5"
#define CAC_ENC_CERTNAME  "C7"

#define MC_1024P_FULLSIZE     140
#define MC_SIZEOF_COMPSIZE      2
#define MC_1024_OFFSET_P        4
#define MC_1024_OFFSET_Q       70
#define MC_1024_OFFSET_PQ     136
#define MC_1024_OFFSET_DP1    202
#define MC_1024_OFFSET_DQ1    268

#define MC_1024P_MOD            4
#define MC_1024P_EXP          134

#define MC_DES_OFFSET_KEY       4

    /* Sizes of particular objects */
#define MSC_SIZEOF_OBJECTID               4
#define MSC_SIZEOF_OBJECTSIZE             4
#define MSC_SIZEOF_KEYINFO                11
#define MSC_SIZEOF_STATUS                 16
#define MSC_SIZEOF_VERSION                2
#define MSC_SIZEOF_FREEMEM                4
#define MSC_SIZEOF_LOGIDS                 2
#define MSC_SIZEOF_ADDINFO                8
#define MSC_SIZEOF_OPTLEN                 2
#define MSC_SIZEOF_GENOPTIONS             1
#define MSC_SIZEOF_KEYSIZE                2
#define MSC_SIZEOF_KEYNUMBER              1
#define MSC_SIZEOF_KEYTYPE                1
#define MSC_SIZEOF_KEYPARTNER             1
#define MSC_SIZEOF_CIPHERMODE             1
#define MSC_SIZEOF_CIPHERDIR              1
#define MSC_SIZEOF_CRYPTLEN               2
#define MSC_SIZEOF_ALGOTYPE               1
#define MSC_SIZEOF_IDUSED                 1
#define MSC_SIZEOF_OFFSET                 4
#define MSC_SIZEOF_ACLSTRUCT              6
#define MSC_SIZEOF_RWDATA                 1
#define MSC_SIZEOF_PINSIZE                1
#define MSC_SIZEOF_CIPHERMODE             1
#define MSC_SIZEOF_CIPHERDIR              1
#define MSC_SIZEOF_DATALOCATION           1
#define MSC_SIZEOF_ACLVALUE               2
#define MSC_SIZEOF_SEEDLENGTH             2
#define MSC_SIZEOF_RANDOMSIZE             2

    /** success */
#define CACMSC_SUCCESS                        0x9000

    /** There have been memory problems on the card */
#define CACMSC_NO_MEMORY_LEFT                 0x6A84
    /** Entered PIN is not correct */
#define CACMSC_AUTH_FAILED                    0x6300
    /** Required feature is not (yet) supported */
#define CACMSC_UNSUPPORTED_FEATURE            0x6D00
    /** Required operation was not authorized because lack of privileges */
#define CACMSC_UNAUTHORIZED                   0x6982
    /** Required object is missing */
#define CACMSC_OBJECT_NOT_FOUND               0x6A82
    /** New object ID already in use */
#define CACMSC_OBJECT_EXISTS                  0x6A80
    /** Algorithm specified is not correct */
#define CACMSC_INCORRECT_ALG                  0x6981
    
    /** Operation has been blocked for security reason  */
#define CACMSC_IDENTITY_BLOCKED               0x6983
    /** Unspecified error */
#define CACMSC_UNSPECIFIED_ERROR              0x6F00
    /** PCSC and driver transport errors */
#define CACMSC_INVALID_PARAMETER              0x6B00
    /** Incorrect P1 parameter */
#define CACMSC_INCORRECT_P1                   0x6B00
    /** Incorrect P2 parameter */
#define CACMSC_INCORRECT_P2                   0x6B00
    /** For debugging purposes */
#define CACMSC_INTERNAL_ERROR                 0x6581
    


#ifdef __cplusplus
  }
#endif

#endif /* __commonAccessCard_h__ */
