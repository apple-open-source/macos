/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*
 * auth.h - .
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */


void auth_reset __P((struct ppp *));	/* check what secrets we have */
void auth_peer_fail __P((struct ppp *ppp, int));
         /* peer failed to authenticate itself */
void auth_peer_success __P((struct ppp *ppp, int, char *, int));
         /* peer successfully authenticated itself */
void auth_withpeer_fail __P((struct ppp *ppp, int));
         /* we failed to authenticate ourselves */
void auth_withpeer_success __P((struct ppp *ppp, int));
				/* we successfully authenticated ourselves */

int  check_passwd __P((struct ppp *, char *, int, char *, int, char **));
				/* Check peer-supplied username/password */
int  get_secret __P((struct ppp *ppp, char *, char *, char *, int *, int));
				/* get "secret" for chap */
int  auth_ip_addr __P((struct ppp *, u_int32_t));
				/* check if IP address is authorized */
int  bad_ip_adrs __P((u_int32_t));
				/* check if IP address is unreasonable */

void auth_link_required __P((struct ppp *ppp));	  /* we are starting to use the link */
void auth_link_terminated __P((struct ppp *ppp));  /* we are finished with the link */
void auth_link_down __P((struct ppp *ppp));	  /* the LCP layer has left the Opened state */

void auth_link_established __P((struct ppp *ppp)); /* the link is up; authenticate now */
void auth_start_networks __P((struct ppp *ppp));  /* start all the network control protos */
void auth_np_up __P((struct ppp *ppp, int));	  /* a network protocol has come up */
void auth_np_down __P((struct ppp *ppp, int));	  /* a network protocol has gone down */
void auth_np_finished __P((struct ppp *ppp, int)); /* a network protocol no longer needs link */
