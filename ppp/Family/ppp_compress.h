/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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


#ifndef __PPP_COMP_H__
#define __PPP_COMP_H__


int ppp_comp_init();
int ppp_comp_dispose();
void ppp_comp_alloc(struct ppp_if *wan);
void ppp_comp_dealloc(struct ppp_if *wan);
int ppp_comp_setcompressor(struct ppp_if *wan, struct ppp_option_data *odp);
void ppp_comp_getstats(struct ppp_if *wan, struct ppp_comp_stats *stats);
void ppp_comp_ccp(struct ppp_if *wan, struct mbuf *m, int rcvd);
void ppp_comp_close(struct ppp_if *wan);
int ppp_comp_compress(struct ppp_if *wan, struct mbuf **m);
int ppp_comp_incompress(struct ppp_if *wan, struct mbuf *m);
int ppp_comp_decompress(struct ppp_if *wan, struct mbuf **m);


#endif
