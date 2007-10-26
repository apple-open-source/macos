/*
 * ntfs_runlist.c - NTFS kernel runlist operations.
 *
 * Copyright (c) 2001-2007 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2002-2005 Richard Russon.  All Rights Reserved.
 * Portions Copyright (c) 2006, 2007 Apple Inc.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 3. Neither the name of Apple Inc. ("Apple") nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ALTERNATIVELY, provided that this notice and licensing terms are retained in
 * full, this file may be redistributed and/or modified under the terms of the
 * GNU General Public License (GPL) Version 2, in which case the provisions of
 * that version of the GPL will apply to you instead of the license terms
 * above.  You can obtain a copy of the GPL Version 2 at
 * http://developer.apple.com/opensource/licenses/gpl-2.txt.
 */

#include <sys/buf.h>
#include <sys/errno.h>

#include <string.h>

#include <libkern/OSMalloc.h>

#include <kern/debug.h>
#include <kern/locks.h>

#include "ntfs.h"
#include "ntfs_debug.h"
#include "ntfs_layout.h"
#include "ntfs_types.h"
#include "ntfs_volume.h"

/*
 * For a description of the runlist structure and various values of LCNs,
 * please read the ntfs_runlist.h header file.
 */
#include "ntfs_runlist.h"

/**
 * ntfs_rl_copy - copy a runlist or runlist fragment
 *
 * It is up to the caller to serialize access to the runlists.
 */
static inline void ntfs_rl_copy(ntfs_rl_element *dst, ntfs_rl_element *src,
		const u32 size)
{
	if (size > 0)
		memcpy(dst, src, size * sizeof(ntfs_rl_element));
}

/**
 * ntfs_rl_inc - append runlist elements to an existing runlist
 * @runlist:	runlist for which to increment the number of runlist elements
 * @delta:	number of elements to add to the runlist
 *
 * Increment the number of elements in the array of runlist elements of the
 * runlist @runlist by @delta.  Reallocate the array buffer if needed.
 *
 * Return 0 on success and ENOMEM if not enough memory to reallocate the
 * runlist, in which case the runlist @runlist is left unmodified.
 *
 * Locking: - The caller must have locked the runlist for writing.
 *	    - The runlist is modified.
 */
static inline errno_t ntfs_rl_inc(ntfs_runlist *runlist, u32 delta)
{
	u32 new_elements, alloc, new_alloc;

	new_elements = runlist->elements + delta;
	alloc = runlist->alloc;
	new_alloc = (new_elements * sizeof(ntfs_rl_element) +
			NTFS_RL_ALLOC_BLOCK - 1) & ~(NTFS_RL_ALLOC_BLOCK - 1);
	if (new_alloc > alloc) {
		ntfs_rl_element *new_rl;

		new_rl = OSMalloc(new_alloc, ntfs_malloc_tag);
		if (!new_rl)
			return ENOMEM;
		ntfs_rl_copy(new_rl, runlist->rl, runlist->elements);
		if (alloc)
			OSFree(runlist->rl, alloc, ntfs_malloc_tag);
		runlist->rl = new_rl;
		runlist->alloc = new_alloc;
	}
	runlist->elements = new_elements;
	return 0;
}

/**
 * ntfs_rl_move - move a fragment of a runlist within the runlist
 *
 * It is up to the caller to serialize access to the runlists.
 */
static inline void ntfs_rl_move(ntfs_rl_element *dst, ntfs_rl_element* src,
		const u32 size)
{
	if (dst != src && size > 0)
		memmove(dst, src, size * sizeof(ntfs_rl_element));
}

/**
 * ntfs_rl_ins - insert runlist elements into an existing runlist
 * @runlist:	runlist to insert elements into
 * @pos:	position (in units of runlist elements) at which to insert
 * @count:	number of runlist elements to insert at @pos
 *
 * Insert @count runlist elements at position @pos in the runlist @runlist.  If
 * there is not enough space in the allocated array of runlist elements, the
 * memory is reallocated thus you cannot use old pointers into the array of
 * runlist elements.
 *
 * The inserted elements are not initialized, the caller is assumed to do this
 * after a successful return from ntfs_rl_ins().
 *
 * Return 0 on success and ENOMEM if not enough memory to reallocate the
 * runlist, in which case the runlist @runlist is left unmodified.
 *
 * Locking: - The caller must have locked the runlist for writing.
 *	    - The runlist is modified and potentially reallocated.
 */
static inline errno_t ntfs_rl_ins(ntfs_runlist *runlist, u32 pos, u32 count)
{
	ntfs_rl_element *new_rl = runlist->rl;
	u32 new_elements, alloc, new_alloc;

	ntfs_debug("Entering with pos %u, count %u.", (unsigned)pos,
			(unsigned)count);
	if (pos > runlist->elements)
		panic("%s(): pos > runlist->elements\n", __FUNCTION__);
	new_elements = runlist->elements + count;
	alloc = runlist->alloc;
	new_alloc = (new_elements * sizeof(ntfs_rl_element) +
			NTFS_RL_ALLOC_BLOCK - 1) & ~(NTFS_RL_ALLOC_BLOCK - 1);
	/* If no memory reallocation needed, it is a simple memmove(). */
	if (new_alloc <= alloc) {
		if (count) {
			new_rl += pos;
			ntfs_rl_move(new_rl + count, new_rl,
					runlist->elements - pos);
			runlist->elements = new_elements;
		}
		ntfs_debug("Done: Simple.");
		return 0;
	}
	/*
	 * Reallocate memory, then do a split memcpy() to the correct locations
	 * of the newly allocated array of runlist elements unless @pos is zero
	 * in which case a single memcpy() is sufficient.
	 */
	new_rl = OSMalloc(new_alloc, ntfs_malloc_tag);
	if (!new_rl)
		return ENOMEM;
	ntfs_rl_copy(new_rl, runlist->rl, pos);
	ntfs_rl_copy(new_rl + pos + count, runlist->rl + pos,
			runlist->elements - pos);
	if (alloc)
		OSFree(runlist->rl, alloc, ntfs_malloc_tag);
	runlist->rl = new_rl;
	runlist->elements = new_elements;
	runlist->alloc = new_alloc;
	ntfs_debug("Done: Realloc.");
	return 0;
}

/**
 * ntfs_rl_can_merge - test if two runlists can be joined together
 * @dst:	original runlist element
 * @src:	new runlist element to test for mergeability with @dst
 *
 * Test if two runlists can be joined together.  For this, their VCNs and LCNs
 * must be adjacent.
 *
 * It is up to the caller to serialize access to the runlists.
 *
 * Return 1 if the runlists can be merged and 0 otherwise.
 */
static inline u32 ntfs_rl_can_merge(ntfs_rl_element *dst, ntfs_rl_element *src)
{
	if (!dst || !src)
		panic("%s(): !dst || !src\n", __FUNCTION__);
	/* We can merge unmapped regions even if they are misaligned. */
	if ((dst->lcn == LCN_RL_NOT_MAPPED) && (src->lcn == LCN_RL_NOT_MAPPED))
		return 1;
	/* If the runs are misaligned, we cannot merge them. */
	if ((dst->vcn + dst->length) != src->vcn)
		return 0;
	/* If both runs are non-sparse and contiguous, we can merge them. */
	if ((dst->lcn >= 0) && (src->lcn >= 0) &&
			((dst->lcn + dst->length) == src->lcn))
		return 1;
	/* If we are merging two holes, we can merge them. */
	if ((dst->lcn == LCN_HOLE) && (src->lcn == LCN_HOLE))
		return 1;
	/* Cannot merge. */
	return 0;
}

/**
 * __ntfs_rl_merge - merge two runlists without testing if they can be merged
 * @dst:	original, destination runlist
 * @src:	new runlist to merge with @dst
 *
 * Merge the two runlists, writing into the destination runlist @dst.  The
 * caller must make sure the runlists can be merged or this will corrupt the
 * destination runlist.
 *
 * It is up to the caller to serialize access to the runlists.
 */
static inline void __ntfs_rl_merge(ntfs_rl_element *dst, ntfs_rl_element *src)
{
	dst->length += src->length;
}

/**
 * ntfs_rl_replace - overwrite a runlist element with another runlist
 * @dst_runlist:	destination runlist to work on
 * @src:		array of runlist elements to be inserted
 * @s_size:		number of elements in @src (excluding end marker)
 * @loc:		index in destination to overwrite with @src
 *
 * Replace the runlist element @loc in the destination runlist @dst_runlist
 * with @src.  Merge the left and right ends of the inserted runlist, if
 * necessary.
 *
 * It is up to the caller to serialize access to the runlists.
 *
 * Return 0 on success and ENOMEM if not enough memory to reallocate the
 * runlist, in which case the destination runlist is left unmodified.
 */
static inline errno_t ntfs_rl_replace(ntfs_runlist *dst_runlist,
		ntfs_rl_element *src, u32 s_size, u32 loc)
{
	ntfs_rl_element *d_rl;
	long delta;
	u32 d_elements;
	u32 tail;	/* Start of tail of @dst_runlist. */
	u32 marker;	/* End of the inserted runs. */
	u32 left;	/* Left end of @src needs merging. */
	u32 right;	/* Right end of @src needs merging. */

	ntfs_debug("Entering.");
	if (!dst_runlist || !src || !s_size)
		panic("%s(): !dst_runlist || !src || !s_size\n", __FUNCTION__);
	d_rl = dst_runlist->rl;
	d_elements = dst_runlist->elements;
	/* First, see if the left and right ends need merging. */
	left = 0;
	if (loc > 0)
		left = ntfs_rl_can_merge(d_rl + loc - 1, src);
	right = 0;
	if (loc + 1 < d_elements)
		right = ntfs_rl_can_merge(src + s_size - 1, d_rl + loc + 1);
	/*
	 * First run after the @src runs that have been inserted, i.e. where
	 * the tail of @dst needs to be moved to.  Nominally, @marker equals
	 * @loc + @s_size, i.e. location + number of runs in @src.  However, if
	 * @left, then the first run of @src will be merged with one in @dst.
	 */
	marker = loc + s_size - left;
	/*
	 * Offset of the tail of the destination runlist.  This needs to be
	 * moved out of the way to make space for the runs to be copied from
	 * @src, i.e. this is the first run of the tail of the destination
	 * runlist.  Nominally, @tail equals @loc + 1, i.e. location, skipping
	 * the replaced run.  However, if @right, then one of the runs of the
	 * destination runlist will be merged into @src.
	 */
	tail = loc + 1 + right;
	/*
	 * Extend the destination runlist reallocating the array buffer if
	 * needed.  Note we will need less if the left, right, or both ends get
	 * merged.  The -1 accounts for the run being replaced.
	 *
	 * If @delta < 0, we cannot reallocate or we would loose the end @delta
	 * element(s) of the destination runlist.  Thus we cheat by not
	 * reallocating at all and by postponing the update of
	 * @dst_runlist->elements to later.
	 */
	delta = s_size - 1 - left - right;
	if (delta > 0) {
		errno_t err;

		 /*
		  * TODO: Optimise by using ntfs_rl_ins() but then the below
		  * becomes different for the delta > 0 and delta <= 0 cases.
		  */
		err = ntfs_rl_inc(dst_runlist, delta);
		if (err)
			return err;
		d_rl = dst_runlist->rl;
	}
	/*
	 * We are guaranteed to succeed from here so can start modifying the
	 * original runlists.
	 */
	/*
	 * First, merge the left and right ends, if necessary.  Note, right end
	 * goes first because, if s_size is 1, and both right and left are 1,
	 * @src is updated in the right merge and the updated value is needed
	 * for the left merge.
	 */
	if (right)
		__ntfs_rl_merge(src + s_size - 1, d_rl + loc + 1);
	if (left)
		__ntfs_rl_merge(d_rl + loc - 1, src);
	/* Move the tail of @dst to its destination. */
	ntfs_rl_move(d_rl + marker, d_rl + tail, d_elements - tail);
	/* Copy in @src. */
	ntfs_rl_copy(d_rl + loc, src + left, s_size - left);
	/* We may have changed the length of the file, so fix the end marker. */
	if (d_elements - tail > 0 && d_rl[marker].lcn == LCN_ENOENT)
		d_rl[marker].vcn = d_rl[marker - 1].vcn +
				d_rl[marker - 1].length;
	/* If we postponed the @dst_runlist->elements update, do it now. */
	if (delta < 0)
		dst_runlist->elements += delta;
	ntfs_debug("Done, delta %ld.", delta);
	return 0;
}

/**
 * ntfs_rl_insert - insert a runlist into another
 * @dst_runlist:	destination runlist to work on
 * @src:		array of runlist elements to be inserted
 * @s_size:		number of elements in @src (excluding end marker)
 * @loc:		index in destination before which to insert @src
 *
 * Insert the runlist @src before the runlist element @loc in the runlist
 * @dst_runlist.  Merge the left end of the new runlist, if necessary.  Adjust
 * the size of the hole after the inserted runlist.
 *
 * Note this function can only be used if not the whole hole is being filled.
 *
 * It is up to the caller to serialize access to the runlists.
 *
 * Return 0 on success and ENOMEM if not enough memory to reallocate the
 * runlist, in which case the destination runlist is left unmodified.
 */
static inline errno_t ntfs_rl_insert(ntfs_runlist *dst_runlist,
		ntfs_rl_element *src, u32 s_size, u32 loc)
{
	ntfs_rl_element *d_rl;
	errno_t err;
	u32 d_elements;
	u32 left;	/* Left end of @src needs merging. */
	u32 disc;	/* Discontinuity between @dst_runlist and @src. */
	u32 marker;	/* End of the inserted runs. */

	ntfs_debug("Entering.");
	if (!dst_runlist || !src || !s_size)
		panic("%s(): !dst_runlist || !src || !s_size\n", __FUNCTION__);
	d_rl = dst_runlist->rl;
	d_elements = dst_runlist->elements;
	/*
	 * Determine if there is a discontinuity between the end of the
	 * destination runlist and the start of @src.  This means we might need
	 * to insert a "not mapped" run.
	 */
	disc = 0;
	if (!loc) {
		left = 0;
		if (src[0].vcn > 0)
			disc = 1;
	} else {
		s64 merged_length;

		left = ntfs_rl_can_merge(d_rl + loc - 1, src);
		merged_length = d_rl[loc - 1].length;
		if (left)
			merged_length += src[0].length;
		if (src[0].vcn > d_rl[loc - 1].vcn + merged_length)
			disc = 1;
	}
	/*
	 * Space required: @dst_runlist size + @src size, less 1 if we will
	 * merge the run on the left, plus 1 if there was a discontinuity.
	 */
	 /* TODO: Optimise by using ntfs_rl_ins(). */
	err = ntfs_rl_inc(dst_runlist, s_size - left + disc);
	if (err)
		return err;
	d_rl = dst_runlist->rl;
	/*
	 * We are guaranteed to succeed from here so can start modifying the
	 * original runlist.
	 */
	if (left)
		__ntfs_rl_merge(d_rl + loc - 1, src);
	/*
	 * First run after the @src runs that have been inserted.  Nominally,
	 * @marker equals @loc + @s_size, i.e. location + number of runs in
	 * @src.  However, if @left, then the first run in @src has been merged
	 * with one in @dst.  And if @disc, then @dst and @src do not meet and
	 * we need an extra run to fill the gap.
	 */
	marker = loc + s_size - left + disc;
	/* Move the tail of @dst_runlist out of the way, then copy in @src. */
	ntfs_rl_move(d_rl + marker, d_rl + loc, d_elements - loc);
	ntfs_rl_copy(d_rl + loc + disc, src + left, s_size - left);
	/* Adjust both VCN and length of the first run after the insertion. */
	d_rl[marker].vcn = d_rl[marker - 1].vcn + d_rl[marker - 1].length;
	if (d_rl[marker].lcn == LCN_HOLE ||
			d_rl[marker].lcn == LCN_RL_NOT_MAPPED)
		d_rl[marker].length = d_rl[marker + 1].vcn - d_rl[marker].vcn;
	/* Writing beyond the end of the file and there is a discontinuity. */
	if (disc) {
		if (loc > 0) {
			d_rl[loc].vcn = d_rl[loc - 1].vcn +
					d_rl[loc - 1].length;
			d_rl[loc].length = d_rl[loc + 1].vcn - d_rl[loc].vcn;
		} else {
			d_rl[loc].vcn = 0;
			d_rl[loc].length = d_rl[loc + 1].vcn;
		}
		d_rl[loc].lcn = LCN_RL_NOT_MAPPED;
	}
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_rl_append - append a runlist after a given element
 * @dst_runlist:	destination runlist to work on
 * @src:		array of runlist elements to be inserted
 * @s_size:		number of elements in @src (excluding end marker)
 * @loc:		index in destination after which to append @src
 *
 * Append the runlist @src after the runlist element @loc in the runlist
 * @dst_runlist.  Merge the right end of the new runlist, if necessary.  Adjust
 * the size of the hole before the appended runlist.
 *
 * It is up to the caller to serialize access to the runlists.
 *
 * Return 0 on success and ENOMEM if not enough memory to reallocate the
 * runlist, in which case the destination runlist is left unmodified.
 */
static inline errno_t ntfs_rl_append(ntfs_runlist *dst_runlist,
		ntfs_rl_element *src, u32 s_size, u32 loc)
{
	ntfs_rl_element *d_rl;
	errno_t err;
	u32 d_elements;
	u32 right;	/* Right end of @src needs merging. */
	u32 marker;	/* End of the inserted runs. */

	ntfs_debug("Entering.");
	if (!dst_runlist || !src || !s_size)
		panic("%s(): !dst_runlist || !src || !s_size\n", __FUNCTION__);
	d_rl = dst_runlist->rl;
	d_elements = dst_runlist->elements;
	/* First, check if the right hand end needs merging. */
	right = 0;
	if (loc + 1 < d_elements)
		right = ntfs_rl_can_merge(src + s_size - 1, d_rl + loc + 1);
	/*
	 * Space required: @dst_runlist size + @src size, less 1 if we will
	 * merge the run on the right.
	 */
	 /* TODO: Optimise by using ntfs_rl_ins(). */
	err = ntfs_rl_inc(dst_runlist, s_size - right);
	if (err)
		return err;
	d_rl = dst_runlist->rl;
	/*
	 * We are guaranteed to succeed from here so can start modifying the
	 * original runlists.
	 */
	/* First, merge the right hand end, if necessary. */
	if (right)
		__ntfs_rl_merge(src + s_size - 1, d_rl + loc + 1);
	/* First run after the @src runs that have been inserted. */
	marker = loc + s_size + 1;
	/* Move the tail of @dst_runlist out of the way, then copy in @src. */
	ntfs_rl_move(d_rl + marker, d_rl + loc + 1 + right,
			d_elements - (loc + 1 + right));
	ntfs_rl_copy(d_rl + loc + 1, src, s_size);
	/* Adjust the size of the preceding hole. */
	d_rl[loc].length = d_rl[loc + 1].vcn - d_rl[loc].vcn;
	/* We may have changed the length of the file, so fix the end marker. */
	if (d_rl[marker].lcn == LCN_ENOENT)
		d_rl[marker].vcn = d_rl[marker - 1].vcn +
				d_rl[marker - 1].length;
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_rl_split - insert a runlist into the centre of a hole
 * @dst_runlist:	destination runlist to insert into
 * @src:		array of runlist elements to be inserted
 * @s_size:		number of elements in @src (excluding end marker)
 * @loc:		index in destination at which to split and insert @src
 *
 * Split the runlist @dst_runlist at @loc into two and insert @src in between
 * the two fragments.  No merging of runlists is necessary.  Adjust the size of
 * the holes either side.
 *
 * It is up to the caller to serialize access to the runlists.
 *
 * Return 0 on success and ENOMEM if not enough memory to reallocate the
 * runlist, in which case the destination runlist is left unmodified.
 */
static inline errno_t ntfs_rl_split(ntfs_runlist *dst_runlist,
		ntfs_rl_element *src, u32 s_size, u32 loc)
{
	ntfs_rl_element *d_rl;
	errno_t err;

	ntfs_debug("Entering.");
	if (!dst_runlist || !src || !s_size)
		panic("%s(): !dst_runlist || !src || !s_size\n", __FUNCTION__);
	/*
	 * Move the tail of the destination runlist out of the way,
	 * reallocating the array buffer if needed.  Note we need space for
	 * both the source runlist and for a new hole.
	 */
	err = ntfs_rl_ins(dst_runlist, loc + 1, s_size + 1);
	if (err)
		return err;
	d_rl = dst_runlist->rl;
	/* Copy @src into the destination runlist. */
	ntfs_rl_copy(d_rl + loc + 1, src, s_size);
	/* Adjust the size of the holes either size of @src. */
	d_rl[loc].length = d_rl[loc + 1].vcn - d_rl[loc].vcn;
	d_rl[loc + s_size + 1].lcn = d_rl[loc].lcn;
	loc += s_size;
	d_rl[loc + 1].vcn = d_rl[loc].vcn + d_rl[loc].length;
	loc += 1;
	d_rl[loc].length = d_rl[loc + 1].vcn - d_rl[loc].vcn;
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_rl_merge - merge two runlists into one
 * @dst_runlist:	destination runlist to merge source runlist into
 * @src_runlist:	source runlist to merge into destination
 *
 * First we sanity check the two runlists @dst_runlist and @src_runlist to make
 * sure that they are sensible and can be merged.  The source runlist
 * @src_runlist must be either after the destination runlist @dst_runlist or
 * completely within a hole (or unmapped region) of @dst_runlist.
 *
 * Merging of runlists is necessary in two cases:
 *   1. When attribute lists are used and a further extent is being mapped.
 *   2. When new clusters are allocated to fill a hole or extend a file.
 *
 * There are four possible ways the source runlist can be merged.  It can:
 *	- be inserted at the beginning of a hole,
 *	- split the hole in two and be inserted between the two fragments,
 *	- be appended at the end of a hole, or it can
 *	- replace the whole hole.
 * It can also be appended to the end of the runlist, which is just a variant
 * of the insert case.
 *
 * On success, return 0 and @dst_runlist is updated to be the merged runlist.
 * Note, @src_runlist is deinitialized and the runlist element arrays of both
 * @src_runlist and @dst_runlist are deallocated before returning so you cannot
 * use old pointers into the arrays for anything any more.  (Strictly speaking
 * the merged runlist may be the same as @dst_runlist but this is irrelevant.)
 *
 * On error, return errno, in which case both runlists are left unmodified.
 * The following error codes are defined:
 *	ENOMEM - Not enough memory to allocate runlist array.
 *	EINVAL - Invalid parameters were passed in.
 *	ERANGE - The runlists overlap and cannot be merged.
 *
 * Locking: - The caller must have locked the destination runlist for writing.
 *	    - The lock of the source runlist is not touched thus is irrelevant.
 *	      It is assumed that the source is just a temporary runlist.
 *	    - The destination runlist is modified.
 *	    - The source runlist's array of runlist elements is deallocated.
 */
errno_t ntfs_rl_merge(ntfs_runlist *dst_runlist, ntfs_runlist *src_runlist)
{
	VCN marker_vcn;
	ntfs_rl_element *d_rl, *s_rl;
	u32 d_elements, s_elements;
	u32 d_alloc, s_alloc;
	u32 di, si;	/* Current index into @[ds]_rl. */
	u32 s_start;	/* First index into @s_rl with lcn >= LCN_HOLE. */
	u32 d_ins;	/* Index into @d_rl at which to insert @s_rl. */
	u32 d_final;	/* The last index into @d_rl with lcn >= LCN_HOLE. */
	u32 s_final;	/* The last index into @s_rl with lcn >= LCN_HOLE. */
	u32 ss;		/* Number of relevant elements in @s_rl. */
	u32 marker;
	errno_t err;
	BOOL start, finish;

	ntfs_debug("Destination runlist:");
	ntfs_debug_rl_dump(dst_runlist);
	ntfs_debug("Source runlist:");
	ntfs_debug_rl_dump(src_runlist);
	if (!src_runlist || !dst_runlist)
		panic("%s(): No %s runlist was supplied.\n", __FUNCTION__,
				!src_runlist ? "source" : "destination");
	s_rl = src_runlist->rl;
	s_elements = src_runlist->elements;
	s_alloc = src_runlist->alloc;
	/* If the source runlist is empty, nothing to do. */
	if (!s_elements) {
		if (s_alloc)
			OSFree(s_rl, s_alloc, ntfs_malloc_tag);
		goto done;
	}
	d_rl = dst_runlist->rl;
	d_elements = dst_runlist->elements;
	d_alloc = dst_runlist->alloc;
	/* Check for the case where the first mapping is being done now. */
	if (!d_elements) {
		/* Complete the source runlist if necessary. */
		if (s_rl[0].vcn) {
			err = ntfs_rl_ins(src_runlist, 0, 1);
			if (err)
				return err;
			s_rl = src_runlist->rl;
			s_rl[0].vcn = 0;
			s_rl[0].lcn = LCN_RL_NOT_MAPPED;
			s_rl[0].length = s_rl[1].vcn;
		}
		/* Return the source runlist as the destination. */
		if (d_alloc)
			OSFree(d_rl, d_alloc, ntfs_malloc_tag);
		dst_runlist->rl = src_runlist->rl;
		dst_runlist->elements = src_runlist->elements;
		dst_runlist->alloc = src_runlist->alloc;
		goto done;
	}
	/*
	 * We have two runlists which we need to merge.  Start, by skipping all
	 * unmapped start elements in the source runlist.
	 */
	si = 0;
	while (s_rl[si].length && s_rl[si].lcn < LCN_HOLE)
		si++;
	/* May not have an entirely unmapped source runlist. */
	if (!s_rl[si].length)
		panic("%s(): Source runlist is entirely unmapped!\n",
				__FUNCTION__);
	/* Record the starting point in @s_rl at which to begin merging. */
	s_start = si;
	/*
	 * Skip forward in @d_rl until we reach the position where @s_rl needs
	 * to be inserted.  If we reach the end of @d_rl, @s_rl just needs to
	 * be appended to @d_rl.
	 */
	for (d_ins = 0; d_rl[d_ins].length; d_ins++) {
		if (d_rl[d_ins + 1].vcn > s_rl[s_start].vcn)
			break;
	}
	/* Sanity check for illegal overlaps. */
	if ((d_rl[d_ins].vcn == s_rl[s_start].vcn) && (d_rl[d_ins].lcn >= 0) &&
			(s_rl[s_start].lcn >= 0)) {
		ntfs_error(NULL, "Runlists overlap.  Cannot merge!");
		return ERANGE;
	}
	/* Scan backwards for the last element with lcn >= LCN_HOLE. */
	for (s_final = s_elements - 1; s_final > 0; s_final--) {
		if (s_rl[s_final].lcn >= LCN_HOLE)
			break;
	}
	for (d_final = d_elements - 1; d_final > 0; d_final--) {
		if (d_rl[d_final].lcn >= LCN_HOLE)
			break;
	}
	/* Calculate the number of elements relevant to the merge. */
	ss = s_final - s_start + 1;
	/*
	 * Work out the type of merge needed.  To do so we setup to variables.
	 *
	 * If @start is true, the merge point is either at the end of the
	 * destination runlist, i.e. at the end of the attribute represented by
	 * the destination runlist, or at the start of a hole or an unmapped
	 * region.
	 *
	 * If @start is false, the merge point is not at the end of the
	 * destination runlist nor is it at the start of a hole or unmapped
	 * region.
	 *
	 * If @finish is true, the merge point is not at the end of the
	 * destination runlist (thus it must be in a hole or unmapped region)
	 * and the end of the hole or unmapped region lies within the end of
	 * the source runlist.
	 *
	 * If @finish is false, the merge point is either at the end of the
	 * destination runlist or the end of the hole or unmapped region lies
	 * outside the end of the source runlist.
	 */
	start = (d_rl[d_ins].lcn < LCN_RL_NOT_MAPPED ||
			d_rl[d_ins].vcn == s_rl[s_start].vcn);
	finish = (d_rl[d_ins].lcn >= LCN_RL_NOT_MAPPED &&
			d_rl[d_ins].vcn + d_rl[d_ins].length <=
			s_rl[s_elements - 1].vcn);
	/* Or we will lose an end marker. */
	if (finish && !d_rl[d_ins].length)
		ss++;
	/*
	 * Is there an end of attribute marker in the source runlist that we
	 * need to preserve?
	 */
	marker = 0;
	marker_vcn = 0;
	if (s_rl[s_elements - 1].lcn == LCN_ENOENT) {
		marker = s_elements - 1;
		marker_vcn = s_rl[marker].vcn;
		if (d_rl[d_ins].vcn + d_rl[d_ins].length >
				s_rl[s_elements - 2].vcn)
			finish = FALSE;
	}
#if 1
	ntfs_debug("d_final %u, d_elements %u", (unsigned)d_final,
			(unsigned)d_elements);
	ntfs_debug("s_start = %u, s_final = %u, s_elements = %u",
			(unsigned)s_start, (unsigned)s_final,
			(unsigned)s_elements);
	ntfs_debug("start = %u, finish = %u", start ? 1 : 0, finish ? 1 : 0);
	ntfs_debug("ss = %u, d_ins = %u", (unsigned)ss, (unsigned)d_ins);
#endif
	/* See above for meanings of @start and @finish. */
	if (start) {
		if (finish)
			err = ntfs_rl_replace(dst_runlist, s_rl + s_start, ss,
					d_ins);
		else
			err = ntfs_rl_insert(dst_runlist, s_rl + s_start, ss,
					d_ins);
	} else {
		if (finish)
			err = ntfs_rl_append(dst_runlist, s_rl + s_start, ss,
					d_ins);
		else
			err = ntfs_rl_split(dst_runlist, s_rl + s_start, ss,
					d_ins);
	}
	if (err) {
		ntfs_error(NULL, "Merge failed (error %d).", (int)err);
		return err;
	}
	/* Merged, can discard source runlist now. */
	OSFree(s_rl, s_alloc, ntfs_malloc_tag);
	d_rl = dst_runlist->rl;
	di = dst_runlist->elements - 1;
	/* Deal with the end of attribute marker if @s_rl ended after @d_rl. */
	if (marker && d_rl[di].vcn <= marker_vcn) {
		u32 slots;

		ntfs_debug("Triggering marker code.");
		d_alloc = dst_runlist->alloc;
		if (d_rl[di].vcn == marker_vcn) {
			ntfs_debug("Old marker = 0x%llx, replacing with "
					"LCN_ENOENT.",
					(unsigned long long)d_rl[di].lcn);
			d_rl[di].lcn = LCN_ENOENT;
			goto done;
		}
		/*
		 * We need to create an unmapped runlist element in @d_rl or
		 * extend an existing one before adding the LCN_ENOENT
		 * terminator element.
		 */
		slots = 0;
		/*
		 * We can discard an existing LCN_ENOENT terminator and hence
		 * gain a slot.
		 */
		if (d_rl[di].lcn == LCN_ENOENT) {
			di--;
			slots = 1;
		}
		/*
		 * If not unmapped, add an unmapped runlist element.
		 * Otherwise simply extend the unmapped element.
		 */
		if (d_rl[di].lcn != LCN_RL_NOT_MAPPED) {
			if (!slots) {
				slots = 2;
				err = ntfs_rl_inc(dst_runlist, 2);
				if (err) {
fatal_oom:
					panic("%s(): Fatal out of memory "
							"condition "
							"encountered (slots "
							"%u).\n",
							__FUNCTION__,
							(unsigned)slots);
				}
				d_rl = dst_runlist->rl;
				/* Need to set vcn. */
				d_rl[di + 1].vcn = d_rl[di].vcn +
						d_rl[di].length;
			}
			di++;
			d_rl[di].lcn = LCN_RL_NOT_MAPPED;
			/* We now used up a slot. */
			slots--;
		}
		d_rl[di].length = marker_vcn - d_rl[di].vcn;
		/* Finally add the LCN_ENOENT terminator. */
		di++;
		if (!slots) {
			err = ntfs_rl_inc(dst_runlist, 1);
			if (err) {
				slots = 1;
				goto fatal_oom;
			}
			d_rl = dst_runlist->rl;
		}
		d_rl[di].vcn = marker_vcn;
		d_rl[di].lcn = LCN_ENOENT;
		d_rl[di].length = 0;
	}
done:
	/* The merge was completed successfully. */
	ntfs_debug("Merged runlist:");
	ntfs_debug_rl_dump(dst_runlist);
	return 0;
}

/**
 * ntfs_mapping_pairs_decompress - convert mapping pairs array to runlist
 * @vol:	ntfs volume on which the attribute resides
 * @a:		attribute record whose mapping pairs array to decompress
 * @runlist:	runlist in which to insert @a's runlist
 *
 * It is up to the caller to serialize access to the runlist @runlist.
 *
 * Decompress the attribute @a's mapping pairs array into a runlist.  On
 * success, return the decompressed runlist in @runlist.
 *
 * If @runlist already contains a runlist, the decompressed runlist is inserted
 * into the appropriate place in @runlist and the resultant, combined runlist
 * is returned in @runlist.
 *
 * On error, return errno.  @runlist is left unmodified in that case.
 *
 * The following error codes are defined:
 *	ENOMEM	- Not enough memory to allocate runlist array.
 *	EIO	- Corrupt attribute.
 *	EINVAL	- Invalid parameters were passed in.
 *	ERANGE	- The two runlists overlap.
 *
 * Locking: - The caller must have locked the runlist for writing.
 *	    - This function does not touch the lock.
 *	    - The runlist is modified.
 *
 * FIXME: For now we take the conceptionally simplest approach of creating the
 * new runlist disregarding the already existing one and then splicing the two
 * into one, if that is possible (we check for overlap and discard the new
 * runlist if overlap is present before returning ERANGE).
 */
errno_t ntfs_mapping_pairs_decompress(ntfs_volume *vol, const ATTR_RECORD *a,
		ntfs_runlist *runlist)
{
	VCN vcn;		/* Current vcn. */
	LCN lcn;		/* Current lcn. */
	s64 deltaxcn;		/* Change in [vl]cn. */
	ntfs_rl_element *rl;	/* The output runlist. */
	u8 *buf;		/* Current position in mapping pairs array. */
	u8 *a_end;		/* End of attribute. */
	unsigned rlsize;	/* Size of runlist buffer. */
	errno_t err = EIO;
	u16 rlpos;		/* Current runlist position in units of
				   runlist_elements. */
	u8 b;			/* Current byte offset in buf. */

	/* Make sure @a exists and is non-resident. */
	if (!a || !a->non_resident || sle64_to_cpu(a->lowest_vcn) < (VCN)0) {
		ntfs_error(vol->mp, "Invalid arguments.");
		return EINVAL;
	}
	/* Start at vcn = lowest_vcn and lcn 0. */
	vcn = sle64_to_cpu(a->lowest_vcn);
	lcn = 0;
	/* Get start of the mapping pairs array. */
	buf = (u8*)a + le16_to_cpu(a->mapping_pairs_offset);
	a_end = (u8*)a + le32_to_cpu(a->length);
	if (buf < (u8*)a || buf > a_end) {
		ntfs_error(vol->mp, "Corrupt attribute.");
		return EIO;
	}
	/* If the mapping pairs array is valid but empty, nothing to do. */
	if (!vcn && !*buf)
		return 0;
	/* Current position in runlist array. */
	rlpos = 0;
	rlsize = NTFS_RL_ALLOC_BLOCK;
	/* Allocate NTFS_RL_ALLOC_BLOCK bytes for the runlist. */
	rl = OSMalloc(rlsize, ntfs_malloc_tag);
	if (!rl)
		return ENOMEM;
	/* Insert unmapped starting element if necessary. */
	if (vcn) {
		rl->vcn = 0;
		rl->lcn = LCN_RL_NOT_MAPPED;
		rl->length = vcn;
		rlpos++;
	}
	while (buf < a_end && *buf) {
		/*
		 * Allocate more memory if needed, including space for the
		 * not-mapped and terminator elements.
		 */
		if (((rlpos + 3) * sizeof(*rl)) > rlsize) {
			ntfs_rl_element *rl2;

			rl2 = OSMalloc(rlsize + NTFS_RL_ALLOC_BLOCK,
					ntfs_malloc_tag);
			if (!rl2) {
				err = ENOMEM;
				goto err;
			}
			memcpy(rl2, rl, rlsize);
			OSFree(rl, rlsize, ntfs_malloc_tag);
			rl = rl2;
			rlsize += NTFS_RL_ALLOC_BLOCK;
		}
		/* Enter the current vcn into the current runlist element. */
		rl[rlpos].vcn = vcn;
		/*
		 * Get the change in vcn, i.e. the run length in clusters.
		 * Doing it this way ensures that we sign-extend negative
		 * values.  A negative run length does not make any sense, but
		 * hey, I did not design NTFS...
		 */
		b = *buf & 0xf;
		if (b) {
			if (buf + b >= a_end)
				goto io_err;
			for (deltaxcn = (s8)buf[b--]; b; b--)
				deltaxcn = (deltaxcn << 8) + buf[b];
		} else { /* The length entry is compulsory. */
			ntfs_error(vol->mp, "Missing length entry in mapping "
					"pairs array.");
			deltaxcn = (s64)-1;
		}
		/*
		 * Assume a negative length to indicate data corruption and
		 * hence clean-up and return NULL.
		 */
		if (deltaxcn < 0) {
			ntfs_error(vol->mp, "Invalid length in mapping pairs "
					"array.");
			goto err;
		}
		/*
		 * Enter the current run length into the current runlist
		 * element.
		 */
		rl[rlpos].length = deltaxcn;
		/* Increment the current vcn by the current run length. */
		vcn += deltaxcn;
		/*
		 * There might be no lcn change at all, as is the case for
		 * sparse clusters on NTFS 3.0+, in which case we set the lcn
		 * to LCN_HOLE.
		 */
		if (!(*buf & 0xf0))
			rl[rlpos].lcn = LCN_HOLE;
		else {
			/* Get the lcn change which really can be negative. */
			u8 b2 = *buf & 0xf;
			b = b2 + ((*buf >> 4) & 0xf);
			if (buf + b >= a_end)
				goto io_err;
			for (deltaxcn = (s8)buf[b--]; b > b2; b--)
				deltaxcn = (deltaxcn << 8) + buf[b];
			/* Change the current lcn to its new value. */
			lcn += deltaxcn;
#ifdef DEBUG
			/*
			 * On NTFS 1.2-, apparently one can have lcn == -1 to
			 * indicate a hole.  But we have not verified ourselves
			 * whether it is really the lcn or the deltaxcn that is
			 * -1.  So if either is found give us a message so we
			 * can investigate it further!
			 */
			if (vol->major_ver < 3) {
				if (deltaxcn == (LCN)-1)
					ntfs_error(vol->mp, "lcn delta == -1");
				if (lcn == (LCN)-1)
					ntfs_error(vol->mp, "lcn == -1");
			}
#endif
			/* Check lcn is not below -1. */
			if (lcn < (LCN)-1) {
				ntfs_error(vol->mp, "Invalid LCN < -1 in "
						"mapping pairs array.");
				goto err;
			}
			/* Enter the current lcn into the runlist element. */
			rl[rlpos].lcn = lcn;
		}
		/* Get to the next runlist element. */
		rlpos++;
		/* Increment the buffer position to the next mapping pair. */
		buf += (*buf & 0xf) + ((*buf >> 4) & 0xf) + 1;
	}
	if (buf >= a_end)
		goto io_err;
	/*
	 * If there is a highest_vcn specified, it must be equal to the final
	 * vcn in the runlist - 1, or something has gone badly wrong.
	 */
	deltaxcn = sle64_to_cpu(a->highest_vcn);
	if (deltaxcn && vcn - 1 != deltaxcn)
		goto io_err;
	/* Setup not mapped runlist element if this is the base extent. */
	if (!a->lowest_vcn) {
		VCN max_cluster;

		max_cluster = ((sle64_to_cpu(a->allocated_size) +
				vol->cluster_size - 1) >>
				vol->cluster_size_shift) - 1;
		/*
		 * A highest_vcn of zero means this is a single extent
		 * attribute so simply terminate the runlist with LCN_ENOENT).
		 */
		if (deltaxcn) {
			/*
			 * If there is a difference between the highest_vcn and
			 * the highest cluster, the runlist is either corrupt
			 * or, more likely, there are more extents following
			 * this one.
			 */
			if (deltaxcn < max_cluster) {
				ntfs_debug("More extents to follow; deltaxcn "
						"= 0x%llx, max_cluster = "
						"0x%llx",
						(unsigned long long)deltaxcn,
						(unsigned long long)
						max_cluster);
				rl[rlpos].vcn = vcn;
				vcn += rl[rlpos].length = max_cluster -
						deltaxcn;
				rl[rlpos].lcn = LCN_RL_NOT_MAPPED;
				rlpos++;
			} else if (deltaxcn > max_cluster) {
				ntfs_error(vol->mp, "Corrupt attribute.  "
						"deltaxcn = 0x%llx, "
						"max_cluster = 0x%llx",
						(unsigned long long)deltaxcn,
						(unsigned long long)
						max_cluster);
				goto io_err;
			}
		}
		rl[rlpos].lcn = LCN_ENOENT;
	} else /* Not the base extent.  There may be more extents to follow. */
		rl[rlpos].lcn = LCN_RL_NOT_MAPPED;
	/* Setup terminating runlist element. */
	rl[rlpos].vcn = vcn;
	rl[rlpos].length = (s64)0;
	/* If no existing runlist was specified, we are done. */
	if (!runlist->elements) {
		if (runlist->alloc)
			OSFree(runlist->rl, runlist->alloc, ntfs_malloc_tag);
		runlist->rl = rl;
		runlist->elements = rlpos + 1;
		runlist->alloc = rlsize;
		ntfs_debug("Mapping pairs array successfully decompressed:");
		ntfs_debug_rl_dump(runlist);
		return 0;
	} else {
		ntfs_runlist tmp_rl;

		/*
		 * An existing runlist was specified, now combine the new and
		 * old runlists checking for overlaps.
		 */
		tmp_rl.rl = rl;
		tmp_rl.elements = rlpos + 1;
		tmp_rl.alloc = rlsize;
		err = ntfs_rl_merge(runlist, &tmp_rl);
		if (!err)
			return err;
		ntfs_error(vol->mp, "Failed to merge runlists.");
	}
err:
	OSFree(rl, rlsize, ntfs_malloc_tag);
	return err;
io_err:
	ntfs_error(vol->mp, "Corrupt mapping pairs array in non-resident "
			"attribute.");
	err = EIO;
	goto err;
}

/**
 * ntfs_rl_vcn_to_lcn - convert a vcn into a lcn given a runlist
 * @rl:		runlist to use for conversion
 * @vcn:	vcn to convert
 * @clusters:	optional return pointer for the number of contiguous clusters
 *
 * Convert the virtual cluster number @vcn of an attribute into a logical
 * cluster number (lcn) of a device using the runlist @rl to map vcns to their
 * corresponding lcns.
 *
 * If @clusters is not NULL, on success (i.e. we return >= LCN_HOLE) we return
 * the number of contiguous clusters after the returned lcn in *@clusters.
 *
 * Since lcns must be >= 0, we use negative return codes with special meaning:
 *
 * Return code		Meaning / Description
 * ==================================================
 *  LCN_HOLE		Hole / not allocated on disk.
 *  LCN_RL_NOT_MAPPED	This is part of the runlist which has not been
 *			inserted into the runlist yet.
 *  LCN_ENOENT		There is no such vcn in the attribute.
 *
 * Locking: - The caller must have locked the runlist (for reading or writing).
 *	    - This function does not touch the lock.
 *	    - The runlist is not modified.
 *
 * TODO: If we pass in the number of runlist elements we could attempt to
 * optimise the for loop into a quasi binary search.
 */
LCN ntfs_rl_vcn_to_lcn(const ntfs_rl_element *rl, const VCN vcn, s64 *clusters)
{
	unsigned i;

	if (vcn < 0)
		panic("%s(): vcn < 0\n", __FUNCTION__);
	/*
	 * If @rl is NULL, assume that we have found an unmapped runlist.  The
	 * caller can then attempt to map it and fail appropriately if
	 * necessary.
	 */
	if (!rl)
		return LCN_RL_NOT_MAPPED;
	/* Catch out of lower bounds vcn. */
	if (vcn < rl[0].vcn)
		return LCN_ENOENT;
	for (i = 0; rl[i].length; i++) {
		if (vcn < rl[i + 1].vcn) {
			const s64 ofs = vcn - rl[i].vcn;
			if (clusters)
				*clusters = rl[i].length - ofs;
			if (rl[i].lcn >= (LCN)0)
				return rl[i].lcn + ofs;
			return rl[i].lcn;
		}
	}
	/*
	 * Set *@clusters just in case rl[i].lcn is LCN_HOLE.  That should
	 * never happen since the terminator element should never be of type
	 * LCN_HOLE but better be safe than sorry.
	 */
	if (clusters)
		*clusters = 0;
	/*
	 * The terminator element is setup to the correct value, i.e. one of
	 * LCN_HOLE, LCN_RL_NOT_MAPPED, or LCN_ENOENT.
	 */
	if (rl[i].lcn < (LCN)0)
		return rl[i].lcn;
	/* Just in case...  We could replace this with panic() some day. */
	return LCN_ENOENT;
}

/**
 * ntfs_rl_read - load data described by an runlist from disk
 * @vol:		ntfs volume from which to read
 * @runlist:		runlist describing vcn to lcn mapping of data
 * @dst:		destination buffer
 * @size:		size of the destination buffer in bytes
 * @initialized_size:	initialized size of the data in the runlist
 *
 * Walk the runlist @runlist and load all clusters from it copying them into
 * the linear buffer @dst.  The maximum number of bytes copied to @dst is @size
 * bytes.  Note, @size does not need to be a multiple of the cluster size.  If
 * @initialized_size is less than @size, the region in @dst between
 * @initialized_size and @size will be zeroed (and in fact not read at all).
 *
 * Return 0 on success or errno on error.
 *
 * Note: Sparse runlists are not supported as this function is only used to
 * load the attribute list attribute value which may not be sparse.
 *
 * Locking: - The caller must not have locked the runlist for writing.
 *	    - This function takes the lock for reading.
 *	    - The runlist is not modified.
 */
errno_t ntfs_rl_read(ntfs_volume *vol, ntfs_runlist *runlist, u8 *dst,
		const s64 size, const s64 initialized_size)
{
	u8 *dst_end = dst + initialized_size;
	ntfs_rl_element *rl;
	vnode_t dev_vn = vol->dev_vn;
	errno_t err;
	int block_size = vol->sector_size;
	const u8 cluster_to_block_shift = vol->cluster_size_shift -
			vol->sector_size_shift;

	ntfs_debug("Entering.");
	if (!vol || !runlist || !dst || size <= 0 || initialized_size < 0 ||
			initialized_size > size) {
		ntfs_error(vol->mp, "Received invalid arguments.");
		return EINVAL;
	}
	if (!initialized_size) {
		bzero(dst, size);
		ntfs_debug("Done (!initialized_size).");
		return 0;
	}
	lck_rw_lock_shared(&runlist->lock);
	rl = runlist->rl;
	if (!rl) {
		ntfs_error(vol->mp, "Cannot read attribute list since runlist "
				"is missing.");
		err = EIO;
		goto err;	
	}
	/* Read all clusters specified by the runlist one run at a time. */
	while (rl->length) {
		daddr64_t block, max_block;

		ntfs_debug("Reading vcn 0x%llx, lcn 0x%llx, length 0x%llx.",
				(unsigned long long)rl->vcn,
				(unsigned long long)rl->lcn,
				(unsigned long long)rl->length);
		/* The runlist may not be sparse. */
		if (rl->lcn < 0) {
			ntfs_error(vol->mp, "Runlist is invalid, not mapped, "
					"or sparse.  Cannot read data.");
			err = EIO;
			goto err;
		}
		/* Read the run from device in chunks of block_size bytes. */
		block = rl->lcn << cluster_to_block_shift;
		max_block = block + (rl->length << cluster_to_block_shift);
		ntfs_debug("max_block 0x%llx.", (unsigned long long)max_block);
		do {
			buf_t buf;

			ntfs_debug("Reading block 0x%llx.",
					(unsigned long long)block);
			err = buf_meta_bread(dev_vn, block, block_size, NOCRED,
					&buf);
			if (err) {
				buf_brelse(buf);
				ntfs_error(vol->mp, "buf_meta_bread() failed "
						"(error %d).   Cannot read "
						"data.", (int)err);
				goto err;
			}
			/* Copy the data into the buffer. */
			if (dst + block_size > dst_end)
				block_size = dst_end - dst;
			memcpy(dst, (u8*)buf_dataptr(buf), block_size);
			buf_brelse(buf);
			dst += block_size;
			if (dst >= dst_end)
				goto done;
		} while (++block < max_block);
		rl++;
	}
done:
	lck_rw_unlock_shared(&runlist->lock);
	/* If the runlist was too short, zero out the unread part. */
	if (dst < dst_end)
		bzero(dst, dst_end - dst);
	/* Zero the uninitialized region if present. */
	if (initialized_size < size)
		bzero(dst_end, size - initialized_size);
	ntfs_debug("Done.");
	return 0;
err:
	lck_rw_unlock_shared(&runlist->lock);
	return err;
}
