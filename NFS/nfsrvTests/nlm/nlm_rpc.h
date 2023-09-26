/*
 * Copyright (c) 1999-2022 Apple Inc. All rights reserved.
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
/*
 * Copyright (c) 1992, 1993, 1994
 *    The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef nlm_rpc_h
#define nlm_rpc_h

#include "nlm_prot.h"

/* --------- NLM_SM --------- */

void NLM_SM_doNullRPC(CLIENT *clnt);

/* --------- NLMv1 --------- */

void NLM_doNullRPC(CLIENT *clnt);
nlm_testres *NLM_doTestRPC(CLIENT *clnt, netobj *cookie, bool_t exclusive, struct nlm_lock *alock);
nlm_res *NLM_doLockRPC(CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, struct nlm_lock *alock, bool_t reclaim, int state);
nlm_res *NLM_doCancelRPC(CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, struct nlm_lock *alock);
nlm_res *NLM_doUnlockRPC(CLIENT *clnt, netobj *cookie, struct nlm_lock *alock);

/* --------- NLMv3 --------- */

void NLM3_doNullRPC(CLIENT *clnt);
nlm_testres *NLM3_doTestRPC(CLIENT *clnt, netobj *cookie, bool_t exclusive, struct nlm_lock *alock);
nlm_res *NLM3_doLockRPC(CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, struct nlm_lock *alock, bool_t reclaim, int state);
nlm_res *NLM3_doCancelRPC(CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, struct nlm_lock *alock);
nlm_res *NLM3_doUnlockRPC(CLIENT *clnt, netobj *cookie, struct nlm_lock *alock);
nlm_shareres *NLM3_doShareRPC(CLIENT *clnt, netobj *cookie, struct nlm_share *share, bool_t reclaim);
nlm_shareres *NLM3_doUnshareRPC(CLIENT *clnt, netobj *cookie, struct nlm_share *share, bool_t reclaim);
void *NLM3_doFreeAllRPC(CLIENT *clnt, char *name);

/* --------- NLMv4 --------- */

void NLM4_doNullRPC(CLIENT *clnt);
nlm4_testres *NLM4_doTestRPC(CLIENT *clnt, netobj *cookie, bool_t exclusive, struct nlm4_lock *alock);
nlm4_res *NLM4_doLockRPC(CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, struct nlm4_lock *alock, bool_t reclaim, int state);
nlm4_res *NLM4_doCancelRPC(CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, struct nlm4_lock *alock);
nlm4_res *NLM4_doUnlockRPC(CLIENT *clnt, netobj *cookie, struct nlm4_lock *alock);
nlm4_shareres *NLM4_doShareRPC(CLIENT *clnt, netobj *cookie, struct nlm4_share *share, bool_t reclaim);
nlm4_shareres *NLM4_doUnshareRPC(CLIENT *clnt, netobj *cookie, struct nlm4_share *share, bool_t reclaim);
void *NLM4_doFreeAllRPC(CLIENT *clnt, char *name);

/* --------- Helpers --------- */

union nlm_alock_t {
	struct nlm_lock    lock;
	struct nlm4_lock   lock4;
};

union nlm_share_t {
	struct nlm_share    share;
	struct nlm4_share   share4;
};

void doNLMNullRPC(int version, CLIENT *clnt);
void doNLMTestRPC(int version, int expected_error, CLIENT *clnt, netobj *cookie, bool_t exclusive, union nlm_alock_t *alock, union nlm_alock_t *expected);
void doNLMLockRPC(int version, int expected_error, CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, union nlm_alock_t *alock, bool_t reclaim, int state);
void doNLMCancelRPC(int version, int expected_error, CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, union nlm_alock_t *alock);
void doNLMUnlockRPC(int version, int expected_error, CLIENT *clnt, netobj *cookie, union nlm_alock_t *alock);
void doNLMShareRPC(int version, int expected_error, CLIENT *clnt, netobj *cookie, union nlm_share_t *share, bool_t reclaim);
void doNLMUnshareRPC(int version, int expected_error, CLIENT *clnt, netobj *cookie, union nlm_share_t *share, bool_t reclaim);
void doNLMFreeAllRPC(int version, CLIENT *clnt, char *name);

#endif /* nlm_rpc_h */
