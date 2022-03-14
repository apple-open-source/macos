/*
 * ntfs_runlist.c - NTFS kernel runlist operations.
 *
 * Copyright (c) 2001-2008 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2002-2005 Richard Russon.  All Rights Reserved.
 * Portions Copyright (c) 2006-2008 Apple Inc.  All Rights Reserved.
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

#include <kern/debug.h>
#include <kern/locks.h>
#include <IOKit/IOLib.h>

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
		const unsigned size)
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
static errno_t ntfs_rl_inc(ntfs_runlist *runlist, unsigned delta)
{
	unsigned new_elements = runlist->elements + delta;
	unsigned count = runlist->alloc_count;
	unsigned new_count = new_elements + NTFS_ALLOC_BLOCK / sizeof(ntfs_rl_element);
	if (new_count > count) {
		ntfs_rl_element* new_rl = IONewData(ntfs_rl_element, new_count);
		if (!new_rl)
			return ENOMEM;
		ntfs_rl_copy(new_rl, runlist->rl, runlist->elements);
		if (count)
			IODeleteData(runlist->rl, ntfs_rl_element, count);
		runlist->rl = new_rl;
		runlist->alloc_count = new_count;
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
		const unsigned size)
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
static errno_t ntfs_rl_ins(ntfs_runlist *runlist, unsigned pos, unsigned ins_count)
{
	ntfs_rl_element *new_rl = runlist->rl;

	ntfs_debug("Entering with pos %u, count %u.", pos, ins_count);
	if (pos > runlist->elements)
		panic("%s(): pos > runlist->elements\n", __FUNCTION__);
	unsigned new_elements = runlist->elements + ins_count;
	unsigned count = runlist->alloc_count;
	unsigned new_count = new_elements + NTFS_ALLOC_BLOCK / sizeof(ntfs_rl_element);
	/* If no memory reallocation needed, it is a simple memmove(). */
	if (new_count <= count) {
		if (ins_count) {
			new_rl += pos;
			ntfs_rl_move(new_rl + ins_count, new_rl, runlist->elements - pos);
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
	new_rl = IONewData(ntfs_rl_element, new_count);
	if (!new_rl)
		return ENOMEM;
	ntfs_rl_copy(new_rl, runlist->rl, pos);
	ntfs_rl_copy(new_rl + pos + ins_count, runlist->rl + pos, runlist->elements - pos);
	if (count)
		IODeleteData(runlist->rl, ntfs_rl_element, count);
	runlist->rl = new_rl;
	runlist->elements = new_elements;
	runlist->alloc_count = new_count;
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
static unsigned ntfs_rl_can_merge(ntfs_rl_element *dst, ntfs_rl_element *src)
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
static errno_t ntfs_rl_replace(ntfs_runlist *dst_runlist, ntfs_rl_element *src,
		unsigned s_size, unsigned loc)
{
	ntfs_rl_element *d_rl;
	long delta;
	unsigned d_elements;
	unsigned tail;	/* Start of tail of @dst_runlist. */
	unsigned marker;/* End of the inserted runs. */
	unsigned left;	/* Left end of @src needs merging. */
	unsigned right;	/* Right end of @src needs merging. */

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
static errno_t ntfs_rl_insert(ntfs_runlist *dst_runlist, ntfs_rl_element *src,
		unsigned s_size, unsigned loc)
{
	ntfs_rl_element *d_rl;
	errno_t err;
	unsigned d_elements;
	unsigned left;	/* Left end of @src needs merging. */
	unsigned disc;	/* Discontinuity between @dst_runlist and @src. */
	unsigned marker;/* End of the inserted runs. */

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
static errno_t ntfs_rl_append(ntfs_runlist *dst_runlist, ntfs_rl_element *src,
		unsigned s_size, unsigned loc)
{
	ntfs_rl_element *d_rl;
	errno_t err;
	unsigned d_elements;
	unsigned right;	/* Right end of @src needs merging. */
	unsigned marker;/* End of the inserted runs. */

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
static errno_t ntfs_rl_split(ntfs_runlist *dst_runlist, ntfs_rl_element *src,
		unsigned s_size, unsigned loc)
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
	unsigned d_elements, s_elements, d_count, s_count;
	unsigned di, si;	/* Current index in @[ds]_rl. */
	unsigned s_start;	/* First index in @s_rl with lcn >= LCN_HOLE. */
	unsigned d_ins;		/* Index in @d_rl at which to insert @s_rl. */
	unsigned d_final;	/* Last index in @d_rl with lcn >= LCN_HOLE. */
	unsigned s_final;	/* Last index in @s_rl with lcn >= LCN_HOLE. */
	unsigned ss;		/* Number of relevant elements in @s_rl. */
	unsigned marker;
	errno_t err;
	BOOL start, finish;

	ntfs_debug("Destination runlist:");
	ntfs_debug_runlist_dump(dst_runlist);
	ntfs_debug("Source runlist:");
	ntfs_debug_runlist_dump(src_runlist);
	if (!src_runlist || !dst_runlist)
		panic("%s(): No %s runlist was supplied.\n", __FUNCTION__,
				!src_runlist ? "source" : "destination");
	s_rl = src_runlist->rl;
	s_elements = src_runlist->elements;
	s_count = src_runlist->alloc_count;
	/* If the source runlist is empty, nothing to do. */
	if (!s_elements) {
		if (s_count)
			IODeleteData(s_rl, ntfs_rl_element, s_count);
		goto done;
	}
	d_rl = dst_runlist->rl;
	d_elements = dst_runlist->elements;
	d_count = dst_runlist->alloc_count;
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
		if (d_count)
			IODeleteData(d_rl, ntfs_rl_element, d_count);
		dst_runlist->rl = src_runlist->rl;
		dst_runlist->elements = src_runlist->elements;
		dst_runlist->alloc_count = src_runlist->alloc_count;
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
	ntfs_debug("d_final %d, d_elements %d", d_final, d_elements);
	ntfs_debug("s_start = %d, s_final = %d, s_elements = %d", s_start,
			s_final, s_elements);
	ntfs_debug("start = %d, finish = %d", start ? 1 : 0, finish ? 1 : 0);
	ntfs_debug("ss = %d, d_ins = %d", ss, d_ins);
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
	IODeleteData(s_rl, ntfs_rl_element, s_count);
	d_rl = dst_runlist->rl;
	di = dst_runlist->elements - 1;
	/* Deal with the end of attribute marker if @s_rl ended after @d_rl. */
	if (marker && d_rl[di].vcn <= marker_vcn) {
		unsigned slots;

		ntfs_debug("Triggering marker code.");
		d_count = dst_runlist->alloc_count;
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
							"%d).\n",
							__FUNCTION__, slots);
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
	ntfs_debug_runlist_dump(dst_runlist);
	return 0;
}

/**
 * ntfs_mapping_pairs_decompress - convert mapping pairs array to runlist
 * @vol:	ntfs volume on which the attribute resides
 * @a:		attribute record whose mapping pairs array to decompress
 * @runlist:	runlist in which to insert @a's runlist
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
	unsigned rlcount;	/* Size of runlist buffer. */
	unsigned rlpos;		/* Current runlist position in units of
				   ntfs_rl_elements. */
	unsigned b;		/* Current byte offset in buf. */
	errno_t err = EIO;

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
	rlcount = NTFS_ALLOC_BLOCK / sizeof (ntfs_rl_element);
	/* Allocate NTFS_ALLOC_BLOCK bytes for the runlist. */
	rl = IONewData(ntfs_rl_element, rlcount);
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
		if (rlpos + 3 > rlcount) {
			ntfs_rl_element *rl2 = IONewData(ntfs_rl_element, rlcount + NTFS_ALLOC_BLOCK / sizeof (ntfs_rl_element));
			if (!rl2) {
				err = ENOMEM;
				goto err;
			}
			memcpy(rl2, rl, rlcount * sizeof (ntfs_rl_element));
			IODeleteData(rl, ntfs_rl_element, rlcount);
			rl = rl2;
			rlcount += NTFS_ALLOC_BLOCK / sizeof (ntfs_rl_element);
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
	 * The highest_vcn must be equal to the final vcn in the runlist - 1,
	 * or something has gone badly wrong.
	 */
	deltaxcn = sle64_to_cpu(a->highest_vcn);
	if (vcn - 1 != deltaxcn)
		goto io_err;
	/* Setup not mapped runlist element if this is the base extent. */
	if (!a->lowest_vcn) {
		VCN max_cluster;

		max_cluster = ((sle64_to_cpu(a->allocated_size) +
				vol->cluster_size_mask) >>
				vol->cluster_size_shift) - 1;
		/*
		 * If there is a difference between the highest_vcn and the
		 * highest cluster, the runlist is either corrupt or, more
		 * likely, there are more extents following this one.
		 */
		if (deltaxcn < max_cluster) {
			ntfs_debug("More extents to follow; deltaxcn = "
					"0x%llx, max_cluster = 0x%llx",
					(unsigned long long)deltaxcn,
					(unsigned long long)max_cluster);
			rl[rlpos].vcn = vcn;
			vcn += rl[rlpos].length = max_cluster - deltaxcn;
			rl[rlpos].lcn = LCN_RL_NOT_MAPPED;
			rlpos++;
		} else if (deltaxcn > max_cluster) {
			ntfs_error(vol->mp, "Corrupt attribute.  deltaxcn = "
					"0x%llx, max_cluster = 0x%llx",
					(unsigned long long)deltaxcn,
					(unsigned long long)max_cluster);
			goto io_err;
		}
		rl[rlpos].lcn = LCN_ENOENT;
	} else /* Not the base extent.  There may be more extents to follow. */
		rl[rlpos].lcn = LCN_RL_NOT_MAPPED;
	/* Setup terminating runlist element. */
	rl[rlpos].vcn = vcn;
	rl[rlpos].length = (s64)0;
	/* If no existing runlist was specified, we are done. */
	if (!runlist->elements) {
		if (runlist->alloc_count)
			IODeleteData(runlist->rl, ntfs_rl_element, runlist->alloc_count);
		runlist->rl = rl;
		runlist->elements = rlpos + 1;
		runlist->alloc_count = rlcount;
		ntfs_debug("Mapping pairs array successfully decompressed:");
		ntfs_debug_runlist_dump(runlist);
		return 0;
	} else {
		ntfs_runlist tmp_rl;

		/*
		 * An existing runlist was specified, now combine the new and
		 * old runlists checking for overlaps.
		 */
		tmp_rl.rl = rl;
		tmp_rl.elements = rlpos + 1;
		tmp_rl.alloc_count = rlcount;
		err = ntfs_rl_merge(runlist, &tmp_rl);
		if (!err)
			return err;
		ntfs_error(vol->mp, "Failed to merge runlists.");
	}
err:
	IODeleteData(rl, ntfs_rl_element, rlcount);
	return err;
io_err:
	ntfs_error(vol->mp, "Corrupt mapping pairs array in non-resident attribute.");
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
 * ntfs_rl_find_vcn_nolock - find a vcn in a runlist
 * @rl:		runlist to search
 * @vcn:	vcn to find
 *
 * Find the virtual cluster number @vcn in the runlist @rl and return the
 * address of the runlist element containing the @vcn on success.
 *
 * Return NULL if @rl is NULL or @vcn is in an unmapped part/out of bounds of
 * the runlist.
 *
 * Locking: The runlist must be locked on entry.
 */
ntfs_rl_element *ntfs_rl_find_vcn_nolock(ntfs_rl_element *rl, const VCN vcn)
{
	if (vcn < 0)
		panic("%s(): vcn < 0\n", __FUNCTION__);
	if (!rl || vcn < rl[0].vcn)
		return NULL;
	while (rl->length) {
		if (vcn < rl[1].vcn) {
			if (rl->lcn >= LCN_HOLE)
				return rl;
			return NULL;
		}
		rl++;
	}
	if (rl->lcn == LCN_ENOENT)
		return rl;
	return NULL;
}

/**
 * ntfs_get_nr_significant_bytes - get number of bytes needed to store a number
 * @n:		number for which to get the number of bytes for
 *
 * Return the number of bytes required to store @n unambiguously as
 * a signed number.
 *
 * This is used in the context of the mapping pairs array to determine how
 * many bytes will be needed in the array to store a given logical cluster
 * number (lcn) or a specific run length.
 *
 * Return the number of bytes written.  This function cannot fail.
 */
static inline int ntfs_get_nr_significant_bytes(const s64 n)
{
	s64 l = n;
	int i;
	s8 j;

	i = 0;
	do {
		l >>= 8;
		i++;
	} while (l != 0 && l != -1);
	j = (s8)(n >> (8 * (i - 1))) & 0xff;
	/* If the sign bit is wrong, we need an extra byte. */
	if ((n < 0 && j >= 0) || (n > 0 && j < 0))
		i++;
	return i;
}

/**
 * ntfs_get_size_for_mapping_pairs - get bytes needed for mapping pairs array
 * @vol:	ntfs volume (needed for the ntfs version)
 * @rl:		locked runlist to determine the size of the mapping pairs of
 * @first_vcn:	first vcn which to include in the mapping pairs array
 * @last_vcn:	last vcn which to include in the mapping pairs array
 * @mp_size:	destination pointer in which to return the size
 *
 * Walk the locked runlist @rl and calculate the size in bytes of the mapping
 * pairs array corresponding to the runlist @rl, starting at vcn @first_vcn and
 * finishing with vcn @last_vcn and return the size in *@mp_size.
 *
 * A @last_vcn of -1 means end of runlist and in that case the size of the
 * mapping pairs array corresponding to the runlist starting at vcn @first_vcn
 * and finishing at the end of the runlist is determined.
 *
 * This for example allows us to allocate a buffer of the right size when
 * building the mapping pairs array.
 *
 * If @rl is NULL, just return *@mp_size = 1 (for the single terminator byte).
 *
 * Return 0 on success and errno on error.  On error *@mp_size is undefined.
 * The following error codes are defined:
 *	EINVAL	- Run list contains unmapped elements.  Make sure to only pass
 *		  fully mapped runlists to this function.
 *	EIO	- The runlist is corrupt.
 *
 * Locking: @rl must be locked on entry (either for reading or writing), it
 *	    remains locked throughout, and is left locked upon return.
 */
errno_t ntfs_get_size_for_mapping_pairs(const ntfs_volume *vol,
		const ntfs_rl_element *rl, const VCN first_vcn,
		const VCN last_vcn, unsigned *mp_size)
{
	LCN prev_lcn;
	int rls;
	BOOL the_end = FALSE;

	if (first_vcn < 0)
		panic("%s(): first_vcn < 0\n", __FUNCTION__);
	if (last_vcn < -1)
		panic("%s(): last_vcn < -1\n", __FUNCTION__);
	if (last_vcn >= 0 && first_vcn > last_vcn)
		panic("%s(): last_vcn >= 0 && first_vcn > last_vcn\n",
				__FUNCTION__);
	if (!rl) {
		if (first_vcn)
			panic("%s(): first_vcn\n", __FUNCTION__);
		if (last_vcn > 0)
			panic("%s(): last_vcn > 0\n", __FUNCTION__);
		*mp_size = 1;
		return 0;
	}
	/* Skip to runlist element containing @first_vcn. */
	while (rl->length && first_vcn >= rl[1].vcn)
		rl++;
	if ((!rl->length && first_vcn > rl->vcn) || first_vcn < rl->vcn)
		return EINVAL;
	prev_lcn = 0;
	/* Always need the termining zero byte. */
	rls = 1;
	/* Do the first partial run if present. */
	if (first_vcn > rl->vcn) {
		s64 delta, length = rl->length;

		/* We know rl->length != 0 already. */
		if (length < 0 || rl->lcn < LCN_HOLE)
			goto err;
		/*
		 * If @last_vcn is given and finishes inside this run, cap the
		 * run length.
		 */
		if (last_vcn >= 0 && rl[1].vcn > last_vcn) {
			s64 s1 = last_vcn + 1;
			if (rl[1].vcn > s1)
				length = s1 - rl->vcn;
			the_end = TRUE;
		}
		delta = first_vcn - rl->vcn;
		/* Header byte + length. */
		rls += 1 + ntfs_get_nr_significant_bytes(length - delta);
		/*
		 * If the logical cluster number (lcn) denotes a hole and we
		 * are on NTFS 3.0+, we don't store it at all, i.e. we need
		 * zero space.  On earlier NTFS versions we just store the lcn.
		 * Note: this assumes that on NTFS 1.2-, holes are stored with
		 * an lcn of -1 and not a delta_lcn of -1 (unless both are -1).
		 */
		if (rl->lcn >= 0 || vol->major_ver < 3) {
			prev_lcn = rl->lcn;
			if (rl->lcn >= 0)
				prev_lcn += delta;
			/* Change in lcn. */
			rls += ntfs_get_nr_significant_bytes(prev_lcn);
		}
		/* Go to next runlist element. */
		rl++;
	}
	/* Do the full runs. */
	for (; rl->length && !the_end; rl++) {
		s64 length = rl->length;

		if (length < 0 || rl->lcn < LCN_HOLE)
			goto err;
		/*
		 * If @last_vcn is given and finishes inside this run, cap the
		 * run length.
		 */
		if (last_vcn >= 0 && rl[1].vcn > last_vcn) {
			s64 s1 = last_vcn + 1;
			if (rl[1].vcn > s1)
				length = s1 - rl->vcn;
			the_end = TRUE;
		}
		/* Header byte + length. */
		rls += 1 + ntfs_get_nr_significant_bytes(length);
		/*
		 * If the logical cluster number (lcn) denotes a hole and we
		 * are on NTFS 3.0+, we don't store it at all, i.e. we need
		 * zero space.  On earlier NTFS versions we just store the lcn.
		 * Note: this assumes that on NTFS 1.2-, holes are stored with
		 * an lcn of -1 and not a delta_lcn of -1 (unless both are -1).
		 */
		if (rl->lcn >= 0 || vol->major_ver < 3) {
			/* Change in lcn. */
			rls += ntfs_get_nr_significant_bytes(rl->lcn -
					prev_lcn);
			prev_lcn = rl->lcn;
		}
	}
	*mp_size = rls;
	return 0;
err:
	if (rl->lcn == LCN_RL_NOT_MAPPED)
		rls = EINVAL;
	else
		rls = EIO;
	return rls;
}

/**
 * ntfs_write_significant_bytes - write the significant bytes of a number
 * @dst:	destination buffer to write to
 * @dst_max:	pointer to last byte of destination buffer for bounds checking
 * @n:		number whose significant bytes to write
 *
 * Store in @dst, the minimum bytes of the number @n which are required to
 * identify @n unambiguously as a signed number, taking care not to exceed
 * @dest_max, the maximum position within @dst to which we are allowed to
 * write.
 *
 * This is used when building the mapping pairs array of a runlist to compress
 * a given logical cluster number (lcn) or a specific run length to the minumum
 * size possible.
 *
 * Return the number of bytes written on success.  On error, i.e. the
 * destination buffer @dst is too small, return -ENOSPC.
 */
static inline int ntfs_write_significant_bytes(s8 *dst, const s8 *dst_max,
		const s64 n)
{
	s64 l = n;
	int i;
	s8 j;

	i = 0;
	do {
		if (dst > dst_max)
			goto err;
		*dst++ = (s8)l & 0xff;
		l >>= 8;
		i++;
	} while (l != 0 && l != -1);
	j = (s8)(n >> (8 * (i - 1))) & 0xff;
	/* If the sign bit is wrong, we need an extra byte. */
	if (n < 0 && j >= 0) {
		if (dst > dst_max)
			goto err;
		i++;
		*dst = (s8)-1;
	} else if (n > 0 && j < 0) {
		if (dst > dst_max)
			goto err;
		i++;
		*dst = (s8)0;
	}
	return i;
err:
	return -ENOSPC;
}

/**
 * ntfs_mapping_pairs_build - build the mapping pairs array from a runlist
 * @vol:	ntfs volume (needed for the ntfs version)
 * @dst:	destination buffer to which to write the mapping pairs array
 * @dst_len:	size of destination buffer @dst in bytes
 * @rl:		locked runlist for which to build the mapping pairs array
 * @first_vcn:	first vcn which to include in the mapping pairs array
 * @last_vcn:	last vcn which to include in the mapping pairs array
 * @stop_vcn:	first vcn outside destination buffer on success or ENOSPC
 *
 * Create the mapping pairs array from the locked runlist @rl, starting at vcn
 * @first_vcn and finishing with vcn @last_vcn and save the array in @dst.
 * @dst_len is the size of @dst in bytes and it should be at least equal to the
 * value obtained by calling ntfs_get_size_for_mapping_pairs().
 *
 * A @last_vcn of -1 means end of runlist and in that case the mapping pairs
 * array corresponding to the runlist starting at vcn @first_vcn and finishing
 * at the end of the runlist is created.
 *
 * If @rl is NULL, just write a single terminator byte to @dst.
 *
 * On success or ENOSPC error, if @stop_vcn is not NULL, *@stop_vcn is set to
 * the first vcn outside the destination buffer.  Note that on error, @dst has
 * been filled with all the mapping pairs that will fit, thus it can be treated
 * as partial success, in that a new attribute extent needs to be created or
 * the next extent has to be used and the mapping pairs build has to be
 * continued with @first_vcn set to *@stop_vcn.
 *
 * Return 0 on success and errno on error.  The following error codes are
 * defined:
 *	EINVAL	- Run list contains unmapped elements.  Make sure to only pass
 *		  fully mapped runlists to this function.
 *	EIO	- The runlist is corrupt.
 *	ENOSPC	- The destination buffer is too small.
 *
 * Locking: @rl must be locked on entry (either for reading or writing), it
 *	    remains locked throughout, and is left locked upon return.
 */
errno_t ntfs_mapping_pairs_build(const ntfs_volume *vol, s8 *dst,
		const unsigned dst_len, const ntfs_rl_element *rl,
		const VCN first_vcn, const VCN last_vcn, VCN *const stop_vcn)
{
	LCN prev_lcn;
	s8 *dst_max, *dst_next;
	errno_t err = ENOSPC;
	BOOL the_end = FALSE;
	s8 len_len, lcn_len;

	if (first_vcn < 0)
		panic("%s(): first_vcn < 0\n", __FUNCTION__);
	if (last_vcn < -1)
		panic("%s(): last_vcn < -1\n", __FUNCTION__);
	if (last_vcn >= 0 && first_vcn > last_vcn)
		panic("%s(): last_vcn >= 0 && first_vcn > last_vcn\n",
				__FUNCTION__);
	if (dst_len < 1)
		panic("%s(): dst_len < 1\n", __FUNCTION__);
	if (!rl) {
		if (first_vcn)
			panic("%s(): first_vcn\n", __FUNCTION__);
		if (last_vcn > 0)
			panic("%s(): last_vcn > 0\n", __FUNCTION__);
		if (stop_vcn)
			*stop_vcn = 0;
		/* Terminator byte. */
		*dst = 0;
		return 0;
	}
	/* Skip to runlist element containing @first_vcn. */
	while (rl->length && first_vcn >= rl[1].vcn)
		rl++;
	if ((!rl->length && first_vcn > rl->vcn) || first_vcn < rl->vcn)
		return EINVAL;
	/*
	 * @dst_max is used for bounds checking in
	 * ntfs_write_significant_bytes().
	 */
	dst_max = dst + dst_len - 1;
	prev_lcn = 0;
	/* Do the first partial run if present. */
	if (first_vcn > rl->vcn) {
		s64 delta, length = rl->length;

		/* We know rl->length != 0 already. */
		if (length < 0 || rl->lcn < LCN_HOLE)
			goto err;
		/*
		 * If @last_vcn is given and finishes inside this run, cap the
		 * run length.
		 */
		if (last_vcn >= 0 && rl[1].vcn > last_vcn) {
			s64 s1 = last_vcn + 1;
			if (rl[1].vcn > s1)
				length = s1 - rl->vcn;
			the_end = TRUE;
		}
		delta = first_vcn - rl->vcn;
		/* Write length. */
		len_len = ntfs_write_significant_bytes(dst + 1, dst_max,
				length - delta);
		if (len_len < 0)
			goto size_err;
		/*
		 * If the logical cluster number (lcn) denotes a hole and we
		 * are on NTFS 3.0+, we don't store it at all, i.e. we need
		 * zero space.  On earlier NTFS versions we just write the lcn
		 * change.  FIXME: Do we need to write the lcn change or just
		 * the lcn in that case?  Not sure as I have never seen this
		 * case on NT4. - We assume that we just need to write the lcn
		 * change until someone tells us otherwise... (AIA)
		 */
		if (rl->lcn >= 0 || vol->major_ver < 3) {
			prev_lcn = rl->lcn;
			if (rl->lcn >= 0)
				prev_lcn += delta;
			/* Write change in lcn. */
			lcn_len = ntfs_write_significant_bytes(dst + 1 +
					len_len, dst_max, prev_lcn);
			if (lcn_len < 0)
				goto size_err;
		} else
			lcn_len = 0;
		dst_next = dst + len_len + lcn_len + 1;
		if (dst_next > dst_max)
			goto size_err;
		/* Update header byte. */
		*dst = lcn_len << 4 | len_len;
		/* Position at next mapping pairs array element. */
		dst = dst_next;
		/* Go to next runlist element. */
		rl++;
	}
	/* Do the full runs. */
	for (; rl->length && !the_end; rl++) {
		s64 length = rl->length;

		if (length < 0 || rl->lcn < LCN_HOLE)
			goto err;
		/*
		 * If @last_vcn is given and finishes inside this run, cap the
		 * run length.
		 */
		if (last_vcn >= 0 && rl[1].vcn > last_vcn) {
			s64 s1 = last_vcn + 1;
			if (rl[1].vcn > s1)
				length = s1 - rl->vcn;
			the_end = TRUE;
		}
		/* Write length. */
		len_len = ntfs_write_significant_bytes(dst + 1, dst_max,
				length);
		if (len_len < 0)
			goto size_err;
		/*
		 * If the logical cluster number (lcn) denotes a hole and we
		 * are on NTFS 3.0+, we don't store it at all, i.e. we need
		 * zero space.  On earlier NTFS versions we just write the lcn
		 * change.  FIXME: Do we need to write the lcn change or just
		 * the lcn in that case?  Not sure as I have never seen this
		 * case on NT4. - We assume that we just need to write the lcn
		 * change until someone tells us otherwise... (AIA)
		 */
		if (rl->lcn >= 0 || vol->major_ver < 3) {
			/* Write change in lcn. */
			lcn_len = ntfs_write_significant_bytes(dst + 1 +
					len_len, dst_max, rl->lcn - prev_lcn);
			if (lcn_len < 0)
				goto size_err;
			prev_lcn = rl->lcn;
		} else
			lcn_len = 0;
		dst_next = dst + len_len + lcn_len + 1;
		if (dst_next > dst_max)
			goto size_err;
		/* Update header byte. */
		*dst = lcn_len << 4 | len_len;
		/* Position at next mapping pairs array element. */
		dst = dst_next;
	}
	/* Success. */
	err = 0;
size_err:
	/* Set stop vcn. */
	if (stop_vcn)
		*stop_vcn = rl->vcn;
	/* Add terminator byte. */
	*dst = 0;
	return err;
err:
	if (rl->lcn == LCN_RL_NOT_MAPPED)
		err = EINVAL;
	else
		err = EIO;
	return err;
}

/**
 * ntfs_rl_shrink - remove runlist elements from the end of an existing runlist
 * @runlist:		runlist to shrink
 * @new_elements:	new number of elements for the runlist
 *
 * Shrink the number of elements in the array of runlist elements of the
 * runlist @runlist making the new number of elements to be @new_elements.
 *
 * Reallocate the array buffer if that would save memory.  If the reallocation
 * fails reduce the number of elements anyway as the only side effect is that
 * we waste a bit of memory for a while.
 *
 * This function cannot fail.
 *
 * Locking: - The caller must have locked the runlist for writing.
 *	    - The runlist is modified.
 */
static void ntfs_rl_shrink(ntfs_runlist *runlist, unsigned new_elements)
{
	if (new_elements > runlist->elements)
		panic("%s(): new_elements > runlist->elements\n",
				__FUNCTION__);
	unsigned count = runlist->alloc_count;
	if (!count || !runlist->rl)
		panic("%s(): !count || !runlist->rl\n", __FUNCTION__);
	unsigned new_count = new_elements + NTFS_ALLOC_BLOCK / sizeof (ntfs_rl_element);
	if (new_count < count) {
		ntfs_rl_element *new_rl = IONewData(ntfs_rl_element, new_count);
		if (new_rl) {
			ntfs_rl_copy(new_rl, runlist->rl, new_elements);
			IODeleteData(runlist->rl, ntfs_rl_element, count);
			runlist->rl = new_rl;
			runlist->alloc_count = new_count;
		} else
			ntfs_debug("Failed to shrink runlist buffer.  This "
					"just wastes a bit of memory "
					"temporarily so we ignore it.");
	} else if (new_count != count)
		panic("%s(): new_count != count\n", __FUNCTION__);
	runlist->elements = new_elements;
}

/**
 * ntfs_rl_truncate_nolock - truncate a runlist starting at a specified vcn
 * @vol:	ntfs volume (needed for error output)
 * @runlist:	runlist to truncate
 * @new_length:	the new length of the runlist in VCNs
 *
 * Truncate the runlist described by @runlist as well as the memory buffer
 * holding the runlist elements to a length of @new_length VCNs.
 *
 * If @new_length lies within the runlist, the runlist elements with VCNs of
 * @new_length and above are discarded.  As a special case if @new_length is
 * zero, the runlist is discarded and set to NULL.
 *
 * If @new_length lies beyond the runlist, a sparse runlist element is added to
 * the end of the runlist @runlist or if the last runlist element is a sparse
 * one already, this is extended.
 *
 * Note, no checking is done for unmapped runlist elements.  It is assumed that
 * the caller has mapped any elements that need to be mapped already.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: The caller must hold @runlist->lock for writing.
 */
errno_t ntfs_rl_truncate_nolock(const ntfs_volume *vol,
		ntfs_runlist *const runlist, const s64 new_length)
{
	ntfs_rl_element *rl;
	unsigned last_element, element;

	ntfs_debug("Entering for new_length 0x%llx.",
			(unsigned long long)new_length);
	if (!runlist)
		panic("%s(): !runlist\n", __FUNCTION__);
	rl = runlist->rl;
	if (!rl && runlist->alloc_count)
		panic("%s(): !rl && runlist->alloc_count\n", __FUNCTION__);
	if (rl && !runlist->alloc_count)
		panic("%s(): rl && !runlist->alloc_count\n", __FUNCTION__);
	if (new_length < 0)
		panic("%s(): new_length < 0\n", __FUNCTION__);
	ntfs_debug_runlist_dump(runlist);
	if (!new_length) {
		ntfs_debug("Freeing runlist.");
		if (rl) {
			IODeleteData(rl, ntfs_rl_element, runlist->alloc_count);
			runlist->rl = NULL;
			runlist->alloc_count = runlist->elements = 0;
		}
		return 0;
	}
	if (!runlist->elements) {
		errno_t err;

		/*
		 * Create a runlist consisting of a sparse runlist element of
		 * length @new_length followed by a terminator runlist element.
		 */
		err = ntfs_rl_inc(runlist, 2);
		if (err) {
			ntfs_error(vol->mp, "Not enough memory to allocate "
					"runlist element buffer.");
			return err;
		}
		rl = runlist->rl;
		rl[1].length = rl->vcn = 0;
		rl->lcn = LCN_HOLE;
		rl[1].vcn = rl->length = new_length;
		rl[1].lcn = LCN_ENOENT;
		goto done;
	}
	if (new_length < rl->vcn)
		panic("%s(): new_length < rl->vcn\n", __FUNCTION__);
	/* Find @new_length in the runlist. */
	last_element = runlist->elements - 1;
	for (element = 0; element < last_element &&
			new_length >= rl[element + 1].vcn; element++)
		;
	/*
	 * If not at the end of the runlist we need to shrink it.  Otherwise we
	 * need to expand it.
	 */
	if (element < last_element) {
		ntfs_debug("Shrinking runlist.");
		/* Truncate the run. */
		rl[element].length = new_length - rl[element].vcn;
		/*
		 * If a run was partially truncated, make the following runlist
		 * element a terminator.
		 */
		if (rl[element].length) {
			element++;
			rl[element].vcn = new_length;
			rl[element].length = 0;
		}
		rl[element].lcn = LCN_ENOENT;
		/* Shrink the number of runlist elements. */
		ntfs_rl_shrink(runlist, element + 1);
	} else /* if (element == last_element) */ {
		if (rl[element].length)
			panic("%s(): rl[element].length\n", __FUNCTION__);
		/*
		 * If the runlist length is already @new_length, there is
		 * nothing to do except to set the terminator to be LCN_ENOENT.
		 * Otherwise need to expand the runlist.
		 */
		if (new_length > rl[element].vcn) {
			unsigned prev_element;

			ntfs_debug("Expanding runlist.");
			/*
			 * If there is a previous runlist element and it is a
			 * sparse one, extend it.  Otherwise need to add a new,
			 * sparse runlist element.
			 */
			if (element > 0 && (prev_element = element - 1,
					rl[prev_element].lcn == LCN_HOLE))
				rl[prev_element].length = new_length -
						rl[prev_element].vcn;
			else {
				errno_t err;

				/* Add one runlist element to the runlist. */
				err = ntfs_rl_inc(runlist, 1);
				if (err) {
					ntfs_error(vol->mp, "Not enough "
							"memory to expand "
							"runlist element "
							"buffer.");
					return err;
				}
				rl = runlist->rl;
				/*
				 * Switch the old terminator runlist element to
				 * a sparse runlist element and adjust its
				 * length.
				 */
				rl[element].lcn = LCN_HOLE;
				rl[element].length = new_length -
						rl[element].vcn;
				/* Add a new terminator runlist element. */
				element++;
				rl[element].length = 0;
			}
			rl[element].vcn = new_length;
		}
		rl[element].lcn = LCN_ENOENT;
	}
done:
	ntfs_debug_runlist_dump(runlist);
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_rl_punch_nolock - punch a hole into a runlist
 * @vol:	ntfs volume (needed for error output)
 * @runlist:	runlist to punch a hole into
 * @start_vcn:	vcn in the runlist @runlist at which to start the hole
 * @len:	size of the hole to be created in units of clusters
 *
 * Punch a hole into the runlist @runlist starting at VCN @start_vcn and of
 * size @len clusters.
 *
 * Return 0 on success and errno on error, in which case @runlist has not been
 * modified.
 *
 * If @start_vcn and/or @start_vcn + @len are outside the runlist return error
 * ENOENT.
 *
 * If the runlist contains unmapped or error elements between @start_vcn and
 * @start_vcn + @len return error EINVAL.
 *
 * Locking: The caller must hold @runlist->lock for writing.
 */
errno_t ntfs_rl_punch_nolock(const ntfs_volume *vol, ntfs_runlist *runlist,
		const VCN start_vcn, const s64 len)
{
	const VCN end_vcn = start_vcn + len;
	s64 delta;
	ntfs_rl_element *rl, *rl_end, *trl;
	unsigned hole_size;
	errno_t err;
	BOOL lcn_fixup = FALSE;

	ntfs_debug("Entering for start_vcn 0x%llx, len 0x%llx.",
			(unsigned long long)start_vcn, (unsigned long long)len);
	if (!runlist || start_vcn < 0 || len < 0 || end_vcn < 0)
		panic("%s(): !runlist || start_vcn < 0 || len < 0 || "
				"end_vcn < 0\n", __FUNCTION__);
	if (!runlist->elements) {
		if (!start_vcn && !len)
			return 0;
		return ENOENT;
	}
	rl = runlist->rl;
	if (!runlist->alloc_count || !rl)
		panic("%s(): !runlist->alloc_count || !rl\n", __FUNCTION__);
	/* Find @start_vcn in the runlist. */
	while (rl->length && start_vcn >= rl[1].vcn)
		rl++;
	rl_end = rl;
	/* Find @end_vcn in the runlist. */
	hole_size = 0;
	while (rl_end->length && end_vcn >= rl_end[1].vcn) {
		/* Verify there are no unmapped or error elements. */
		if (rl_end->lcn < LCN_HOLE)
			return EINVAL;
		rl_end++;
		hole_size++;
	}
	/* Check the last element. */
	if (rl_end->length && rl_end->lcn < LCN_HOLE)
		return EINVAL;
	/* This covers @start_vcn being out of bounds, too. */
	if (!rl_end->length && end_vcn > rl_end->vcn)
		return ENOENT;
	if (!len)
		return 0;
	if (!rl->length)
		return ENOENT;
	/* If @start is in a hole simply extend the hole. */
	if (rl->lcn == LCN_HOLE) {
		/*
		 * If both @start_vcn and @end_vcn are in the same sparse run,
		 * we are done.
		 */
		if (end_vcn <= rl[1].vcn) {
			ntfs_debug("Done (requested hole is already sparse).");
			return 0;
		}
extend_hole:
		/* Extend the hole. */
		rl->length = end_vcn - rl->vcn;
		/* If @end_vcn is in a hole, merge it with the current one. */
		if (rl_end->lcn == LCN_HOLE) {
			rl_end++;
			hole_size++;
			rl->length = rl_end->vcn - rl->vcn;
		}
		/* We have done the hole.  Now deal with the remaining tail. */
		rl++;
		hole_size--;
		/* Cut out all runlist elements up to @end. */
		if (rl < rl_end)
			memmove(rl, rl_end, (u8*)&runlist->rl[
					runlist->elements] - (u8*)rl_end);
		/* Adjust the beginning of the tail if necessary. */
		if (end_vcn > rl->vcn) {
			delta = end_vcn - rl->vcn;
			rl->vcn = end_vcn;
			rl->length -= delta;
			/* Only adjust the lcn if it is real. */
			if (rl->lcn >= 0)
				rl->lcn += delta;
		}
shrink_allocation:
		/* Reallocate memory if the allocation changed. */
		if (rl < rl_end)
			ntfs_rl_shrink(runlist, runlist->elements - hole_size);
		ntfs_debug("Done (extend hole).");
		return 0;
	}
	/*
	 * If @start_vcn is at the beginning of a run things are easier as
	 * there is no need to split the first run.
	 */
	if (start_vcn == rl->vcn) {
		/*
		 * @start_vcn is at the beginning of a run.
		 *
		 * If the previous run is sparse, extend its hole.
		 *
		 * If @end_vcn is not in the same run, switch the run to be
		 * sparse and extend the newly created hole.
		 *
		 * Thus both of these cases reduce the problem to the above
		 * case of "@start_vcn is in a hole".
		 */
		if (rl > runlist->rl && (rl - 1)->lcn == LCN_HOLE) {
			rl--;
			hole_size++;
			goto extend_hole;
		}
		if (end_vcn >= rl[1].vcn) {
			rl->lcn = LCN_HOLE;
			goto extend_hole;
		}
		/*
		 * The final case is when @end_vcn is in the same run as
		 * @start_vcn.  For this need to split the run into two.  One
		 * run for the sparse region between the beginning of the old
		 * run, i.e. @start_vcn, and @end_vcn and one for the remaining
		 * non-sparse region, i.e. between @end_vcn and the end of the
		 * old run.
		 */
		trl = runlist->rl;
		err = ntfs_rl_inc(runlist, 1);
		if (err)
			goto err;
		/*
		 * If the runlist buffer was reallocated need to update our
		 * pointers.
		 */
		if (runlist->rl != trl)
			rl = (ntfs_rl_element*)((u8*)runlist->rl +
					((u8*)rl - (u8*)trl));
split_end:
		/* Shift all the runs up by one. */
		memmove((u8*)rl + sizeof(*rl), rl, (u8*)&runlist->rl[
				runlist->elements - 1] - (u8*)rl);
		/* Finally, setup the two split runs. */
		rl->lcn = LCN_HOLE;
		rl->length = len;
		rl++;
		rl->vcn += len;
		/* Only adjust the lcn if it is real. */
		if (rl->lcn >= 0 || lcn_fixup)
			rl->lcn += len;
		rl->length -= len;
		ntfs_debug("Done (split one).");
		return 0;
	}
	/*
	 * @start_vcn is neither in a hole nor at the beginning of a run.
	 *
	 * If @end_vcn is in a hole, things are easier as simply truncating the
	 * run @start_vcn is in to end at @start_vcn - 1, deleting all runs
	 * after that up to @end_vcn, and finally extending the beginning of
	 * the run @end_vcn is in to be @start_vcn is all that is needed.
	 */
	if (rl_end->lcn == LCN_HOLE) {
		/* Truncate the run containing @start. */
		rl->length = start_vcn - rl->vcn;
		rl++;
		hole_size--;
		/* Cut out all runlist elements up to @end. */
		if (rl < rl_end)
			memmove(rl, rl_end, (u8*)&runlist->rl[
					runlist->elements] - (u8*)rl_end);
		/* Extend the beginning of the run @end is in to be @start. */
		rl->vcn = start_vcn;
		rl->length = rl[1].vcn - start_vcn;
		goto shrink_allocation;
	}
	/* 
	 * If @end_vcn is not in a hole there are still two cases to
	 * distinguish.  Either @end_vcn is or is not in the same run as
	 * @start_vcn.
	 *
	 * The second case is easier as it can be reduced to an already solved
	 * problem by truncating the run @start_vcn is in to end at @start_vcn
	 * - 1.  Then, if @end_vcn is in the next run need to split the run
	 * into a sparse run followed by a non-sparse run which we already
	 * covered above and if @end_vcn is not in the next run switching it to
	 * be sparse reduces the problem to the case of "@start_vcn is in a
	 * hole" which we also covered above.
	 */
	if (end_vcn >= rl[1].vcn) {
		/*
		 * If @end_vcn is not in the next run, reduce the problem to
		 * the case of "@start_vcn is in a hole".
		 */
		if (rl[1].length && end_vcn >= rl[2].vcn) {
			/* Truncate the run containing @start_vcn. */
			rl->length = start_vcn - rl->vcn;
			rl++;
			hole_size--;
			rl->vcn = start_vcn;
			rl->lcn = LCN_HOLE;
			goto extend_hole;
		}
		trl = runlist->rl;
		err = ntfs_rl_inc(runlist, 1);
		if (err)
			goto err;
		/*
		 * If the runlist buffer was reallocated need to update our
		 * pointers.
		 */
		if (runlist->rl != trl)
			rl = (ntfs_rl_element*)((u8*)runlist->rl +
					((u8*)rl - (u8*)trl));
		/* Truncate the run containing @start_vcn. */
		rl->length = start_vcn - rl->vcn;
		rl++;
		/*
		 * @end_vcn is in the next run, reduce the problem to the case
		 * where "@start_vcn is at the beginning of a run and @end_vcn
		 * is in the same run as @start_vcn".
		 */
		delta = rl->vcn - start_vcn;
		rl->vcn = start_vcn;
		if (rl->lcn >= 0) {
			rl->lcn -= delta;
			/* Need this in case the lcn just became negative. */
			lcn_fixup = TRUE;
		}
		rl->length += delta;
		goto split_end;
	}
	/*
	 * The first case from above, i.e. @end_vcn is in the same non-sparse
	 * run as @start_vcn.  We need to split the run into three.  One run
	 * for the non-sparse region between the beginning of the old run and
	 * @start_vcn, one for the sparse region between @start_vcn and
	 * @end_vcn, and one for the remaining non-sparse region, i.e. between
	 * @end_vcn and the end of the old run.
	 */
	trl = runlist->rl;
	err = ntfs_rl_inc(runlist, 2);
	if (err)
		goto err;
	/* If the runlist buffer was reallocated need to update our pointers. */
	if (runlist->rl != trl)
		rl = (ntfs_rl_element*)((u8*)runlist->rl +
				((u8*)rl - (u8*)trl));
	/* Shift all the runs up by two. */
	memmove((u8*)rl + 2 * sizeof(*rl), rl,
			(u8*)&runlist->rl[runlist->elements - 2] - (u8*)rl);
	/* Finally, setup the three split runs. */
	rl->length = start_vcn - rl->vcn;
	rl++;
	rl->vcn = start_vcn;
	rl->lcn = LCN_HOLE;
	rl->length = len;
	rl++;
	delta = end_vcn - rl->vcn;
	rl->vcn = end_vcn;
	rl->lcn += delta;
	rl->length -= delta;
	ntfs_debug("Done (split both).");
	return 0;
err:
	ntfs_error(vol->mp, "Failed to extend runlist buffer (error %d).", err);
	return err;
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
 * Locking: The caller must have locked the runlist or otherwise ensure that no
 *	    one is modifying the runlist under whilst ntfs_rl_read() is
 *	    executing.
 */
errno_t ntfs_rl_read(ntfs_volume *vol, ntfs_runlist *runlist, u8 *dst,
		const s64 size, const s64 initialized_size)
{
	u8 *dst_end = dst + initialized_size;
	ntfs_rl_element *rl;
	buf_t buf;
	errno_t err;
	unsigned block_size = vol->sector_size;
	const u8 cluster_to_block_shift = vol->cluster_size_shift -
			vol->sector_size_shift;

	ntfs_debug("Entering.");
	if (!vol || !runlist || !dst || size <= 0 || initialized_size < 0 ||
			initialized_size > size) {
        ntfs_error((vol ? vol->mp : NULL), "Received invalid arguments.");
		return EINVAL;
	}
    vnode_t dev_vn = vol->dev_vn;
	if (!initialized_size) {
		bzero(dst, size);
		ntfs_debug("Done (!initialized_size).");
		return 0;
	}
	rl = runlist->rl;
	if (!rl) {
		ntfs_error(vol->mp, "Cannot read attribute list since runlist "
				"is missing.");
		return EIO;
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
			return EIO;
		}
		/* Read the run from device in chunks of block_size bytes. */
		block = rl->lcn << cluster_to_block_shift;
		max_block = block + (rl->length << cluster_to_block_shift);
		ntfs_debug("max_block 0x%llx.", (unsigned long long)max_block);
		do {
			u8 *src;

			ntfs_debug("Reading block 0x%llx.",
					(unsigned long long)block);
			err = buf_meta_bread(dev_vn, block, block_size, NOCRED,
					&buf);
			if (err) {
				ntfs_error(vol->mp, "buf_meta_bread() failed "
						"(error %d).  Cannot read "
						"data.", (int)err);
				goto err;
			}
			err = buf_map(buf, (caddr_t*)&src);
			if (err) {
				ntfs_error(vol->mp, "buf_map() failed (error "
						"%d).  Cannot read data.",
						(int)err);
				goto err;
			}
			/* Copy the data into the buffer. */
			if (dst + block_size > dst_end)
				block_size = dst_end - dst;
			memcpy(dst, src, block_size);
			err = buf_unmap(buf);
			if (err)
				ntfs_error(vol->mp, "buf_unmap() failed "
						"(error %d).", (int)err);
			buf_brelse(buf);
			dst += block_size;
			if (dst >= dst_end)
				goto done;
		} while (++block < max_block);
		rl++;
	}
done:
	/* If the runlist was too short, zero out the unread part. */
	if (dst < dst_end)
		bzero(dst, dst_end - dst);
	/* Zero the uninitialized region if present. */
	if (initialized_size < size)
		bzero(dst_end, size - initialized_size);
	ntfs_debug("Done.");
	return 0;
err:
	buf_brelse(buf);
	return err;
}

/**
 * ntfs_rl_write - write data to disk as described by an runlist
 * @vol:	ntfs volume to which to write
 * @src:	source buffer
 * @size:	size of source buffer @src in bytes
 * @runlist:	runlist describing vcn to lcn mapping of data
 * @ofs:	offset into buffer/runlist at which to start writing
 * @cnt:	number of bytes to write starting at @ofs or zero
 *
 * Walk the runlist @runlist and write the data contained in @src starting at
 * offset @ofs into @src to the corresponding clusters as specified by the
 * runlist starting at offset @ofs into the runlist.  If @cnt is not zero it is
 * the number of bytes to write starting at @ofs.  If @cnt is zero we write
 * until the end of the source buffer @src is reached.
 *
 * Note @ofs will be aligned to a device block boundary and @cnt will be
 * adjusted accordingly and it will be rounded up to the next device block
 * boundary and anything outside @size will be written as zeroes.
 *
 * Return 0 on success and errno on error.
 *
 * Note: Sparse runlists are not supported by this function.
 *
 * Locking: The caller must have locked the runlist or otherwise ensure that no
 *	    one is modifying the runlist under whilst ntfs_rl_write() is
 *	    executing.
 */
errno_t ntfs_rl_write(ntfs_volume *vol, u8 *src, const s64 size,
		ntfs_runlist *runlist, s64 ofs, const s64 cnt)
{
	VCN vcn;
	u8 *src_end, *src_stop;
	ntfs_rl_element *rl;
	errno_t err;
	unsigned block_size, block_shift, cluster_shift, shift, delta, vcn_ofs;

	ntfs_debug("Entering for size 0x%llx, ofs 0x%llx.",
			(unsigned long long)size, (unsigned long long)ofs);
	if (!vol || !src || size <= 0 || !runlist || !runlist->elements ||
			ofs < 0 || cnt < 0 || ofs + cnt > size) {
        ntfs_error((vol ? vol->mp : NULL), "Received invalid arguments.");
		return EINVAL;
	}

    vnode_t dev_vn = vol->dev_vn;
	src_stop = src_end = src + size;
	if (cnt) {
		src_stop = src + ofs + cnt;
		if (src_stop > src_end)
			panic("%s(): src_stop > src_end\n", __FUNCTION__);
	}
	block_size = vol->sector_size;
	block_shift = vol->sector_size_shift;
	cluster_shift = vol->cluster_size_shift;
	shift = cluster_shift - block_shift;
	/*
	 * Align the start offset to contain a whole buffer.  This makes things
	 * simpler.
	 */
	delta = ofs & vol->sector_size_mask;
	ofs -= delta;
	src += ofs;
	rl = runlist->rl;
	/* Skip to the start offset @ofs in the runlist. */
	vcn = ofs >> cluster_shift;
	vcn_ofs = ofs & vol->cluster_size_mask;
	rl = ntfs_rl_find_vcn_nolock(rl, vcn);
	if (!rl || !rl->length)
		panic("%s(): !rl || !rl->length\n", __FUNCTION__);
	/* Write the clusters specified by the runlist one at a time. */
	do {
		LCN lcn;
		daddr64_t block, end_block;

		lcn = ntfs_rl_vcn_to_lcn(rl, vcn, NULL);
		if (lcn < 0)
			panic("%s(): lcn < 0\n", __FUNCTION__);
		ntfs_debug("Writing vcn 0x%llx, start offset 0x%x, lcn "
				"0x%llx.", (unsigned long long)vcn, vcn_ofs,
				(unsigned long long)lcn);
		/* Write to the device in chunks of sectors. */
		block = ((lcn << cluster_shift) + vcn_ofs) >> block_shift;
		end_block = (lcn + 1) << shift;
		ntfs_debug("end_block 0x%llx.", (unsigned long long)end_block);
		do {
			buf_t buf;
			u8 *dst;

			ntfs_debug("Writing block 0x%llx.",
					(unsigned long long)block);
			/* Obtain the buffer, possibly not uptodate. */
			buf = buf_getblk(dev_vn, block, block_size, 0, 0,
					BLK_META);
			if (!buf)
				panic("%s(): !buf\n", __FUNCTION__);
			err = buf_map(buf, (caddr_t*)&dst);
			if (err) {
				ntfs_error(vol->mp, "buf_map() failed (error "
						"%d).", err);
				buf_brelse(buf);
				goto err;
			}
			/*
			 * Zero the area outside the size of the attribute list
			 * attribute in the final partial buffer.
			 */
			if (src + block_size > src_end) {
				delta = src_end - src;
				bzero(dst + delta, block_size - delta);
				block_size = delta;
			}
			/*
			 * Copy the modified data into the buffer and write it
			 * synchronously.
			 */
			memcpy(dst, src, block_size);
			err = buf_unmap(buf);
			if (err)
				ntfs_error(vol->mp, "buf_unmap() failed "
						"(error %d).", err);
			err = buf_bwrite(buf);
			if (err) {
				ntfs_error(vol->mp, "buf_bwrite() failed "
						"(error %d).", err);
				goto err;
			}
			src += block_size;
			if (src >= src_stop)
				goto done;
		} while (++block < end_block);
		if (++vcn >= rl[1].vcn) {
			rl++;
			if (!rl->length)
				break;
		}
		vcn_ofs = 0;
	} while (1);
done:
	ntfs_debug("Done.");
	return 0;
err:
	ntfs_error(vol->mp, "Failed to update attribute list attribute on "
			"disk due to i/o error on buffer write.  Leaving "
			"inconsistent metadata.  Run chkdsk.");
	NVolSetErrors(vol);
	return EIO;
}

/**
 * ntfs_rl_set - fill data on disk as described by an runlist with a value
 * @vol:	ntfs volume to which to write
 * @rl:		runlist describing clusters to fill with value
 * @val:	value to fill each byte in the clusters with
 *
 * Walk the runlist elements in at @rl and fill all bytes in all clusters @rl
 * describes with the value @val.
 *
 * Return 0 on success and errno on error.
 *
 * Note: This function will simply skip unmapped and sparse runs thus you need
 * to make sure that all wanted runs are mapped.
 *
 * Locking: - The caller must have locked the runlist for writing.
 *	    - The runlist is not modified.
 */
errno_t ntfs_rl_set(ntfs_volume *vol, const ntfs_rl_element *rl, const u8 val)
{
	VCN vcn;
	errno_t err;
	unsigned block_size, shift;

	ntfs_debug("Entering (val 0x%x).", (unsigned)val);
	if (!vol || !rl || !rl->length) {
        ntfs_error((vol ? vol->mp : NULL), "Received invalid arguments.");
		return EINVAL;
	}
    vnode_t dev_vn = vol->dev_vn;
	block_size = vol->sector_size;
	shift = vol->cluster_size_shift - vol->sector_size_shift;
	/* Write the clusters specified by the runlist one at a time. */
	do {
		LCN lcn;
		daddr64_t block, end_block;

		vcn = rl->vcn;
		if (vcn < 0)
			panic("%s(): vcn < 0\n", __FUNCTION__);
		lcn = rl->lcn;
		if (lcn < 0) {
			if (lcn == LCN_HOLE || lcn == LCN_RL_NOT_MAPPED)
				continue;
			ntfs_error(vol->mp, "Invalid LCN (%lld) in runlist.",
					(long long)lcn);
			return EIO;
		}
		/* Write to the device in chunks of sectors. */
		block = lcn << shift;
		end_block = (lcn + rl->length) << shift;
		ntfs_debug("end_block 0x%llx.", (unsigned long long)end_block);
		do {
			buf_t buf;
			u8 *dst;

			ntfs_debug("Setting block 0x%llx.",
					(unsigned long long)block);
			/* Obtain the buffer, possibly not uptodate. */
			buf = buf_getblk(dev_vn, block, block_size, 0, 0,
					BLK_META);
			if (!buf)
				panic("%s(): !buf\n", __FUNCTION__);
			err = buf_map(buf, (caddr_t*)&dst);
			if (err) {
				ntfs_error(vol->mp, "buf_map() failed (error "
						"%d).", err);
				buf_brelse(buf);
				return err;
			}
			/*
			 * Set the bytes in the buffer to @val and write it
			 * synchronously.
			 */
			memset(dst, val, block_size);
			err = buf_unmap(buf);
			if (err)
				ntfs_error(vol->mp, "buf_unmap() failed "
						"(error %d).", err);
			err = buf_bwrite(buf);
			if (err) {
				ntfs_error(vol->mp, "buf_bwrite() failed "
						"(error %d).", err);
				return err;
			}
		} while (++block < end_block);
	} while ((++rl)->length);
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_rl_get_nr_real_clusters - determine number of real clusters in a runlist
 * @runlist:	runlist for which to determine the number of real clusters
 * @start_vcn:	vcn at which to start counting the real clusters
 * @cnt:	number of clusters to scan starting at @start_vcn
 *
 * Find the virtual cluster number @start_vcn in the runlist @runlist and add
 * up the number of real clusters in the range @start_vcn to @start_vcn + @cnt.
 *
 * If @cnt is -1 it is taken to mean the end of the runlist.
 *
 * Return the numbero f real clusters in the range.
 *
 * Locking: The runlist must be locked on entry.
 */
s64 ntfs_rl_get_nr_real_clusters(ntfs_runlist *runlist, const VCN start_vcn,
		const s64 cnt)
{
	VCN end_vcn;
	s64 nr_real_clusters;
	ntfs_rl_element *rl;

	if (!runlist || start_vcn < 0 || cnt < 0)
		panic("%s(): !runlist || start_vcn < 0 || cnt < 0\n",
				__FUNCTION__);
	nr_real_clusters = 0;
	if (!runlist->elements || !cnt)
		goto done;
	end_vcn = -1;
	if (cnt >= 0)
		end_vcn = start_vcn + cnt;
	rl = runlist->rl;
	if (start_vcn > 0)
		rl = ntfs_rl_find_vcn_nolock(rl, start_vcn);
	if (!rl || !rl->length)
		goto done;
	if (rl->lcn >= 0) {
		s64 delta;
		
		delta = start_vcn - rl->vcn;
		nr_real_clusters = rl[1].vcn - delta;
		if (nr_real_clusters > cnt) {
			nr_real_clusters = cnt;
			goto done;
		}
	}
	rl++;
	while (rl->length) {
		/*
		 * If this is the last run of interest, deal with it specially
		 * as it may be partial and then we are done.
		 */
		if (end_vcn >= 0 && rl[1].vcn >= end_vcn) {
			if (rl->lcn >= 0)
				nr_real_clusters += end_vcn - rl->vcn;
			break;
		}
		if (rl->lcn >= 0)
			nr_real_clusters += rl->length;
		rl++;
	}
done:
	ntfs_debug("Done (nr_real_clusters 0x%llx).", nr_real_clusters);
	return nr_real_clusters;
}
