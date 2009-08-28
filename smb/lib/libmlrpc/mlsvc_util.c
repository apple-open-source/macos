/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Utility functions to support the RPC interface library.
 */

//#include <stdio.h>
//#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <libmlrpc/mlsvc_util.h>
#include <libmlrpc/mlsvc.h>

/*
 * mlsvc_string_save
 *
 * This is a convenience function to prepare strings for an RPC call.
 * An ms_string_t is set up with the appropriate lengths and str is
 * set up to point to a copy of the original string on the heap. The
 * macro MLRPC_HEAP_STRSAVE is an alias for mlrpc_heap_strsave, which
 * extends the heap and copies the string into the new area.
 */
int
mlsvc_string_save(ms_string_t *ms, const char *str, struct mlrpc_xaction *mxa)
{
	if (str == NULL)
		return (0);

	ms->length = mts_wcequiv_strlen(str);
	ms->allosize = ms->length + sizeof (mts_wchar_t);

	if ((ms->str = (LPTSTR)MLRPC_HEAP_STRSAVE(mxa, str)) == NULL)
		return (0);

	return (1);
}

/*
 * mlsvc_sid_save
 *
 * Expand the heap and copy the sid into the new area.
 * Returns a pointer to the copy of the sid on the heap.
 */
nt_sid_t *
mlsvc_sid_save(const nt_sid_t *sid, struct mlrpc_xaction *mxa)
{
	nt_sid_t *heap_sid;

	if (sid == NULL)
		return (NULL);

	heap_sid = (nt_sid_t *)MLRPC_HEAP_MALLOC(mxa, sizeof(nt_sid_t));
	if (heap_sid == NULL)
		return (0);

	memcpy(heap_sid, sid, sizeof(nt_sid_t));
	return (heap_sid);
}

/*
 * mlsvc_is_null_handle
 *
 * Check a handle against a null handle. Returns 1 if the handle is
 * null. Otherwise returns 0.
 */
bool
mlsvc_is_null_handle(mlsvc_handle_t *handle)
{
	const static ms_handle_t zero_handle;

	if (handle == NULL || handle->context == NULL)
		return (true);

	if (!memcmp(&handle->handle, &zero_handle, sizeof (ms_handle_t)))
		return (true);

	return (false);
}

