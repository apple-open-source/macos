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
            Title  : cryptoflex.h
            Package: card edge
            Author : David Corcoran
            Date   : 10/02/01
            License: Copyright (C) 2001 David Corcoran
                     <corcoran@linuxnet.com>
            Purpose: This abstracts the MUSCLE Card Edge Inteface
 
 
********************************************************************/

#ifndef __cryptoflex_h__
#define __cryptoflex_h__

#ifdef __cplusplus
  extern "C" {
#endif

#define CLA_F0          0xF0
#define CLA_C0          0xC0

/* Some useful offsets in the buffer */
#define OFFSET_CLA	0x00
#define OFFSET_INS	0x01
#define OFFSET_P1	0x02
#define OFFSET_P2	0x03
#define OFFSET_P3	0x04
#define OFFSET_DATA	0x05

#define OFFSET_ENCODING 0x00
#define OFFSET_KEYTYPE  0x01
#define OFFSET_KEYSIZE  0x02
#define OFFSET_KEYDATA  0x04

#define CF_SIZEOF_MSBLEN        1
#define CF_SIZEOF_LSBLEN        1
#define CF_SIZEOF_KEYNUM        1
#define CF_SIZEOF_OBJID         2

/* Defines for 1024 bit RSA keys */
#define CF_1024_FULLSIZE    0x143
#define CF_1024_FULLSIZE_1   0x01
#define CF_1024_FULLSIZE_2   0x43
#define CF_1024P_FULLSIZE   0x147

#define CF_1024_COMPSIZE     0x40

#define CF_1024_KEYSIZE      1024
#define CF_1024P_MODSIZE      128
#define CF_1024P_EXPSIZE        4
#define CF_1024P_MOD            3
#define CF_1024P_EXP          323

#define MC_1024P_FULLSIZE     140
#define MC_SIZEOF_COMPSIZE      2
#define MC_1024_OFFSET_P        4
#define MC_1024_OFFSET_Q       70
#define MC_1024_OFFSET_PQ     136
#define MC_1024_OFFSET_DP1    202
#define MC_1024_OFFSET_DQ1    268


#define MC_1024P_MOD            4
#define MC_1024P_EXP          134
#define MC_1024P_MODSIZE      128


#define CF_DES_OFFSET_UKEY   0x19
#define MC_DES_OFFSET_KEY       4

    /* Sizes of particular objects */
#define MSC_SIZEOF_OBJECTID               4
#define MSC_SIZEOF_OBJECTSIZE             4
#define MSC_SIZEOF_KEYINFO                16
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
#define MSC_SIZEOF_POLICYVALUE            2
#define MSC_SIZEOF_KEYMAPPING             1
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

    // Keys' use and management
#define INS_MSC_GEN_KEYPAIR     0x46
#define INS_IMPORT_KEY      0x32
#define INS_EXPORT_KEY      0x34
#define INS_COMPUTE_CRYPT   0x88

    // External authentication
#define INS_CREATE_PIN      0x40
#define INS_VERIFY_PIN      0x20
#define INS_CHANGE_PIN      0x24
#define INS_UNBLOCK_PIN     0x2C
#define INS_LOGOUT_ALL      0x22
#define INS_GET_CHALLENGE   0x84
#define INS_EXT_AUTH        0x82

    // Objects' use and management
#define INS_CREATE_OBJ      0xE0
#define INS_DELETE_OBJ      0xE4
#define INS_READ_OBJ        0xB0
#define INS_WRITE_OBJ       0xD6

    // Status information
#define INS_LIST_OBJECTS    0xA8
#define INS_LIST_PINS       0x48
#define INS_LIST_KEYS       0x3A
#define INS_GET_STATUS      0x3C

    /** success */
#define CFMSC_SUCCESS                        0x9000

    /** There have been memory problems on the card */
#define CFMSC_NO_MEMORY_LEFT                 0x6A84
#define CFMSC_NO_MEMORY_LEFT_1               0x6A83
    /** Entered PIN is not correct */
#define CFMSC_AUTH_FAILED                    0x6300
    /** Required operation is not allowed in actual circumstances */
#define CFMSC_OPERATION_NOT_ALLOWED          0x9C03
    /** Required operation is inconsistent with memory contents */
#define CFMSC_INCONSISTENT_STATUS            0x9C04
    /** Required feature is not (yet) supported */
#define CFMSC_UNSUPPORTED_FEATURE            0x6D00
    /** Required operation was not authorized because of a lack of privileges */
#define CFMSC_UNAUTHORIZED                   0x6982
    /** Required object is missing */
#define CFMSC_OBJECT_NOT_FOUND               0x6A82
    /** New object ID already in use */
#define CFMSC_OBJECT_EXISTS                  0x6A80
    /** Algorithm specified is not correct */
#define CFMSC_INCORRECT_ALG                  0x6981
    
    /** Verify operation detected an invalid signature */
#define CFMSC_SIGNATURE_INVALID              0x9C0B
    /** Operation has been blocked for security reason  */
#define CFMSC_IDENTITY_BLOCKED               0x6983
    /** Unspecified error */
#define CFMSC_UNSPECIFIED_ERROR              0x6F00
    /** PCSC and driver transport errors */
#define CFMSC_TRANSPORT_ERROR                0x9C0E
    /** Invalid parameter given */
#define CFMSC_INVALID_PARAMETER              0x6B00
    /** Incorrect P1 parameter */
#define CFMSC_INCORRECT_P1                   0x6B00
    /** Incorrect P2 parameter */
#define CFMSC_INCORRECT_P2                   0x6B00
    /** End of sequence */
#define CFMSC_SEQUENCE_END                   0x9C12
    /** For debugging purposes */
#define CFMSC_INTERNAL_ERROR                 0x6581
    

#define CFMSC_CANCELLED                      0x9C50


#ifdef __cplusplus
  }
#endif

#endif /* __cryptoflex_h__ */
