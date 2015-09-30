/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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


#include <stdio.h>

#include "secd_regressions.h"

#include <CoreFoundation/CFData.h>
#include <Security/SecOTRSession.h>
#include <Security/SecOTRIdentityPriv.h>
#include <Security/SecInternal.h>
#include <Security/SecBasePriv.h>
#include <Security/SecKeyPriv.h>
#include <AssertMacros.h>

#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSForerunnerSession.h>

#include "SOSCircle_regressions.h"
#include "SOSRegressionUtilities.h"
#include "SOSTestDataSource.h"
#include "SecOTRRemote.h"
#include "SOSAccount.h"

#include "SecdTestKeychainUtilities.h"

#define FRT_USERNAME "username"
#define FRT_CIRCLE_SECRET "867530"
#define FRT_CIRCLE_WRONG_SECRET "789345"

#define FRT_DSID 4241983

static const unsigned char frt_hsa2_data[] = "1138";

enum {
	CORRUPT_REQUEST,
	CORRUPT_CHALLENGE,
	CORRUPT_RESPONSE,
	CORRUPT_HSA2,
	WRONG_SECRET,
};

static const int _success_test_count = 8;
static const int _failure_test_count = 1;
static const int _failure_test_runs = 5;

static const int _test_count = _success_test_count +
		(_failure_test_count * _failure_test_runs);

static void
corrupt_data(CFDataRef data, bool partial)
{
	uint8_t *ptr = NULL;
	size_t len = 0;
	size_t i = 0;

	ptr = (uint8_t *)CFDataGetBytePtr(data);
	len = CFDataGetLength(data);

	// Don't corrupt the magic number and version, so we're forced to exercise
	// the validation logic for SRP.
	if (partial && len >= 16) {
		ptr += 32;
		len -= 32;
	}

	for (i = 0; i < len; i++) {
		ptr[i] = ~(ptr[i]);
	}
}

static void
success_path(void)
{
	CFErrorRef cferror = NULL;
	SOSForerunnerRequestorSessionRef requestor = NULL;
	SOSForerunnerAcceptorSessionRef acceptor = NULL;

	CFDataRef request = NULL;
	CFDataRef challenge = NULL;
	CFDataRef response = NULL;
	CFDataRef hsa2 = NULL;
	CFDataRef hsa2_decrypted = NULL;

	CFDataRef hsa2code = NULL;
	CFDataRef unencrypted = NULL;
	CFDataRef encrypted = NULL;
	CFDataRef decrypted = NULL;

	requestor = SOSForerunnerRequestorSessionCreate(NULL,
			CFSTR(FRT_USERNAME), FRT_DSID);
	ok(requestor, "requestor session created");
	require(requestor, xit);

	acceptor = SOSForerunnerAcceptorSessionCreate(NULL, CFSTR(FRT_USERNAME),
			FRT_DSID, CFSTR(FRT_CIRCLE_SECRET));
	ok(acceptor, "acceptor session created");
	require(acceptor, xit);

	request = SOSFRSCopyRequestPacket(requestor, &cferror);
	ok(request, "request packet created, error = %@", cferror);
	require(request, xit);

	challenge = SOSFASCopyChallengePacket(acceptor, request, &cferror);
	ok(challenge, "challenge packet created, error = %@", cferror);
	require(challenge, xit);

	response = SOSFRSCopyResponsePacket(requestor, challenge,
			CFSTR(FRT_CIRCLE_SECRET), NULL, &cferror);
	ok(response, "response packet created, error = %@", cferror);
	require(response, xit);

	hsa2code = CFDataCreate(NULL, frt_hsa2_data, sizeof(frt_hsa2_data) - 1);
	hsa2 = SOSFASCopyHSA2Packet(acceptor, response, hsa2code, &cferror);
	ok(hsa2, "hsa2 packet created, error = %@", cferror);
	require(hsa2, xit);

	hsa2_decrypted = SOSFRSCopyHSA2CodeFromPacket(requestor, hsa2, &cferror);
	ok(hsa2_decrypted);
	require(hsa2_decrypted, xit);

	ok(CFEqual(hsa2_decrypted, hsa2code));

xit:
	CFReleaseNull(requestor);
	CFReleaseNull(acceptor);

	CFReleaseNull(hsa2code);
	CFReleaseNull(hsa2_decrypted);
	CFReleaseNull(hsa2);
	CFReleaseNull(request);
	CFReleaseNull(challenge);
	CFReleaseNull(response);

	CFReleaseNull(unencrypted);
	CFReleaseNull(encrypted);
	CFReleaseNull(decrypted);
}

static void
failure_path(int which)
{
	CFErrorRef cferror = NULL;
	SOSForerunnerRequestorSessionRef requestor = NULL;
	SOSForerunnerAcceptorSessionRef acceptor = NULL;

	CFDataRef hsa2code = NULL;
	CFDataRef request = NULL;
	CFDataRef challenge = NULL;
	CFDataRef response = NULL;
	CFDataRef hsa2packet = NULL;
	CFDataRef hsa2_decrypted = NULL;
	CFStringRef secret = CFSTR(FRT_CIRCLE_SECRET);

	requestor = SOSForerunnerRequestorSessionCreate(NULL, CFSTR(FRT_USERNAME),
			FRT_DSID);
	require(requestor, xit);

	acceptor = SOSForerunnerAcceptorSessionCreate(NULL, CFSTR(FRT_USERNAME),
			FRT_DSID, CFSTR(FRT_CIRCLE_SECRET));
	require(acceptor, xit);

	request = SOSFRSCopyRequestPacket(requestor, &cferror);
	require(request, xit);

	if (which == CORRUPT_REQUEST) {
		corrupt_data(request, false);
	}

	challenge = SOSFASCopyChallengePacket(acceptor, request, &cferror);
	if (which == CORRUPT_REQUEST) {
		ok(challenge == NULL, "did not create challenge packet");
		goto xit;
	} else {
		require(challenge, xit);
	}

	if (which == CORRUPT_CHALLENGE) {
		corrupt_data(challenge, true);
	} else if (which == WRONG_SECRET) {
		secret = CFSTR(FRT_CIRCLE_WRONG_SECRET);
	}

	response = SOSFRSCopyResponsePacket(requestor, challenge, secret, NULL,
			&cferror);
	if (which == CORRUPT_CHALLENGE) {
		ok(response == NULL, "did not create response packet");
		goto xit;
	} else {
		require(response, xit);
	}

	if (which == CORRUPT_RESPONSE) {
		corrupt_data(response, true);
	}

	hsa2code = CFDataCreate(NULL, frt_hsa2_data, sizeof(frt_hsa2_data) - 1);
	hsa2packet = SOSFASCopyHSA2Packet(acceptor, response, hsa2code, &cferror);
	if (which == CORRUPT_RESPONSE) {
		ok(hsa2packet == NULL, "did not create hsa2 packet");
		goto xit;
	} else if (which == WRONG_SECRET) {
		ok(hsa2packet == NULL, "did not create hsa2 packet from bad secret");
		goto xit;
	} else {
		require(hsa2packet, xit);
	}

	if (which == CORRUPT_HSA2) {
		corrupt_data(hsa2packet, true);
	}

	hsa2_decrypted = SOSFRSCopyHSA2CodeFromPacket(requestor, hsa2packet,
			&cferror);
	if (which == CORRUPT_HSA2) {
		ok(hsa2_decrypted == NULL, "did not decrypt hsa2 code, error = %@",
				cferror);
		goto xit;
	} else {
		require(hsa2packet, xit);
	}

xit:
	CFReleaseNull(requestor);
	CFReleaseNull(acceptor);

	CFReleaseNull(hsa2code);
	CFReleaseNull(hsa2packet);
	CFReleaseNull(hsa2_decrypted);
	CFReleaseNull(request);
	CFReleaseNull(challenge);
	CFReleaseNull(response);
}

static void
tests(void)
{
	success_path();
	failure_path(CORRUPT_REQUEST);
	failure_path(CORRUPT_CHALLENGE);
	failure_path(CORRUPT_RESPONSE);
	failure_path(CORRUPT_HSA2);
	failure_path(WRONG_SECRET);
}

int
sc_140_hsa2(int argc, char *const *argv)
{
	plan_tests(_test_count);

	tests();

	return 0;
}
