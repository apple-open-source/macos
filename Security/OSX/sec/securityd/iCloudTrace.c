/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#include "iCloudTrace.h"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecItem.h>
#include <utilities/iCloudKeychainTrace.h>
#include <securityd/SecItemServer.h>
#include <sys/stat.h>
#include <string.h>
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
#include <pwd.h>
#endif
#include <utilities/SecCFWrappers.h>


/* --------------------------------------------------------------------------
	Function:		Bucket
	
	Description:	In order to preserve annominity of a user, take an
					absolute value and return back the most significant 
					value in base 10 
   -------------------------------------------------------------------------- */
static	int64_t Bucket(int64_t value)
{
    if (value < 10)
    {
       return value;
    }

    if (value < 100)
    {
        return (value / 10) * 10;
    }

    if (value < 1000)
    {
        return (value / 100) * 100;
    }

    if (value < 10000)
    {
        return (value / 1000) * 1000;
    }

    if (value < 100000)
    {
        return (value / 10000) * 10000;
    }

    if (value < 1000000)
    {
        return (value / 100000) * 10000;
    }

    return value;
}

static int64_t
Bucket2Significant(int64_t value)
{
    if (value < 100)
	return value;
    return 10 * Bucket2Significant(value / 10);
}

static void
TraceKeyClassItem(void *token, CFStringRef keyclass, CFStringRef name, int64_t num)
{
    CFStringRef key = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@.%@"), keyclass, name);
    if (key) {
	num = Bucket2Significant(num);
	AddKeyValuePairToKeychainLoggingTransaction(token, key, num);
	CFReleaseNull(key);
    }

}

static void
TraceKeyClass(void *token, CFStringRef keyclass, const struct _SecServerKeyStats *stats)
{
    TraceKeyClassItem(token, keyclass, CFSTR("AverageSize"), stats->averageSize);
    TraceKeyClassItem(token, keyclass, CFSTR("MaxSize"), stats->maxDataSize);
    TraceKeyClassItem(token, keyclass, CFSTR("NumItems"), stats->items);
}

/* --------------------------------------------------------------------------
	Function:		DoLogging
	
	Description:	If it has been determined that logging should be done
					this function will perform the logging
   -------------------------------------------------------------------------- */
void CloudKeychainTrace(CFIndex num_peers, size_t num_items,
                        const struct _SecServerKeyStats *genpStats,
                        const struct _SecServerKeyStats *inetStats,
                        const struct _SecServerKeyStats *keysStats)
{
	void *token = BeginCloudKeychainLoggingTransaction();
    AddKeyValuePairToKeychainLoggingTransaction(token, kNumberOfiCloudKeychainPeers, (int64_t)num_peers);
    AddKeyValuePairToKeychainLoggingTransaction(token, kNumberOfiCloudKeychainItemsBeingSynced, Bucket((int64_t)num_items));
    TraceKeyClass(token, CFSTR("genp"), genpStats);
    TraceKeyClass(token, CFSTR("inet"), inetStats);
    TraceKeyClass(token, CFSTR("keys"), keysStats);
	CloseCloudKeychainLoggingTransaction(token);
}
