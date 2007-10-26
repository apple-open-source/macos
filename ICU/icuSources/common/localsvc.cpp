//
// localsvc.cpp -- Apple-specific service hook
// Copyright (c) 2007 Apple Inc. All rights reserved.
//

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "aaplbfct.h"
#include "cstring.h"
#if defined(U_DARWIN)
#include <TargetConditionals.h>
#endif

// Return an appropriate Apple-specific object, based on the service in question
U_CAPI void* uprv_svc_hook(const char *what, UErrorCode *status)
{
	if (uprv_strcmp(what, "languageBreakFactory") == 0) {
#if defined(U_DARWIN) && TARGET_OS_MAC
		return new AppleLanguageBreakFactory(*status);
	}
#else
	}
#endif
	return NULL;
}

#endif
