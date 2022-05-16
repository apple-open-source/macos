/*
	Copyright (c) 2022 Apple Inc. All rights reserved.
*/

#include "od_debug.h"

#include <os/variant_private.h>
#include <stdlib.h>

bool
od_debug_enabled(void)
{
	bool enabled = false;
	if (os_variant_allows_internal_security_policies("com.apple.libinfo") && getenv("OD_DEBUG_MODE")) {
		enabled = true;
	}
	return enabled;
}
