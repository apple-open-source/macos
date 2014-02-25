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

#ifndef _IKE_SESSION_H
#define _IKE_SESSION_H

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <net/pfkeyv2.h>
#include <netinet/in.h>
#include <dispatch/dispatch.h>
#include "handler.h"
#include "ipsecSessionTracer.h"

typedef struct ike_session_id {
	struct sockaddr_storage local;
	struct sockaddr_storage remote;
} ike_session_id_t;

typedef struct ike_session_stats {
	u_int32_t							 counters[IPSECSESSIONEVENTCODE_MAX];
} ike_session_stats_t;

typedef struct ike_session_ikev1 {
	/* list of ph1s */
	int                                  active_ph1cnt;
	int                                  ph1cnt;	/* the number which is negotiated for this session */
	/* list of ph2s */
	int                                  active_ph2cnt;
	int                                  ph2cnt;	/* the number which is negotiated for this session */
} ike_session_ikev1_t;

typedef struct ike_session_sastats {
    int                                  interv_mon;
    int                                  interv_idle;
    int                                  dir_idle;
    schedule_ref                         sc_mon;
    schedule_ref                         sc_idle;

    u_int32_t                            num_in_curr_req;
    u_int32_t                            num_in_last_poll;
    struct sastat                        in_curr_req[8];
    struct sastat                        in_last_poll[8];

    u_int32_t                            num_out_curr_req;
    u_int32_t                            num_out_last_poll;
    struct sastat                        out_curr_req[8];
    struct sastat                        out_last_poll[8];
} ike_sesssion_sastats_t;


struct ike_session {
	u_int8_t				             mode;			/* mode of protocol, see ipsec.h */
	u_int16_t                            proto;			/* IPPROTO_ESP or IPPROTO_AH */

	ike_session_id_t                     session_id;

	int                                  established:1;
	int                                  ports_floated:1;
	int                                  is_cisco_ipsec:1;	
	int									 is_l2tpvpn_ipsec:1;
	int									 is_btmm_ipsec:1;
	int									 stopped_by_vpn_controller:1;
    int                                  peer_sent_data_sc_dpd:1;
    int                                  peer_sent_data_sc_idle:1;
    int                                  i_sent_data_sc_dpd:1;
    int                                  i_sent_data_sc_idle:1;
    int                                  is_client:1;
    time_t                               last_time_data_sc_detected;
    int                                  controller_awaiting_peer_resp:1;
    int                                  is_dying:1;
    int                                  is_asserted:1;
    u_int32_t                            natt_flags;
	u_int32_t                            natt_version;
	char                                *term_reason;

	struct timeval						 start_timestamp;
	struct timeval						 estab_timestamp;
	struct timeval						 stop_timestamp;
	ike_session_ikev1_t					 ikev1_state;

	ike_session_stats_t					 stats;

    ike_sesssion_sastats_t               traffic_monitor;
    schedule_ref                         sc_idle;
    schedule_ref                         sc_xauth;
    
    LIST_HEAD(_ph1tree_, phase1handle)   ph1tree;
    LIST_HEAD(_ph2tree_, phase2handle)   ph2tree;

	LIST_ENTRY(ike_session)              chain;

};

typedef enum ike_session_rekey_type {
	IKE_SESSION_REKEY_TYPE_NONE = 0,
	IKE_SESSION_REKEY_TYPE_PH1,
	IKE_SESSION_REKEY_TYPE_PH2,
} ike_session_rekey_type_t;

extern const char *	ike_session_stopped_by_vpn_disconnect;
extern const char *	ike_session_stopped_by_controller_comm_lost;
extern const char *	ike_session_stopped_by_flush;
extern const char *	ike_session_stopped_by_sleepwake;
extern const char *	ike_session_stopped_by_assert;
extern const char * ike_session_stopped_by_peer;

extern void               ike_session_init (void);
extern ike_session_t *    ike_session_create_session (ike_session_id_t *session_id);
extern void               ike_session_release_session (ike_session_t *session);
extern ike_session_t *	  ike_session_get_session (struct sockaddr_storage *, struct sockaddr_storage *, int, isakmp_index *);
extern u_int              ike_session_get_rekey_lifetime (int, u_int);
extern void               ike_session_update_mode (phase2_handle_t *iph2);
extern int                ike_session_link_phase1 (ike_session_t *, phase1_handle_t *);
extern int                ike_session_link_phase2 (ike_session_t *, phase2_handle_t *);
extern int                ike_session_link_ph2_to_ph1 (phase1_handle_t *, phase2_handle_t *);
extern int                ike_session_unlink_phase1 (phase1_handle_t *);
extern int                ike_session_unlink_phase2 (phase2_handle_t *);
extern int                ike_session_has_other_established_ph1 (ike_session_t *, phase1_handle_t *);
extern int                ike_session_has_other_negoing_ph1 (ike_session_t *, phase1_handle_t *);
extern int                ike_session_has_other_established_ph2 (ike_session_t *, phase2_handle_t *);
extern int                ike_session_has_other_negoing_ph2 (ike_session_t *, phase2_handle_t *);
extern phase1_handle_t  * ike_session_update_ph1_ph2tree (phase1_handle_t *);
extern phase1_handle_t  * ike_session_update_ph2_ph1bind (phase2_handle_t *);
extern void               ike_session_ikev1_float_ports (phase1_handle_t *);
extern void               ike_session_ph2_established (phase2_handle_t *);
extern void               ike_session_replace_other_ph1 (phase1_handle_t *, phase1_handle_t *);
extern void               ike_session_cleanup_other_established_ph1s (ike_session_t *, phase1_handle_t *);
extern void               ike_session_cleanup_other_established_ph2s (ike_session_t *, phase2_handle_t *);
extern void				  ike_session_stopped_by_controller (ike_session_t *, const char *);
extern void				  ike_sessions_stopped_by_controller (struct sockaddr_storage *, int, const char *);
extern void               ike_session_purge_ph2s_by_ph1 (phase1_handle_t *);
extern phase1_handle_t  * ike_session_get_established_ph1 (ike_session_t *);
extern phase1_handle_t *  ike_session_get_established_or_negoing_ph1 (ike_session_t *);
extern void               ike_session_update_ph2_ports (phase2_handle_t *);
extern u_int32_t          ike_session_get_sas_for_stats (ike_session_t *, u_int8_t, u_int32_t *, struct sastat  *, u_int32_t);
extern void               ike_session_update_traffic_idle_status (ike_session_t *, u_int32_t, struct sastat *, u_int32_t);
extern void               ike_session_cleanup (ike_session_t *, const char *);
extern int                ike_session_has_negoing_ph1 (ike_session_t *);
extern int                ike_session_has_established_ph1 (ike_session_t *);
extern int                ike_session_has_negoing_ph2 (ike_session_t *);
extern int                ike_session_has_established_ph2 (ike_session_t *);
extern void               ike_session_cleanup_ph1s_by_ph2 (phase2_handle_t *);
extern int                ike_session_is_client_ph2_rekey (phase2_handle_t *);
extern int                ike_session_is_client_ph1_rekey (phase1_handle_t *);
extern int                ike_session_is_client_ph1 (phase1_handle_t *);
extern int                ike_session_is_client_ph2 (phase2_handle_t *);
extern void               ike_session_start_xauth_timer (phase1_handle_t *);
extern void               ike_session_stop_xauth_timer (phase1_handle_t *);
extern int                ike_session_get_sainfo_r (phase2_handle_t *);
extern int                ike_session_get_proposal_r (phase2_handle_t *);
extern void               ike_session_update_natt_version (phase1_handle_t *);
extern int                ike_session_get_natt_version (phase1_handle_t *);
extern int                ike_session_drop_rekey (ike_session_t *, ike_session_rekey_type_t);
extern void               ike_session_sweep_sleepwake (void);
extern int                ike_session_assert (struct sockaddr_storage *, struct sockaddr_storage *);
extern int                ike_session_assert_session (ike_session_t *);
extern void               ike_session_unbindph12(phase2_handle_t *);  
extern void               ike_session_ph2_retransmits (phase2_handle_t *);
extern void               ike_session_ph1_retransmits (phase1_handle_t *);

#endif /* _IKE_SESSION_H */
