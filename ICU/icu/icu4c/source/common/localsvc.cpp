//
// localsvc.cpp -- Apple-specific service hook
// Copyright (c) 2007 Apple Inc. All rights reserved.
//
// originally added per rdar://4448220 Add user dictionary support
//

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "aaplbfct.h"
#include "cstring.h"
// platform.h now includes <TargetConditionals.h> if U_PLATFORM_IS_DARWIN_BASED

// Return an appropriate Apple-specific object, based on the service in question
U_CAPI void* uprv_svc_hook(const char *what, UErrorCode *status)
{
	if (uprv_strcmp(what, "languageBreakFactory") == 0) {
// NOTE: The implementation of AppleLanguageBreakFactory has been commented out since 2012 (check git blame on
// aaplbfct.cpp), and having it here interferes with the new external-break-engine stuff in ICU 74, so comment
// it out here.  If we need our own language break engine in the future, we need to use the new mechanism.
#if /*U_PLATFORM_IS_DARWIN_BASED && TARGET_OS_MAC*/ 0
		return new icu::AppleLanguageBreakFactory(*status);
	}
#else
	}
#endif
	return NULL;
}

#endif
