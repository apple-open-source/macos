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

#import <XCTest/XCTest.h>

#include "nlm_rpc.h"

/* --------- SM --------- */

void
NLM_SM_doNullRPC(CLIENT *clnt)
{
	void *arg = NULL;
	void *result = NULL;

	result = nlm_sm_null_0((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm_sm_null_0 returned null");
	}
}

/* --------- NLMv1 --------- */

void
NLM_doNullRPC(CLIENT *clnt)
{
	void *arg = NULL;
	void *result = NULL;

	result = nlm_null_1((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm_null_1 returned null");
	}
}

nlm_testres *
NLM_doTestRPC(CLIENT *clnt, netobj *cookie, bool_t exclusive, struct nlm_lock *alock)
{
	nlm_testargs arg = { .cookie = *cookie, .exclusive = exclusive, .alock = *alock };
	nlm_testres *result = NULL;

	result = nlm_test_1((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm_test_1 returned null");
	}

	return result;
}

nlm_res *
NLM_doLockRPC(CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, struct nlm_lock *alock, bool_t reclaim, int state)
{
	nlm_lockargs arg = { .cookie = *cookie, .block = block, .exclusive = exclusive, .alock = *alock, .reclaim = reclaim, .state = state };
	nlm_res *result = NULL;

	result = nlm_lock_1((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm_lock_1 returned null");
	}

	return result;
}

nlm_res *
NLM_doCancelRPC(CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, struct nlm_lock *alock)
{
	nlm_cancargs arg = { .cookie = *cookie, .block = block, .exclusive = exclusive, .alock = *alock };
	nlm_res *result = NULL;

	result = nlm_cancel_1((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm_cancel_1 returned null");
	}

	return result;
}

nlm_res *
NLM_doUnlockRPC(CLIENT *clnt, netobj *cookie, struct nlm_lock *alock)
{
	nlm_unlockargs arg = { .cookie = *cookie, .alock = *alock };
	nlm_res *result = NULL;

	result = nlm_unlock_1((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm_unlock_1 returned null");
	}

	return result;
}

/* --------- NLMv3 --------- */

void
NLM3_doNullRPC(CLIENT *clnt)
{
	void *arg = NULL;
	void *result = NULL;

	result = nlm_null_3((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm_null_3 returned null");
	}
}

nlm_testres *
NLM3_doTestRPC(CLIENT *clnt, netobj *cookie, bool_t exclusive, struct nlm_lock *alock)
{
	nlm_testargs arg = { .cookie = *cookie, .exclusive = exclusive, .alock = *alock };
	nlm_testres *result = NULL;

	result = nlm_test_3((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm_test_3 returned null");
	}

	return result;
}

nlm_testres *
NLM3_doTestMsgRPC(CLIENT *clnt, netobj *cookie, bool_t exclusive, struct nlm_lock *alock)
{
	nlm_testargs arg = { .cookie = *cookie, .exclusive = exclusive, .alock = *alock };
	nlm_testres *result = NULL;

	result = nlm_test_msg_3((void*)&arg, clnt);
	if (result != NULL) {
		XCTFail("nlm_test_3 returned null");
	}

	result = nlm_test_res_3((void*)&arg, clnt);


	return result;
}

nlm_res *
NLM3_doLockRPC(CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, struct nlm_lock *alock, bool_t reclaim, int state)
{
	nlm_lockargs arg = { .cookie = *cookie, .block = block, .exclusive = exclusive, .alock = *alock, .reclaim = reclaim, .state = state };
	nlm_res *result = NULL;

	result = nlm_lock_3((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm3_lock_3 returned null");
	}

	return result;
}

nlm_res *
NLM3_doCancelRPC(CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, struct nlm_lock *alock)
{
	nlm_cancargs arg = { .cookie = *cookie, .block = block, .exclusive = exclusive, .alock = *alock };
	nlm_res *result = NULL;

	result = nlm_cancel_3((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm_cancel_3 returned null");
	}

	return result;
}

nlm_res *
NLM3_doUnlockRPC(CLIENT *clnt, netobj *cookie, struct nlm_lock *alock)
{
	nlm_unlockargs arg = { .cookie = *cookie, .alock = *alock };
	nlm_res *result = NULL;

	result = nlm_unlock_3((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm_unlock_3 returned null");
	}

	return result;
}

nlm_shareres *
NLM3_doShareRPC(CLIENT *clnt, netobj *cookie, struct nlm_share *share, bool_t reclaim)
{
	nlm_shareargs arg = { .cookie = *cookie, .share = *share, .reclaim = reclaim };
	nlm_shareres *result = NULL;

	result = nlm_share_3((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm_share_3 returned null");
	}

	return result;
}

nlm_shareres *
NLM3_doUnshareRPC(CLIENT *clnt, netobj *cookie, struct nlm_share *share, bool_t reclaim)
{
	nlm_shareargs arg = { .cookie = *cookie, .share = *share, .reclaim = reclaim };
	nlm_shareres *result = NULL;

	result = nlm_unshare_3((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm_unshare_3 returned null");
	}

	return result;
}

void *
NLM3_doFreeAllRPC(CLIENT *clnt, char *name)
{
	nlm_notify arg = { .name = name, .state = 0};
	char *result = NULL;

	result = nlm_free_all_3((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm_free_all_3 returned null");
	}

	return result;
}


/* --------- NLMv4 --------- */

void
NLM4_doNullRPC(CLIENT *clnt)
{
	void *arg = NULL;
	void *result = NULL;

	result = nlm4_null_4((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm4_null_4 returned null");
	}
}

nlm4_testres *
NLM4_doTestRPC(CLIENT *clnt, netobj *cookie, bool_t exclusive, struct nlm4_lock *alock)
{
	nlm4_testargs arg = { .cookie = *cookie, .exclusive = exclusive, .alock = *alock };
	nlm4_testres *result = NULL;

	result = nlm4_test_4((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm4_test_4 returned null");
	}

	return result;
}

nlm4_res *
NLM4_doLockRPC(CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, struct nlm4_lock *alock, bool_t reclaim, int state)
{
	nlm4_lockargs arg = { .cookie = *cookie, .block = block, .exclusive = exclusive, .alock = *alock, .reclaim = reclaim, .state = state };
	nlm4_res *result = NULL;

	result = nlm4_lock_4((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm4_lock_4 returned null");
	}

	return result;
}

nlm4_res *
NLM4_doCancelRPC(CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, struct nlm4_lock *alock)
{
	nlm4_cancargs arg = { .cookie = *cookie, .block = block, .exclusive = exclusive, .alock = *alock };
	nlm4_res *result = NULL;

	result = nlm4_cancel_4((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm4_cancel_4 returned null");
	}

	return result;
}

nlm4_res *
NLM4_doUnlockRPC(CLIENT *clnt, netobj *cookie, struct nlm4_lock *alock)
{
	nlm4_unlockargs arg = { .cookie = *cookie, .alock = *alock };
	nlm4_res *result = NULL;

	result = nlm4_unlock_4((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm4_unlock_4 returned null");
	}

	return result;
}

nlm4_shareres *
NLM4_doShareRPC(CLIENT *clnt, netobj *cookie, struct nlm4_share *share, bool_t reclaim)
{
	nlm4_shareargs arg = { .cookie = *cookie, .share = *share, .reclaim = reclaim };
	nlm4_shareres *result = NULL;

	result = nlm4_share_4((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm4_share_4 returned null");
	}

	return result;
}

nlm4_shareres *
NLM4_doUnshareRPC(CLIENT *clnt, netobj *cookie, struct nlm4_share *share, bool_t reclaim)
{
	nlm4_shareargs arg = { .cookie = *cookie, .share = *share, .reclaim = reclaim };
	nlm4_shareres *result = NULL;

	result = nlm4_unshare_4((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm4_unshare_4 returned null");
	}

	return result;
}

void *
NLM4_doFreeAllRPC(CLIENT *clnt, char *name)
{
	nlm4_notify arg = { .name = name, .state = 0};
	char *result = NULL;

	result = nlm4_free_all_4((void*)&arg, clnt);
	if (result == NULL) {
		XCTFail("nlm4_free_all_4 returned null");
	}

	return result;
}

/* --------- Helpers --------- */

void
doNLMNullRPC(int version, CLIENT *clnt)
{
	if (version == NLM_SM) {
		NLM_SM_doNullRPC(clnt);
	} else if (version == NLM_VERS) {
		NLM_doNullRPC(clnt);
	} else if (version == NLM_VERSX) {
		NLM3_doNullRPC(clnt);
	} else {
		NLM4_doNullRPC(clnt);
	}
}

void
doNLMTestRPC(int version, int expected_error, CLIENT *clnt, netobj *cookie, bool_t exclusive, union nlm_alock_t *alock, union nlm_alock_t *expected)
{
	if (version == NLM_VERS || version == NLM_VERSX) {
		nlm_testres *res;
		if (version == NLM_VERS) {
			res = NLM_doTestRPC(clnt, cookie, exclusive, &alock->lock);
			if (res->stat.stat != expected_error) {
				XCTFail("NLM_doTestRPC failed, got %d, expected %d", res->stat.stat, expected_error);
			}
		} else {
			res = NLM3_doTestRPC(clnt, cookie, exclusive, &alock->lock);
			if (res->stat.stat != expected_error) {
				XCTFail("NLM3_doTestRPC failed, got %d, expected %d", res->stat.stat, expected_error);
			}
		}
		if (expected) {
			XCTAssertEqual(res->stat.nlm_testrply_u.holder.svid, expected->lock.svid);
			XCTAssertEqual(res->stat.nlm_testrply_u.holder.oh.n_len, expected->lock.oh.n_len);
			XCTAssertEqual(memcmp(res->stat.nlm_testrply_u.holder.oh.n_bytes, expected->lock.oh.n_bytes, expected->lock.oh.n_len), 0);
			XCTAssertEqual(res->stat.nlm_testrply_u.holder.l_offset, expected->lock.l_offset);
			XCTAssertEqual(res->stat.nlm_testrply_u.holder.l_len, expected->lock.l_len);
		}
	} else {
		nlm4_testres *res;
		res = NLM4_doTestRPC(clnt, cookie, exclusive, &alock->lock4);
		if (res->stat.stat != expected_error) {
			XCTFail("NLM4_doTestRPC failed, got %d, expected %d", res->stat.stat, expected_error);
		}
		if (expected) {
			XCTAssertEqual(res->stat.nlm4_testrply_u.holder.svid, expected->lock4.svid);
			XCTAssertEqual(res->stat.nlm4_testrply_u.holder.oh.n_len, expected->lock4.oh.n_len);
			XCTAssertEqual(memcmp(res->stat.nlm4_testrply_u.holder.oh.n_bytes, expected->lock4.oh.n_bytes, expected->lock4.oh.n_len), 0);
			XCTAssertEqual(res->stat.nlm4_testrply_u.holder.l_offset, expected->lock4.l_offset);
			XCTAssertEqual(res->stat.nlm4_testrply_u.holder.l_len, expected->lock4.l_len);
		}
	}
}

void
doNLMLockRPC(int version, int expected_error, CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, union nlm_alock_t *alock, bool_t reclaim, int state)
{
	if (version == NLM_VERS) {
		nlm_res *res = NLM_doLockRPC(clnt, cookie, block, exclusive, &alock->lock, reclaim, state);
		if (res->stat.stat != expected_error) {
			XCTFail("NLM_doLockRPC failed, got %d, expected %d", res->stat.stat, expected_error);
		}
	} else if (version == NLM_VERSX) {
		nlm_res *res = NLM3_doLockRPC(clnt, cookie, block, exclusive, &alock->lock, reclaim, state);
		if (res->stat.stat != expected_error) {
			XCTFail("NLM3_doLockRPC failed, got %d, expected %d", res->stat.stat, expected_error);
		}
	} else {
		nlm4_res *res = NLM4_doLockRPC(clnt, cookie, block, exclusive, &alock->lock4, reclaim, state);
		if (res->stat.stat != expected_error) {
			XCTFail("NLM4_doLockRPC failed, got %d, expected %d", res->stat.stat, expected_error);
		}
	}
}

void
doNLMCancelRPC(int version, int expected_error, CLIENT *clnt, netobj *cookie, bool_t block, bool_t exclusive, union nlm_alock_t *alock)
{
	if (version == NLM_VERS) {
		nlm_res *res = NLM_doCancelRPC(clnt, cookie, block, exclusive, &alock->lock);
		if (res->stat.stat != expected_error) {
			XCTFail("NLM_doCancelRPC failed, got %d, expected %d", res->stat.stat, expected_error);
		}
	} else if (version == NLM_VERSX) {
		nlm_res *res = NLM3_doCancelRPC(clnt, cookie, block, exclusive, &alock->lock);
		if (res->stat.stat != expected_error) {
			XCTFail("NLM3_doCancelRPC failed, got %d, expected %d", res->stat.stat, expected_error);
		}
	} else {
		nlm4_res *res = NLM4_doCancelRPC(clnt, cookie, block, exclusive, &alock->lock4);
		if (res->stat.stat != expected_error) {
			XCTFail("NLM4_doCancelRPC failed, got %d, expected %d", res->stat.stat, expected_error);
		}
	}
}

void
doNLMUnlockRPC(int version, int expected_error, CLIENT *clnt, netobj *cookie, union nlm_alock_t *alock)
{
	if (version == NLM_VERS) {
		nlm_res *res = NLM_doUnlockRPC(clnt, cookie, &alock->lock);
		if (res->stat.stat != expected_error) {
			XCTFail("NLM_doUnlockRPC failed, got %d, expected %d", res->stat.stat, expected_error);
		}
	} else if (version == NLM_VERSX) {
		nlm_res *res = NLM3_doUnlockRPC(clnt, cookie, &alock->lock);
		if (res->stat.stat != expected_error) {
			XCTFail("NLM3_doUnlockRPC failed, got %d, expected %d", res->stat.stat, expected_error);
		}
	} else {
		nlm4_res *res = NLM4_doUnlockRPC(clnt, cookie, &alock->lock4);
		if (res->stat.stat != expected_error) {
			XCTFail("NLM4_doUnlockRPC failed, got %d, expected %d", res->stat.stat, expected_error);
		}
	}
}

void
doNLMShareRPC(int version, int expected_error, CLIENT *clnt, netobj *cookie, union nlm_share_t *share, bool_t reclaim)
{
	if (version == NLM_VERSX) {
		nlm_shareres *res = NLM3_doShareRPC(clnt, cookie, &share->share, reclaim);
		if (res->stat != expected_error) {
			XCTFail("NLM3_doShareRPC failed, got %d, expected %d", res->stat, expected_error);
		}
	} else {
		nlm4_shareres *res4 = NLM4_doShareRPC(clnt, cookie, &share->share4, reclaim);
		if (res4->stat != expected_error) {
			XCTFail("NLM4_doShareRPC failed, got %d, expected %d", res4->stat, expected_error);
		}
	}
}

void
doNLMUnshareRPC(int version, int expected_error, CLIENT *clnt, netobj *cookie, union nlm_share_t *share, bool_t reclaim)
{
	if (version == NLM_VERSX) {
		nlm_shareres *res = NLM3_doUnshareRPC(clnt, cookie, &share->share, reclaim);
		if (res->stat != expected_error) {
			XCTFail("NLM3_doUnshareRPC failed, got %d, expected %d", res->stat, expected_error);
		}
	} else {
		nlm4_shareres *res4 = NLM4_doUnshareRPC(clnt, cookie, &share->share4, reclaim);
		if (res4->stat != expected_error) {
			XCTFail("NLM4_doUnshareRPC failed, got %d, expected %d", res4->stat, expected_error);
		}
	}
}

void
doNLMFreeAllRPC(int version, CLIENT *clnt, char *name)
{
	if (version == NLM_VERSX) {
		NLM3_doFreeAllRPC(clnt, name);
	} else {
		NLM4_doFreeAllRPC(clnt, name);
	}
}
