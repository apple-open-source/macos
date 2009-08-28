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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Portions Copyright 2008 Apple, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * MLRPC heap management. The heap is used for temporary storage by
 * both the client and server side library routines.  In order to
 * support the different requirements of the various RPCs, the heap
 * can grow dynamically if required.  We start with a single block
 * and perform sub-allocations from it.  If an RPC requires more space
 * we will continue to add it a block at a time.  This means that we
 * don't hog lots of memory on every call to support the few times
 * that we actually need a lot heap space.
 *
 * Note that there is no individual free function.  Once space has been
 * allocated, it remains allocated until the heap is destroyed.  This
 * shouldn't be an issue because the heap is being filled with data to
 * be marshalled or unmarshalled and we need it all to be there until
 * the point that the entire heap is no longer required.
 */

#include "mlrpc.h"
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <malloc/malloc.h>

struct mlrpc_heap
{
	malloc_zone_t *		heap_zone;
	malloc_statistics_t	heap_stats;
};

/*
 * Allocate a heap structure and the first heap block.  For many RPC
 * operations this will be the only time we need to malloc memory
 * in this instance of the heap.
 *
 * Returns a pointer to the newly created heap, which is used like an
 * opaque handle with the rest of the heap management interface..
 */
mlrpc_heap_t *
mlrpc_heap_create(void)
{
	mlrpc_heap_t *heap;

	heap = calloc(1, sizeof(mlrpc_heap_t));
	if (heap == NULL) {
		return NULL;
	}

	heap->heap_zone = malloc_create_zone(getpagesize(), 0);
	if (heap->heap_zone == NULL) {
		mlrpc_heap_destroy(heap);
		return NULL;
	}

	return heap;
}

/*
 * Deallocate all of the memory associated with a heap.  This is the
 * only way to deallocate heap memory, it isn't possible to free the
 * space obtained by individual malloc calls.
 */
void
mlrpc_heap_destroy(mlrpc_heap_t *heap)
{
	if (heap && heap->heap_zone) {
		malloc_destroy_zone(heap->heap_zone);
	}

	if (heap) {
		free(heap);
	}
}

/*
 * Allocate zeroed space in the specified heap.  All requests are padded, if
 * required, to ensure dword alignment.
 *
 * On success, a pointer to the allocated (dword aligned) area is
 * returned.  Otherwise a null pointer is returned.
 */
void *
mlrpc_heap_malloc(mlrpc_heap_t *heap, size_t size)
{
	if (heap == NULL || size == 0) {
		return (NULL);
	}

	return malloc_zone_calloc(heap->heap_zone, 1, size);
}

/*
 * Convenience function to do heap strdup.
 */
char *
mlrpc_heap_strsave(mlrpc_heap_t *heap, const char *s)
{
	size_t len;
	char *p;

	if (heap == NULL || s == NULL) {
		return (NULL);
	}

	/*
	 * We don't need to clutter the heap with empty strings.
	 */
	len = strlen(s);
	if (len == 0) {
		return ("");
	}

	p = mlrpc_heap_malloc(heap, len + 1);
	if (p == NULL) {
		return NULL;
	}

	strlcpy(p, s, len + 1);
	return (p);
}

/*
 * Our regular string marshalling always creates null terminated strings
 * but some Windows clients and servers are pedantic about the string
 * formats they will accept and require non-null terminated strings.
 * This function can be used to build a wide-char, non-null terminated
 * string in the heap as a varying/conformant array.  We need to do the
 * wide-char conversion here because the marshalling code won't be
 * aware that this is really a string.
 */
void
mlrpc_heap_mkvcs(mlrpc_heap_t *heap, const char *s, mlrpc_vcbuf_t *vcs)
{
	int mlen;

	vcs->wclen = mts_wcequiv_strlen(s);
	vcs->wcsize = vcs->wclen;

	mlen = sizeof (struct mlrpc_vcb) + vcs->wcsize + sizeof (mts_wchar_t);

	vcs->vcb = (struct mlrpc_vcb *)mlrpc_heap_malloc(heap, mlen);

	if (vcs->vcb) {
		vcs->vcb->vc_first_is = 0;
		vcs->vcb->vc_length_is = vcs->wclen / sizeof (mts_wchar_t);
		(void) mts_mbstowcs((mts_wchar_t *)vcs->vcb->buffer, s,
		    vcs->vcb->vc_length_is);
	}
}

size_t
mlrpc_heap_used(mlrpc_heap_t *heap)
{
	malloc_statistics_t mstat;

	malloc_zone_statistics(heap->heap_zone, &mstat);
	return mstat.size_in_use;
}

