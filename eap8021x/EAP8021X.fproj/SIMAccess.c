/*
 * Copyright (c) 2009, 2012-2013 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * January 15, 2009	Dieter Siegmund (dieter@apple.com)
 * - created
 */

/*
 * SIMAccess.c
 * - API's to access the SIM
 */


#include "SIMAccess.h"
#include <TargetConditionals.h>
#include <dispatch/dispatch.h>
#include <strings.h>
#include "symbol_scope.h"
#include "myCFUtil.h"
#include "EAPLog.h"



#if ! TARGET_OS_EMBEDDED
PRIVATE_EXTERN CFStringRef
SIMCopyIMSI(void)
{
    return (NULL);
}

CFStringRef
SIMCopyRealm(void)
{
    return (NULL);
}

PRIVATE_EXTERN bool
SIMAuthenticateGSM(const uint8_t * rand_p, int count,
		   uint8_t * kc_p, uint8_t * sres_p)
{
    return (false);
}

PRIVATE_EXTERN bool
SIMAuthenticateAKA(CFDataRef rand, CFDataRef autn, AKAAuthResultsRef results)
{
    AKAAuthResultsInit(results);
    return (false);
}

#endif /* ! TARGET_OS_EMBEDDED */


PRIVATE_EXTERN void
AKAAuthResultsSetCK(AKAAuthResultsRef results, CFDataRef ck)
{
    my_FieldSetRetainedCFType(&results->ck, ck);
    return;
}

PRIVATE_EXTERN void
AKAAuthResultsSetIK(AKAAuthResultsRef results, CFDataRef ik)
{
    my_FieldSetRetainedCFType(&results->ik, ik);
    return;
}

PRIVATE_EXTERN void
AKAAuthResultsSetRES(AKAAuthResultsRef results, CFDataRef res)
{
    my_FieldSetRetainedCFType(&results->res, res);
    return;
}

PRIVATE_EXTERN void
AKAAuthResultsSetAUTS(AKAAuthResultsRef results, CFDataRef auts)
{
    my_FieldSetRetainedCFType(&results->auts, auts);
    return;
}

PRIVATE_EXTERN void
AKAAuthResultsInit(AKAAuthResultsRef results)
{
    bzero(results, sizeof(*results));
    return;
}

PRIVATE_EXTERN void
AKAAuthResultsRelease(AKAAuthResultsRef results)
{
    AKAAuthResultsSetCK(results, NULL);
    AKAAuthResultsSetIK(results, NULL);
    AKAAuthResultsSetRES(results, NULL);
    AKAAuthResultsSetAUTS(results, NULL);
    return;
}


#ifdef TEST_SIMACCESS
#define USE_SYSTEMCONFIGURATION_PRIVATE_HEADERS 1
#include <SystemConfiguration/SCPrivate.h>

#define N_TRIPLETS	3

STATIC void
AKAAuthResultsPrint(AKAAuthResultsRef results)
{
    if (results->ck != NULL) {
	SCPrint(TRUE, stdout, CFSTR("CK=%@\nCK=%@\nRES=%@\n"),
		results->ck, results->ik, results->res);
    }
    else {
	if (results->auts != NULL) {
	    SCPrint(TRUE, stdout, CFSTR("AUTS=%@\n"), results->auts);
	}
	else {
	    printf("---- Authentication Reject ----\n");
	}
    }
    return;
}

#include "printdata.h"

int
main(int argc, char * argv[])
{
    static const uint8_t	rand[SIM_RAND_SIZE * N_TRIPLETS] = { 
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,

	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,

	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
    };
    if (argc == 1) {
	uint8_t		kc[SIM_KC_SIZE * N_TRIPLETS];
	uint8_t		sres[SIM_SRES_SIZE * N_TRIPLETS];

	if (SIMAuthenticateGSM(rand, N_TRIPLETS, kc, sres) == false) {
	    fprintf(stderr, "SIMProcessRAND failed\n");
	    exit(1);
	}
	printf("Kc\n");
	print_data(kc, sizeof(kc));
	printf("SRES\n");
	print_data(sres, sizeof(sres));
	exit(0);
    }
    else {
	bool			auth_success;
	CFDataRef		autn_data;
	CFDataRef		rand_data;
	AKAAuthResults		results;

	autn_data = CFDataCreate(NULL, rand + SIM_RAND_SIZE, SIM_RAND_SIZE);
	rand_data = CFDataCreate(NULL, rand, SIM_RAND_SIZE);
	auth_success = SIMAuthenticateAKA(rand_data, autn_data, &results);
	CFRelease(rand_data);
	CFRelease(autn_data);
	if (auth_success) {
	    AKAAuthResultsPrint(&results);
	}
	else {
	    printf("SIMAuthenticateAKA() failed\n");
	}
	AKAAuthResultsRelease(&results);
    }
    return (0);
}

#endif /* TEST_SIMACCESS */
