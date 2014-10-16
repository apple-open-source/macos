/*
 * Copyright (c) 2008, 2012, 2013 Apple Computer, Inc. All rights reserved.
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


#include "fsm.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>

#include "var.h"
#include "misc.h"
#include "session.h"
#include "isakmp.h"
#include "ike_session.h"
#include "isakmp_var.h"
#include "isakmp_ident.h"
#include "isakmp_agg.h"
#include "isakmp_quick.h"
#include "isakmp_inf.h"
#include "vpn_control_var.h"

#include "plog.h"
#include "schedule.h"

void
fsm_set_state(int *var, int state)
{   
    *var = state;
    plog(ASL_LEVEL_DEBUG, "****** state changed to: %s\n", s_isakmp_state(0, 0, state));                                                                                    
}


//================================
// Version Agnostic Events
//================================
void
fsm_api_handle_connect (struct sockaddr_storage *remote, const int connect_mode)
{
    
    
}

void
fsm_api_handle_disconnect (struct sockaddr_storage *remote, const char *reason)
{
    
    
}

void
fsm_pfkey_handle_acquire (phase2_handle_t *iph2)
{
    
    
}

void
fsm_pfkey_getspi_complete (phase2_handle_t *iph2)
{
    
}

void
fsm_isakmp_initial_pkt (vchar_t *pkt, struct sockaddr_storage *local, struct sockaddr_storage *remote)
{
    
    
}

//================================
// IKEv1 Events
//================================

int
fsm_ikev1_phase1_process_payloads (phase1_handle_t *iph1, vchar_t *msg)
{
    
    int error = 0;
#ifdef ENABLE_STATS
    struct timeval start, end;

    gettimeofday(&start, NULL);
#endif

    switch (iph1->status) {
        case IKEV1_STATE_PHASE1_ESTABLISHED:
            return 0;     // ignore - already established
            
        case IKEV1_STATE_IDENT_I_MSG1SENT:
            error = ident_i2recv(iph1, msg);
            break;
            
        case IKEV1_STATE_IDENT_I_MSG3SENT:
            error = ident_i4recv(iph1, msg);
            break;
            
        case IKEV1_STATE_IDENT_I_MSG5SENT:
            error = ident_i6recv(iph1, msg);
            break;
            
        case IKEV1_STATE_IDENT_R_START:
            error = ident_r1recv(iph1, msg);
            if (error) {
                plog(ASL_LEVEL_ERR, "failed to pre-process packet.\n");
                goto fail;
            }
            break;
            
        case IKEV1_STATE_IDENT_R_MSG2SENT:
            error = ident_r3recv(iph1, msg);
            break;
            
        case IKEV1_STATE_IDENT_R_MSG4SENT:
            error = ident_r5recv(iph1, msg);
            break;

        case IKEV1_STATE_AGG_R_START:
            error = agg_r1recv(iph1, msg);
            if (error) {
                plog(ASL_LEVEL_ERR, "failed to pre-process packet.\n");
                goto fail;
            }
            break;
            
        case IKEV1_STATE_AGG_I_MSG1SENT:
            error = agg_i2recv(iph1, msg);
            break;
                        
        case IKEV1_STATE_AGG_R_MSG2SENT:
            error = agg_r3recv(iph1, msg);
            break;
            
        default:
            // log invalid state
            error = -1;
            break;
    }
    if (error)
        return 0; // ignore error and keep phase 1 handle
        
    VPTRINIT(iph1->sendbuf);
    /* turn off schedule */
    SCHED_KILL(iph1->scr);
    
    /* send */
    plog(ASL_LEVEL_DEBUG, "===\n");
    if ((error = fsm_ikev1_phase1_send_response(iph1, msg))) {
        plog(ASL_LEVEL_ERR, "failed to process packet.\n");
        goto fail;    
    }
    
#ifdef ENABLE_STATS
    gettimeofday(&end, NULL);
    syslog(LOG_NOTICE, "%s(%s): %8.6f",
           "Phase 1", s_isakmp_state(iph1->etype, iph1->side, iph1->status),
           timedelta(&start, &end));
#endif
    if (FSM_STATE_IS_ESTABLISHED(iph1->status))
        ikev1_phase1_established(iph1);
        
    return 0;
        
fail:
        plog(ASL_LEVEL_ERR, "Phase 1 negotiation failed.\n");
        ike_session_unlink_phase1(iph1);   
        return error;
        
}


int
fsm_ikev1_phase1_send_response(phase1_handle_t *iph1, vchar_t *msg)
{
    
    int error = 0;
    
    switch (iph1->status) {
        case IKEV1_STATE_IDENT_I_START:
            error = ident_i1send(iph1, msg);
            break;
            
        case IKEV1_STATE_IDENT_I_MSG2RCVD:
            error = ident_i3send(iph1, msg);
            break;
            
        case IKEV1_STATE_IDENT_I_MSG4RCVD:
            error = ident_i5send(iph1, msg);
            break;
            
        case IKEV1_STATE_IDENT_I_MSG6RCVD:
            error = ident_ifinalize(iph1, msg);
            break;
            
        case IKEV1_STATE_IDENT_R_MSG1RCVD:
            error = ident_r2send(iph1, msg);
            break;
            
        case IKEV1_STATE_IDENT_R_MSG3RCVD:
            error = ident_r4send(iph1, msg);
            break;
            
        case IKEV1_STATE_IDENT_R_MSG5RCVD:
            error = ident_r6send(iph1, msg);
            break;
            
        case IKEV1_STATE_AGG_I_START:
            error = agg_i1send(iph1, msg);
            break;
            
        case IKEV1_STATE_AGG_I_MSG2RCVD:
            error = agg_i3send(iph1, msg);
            break;
            
        case IKEV1_STATE_AGG_R_MSG1RCVD:
            error = agg_r2send(iph1, msg);
            break;
            
        case IKEV1_STATE_AGG_R_MSG3RCVD:
            error = agg_rfinalize(iph1, msg);
            break;
            
        default:
            // log invalid state
            error = -1;
            break;;
    }
    
    if (error) {
        u_int32_t address;
        if (iph1->remote->ss_family == AF_INET)
            address = ((struct sockaddr_in *)iph1->remote)->sin_addr.s_addr;
        else {
            address = 0;
        }
        vpncontrol_notify_ike_failed(error, FROM_LOCAL, address, 0, NULL);
    }
    
    return error;
}

int
fsm_ikev1_phase2_process_payloads (phase2_handle_t *iph2, vchar_t *msg)
{
    
    int error = 0;
#ifdef ENABLE_STATS
    struct timeval start, end;

    gettimeofday(&start, NULL);
#endif
    
    switch (iph2->status) {
            /* ignore a packet */
        case IKEV1_STATE_PHASE2_ESTABLISHED:
        case IKEV1_STATE_QUICK_I_GETSPISENT:
        case IKEV1_STATE_QUICK_R_GETSPISENT:
            return 0;

        case IKEV1_STATE_QUICK_I_MSG1SENT:
            error = quick_i2recv(iph2, msg);
            break;
        
        case IKEV1_STATE_QUICK_I_MSG3SENT:
            error = quick_i4recv(iph2, msg);
            break;
            
        case IKEV1_STATE_QUICK_R_START:
            error = quick_r1recv(iph2, msg);
            break;
            
        case IKEV1_STATE_QUICK_R_MSG2SENT:
            error = quick_r3recv(iph2, msg);
            break;
            
        default:
            // log invalid state
            error = -1;
            break;
    }
    
    if (error) {
        plog(ASL_LEVEL_ERR, "failed to pre-process packet.\n");
        if (error == ISAKMP_INTERNAL_ERROR)
            fatal_error(-1);
        isakmp_info_send_n1(iph2->ph1, error, NULL);
        goto fail;
    }
    
    /* when using commit bit, status will be reached here. */
    //if (iph2->status == PHASE2ST_ADDSA)	//%%% BUG FIX - wrong place
    //	return 0;
    
    /* free resend buffer */
    if (iph2->sendbuf == NULL && iph2->status != IKEV1_STATE_QUICK_R_MSG1RCVD) {
        plog(ASL_LEVEL_ERR,
             "no buffer found as sendbuf\n"); 
        error = -1;
        goto fail;
    }
    VPTRINIT(iph2->sendbuf);
    
    /* turn off schedule */
    SCHED_KILL(iph2->scr);
    
    /* when using commit bit, will be finished here - no more packets to send */
    if (iph2->status == IKEV1_STATE_QUICK_I_ADDSA)
        return 0;
    
    error = fsm_ikev1_phase2_send_response(iph2, msg);
    if (error) {
        plog(ASL_LEVEL_ERR, "failed to process packet.\n");
        goto fail;
    }

#ifdef ENABLE_STATS
    gettimeofday(&end, NULL);
    syslog(LOG_NOTICE, "%s(%s): %8.6f",
           "Phase 2",
           s_isakmp_state(ISAKMP_ETYPE_QUICK, iph2->side, iph2->status),
           timedelta(&start, &end));
#endif

    return 0;
    
fail:
    plog(ASL_LEVEL_ERR, "Phase 2 negotiation failed.\n");
    ike_session_unlink_phase2(iph2);   
    return error;
    
}

int
fsm_ikev1_phase2_send_response(phase2_handle_t *iph2, vchar_t *msg)
{
    
    int error = 0;
    
    switch (iph2->status) {
        case IKEV1_STATE_QUICK_R_MSG1RCVD:
            error = quick_rprep(iph2, msg);
            break;
            
        case IKEV1_STATE_QUICK_I_GETSPIDONE:
            error = quick_i1send(iph2, msg);
            break;
            
        case IKEV1_STATE_QUICK_I_MSG2RCVD:
            error = quick_i3send(iph2, msg);
            break;
                        
        case IKEV1_STATE_QUICK_R_GETSPIDONE:
            error = quick_r2send(iph2, msg);
            break;
            
        case IKEV1_STATE_QUICK_R_MSG3RCVD:
            error = quick_r4send(iph2, msg);
            break;
            
        case IKEV1_STATE_QUICK_R_COMMIT:
            error = quick_rfinalize(iph2, msg);
            break;
            
        default:
            // log invalid state
            error = -1;
            break;;
    }
    return error;
    
}
