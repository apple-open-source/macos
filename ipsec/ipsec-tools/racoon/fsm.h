/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _FSM_H
#define _FSM_H

#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include "vmbuf.h"
#include "ike_session.h"
#include "handler.h"
#include "strnames.h"
#include "ipsec_xpc.h"

//================================
// Defines
//================================
//

//
// State Flags
//
// bit#     
//  0       Ike Version     0 = v1  1= v2
//  1       Expired
//  2       Established
//  3       Negotiating
//  4-5     Ike Phase       1 = Phase1  2 = phase2
//  6     Reserved
//  7       Direction       0 = Initiator   1 = Responder
//

// STATE FLAG MASKS
#define IKE_STATE_MASK_VERSION              0x8000
#define IKE_STATE_MASK_EXPIRED              0x4000
#define IKE_STATE_MASK_ESTABLISHED          0x2000
#define IKE_STATE_MASK_NEGOTIATING          0x1000
#define IKE_STATE_MASK_PHASE                0x0C00
#define IKE_STATE_MASK_XAUTH_OR_EAP_SUCC    0x0200
#define IKE_STATE_MASK_DIRECTION            0x0100
#define IKE_STATE_MASK_MODE                 0x00C0
#define IKE_STATE_MASK_STATE                0X003F

#define IKE_STATE_FLAG_VALUE_IKEV1          0x0000
#define IKE_STATE_FLAG_VALUE_IKEV2          0x8000
#define IKE_STATE_FLAG_VALUE_EXPIRED        0x4000
#define IKE_STATE_FLAG_VALUE_ESTABLISED     0x2000
#define IKE_STATE_FLAG_VALUE_NEGOTIATING    0x1000
#define IKE_STATE_FLAG_VALUE_PHASE1         0x0400
#define IKE_STATE_FLAG_VALUE_PHASE2         0x0800
#define IKE_STATE_FLAG_XAUTH_OR_EAP_SUCC    0x0200
#define IKE_STATE_FLAG_VALUE_INITIATOR      0x0000
#define IKE_STATE_FLAG_VALUE_RESPONDER      0x0100


//================================
// MACROS
//================================

#define FSM_STATE_IS_EXPIRED(s)             \
    ((s) & IKE_STATE_MASK_EXPIRED)

#define FSM_STATE_IS_ESTABLISHED(s)         \
    ((s) & IKE_STATE_MASK_ESTABLISHED)

#define FSM_STATE_IS_ESTABLISHED_OR_EXPIRED(s)  \
    (((s) & IKE_STATE_MASK_ESTABLISHED) | ((s) & IKE_STATE_MASK_EXPIRED))

#define FSM_STATE_IS_NEGOTIATING(s)         \
((s) & IKE_STATE_MASK_NEGOTIATING)

#define FSM_STATE_IS_INITIATOR(s)           \
    ((s & IKE_STATE_MASK_DIRECTION) == IKE_STATE_FLAG_VALUE_INITIATOR)

#define FSM_STATE_IS_RESPONDER(s)           \
((s & IKE_STATE_MASK_DIRECTION) == IKE_STATE_FLAG_VALUE_RESPONDER)

//================================
// API States
//================================

//================================
// IKEv1 States
//================================

#define IKEV1_STATE_FLAG_VALUE_INFO         (IKE_STATE_FLAG_VALUE_IKEV1 | 0x0000)
#define IKEV1_STATE_FLAG_VALUE_IDENTMODE    (IKE_STATE_FLAG_VALUE_IKEV1 | 0x0040)
#define IKEV1_STATE_FLAG_VALUE_AGGMODE      (IKE_STATE_FLAG_VALUE_IKEV1 | 0x0080)
#define IKEV1_STATE_FLAG_VALUE_QUICKMODE    (IKE_STATE_FLAG_VALUE_IKEV1 | 0x00C0)


#define IKEV1_STATE_FLAG_VALUE_SENT     0x0020
#define IKEV1_STATE_FLAG_VALUE_SPI      0x0010
#define IKEV1_STATE_FLAG_VALUE_ADDSA    0x0008
    

#define IKEV1_STATE_INITIATOR_IDENT  (IKE_STATE_FLAG_VALUE_PHASE1 | IKE_STATE_MASK_NEGOTIATING  \
            | IKE_STATE_FLAG_VALUE_INITIATOR | IKEV1_STATE_FLAG_VALUE_IDENTMODE)

#define IKEV1_STATE_RESPONDER_IDENT  (IKE_STATE_FLAG_VALUE_PHASE1 | IKE_STATE_MASK_NEGOTIATING  \
            | IKE_STATE_FLAG_VALUE_RESPONDER | IKEV1_STATE_FLAG_VALUE_IDENTMODE)

#define IKEV1_STATE_INITIATOR_AGG  (IKE_STATE_FLAG_VALUE_PHASE1 | IKE_STATE_MASK_NEGOTIATING    \
            | IKE_STATE_FLAG_VALUE_INITIATOR | IKEV1_STATE_FLAG_VALUE_AGGMODE)

#define IKEV1_STATE_RESPONDER_AGG  (IKE_STATE_FLAG_VALUE_PHASE1 | IKE_STATE_MASK_NEGOTIATING    \
            | IKE_STATE_FLAG_VALUE_RESPONDER | IKEV1_STATE_FLAG_VALUE_AGGMODE)

#define IKEV1_STATE_INITIATOR_QUICK  (IKE_STATE_FLAG_VALUE_PHASE2 | IKE_STATE_MASK_NEGOTIATING  \
            | IKE_STATE_FLAG_VALUE_INITIATOR | IKEV1_STATE_FLAG_VALUE_QUICKMODE)

#define IKEV1_STATE_RESPONDER_QUICK  (IKE_STATE_FLAG_VALUE_PHASE2 | IKE_STATE_MASK_NEGOTIATING  \
            | IKE_STATE_FLAG_VALUE_RESPONDER | IKEV1_STATE_FLAG_VALUE_QUICKMODE)


#define IKEV1_STATE_PHASE1_ESTABLISHED         (IKE_STATE_FLAG_VALUE_IKEV1 | IKE_STATE_FLAG_VALUE_PHASE1| IKE_STATE_FLAG_VALUE_ESTABLISED)
#define IKEV1_STATE_PHASE2_ESTABLISHED         (IKE_STATE_FLAG_VALUE_IKEV1 | IKE_STATE_FLAG_VALUE_PHASE2| IKE_STATE_FLAG_VALUE_ESTABLISED)
#define IKEV1_STATE_PHASE1_EXPIRED             (IKE_STATE_FLAG_VALUE_IKEV1 | IKE_STATE_FLAG_VALUE_PHASE1| IKE_STATE_FLAG_VALUE_EXPIRED)
#define IKEV1_STATE_PHASE2_EXPIRED             (IKE_STATE_FLAG_VALUE_IKEV1 | IKE_STATE_FLAG_VALUE_PHASE2| IKE_STATE_FLAG_VALUE_EXPIRED)

    // PHASE 1 INFO
#define IKEV1_STATE_INFO                (IKE_STATE_FLAG_VALUE_IKEV1 | IKEV1_STATE_FLAG_VALUE_INFO | 0x3F)

    // IDENT MODE
#define IKEV1_STATE_IDENT_I_START       (IKEV1_STATE_INITIATOR_IDENT)
#define IKEV1_STATE_IDENT_I_MSG1SENT    (IKEV1_STATE_INITIATOR_IDENT | IKEV1_STATE_FLAG_VALUE_SENT | 1)
#define IKEV1_STATE_IDENT_I_MSG2RCVD    (IKEV1_STATE_INITIATOR_IDENT | 2)
#define IKEV1_STATE_IDENT_I_MSG3SENT    (IKEV1_STATE_INITIATOR_IDENT | IKEV1_STATE_FLAG_VALUE_SENT | 3)
#define IKEV1_STATE_IDENT_I_MSG4RCVD    (IKEV1_STATE_INITIATOR_IDENT | 4)
#define IKEV1_STATE_IDENT_I_MSG5SENT    (IKEV1_STATE_INITIATOR_IDENT | IKEV1_STATE_FLAG_VALUE_SENT | 5)
#define IKEV1_STATE_IDENT_I_MSG6RCVD    (IKEV1_STATE_INITIATOR_IDENT | 6)

#define IKEV1_STATE_IDENT_R_START       (IKEV1_STATE_RESPONDER_IDENT)
#define IKEV1_STATE_IDENT_R_MSG1RCVD    (IKEV1_STATE_RESPONDER_IDENT | 1)
#define IKEV1_STATE_IDENT_R_MSG2SENT    (IKEV1_STATE_RESPONDER_IDENT | IKEV1_STATE_FLAG_VALUE_SENT | 2)
#define IKEV1_STATE_IDENT_R_MSG3RCVD    (IKEV1_STATE_RESPONDER_IDENT | 3)
#define IKEV1_STATE_IDENT_R_MSG4SENT    (IKEV1_STATE_RESPONDER_IDENT | IKEV1_STATE_FLAG_VALUE_SENT | 4)
#define IKEV1_STATE_IDENT_R_MSG5RCVD    (IKEV1_STATE_RESPONDER_IDENT | 5)
    // AGG MODE
#define IKEV1_STATE_AGG_I_START         (IKEV1_STATE_INITIATOR_AGG)
#define IKEV1_STATE_AGG_I_MSG1SENT      (IKEV1_STATE_INITIATOR_AGG | IKEV1_STATE_FLAG_VALUE_SENT | 1)
#define IKEV1_STATE_AGG_I_MSG2RCVD      (IKEV1_STATE_INITIATOR_AGG | 2)
#define IKEV1_STATE_AGG_I_MSG3SENT      (IKEV1_STATE_INITIATOR_AGG | IKEV1_STATE_FLAG_VALUE_SENT | 3)
#define IKEV1_STATE_AGG_R_START         (IKEV1_STATE_RESPONDER_AGG)
#define IKEV1_STATE_AGG_R_MSG1RCVD      (IKEV1_STATE_RESPONDER_AGG | 1)
#define IKEV1_STATE_AGG_R_MSG2SENT      (IKEV1_STATE_RESPONDER_AGG | IKEV1_STATE_FLAG_VALUE_SENT | 2)
#define IKEV1_STATE_AGG_R_MSG3RCVD      (IKEV1_STATE_RESPONDER_AGG | 3)
    // QUICK MODE
#define IKEV1_STATE_QUICK_I_START       (IKEV1_STATE_INITIATOR_QUICK)
#define IKEV1_STATE_QUICK_I_GETSPISENT  (IKEV1_STATE_INITIATOR_QUICK | IKEV1_STATE_FLAG_VALUE_SENT | IKEV1_STATE_FLAG_VALUE_SPI)
#define IKEV1_STATE_QUICK_I_GETSPIDONE  (IKEV1_STATE_INITIATOR_QUICK | IKEV1_STATE_FLAG_VALUE_SPI)
#define IKEV1_STATE_QUICK_I_MSG1SENT    (IKEV1_STATE_INITIATOR_QUICK | IKEV1_STATE_FLAG_VALUE_SENT | 1)
#define IKEV1_STATE_QUICK_I_MSG2RCVD    (IKEV1_STATE_INITIATOR_QUICK | 2)
#define IKEV1_STATE_QUICK_I_MSG3SENT    (IKEV1_STATE_INITIATOR_QUICK | IKEV1_STATE_FLAG_VALUE_SENT | 3)
#define IKEV1_STATE_QUICK_I_ADDSA       (IKEV1_STATE_INITIATOR_QUICK | IKEV1_STATE_FLAG_VALUE_ADDSA)
#define IKEV1_STATE_QUICK_R_START       (IKEV1_STATE_RESPONDER_QUICK)
#define IKEV1_STATE_QUICK_R_MSG1RCVD    (IKEV1_STATE_RESPONDER_QUICK | 1)
#define IKEV1_STATE_QUICK_R_GETSPISENT  (IKEV1_STATE_RESPONDER_QUICK | IKEV1_STATE_FLAG_VALUE_SENT | IKEV1_STATE_FLAG_VALUE_SPI)
#define IKEV1_STATE_QUICK_R_GETSPIDONE  (IKEV1_STATE_RESPONDER_QUICK | IKEV1_STATE_FLAG_VALUE_SPI)
#define IKEV1_STATE_QUICK_R_MSG2SENT    (IKEV1_STATE_RESPONDER_QUICK | IKEV1_STATE_FLAG_VALUE_SENT | 2)
#define IKEV1_STATE_QUICK_R_MSG3RCVD    (IKEV1_STATE_RESPONDER_QUICK | 3)
#define IKEV1_STATE_QUICK_R_COMMIT      (IKEV1_STATE_RESPONDER_QUICK | 4)
#define IKEV1_STATE_QUICK_R_ADDSA       (IKEV1_STATE_RESPONDER_QUICK | IKEV1_STATE_FLAG_VALUE_ADDSA)


extern void fsm_set_state(int *var, int state);
//================================
// Version Agnostic Events
//================================
extern void fsm_api_handle_connect (struct sockaddr_storage *remote, const int connect_mode);
extern void fsm_api_handle_disconnect (struct sockaddr_storage *remote, const char *reason);

extern void fsm_pfkey_handle_acquire (phase2_handle_t *iph2);
extern void fsm_pfkey_getspi_complete (phase2_handle_t *iph2);

extern void fsm_isakmp_initial_pkt (vchar_t *msg, struct sockaddr_storage *local, struct sockaddr_storage *remote);

//================================
// IKEv1 Events
//================================

extern int fsm_ikev1_phase1_process_payloads (phase1_handle_t *iph1, vchar_t *msg);
extern int fsm_ikev1_phase2_process_payloads (phase2_handle_t *iph2, vchar_t *msg);
extern int fsm_ikev1_phase1_send_response(phase1_handle_t *iph1, vchar_t *msg);
extern int fsm_ikev1_phase2_send_response(phase2_handle_t *iph2, vchar_t *msg);


#endif /* _FSM_H */
