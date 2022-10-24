/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#include <CoreWiFi/CoreWiFi.h>
#include <CoreFoundation/CoreFoundation.h>

#include "WiFiUtil.h"

__private_extern__ Boolean
WiFiIsExpensive(void)
{
	Boolean		expensive = FALSE;

	if ([CWFInterface class] != nil) {
		@autoreleasepool {
			CWFScanResult	*currentNetwork;
			CWFInterface	*interface;

			interface = [[CWFInterface alloc] init];
			[interface activate];
			currentNetwork = [interface currentScanResult];
			if (currentNetwork.isMetered) {
				expensive = TRUE;
			}
			[interface invalidate];
			[interface release];
		}
	}
	return (expensive);
}

#ifdef TEST_WIFIUTIL
#include <stdio.h>

int
main()
{
	printf("WiFi is %sexpensive\n",
	       WiFiIsExpensive() ? "" : "not ");
	exit(0);
}

#endif
