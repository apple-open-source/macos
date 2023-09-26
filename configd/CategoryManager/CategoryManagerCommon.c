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

/*
 * CategoryManagerCommon.c
 */

/*
 * Modification History
 *
 * December 22, 2022	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#include "CategoryManagerCommon.h"
#include "CategoryManagerInternal.h"
#include <SystemConfiguration/SCValidation.h>
#include "symbol_scope.h"

PRIVATE_EXTERN CFStringRef
CategoryInformationGetCategory(CategoryInformationRef info)
{
	CFStringRef	str;

	str = CFDictionaryGetValue(info, kCategoryInformationKeyCategory);
	return (isA_CFString(str));
}

PRIVATE_EXTERN CFStringRef
CategoryInformationGetInterfaceName(CategoryInformationRef info)
{
	CFStringRef	str;

	str = CFDictionaryGetValue(info, kCategoryInformationKeyInterfaceName);
	return (isA_CFString(str));
}

PRIVATE_EXTERN CFStringRef
CategoryInformationGetValue(CategoryInformationRef info)
{
	CFStringRef	str;

	str = CFDictionaryGetValue(info, kCategoryInformationKeyValue);
	return (isA_CFString(str));
}

PRIVATE_EXTERN SCNetworkCategoryManagerFlags
CategoryInformationGetFlags(CategoryInformationRef info)
{
	SCNetworkCategoryManagerFlags	flags = 0;
	CFNumberRef			flags_cf;

	flags_cf = CFDictionaryGetValue(info, kCategoryInformationKeyFlags);
	if (isA_CFNumber(flags_cf) != NULL) {
		CFNumberGetValue(flags_cf, kCFNumberSInt32Type, &flags);
	}
	return (flags);
}

