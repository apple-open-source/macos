/*
* Copyright (c) 2025 Apple Inc. All rights reserved.
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

#include <signal.h>

#include <darwintest.h>

T_DECL(sigsetops, "Test basic sigsetops usage")
{
	sigset_t set;
	int error;

	(void)sigismember(&set, 0);

	/*
	 * We avoid signal 0 here because the mask doesn't cover that; it does,
	 * however, cover NSIG (which is usually one past the last usable
	 * signal).
	 */
	sigemptyset(&set);
	for (int signo = 1; signo <= NSIG; signo++) {
		error = sigismember(&set, signo);
		T_QUIET;
		T_EXPECT_EQ_INT(error, 0, NULL);

		(void)sigaddset(&set, signo);
		error = sigismember(&set, signo);
		T_QUIET;
		T_EXPECT_EQ_INT(error, 1, NULL);

		(void)sigdelset(&set, signo);
		error = sigismember(&set, signo);
		T_QUIET;
		T_EXPECT_EQ_INT(error, 0, NULL);
	}

	sigfillset(&set);
	for (int signo = 1; signo <= NSIG; signo++) {
		error = sigismember(&set, signo);
		T_QUIET;
		T_EXPECT_EQ_INT(error, 1, NULL);
	}

}
