/*
 *  ipsec_manager.h
 *  ppp
 *
 *  Created by Christophe Allie on 10/30/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __IPSEC_MANAGER__
#define __IPSEC_MANAGER__


enum {
    IPSEC_IDLE = 0,
    IPSEC_INITIALIZE,
    IPSEC_CONTACT,
    IPSEC_PHASE1,
    IPSEC_PHASE1AUTH,
    IPSEC_PHASE2,
    IPSEC_RUNNING,
    IPSEC_TERMINATE,
    IPSEC_WAITING
};

u_int16_t ipsec_subtype(CFStringRef subtypeRef);

int ipsec_new_service(struct service *serv);
int ipsec_dispose_service(struct service *serv);
int ipsec_setup_service(struct service *serv);

int ipsec_start(struct service *serv, CFDictionaryRef options, uid_t uid, gid_t gid, mach_port_t bootstrap, u_int8_t onTraffic, u_int8_t onDemand);
int ipsec_stop(struct service *serv, int signal);
int ipsec_getstatus(struct service *serv);
int ipsec_copyextendedstatus (struct service *serv, void **reply, u_int16_t *replylen);
int ipsec_copystatistics(struct service *serv, void **reply, u_int16_t *replylen);
int ipsec_getconnectdata(struct service *serv, void **reply, u_int16_t *replylen, int all);

int ipsec_can_sleep(struct service *serv);
int ipsec_will_sleep(struct service *serv, int checking);
void ipsec_wake_up(struct service *serv);
void ipsec_log_out(struct service *serv);
void ipsec_log_in(struct service *serv);
void ipsec_log_switch(struct service *serv);
void ipsec_ipv4_state_changed(struct service *serv);
void ipsec_user_notification_callback(struct service *serv, CFUserNotificationRef userNotification, CFOptionFlags responseFlags);
int ipsec_ondemand_add_service_data(struct service *serv, CFMutableDictionaryRef ondemand_dict);

#endif