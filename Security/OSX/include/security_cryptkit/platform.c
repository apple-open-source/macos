/* Copyright (c) 1998,2011,2014 Apple Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * platform.c - platform-dependent C functions
 *
 * Revision History
 * ----------------
 *  6 Sep 96 at NeXT
 *	Created.
 */

#include "platform.h"
#include <stdio.h>
#include "feeDebug.h"

#import "feeDebug.h"

#if __MAC_BUILD__
/*
 * Macintosh, all flavors.
 */
#include <stdlib.h>
#include <CrashReporterClient.h>

void CKRaise(const char *reason) {
	#if	FEE_DEBUG
	printf("CryptKit fatal error: %s\n", reason);
	#endif
    char * msg = NULL;
    if(asprintf(&msg, "CryptKit fatal error: %s", reason)) {
        CRSetCrashLogMessage(msg);
    } else {
        CRSetCrashLogMessage("CryptKit fatal error");
    }
    abort();
}

#else

#error platform-specific work needed in security_cryptkit/platform.c

#endif
