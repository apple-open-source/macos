/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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
 * Support for stack logging lite in the malloc stack logging library. This code
 * will go away when MSL lite no longer requires its own zone.
 */
#ifndef MALLOC_ENABLE_MSL_LITE_SPI
#define MALLOC_ENABLE_MSL_LITE_SPI 1
#endif // MALLOC_ENABLE_MSL_LITE_SPI

#include "internal.h"

// These definitions are here and not in a header file because all of this code
// is intended to go away very soon.
extern malloc_zone_t **malloc_zones;
extern malloc_zone_t* lite_zone;
extern boolean_t has_default_zone0(void);

static void
insert_msl_lite_zone(malloc_zone_t *zone)
{
	lite_zone = zone;
	malloc_slowpath_update();
}

static malloc_zone_t *
get_global_helper_zone(void)
{
	return malloc_zones[0];  // TODO: this is broken
}

// This struct contains many more function pointers than the ones given here. MSL previously used
// those when it called into libmalloc to create a lite zone.
// Now that MSL creates a lite zone itself, we don't need to provide those other function pointers.
static struct _malloc_msl_lite_hooks_s malloc_msl_lite_hooks = {
	.has_default_zone0      = &has_default_zone0,
	.insert_msl_lite_zone   = &insert_msl_lite_zone,
	.get_global_helper_zone = &get_global_helper_zone,
};

/*
 * Copies the malloc library's _malloc_msl_lite_hooks_t structure to a given
 * location. We pass the structure size to allow the structure to
 * grow. Since this is a temporary arrangement, we don't need to worry about
 * pointer authentication here or in the _malloc_msl_lite_hooks_t structure itself.
 */
MALLOC_NOEXPORT
void
set_msl_lite_hooks(set_msl_lite_hooks_callout_t callout)
{
	callout(&malloc_msl_lite_hooks, sizeof(malloc_msl_lite_hooks));
}
