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

/*!
 * @header nibind_glue
 */

#ifdef __cplusplus
extern "C" {
#endif


	void *nibind_new(struct in_addr *);
	ni_status nibind_listreg(void *, nibind_registration **, unsigned *);
	ni_status nibind_getregister(void *, char *, nibind_addrinfo **);
	ni_status nibind_register(void *, nibind_registration *);
	ni_status nibind_unregister(void *, char *);
	ni_status nibind_createmaster(void *, char *);
	ni_status nibind_createclone(void *, char *, char *, struct in_addr *, char *);
	ni_status nibind_destroydomain(void *, char *);
	void nibind_free(void *);

#ifdef __cplusplus
}
#endif

