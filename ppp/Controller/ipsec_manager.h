/*
 * Copyright (c) 2000-2009 Apple Computer, Inc. All rights reserved.
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

#ifndef __IPSEC_MANAGER__
#define __IPSEC_MANAGER__


enum {
    IPSEC_NOT_ASSERTED = 0,
    IPSEC_ASSERTED_IDLE,
    IPSEC_ASSERTED_INITIALIZE,
    IPSEC_ASSERTED_CONTACT,
    IPSEC_ASSERTED_PHASE1,
    IPSEC_ASSERTED_PHASE2
};

#define IPSEC_ASSERT(ipsec, x) do {											\
										if (ipsec.phase == IPSEC_RUNNING) {	\
											ipsec.asserted = x;					\
										}											\
									} while (0)
#define IPSEC_UNASSERT(ipsec) (ipsec.asserted = IPSEC_NOT_ASSERTED)

#define IPSEC_ASSERT_IDLE(ipsec) IPSEC_ASSERT(ipsec, IPSEC_ASSERTED_IDLE)
#define IPSEC_ASSERT_INITIALIZE(ipsec) IPSEC_ASSERT(ipsec, IPSEC_ASSERTED_INITIALIZE)
#define IPSEC_ASSERT_CONTACT(ipsec) IPSEC_ASSERT(ipsec, IPSEC_ASSERTED_CONTACT)
#define IPSEC_ASSERT_PHASE1(ipsec) IPSEC_ASSERT(ipsec, IPSEC_ASSERTED_PHASE1)
#define IPSEC_ASSERT_PHASE2(ipsec) IPSEC_ASSERT(ipsec, IPSEC_ASSERTED_PHASE2)

#define IPSEC_IS_ASSERTED(ipsec, x) (ipsec.phase == IPSEC_RUNNING && ipsec.asserted == x)
#define IPSEC_IS_NOT_ASSERTED(ipsec) (ipsec.asserted == 0)
#define IPSEC_IS_ASSERTED_IDLE(ipsec) IPSEC_IS_ASSERTED(ipsec, IPSEC_ASSERTED_IDLE)
#define IPSEC_IS_ASSERTED_INITIALIZE(ipsec) IPSEC_IS_ASSERTED(ipsec, IPSEC_ASSERTED_INITIALIZE)
#define IPSEC_IS_ASSERTED_CONTACT(ipsec) IPSEC_IS_ASSERTED(ipsec, IPSEC_ASSERTED_CONTACT)
#define IPSEC_IS_ASSERTED_PHASE1(ipsec) IPSEC_IS_ASSERTED(ipsec, IPSEC_ASSERTED_PHASE1)
#define IPSEC_IS_ASSERTED_PHASE2(ipsec) IPSEC_IS_ASSERTED(ipsec, IPSEC_ASSERTED_PHASE2)
#define IPSEC_IS_ASSERTED_ANY(ipsec) (ipsec.phase == IPSEC_RUNNING && ipsec.asserted)

/* try to handle as many types of dns delimiters as possible */
#define GET_SPLITDNS_DELIM(data, delim) do {	\
		if (strstr(data, ",")) {				\
			delim = ",";						\
		} else if (strstr(data, ";")) {			\
			delim = ";";						\
		} else if (strstr(data, "\n")) {		\
			delim = "\n";						\
		} else if (strstr(data, "\r")) {		\
			delim = "\r";						\
		} else if (strstr(data, " ")) {			\
			delim = " ";						\
		} else {								\
			delim = "\0";						\
		}										\
	} while(0)

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
void ipsec_cellular_event(struct service *serv, int event);

#endif