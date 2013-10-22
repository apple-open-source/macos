/*
 * Copyright (c) 2012 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * eap_plugin.h - Extensible Authentication Protocol Plugin API.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: eap_plugin.h,v 1.4 2003/08/14 00:00:29 callie Exp $
 */

/* ----------------------------------------------------------------------
 IMPORTANT: EAP Plugin API is not stable.
 The API will change in the upcoming releases
 ---------------------------------------------------------------------- */

#ifndef __EAP_SIM__
#define __EAP_SIM__

#include "eap.h"

/* EAP-AKA Subtypes */
#define EAP_AKA_SUBTYPE_CHALLENGE           1
#define EAP_AKA_SUBTYPE_AUTH_REJECT         2
#define EAP_AKA_SUBTYPE_SYNC_FAIL           4
#define EAP_AKA_SUBTYPE_IDENTITY            5
#define EAP_AKA_SUBTYPE_NOTIFICATION        12
#define EAP_AKA_SUBTYPE_REAUTH              13
#define EAP_AKA_SUBTYPE_CLIENT_ERROR        14

/* EAP-SIM Subtypes */
#define EAP_SIM_SUBTYPE_START               10
#define EAP_SIM_SUBTYPE_CHALLENGE           11
#define EAP_SIM_SUBTYPE_NOTIFICATION        12
#define EAP_SIM_SUBTYPE_REAUTH              13
#define EAP_SIM_SUBTYPE_CLIENT_ERROR        14

/* Non-skippable attributes */
#define EAP_AT_RAND                         1
#define EAP_AT_AUTN                         2
#define EAP_AT_RES                          3
#define EAP_AT_AUTS                         4
#define EAP_AT_PADDING                      6
#define EAP_AT_NONCE_MT                     7
#define EAP_AT_PERMANENT_ID_REQ             10
#define EAP_AT_MAC                          11
#define EAP_AT_NOTIFICATION                 12
#define EAP_AT_ANY_ID_REQ                   13
#define EAP_AT_IDENTITY                     14
#define EAP_AT_VERSION_LIST                 15
#define EAP_AT_SELECTED_VERSION             16
#define EAP_AT_FULL_AUTH_ID_REQ             17
#define EAP_AT_COUNTER                      19
#define EAP_AT_COUNTER_TOO_SMALL            20
#define EAP_AT_NONCE_S                      21
#define EAP_AT_CLIENT_ERROR_CODE            22
#define EAP_AT_KDF_INPUT                    23
#define EAP_AT_KDF                          24

/* Skippable attributes */
#define EAP_AT_IV                           129
#define EAP_AT_ENCR_DATA                    130
#define EAP_AT_NEXT_PSEUDONYM               132
#define EAP_AT_NEXT_REAUTH_ID               133
#define EAP_AT_CHECKCODE                    134
#define EAP_AT_RESULT_IND                   135
#define EAP_AT_BIDDING                      136
#define EAP_AT_IPMS_IND                     137
#define EAP_AT_IPMS_RES                     138
#define EAP_AT_TRUST_IND                    139

/* Attribute notification values */
#define EAP_AT_NOTIFICATION_GEN_FAIL_POST_AUTH  0       /* General failure after authentication */
#define EAP_AT_NOTIFICATION_USER_DENIED         1026    /* User has been temporarily denied access */
#define EAP_AT_NOTIFICATION_NOT_SUBSCRIBED      1031    /* User has not subscribed to the requested service */
#define EAP_AT_NOTIFICATION_GEN_FAIL            16384   /* General failure */
#define EAP_AT_NOTIFICATION_SUCCESS             32768   /* Success */

#define EAP_SIM_VERSION_1                       1

typedef struct eap_sim_hdr {
    u_int8_t             eap_type;           /* Must be EAP-SIM, 18 */
    u_int8_t             eap_subtype;
    u_int16_t            reserved;
} __attribute__((__packed__)) eap_sim_hdr_t;

typedef struct eap_sim_attribute {
    u_int8_t             at_type;
    u_int8_t             at_len;
    u_int16_t            at_value;
    /* Followed by variable-length value */
} __attribute__((__packed__)) eap_sim_attr_t;

typedef struct eap_sim_msg {
    eap_sim_hdr_t       eap_hdr;
    eap_sim_attr_t      payload[0];         /* Multiple attributes */
} __attribute__((__packed__)) eap_sim_t;

int EAPSIMIdentity(char *identity, int maxlen);
int EAPSIMInit(EAP_Input_t *eap_in, void **context, CFDictionaryRef options);
int EAPSIMDispose(void *context);
int EAPSIMProcess(void *context, EAP_Input_t *eap_in, EAP_Output_t *eap_out);
int EAPSIMFree(void *context, EAP_Output_t *eap_out);
int EAPSIMGetAttribute(void *context, EAP_Attribute_t *eap_attr);

int EAPAKAIdentity(char *identity, int maxlen);
int EAPAKAInit(EAP_Input_t *eap_in, void **context, CFDictionaryRef options);
int EAPAKADispose(void *context);
int EAPAKAProcess(void *context, EAP_Input_t *eap_in, EAP_Output_t *eap_out);
int EAPAKAFree(void *context, EAP_Output_t *eap_out);
int EAPAKAGetAttribute(void *context, EAP_Attribute_t *eap_attr);

#endif
