/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#ifndef _MOH_MSG_H
#define _MOH_MSG_H

#include <sys/types.h>

/* local socket path */
#define MOH_PATH		"/tmp/.modemOnHold"
#define CURRENT_VERSION		1

/* MOH message paquets */
struct moh_msg_hdr {
	u_int32_t	m_type;		// type of the message
	u_int32_t	m_result;	// error code of notification message
	u_int32_t	m_cookie;	// user param
	u_int32_t	m_link;		// link for this message
	u_int32_t	m_len;		// len of the following data
};

struct moh_msg {
	u_int32_t	m_type;		// type of the message
	u_int32_t	m_result;	// error code of notification message
	u_int32_t	m_cookie;	// user param, or error num for event
	u_int32_t	m_link;		// link for this message
	u_int32_t	m_len;		// len of the following data
	u_char		m_data[1];	// msg data sent or received
};

/* codes for MOH messages */
enum {
	/* API client commands */
	MOH_VERSION = 1,
	MOH_DEVICE_SUPPORTS_HOLD,
	MOH_SESSION_SUPPORTS_HOLD,
	MOH_PUT_SESSION_ON_HOLD,
	MOH_RESUME_SESSION_ON_HOLD,
	MOH_SESSION_IS_ON_HOLD,
	MOH_SESSION_GET_STATUS
};

#endif	/* _MOH_MSG_H */

