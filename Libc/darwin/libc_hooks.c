/*
* Copyright (c) 2024 Apple Inc. All rights reserved.
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

#include "libc_hooks.h"

#include <assert.h>

__attribute__((visibility("hidden"))) libc_hooks_t libc_hooks;

void
libc_set_introspection_hooks(const libc_hooks_t *new_hooks, libc_hooks_t *old_hooks, size_t size) {
	// Any version of the hooks always contains at least the version as the first field
	assert(size >= sizeof(libc_hooks_version));

	if (old_hooks) {
		// There are no older versions of the SPI so we can just assert here
		assert(size >= sizeof(libc_hooks));

		// If caller is offering an oversized libc_hooks_t, it could be
		// from a later version of the SPI zero that part out
		if (size > sizeof(libc_hooks))
			bzero(old_hooks + sizeof(libc_hooks), size - sizeof(libc_hooks));

		// We have the room to copy the current libc_hooks back to the user
		*old_hooks = libc_hooks;
	}

	if (new_hooks) {
		// There are no older versions of the SPI so we can just assert here
		assert(new_hooks->version >= libc_hooks_version);

		// The size had better be at least big enough to hold our libc_hooks
		// since libc_hooks is only allowed to grow
		assert(size >= sizeof(libc_hooks));

		// Copy new_hooks since it's lifetime of new_hooks is unknowable.
		libc_hooks = *new_hooks;

		// Set the version since we might have been offered a version of
		// libc_hooks_t from the future that we don't know what to do with.
		libc_hooks.version = libc_hooks_version;
	}
}
