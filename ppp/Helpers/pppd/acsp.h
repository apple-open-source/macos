/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#ifndef __ACSP_H__
#define __ACSP_H__

// ACSP payload dumping macros... for use within acsp_data_printpkt only
#define ACSP_PRINT_PAYLOAD(payload_name, flag_names...)															\
	(payload_name)?																								\
		printer(arg, " <payload len %d, packet seq %d, %s, flags:%s%s%s%s>",  len, pkt->seq, payload_name, ## flag_names) :	\
		printer(arg, " <payload len %d, packet seq %d, CI_TYPE %d, flags:%s%s%s%s>", len, pkt->seq, pkt->type, ## flag_names)

#define ACSP_PRINTPKT_PAYLOAD(payload_name)												\
	ACSP_PRINT_PAYLOAD(payload_name,													\
						((flags & ACSP_FLAG_START) != 0)? " START" : "",				\
						((flags & ACSP_FLAG_END) != 0)? " END" : "",					\
						((flags & ACSP_FLAG_REQUIRE_ACK) != 0)? " REQUIRE-ACK" : "",	\
						((flags & ACSP_FLAG_ACK) != 0)? " ACK" : "")

//
// ACSP function prototypes
//
void acsp_start(int mtu);
void acsp_stop(void);
void acsp_init_plugins(void *arg, uintptr_t phase);

void acsp_data_input(int unit, u_char *pkt, int len);
int acsp_printpkt(u_char *, int, void (*) __P((void *, char *, ...)), void *);


#endif