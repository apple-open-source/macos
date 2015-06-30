/*
 * ntfs_compress.c - NTFS kernel compressed attribute operations.
 *
 * Copyright (c) 2006-2008 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2008,2015 Apple Inc.  All Rights Reserved.
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

#include <sys/errno.h>
#include <sys/ucred.h>
#include <sys/ubc.h>
#include <sys/uio.h>
#include <sys/types.h>

#include <mach/memory_object_types.h>

#include <string.h>

#include <libkern/OSMalloc.h>

#include <kern/debug.h>
#include <kern/locks.h>

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_compress.h"
#include "ntfs_debug.h"
#include "ntfs_inode.h"
#include "ntfs_runlist.h"
#include "ntfs_types.h"
#include "ntfs_volume.h"

/**
 * Compression related constants.
 */
enum {
	/* Compression block (cb) types. */
	NTFS_CB_SPARSE		= -1,
	NTFS_CB_COMPRESSED	= -2,
	NTFS_CB_UNCOMPRESSED	= -3,

	/* Compression sub-block (sb) constants. */
	NTFS_SB_SIZE_MASK	= 0x0fff,
	NTFS_SB_SIZE		= 0x1000,
	NTFS_SB_IS_COMPRESSED	= 0x8000,

	/* Token types and access mask. */
	NTFS_SYMBOL_TOKEN	= 0,
	NTFS_PHRASE_TOKEN	= 1,
	NTFS_TOKEN_MASK		= 1,
};

/**
 * ntfs_get_cb_type - determine the type of a compression block
 * @ni:		raw ntfs inode to which the compression block belongs
 * @ofs:	byte offset of start of compression block
 *
 * Determine whether the compression block is sparse, compressed, or
 * uncompressed by looking at the runlist.
 *
 * If the first cluster in the compression block is sparse the whole
 * compression is sparse.  In that case we return NTFS_CB_SPARSE.
 *
 * If the last cluster in the compression block is sparse the compression block
 * is compressed.  In that case we return NTFS_CB_COMPRESSED.
 *
 * If the whole compression block is backed by real clusters it is not
 * compressed.  In that case we return NTFS_CB_UNCOMPRESSED.
 *
 * Return the compression block type (< 0) and errno (>= 0) on error.
 */
static inline int ntfs_get_cb_type(ntfs_inode *ni, s64 ofs)
{
	VCN start_vcn, vcn, end_vcn;
	LCN lcn;
	s64 clusters;
	ntfs_rl_element *rl;
	int ret = EIO;
#ifdef DEBUG
	const char *cb_type_str[3] = { "sparse", "compressed", "uncompressed" };
#endif /* DEBUG */
	BOOL is_retry, write_locked;

	ntfs_debug("Entering for compressed file inode 0x%llx, offset 0x%llx.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)ofs);
	vcn = start_vcn = ofs >> ni->vol->cluster_size_shift;
	end_vcn = start_vcn + ni->compression_block_clusters;
	write_locked = is_retry = FALSE;
	lck_rw_lock_shared(&ni->rl.lock);
retry_remap:
	rl = ni->rl.rl;
	if (ni->rl.elements) {
next_vcn:
		/* Seek to element containing target vcn. */
		while (rl->length && rl[1].vcn <= vcn)
			rl++;
		lcn = ntfs_rl_vcn_to_lcn(rl, vcn, &clusters);
		/*
		 * If we found a hole we are done.  If we were looking up the
		 * starting vcn the entire compression block must be sparse.
		 * Otherwise only part of the compression block is sparse, thus
		 * it is compressed.
		 */
		if (lcn == LCN_HOLE) {
			ret = NTFS_CB_COMPRESSED;
			if (vcn == start_vcn)
				ret = NTFS_CB_SPARSE;
			goto done;
		}
		/*
		 * If we found a real cluster allocation and it covers the end
		 * of the compression block the compression block is not
		 * compressed.  Otherwise we need to look at the last cluster
		 * in the compression block.  Note there is no need to look at
		 * the intervening clusters because it is not possible to have
		 * sparse clusters in the middle of a compression block but not
		 * at its end and neither is it possible to have a partial
		 * compression block at the end of the attribute.
		 */
		if (lcn >= 0) {
			if (vcn + clusters >= end_vcn) {
				ret = NTFS_CB_UNCOMPRESSED;
				goto done;
			}
			vcn = end_vcn - 1;
			is_retry = FALSE;
			goto next_vcn;
		}
	} else
		lcn = LCN_RL_NOT_MAPPED;
	/* The runlist is not mapped or an error occured. */
	if (!write_locked) {
		write_locked = TRUE;
		if (!lck_rw_lock_shared_to_exclusive(&ni->rl.lock)) {
			lck_rw_lock_exclusive(&ni->rl.lock);
			goto retry_remap;
		}
	}
	if (!is_retry && lcn == LCN_RL_NOT_MAPPED) {
		ret = ntfs_map_runlist_nolock(ni, vcn, NULL);
		if (!ret) {
			is_retry = TRUE;
			goto retry_remap;
		}
	} else if (!ret)
		ret = EIO;
done:
	if (write_locked)
		lck_rw_unlock_exclusive(&ni->rl.lock);
	else
		lck_rw_unlock_shared(&ni->rl.lock);
	if (ret < 0)
		ntfs_debug("Done (compression block is %s).",
				cb_type_str[-ret - 1]);
	else
		ntfs_error(ni->vol->mp, "Failed (error %d, lcn %lld).", ret,
				(long long)lcn);
	return ret;
}

/**
 * ntfs_decompress - decompress a compression block into a destination buffer
 *
 * Decompress the compression block @cb_start of size @cb_size into the
 * destination buffer @dst_start.
 *
 * If @pl is not NULL, i.e. a page list is present, skip over any valid pages
 * in the destination buffer.
 *
 * Return 0 on success and errno on error.
 *
 * The decompression algorithm:
 *
 * Compressed data is organized in logical "compression" blocks (cb).  Each cb
 * has a size (cb_size) of 2^compression_unit clusters.  In all versions of
 * Windows, NTFS (NT/2k/XP, NTFS 1.2-3.1), the only valid compression_unit is
 * 4, IOW, each cb is 2^4 = 16 clusters in size.
 *
 * Compression is only supported for cluster sizes between 512 and 4096. Thus a
 * cb can be between 8 and 64kiB in size.
 *
 * Each cb is independent of the other cbs and is thus the minimal unit we have
 * to parse even if we wanted to decompress only one byte.
 *
 * Also, a cb can be totally uncompressed and this would be indicated as a
 * sparse cb in the runlist.
 *
 * Thus, we need to look at the runlist of the compressed data stream, starting
 * at the beginning of the first cb overlapping @page. So we convert the page
 * offset into units of clusters (vcn), and round the vcn down to a mutliple of
 * cb_size clusters.
 *
 * We then scan the runlist for the appropriate position. Based on what we find
 * there, we decide how to proceed.
 *
 * If the cb is not compressed at all, we copy the uncompressed data over from
 * the compressed data.
 *
 * If the cb is completely sparse, we just zero out the destination.
 *
 * In all other cases we initiate the decompression engine, but first some more
 * on the compression algorithm.
 *
 * Before compression the data of each cb is further divided into 4kiB blocks,
 * we call them "sub compression" blocks (sb), each including a header
 * specifying its compressed length.  So we could just scan the cb for the
 * first sb overlapping page and skip the sbs before that, or we could
 * decompress the whole cb injecting the superfluous decompressed pages into
 * the page cache as a form of read ahead (this is what we do when invoked via
 * VNOP_READ()).
 *
 * In either case, we then need to read and decompress all sbs overlapping the
 * destination, potentially having to decompress one or more other cbs, too.
 *
 * Because the sbs follow each other directly, we need to actually read in the
 * whole cb in order to be able to scan through the cb to find the first sb
 * overlapping the destination.
 *
 * So, we read the whole cb from disk and start at the first sb.
 *
 * As mentioned above, each sb is started with a header.  The header is 16 bits
 * of which the lower twelve bits (i.e. bits 0 to 11) are the length (L) - 3 of
 * the sb (including the two bytes for the header itself, or L - 1 not counting
 * the two bytes for the header).  The higher four bits are set to 1011 (0xb)
 * by the compressor for a compressed block, or to 0000 for an uncompressed
 * block, but the decompressor only checks the most significant bit taking a 1
 * to signify a compressed block, and a 0 an uncompressed block.
 *
 * So from the header we know how many compressed bytes we need to decompress to
 * obtain the next 4kiB of uncompressed data and if we did not want to
 * decompress this sb we could just seek to the next next one using the length
 * read from the header.  We could then continue seeking until we reach the
 * first sb overlapping the destination.
 *
 * In either case, we will reach a sb which we want to decompress.
 *
 * Having dealt with the 16-bit header of the sb, we now have length bytes of
 * compressed data to decompress.  This compressed stream is further split into
 * tokens which are organized into groups of eight tokens.  Each token group
 * (tg) starts with a tag byte, which is an eight bit bitmap, the bits
 * specifying the type of each of the following eight tokens.  The least
 * significant bit (LSB) corresponds to the first token and the most
 * significant bit (MSB) corresponds to the last token.
 * 
 * The two types of tokens are symbol tokens, specified by a zero bit, and
 * phrase tokens, specified by a set bit.
 * 
 * A symbol token (st) is a single byte and is to be taken literally and copied
 * into the sliding window (the decompressed data).
 * 
 * A phrase token (pt) is a pointer back into the sliding window (in bytes),
 * together with a length (again in bytes), starting at the byte the back
 * pointer is pointing to.  Thus a phrase token defines a sequence of bytes in
 * the sliding window which need to be copied at the current position into the
 * sliding window (the decompressed data stream).
 *
 * Each pt consists of 2 bytes split into the back pointer (p) and the length
 * (l), each of variable bit width (but the sum of the widths of p and l is
 * fixed at 16 bits).  p is at least 4 bits and l is at most 12 bits.
 *
 * The most significant bits contain the back pointer (p), while the least
 * significant bits contain the length (l).
 *
 * l is actually stored as the number of bytes minus 3 (unsigned) as anything
 * shorter than that would be at least as long as the 2 bytes needed for the
 * actual pt, so no compression would be achieved.
 *
 * p is stored as the positive number of bytes minus 1 (unsigned) as going zero
 * bytes back is meaningless.
 *
 * Note that decompression has to occur byte by byte, as it is possible that
 * some of the bytes pointed to by the pt will only be generated in the sliding
 * window as the byte sequence pointed to by the pt is being copied into it!
 *
 * To give a concrete example: a block full of the letter A would be compressed
 * by storing the byte A once as a symbol token, followed by a single phrase
 * token with back pointer -1 (p = 0, therefore go back by -(0 + 1) bytes) and
 * length 4095 (l=0xffc, therefore length 0xffc + 3 bytes).
 *
 * The widths of p and l are determined from the current position within the
 * decompressed data (dst).  We do not actually care about the widths as such
 * however, but instead we want the mask (l_mask) with which to AND the pt to
 * obtain l, and the number of bits (p_shift) by which to right shift the pt to
 * obtain p.  These are determined using the following algorithm:
 *
 * for (i = cur_pos, l_mask = 0xfff, p_shift = 12; i >= 0x10; i >>= 1) {
 *	l_mask >>= 1;
 *	p_shift--;
 * }
 *
 * The above is the conventional algorithm.  As an optimization we actually use
 * a different algorithm as this offers O(1) performance instead of O(n) of the
 * above conventional algorithm.  Our optimized algorithm first calculates
 * log2(current destination position in sb) and then uses that to derive p and
 * l without having to iterate.  We just need an arch-optimized log2() function
 * now to make it really fast as we for now still have a small loop which we
 * need to determine the log2().  See the below code for details.
 *
 * Note, that as usual in NTFS, the sb header, as well as each pt, are stored
 * in little endian format.
 */
static inline errno_t ntfs_decompress(ntfs_volume *vol, u8 *dst_start,
		const int dst_ofs_in_cb, int dst_size, u8 *const cb_start,
		const int cb_size, upl_page_info_t *pl, int cur_pg,
		const int pages_per_cb)
{
	/*
	 * Pointers into the compressed data, i.e. the compression block (cb),
	 * and the therein contained sub-blocks (sb).
	 */
	u8 *cb;			/* Current position in compression block. */
	u8 *cb_end;		/* End of compression block. */
	u8 *cb_sb_start;	/* Beginning of the current sb in the cb. */
	u8 *cb_sb_end;		/* End of current sb / beginning of next sb. */
	/* Variables for uncompressed data / destination buffer. */
	u8 *dst;		/* Current position in destination. */
	u8 *dst_end;		/* End of destination buffer. */
	u8 *dst_sb_start;	/* Start of current sub-block in destination. */
	u8 *dst_sb_end;		/* End of current sub-block in destination. */
	/* Variables for tag and token parsing. */
	u8 tag;			/* Current tag. */
	unsigned token;		/* Loop counter for the eight tokens in tag. */
	unsigned skip_sbs;
	BOOL skip_valid_pages;

	ntfs_debug("Entering, compression block size 0x%x bytes.", cb_size);
	/* Do we need to test for and skip valid pages in the destination? */
	skip_valid_pages = FALSE;
	if (pl && pages_per_cb > 1)
		skip_valid_pages = TRUE;
	/*
	 * Do we need to skip any sub-blocks because the destination buffer
	 * does not begin at the beginning of the compression block?
	 */
	skip_sbs = 0;
	if (dst_ofs_in_cb)
		skip_sbs = dst_ofs_in_cb / NTFS_SB_SIZE;
	cb = cb_start;
	cb_end = cb_start + cb_size;
	dst = dst_start;
	dst_end = dst + dst_size;
next_sb:
	ntfs_debug("Beginning sub-block at offset 0x%lx in the compression "
			"block.", (unsigned long)(cb - cb_start));
	/*
	 * Have we reached the end of the compression block or the end of the
	 * decompressed data?  The latter can happen for example if the current
	 * position in the compression block is one byte before its end so the
	 * first two checks do not detect it.
	 */
	if (cb == cb_end || !le16_to_cpup((le16*)cb) || dst == dst_end) {
        /*
         * Zero fill any remaining portion of the destination buffer.
         * I'm unsure whether this should be considered an I/O error;
         * If not, we at least need to avoid returning uninitialized data.
         */
        if (dst < dst_end)
            bzero(dst, dst_end - dst);
		ntfs_debug("Done.");
		return 0;
	}
	/* Setup offsets for the current sub-block destination. */
	dst_sb_start = dst;
	dst_sb_end = dst + NTFS_SB_SIZE;
	/* Check that we are still within allowed boundaries. */
	if (dst_sb_end > dst_end)
		goto err;
	/* Does the minimum size of a compressed sb overflow valid range? */
	if (cb + 6 > cb_end)
		goto err;
	/* Setup the current sub-block source pointers and validate range. */
	cb_sb_start = cb;
	cb_sb_end = cb + (le16_to_cpup((le16*)cb) & NTFS_SB_SIZE_MASK) + 3;
	if (cb_sb_end > cb_end)
		goto err;
	/*
	 * If the destination buffer does not start at the beginning of the
	 * compression block, skip sub-blocks until we reach the beginning of
	 * the destination buffer.
	 */
	if (skip_sbs) {
		skip_sbs--;
		/* Advance source position to next sub-block. */
		cb = cb_sb_end;
		goto next_sb;
	}
	/*
	 * If the destination page corresponding to this sub-block is valid,
	 * skip the sub-block.
	 */
	if (skip_valid_pages) {
		BOOL skip_sb;

		skip_sb = upl_valid_page(pl, cur_pg);
		/*
		 * Advance current page if the destination pointer is going to
		 * cross a page boundary.  Doing this here unconditionally
		 * means we do not need to advance it later on when switching
		 * to the next sub-block thus it saves us one test for
		 * @skip_valid_pages.
		 */
		if (!((dst - dst_start + NTFS_SB_SIZE) & PAGE_MASK))
			cur_pg++;
		if (skip_sb) {
			/* Advance position to next sub-block. */
			cb = cb_sb_end;
			dst = dst_sb_end;
			goto next_sb;
		}
	}
	/* Now, we are ready to process the current sub-block. */
	if (!(le16_to_cpup((le16*)cb) & NTFS_SB_IS_COMPRESSED)) {
		/*
		 * This sb is not compressed, just copy its data into the
		 * destination buffer.
		 */
		ntfs_debug("Found uncompressed sub-block.");
		/* Advance source position to first data byte. */
		cb += 2;
		/* An uncompressed sb must be full size. */
		if (cb_sb_end - cb != NTFS_SB_SIZE)
			goto err;
		/* Copy the sub-block data. */
		memcpy(dst, cb, NTFS_SB_SIZE);
		/* Advance position to next sub-block. */
		cb = cb_sb_end;
		dst = dst_sb_end;
		goto next_sb;
	}
	/* This sb is compressed, decompress it into the destination buffer. */
	ntfs_debug("Found compressed sub-block.");
	/* Forward to the first tag in the sub-block. */
	cb += 2;
next_tag:
	if (cb == cb_sb_end) {
		/* Check if the decompressed sub-block was not full-length. */
		if (dst < dst_sb_end) {
			ntfs_debug("Filling incomplete sub-block with "
					"zeroes.");
			/* Zero remainder and update destination position. */
			bzero(dst, dst_sb_end - dst);
			dst = dst_sb_end;
		}
		/* We have finished the current sub-block. */
		goto next_sb;
	}
	/* Check we are still in range. */
	if (cb > cb_sb_end || dst > dst_sb_end)
		goto err;
	/* Get the next tag and advance to first token. */
	tag = *cb++;
	/* Parse the eight tokens described by the tag. */
	for (token = 0; token < 8; token++, tag >>= 1) {
		unsigned lg, u, pt, length, max_non_overlap;
		u8 *dst_back_addr;

		/* Check if we are done / still in range. */
		if (cb >= cb_sb_end || dst > dst_sb_end)
			break;
		/* Determine token type and parse appropriately.*/
		if ((tag & NTFS_TOKEN_MASK) == NTFS_SYMBOL_TOKEN) {
			/*
			 * We have a symbol token, copy the symbol across, and
			 * advance the source and destination positions.
			 */
			*dst++ = *cb++;
			/* Continue with the next token. */
			continue;
		}
		/*
		 * We have a phrase token.  Make sure it is not the first tag
		 * in the sub-block as this is illegal and would confuse the
		 * code below.
		 */
		if (dst == dst_sb_start)
			goto err;
		/*
		 * Determine the number of bytes to go back (p) and the number
		 * of bytes to copy (l).  We use an optimized algorithm in
		 * which we first calculate log2(current destination position
		 * in sb), which allows determination of l and p in O(1) rather
		 * than O(n).  We just need an arch-optimized log2() function
		 * now.
		 */
		lg = 0;
		for (u = dst - dst_sb_start - 1; u >= 0x10; u >>= 1)
			lg++;
		/* Get the phrase token. */
		pt = le16_to_cpup((u16*)cb);
		/*
		 * Calculate starting position of the byte sequence in the
		 * destination using the fact that p = (pt >> (12 - lg)) + 1
		 * and make sure we do not go too far back.
		 */
		dst_back_addr = dst - (pt >> (12 - lg)) - 1;
		if (dst_back_addr < dst_sb_start)
			goto err;
		/* Now calculate the length (l) of the byte sequence. */
		length = (pt & (0xfff >> lg)) + 3;
		/* Verify destination is in range. */
		if (dst + length > dst_sb_end)
			goto err;
		/* The number of non-overlapping bytes. */
		max_non_overlap = dst - dst_back_addr;
		if (length <= max_non_overlap) {
			/* The byte sequence does not overlap, just copy it. */
			memcpy(dst, dst_back_addr, length);
			/* Advance destination pointer. */
			dst += length;
		} else {
			/*
			 * The byte sequence does overlap, copy non-overlapping
			 * part and then do a slow byte by byte copy for the
			 * overlapping part.  Also, advance the destination
			 * pointer.
			 */
			memcpy(dst, dst_back_addr, max_non_overlap);
			dst += max_non_overlap;
			dst_back_addr += max_non_overlap;
			length -= max_non_overlap;
			while (length--)
				*dst++ = *dst_back_addr++;
		}
		/* Advance source position and continue with the next token. */
		cb += 2;
	}
	/* No tokens left in the current tag.  Continue with the next tag. */
	goto next_tag;
err:
	ntfs_error(vol->mp, "Compressed data is corrupt.  Run chkdsk.");
	NVolSetErrors(vol);
	return EOVERFLOW;
}

/**
 * ntfs_read_compressed - read and decompress data from a compressed attribute
 * @ni:			non-raw ntfs inode to which the raw inode belongs
 * @raw_ni:		raw compressed ntfs inode to read from
 * @ofs:		byte offset into uncompressed data stream to read from
 * @count:		number of bytes to return in the destination buffer
 * @dst_start:		destination buffer in which to return the data
 * @pl:			page list in which @dst_start resides (or NULL)
 * @ioflags:		flags further describing the read (see ntfs_vnop_read())
 *
 * Read compressed data from the raw inode @raw_ni, decompress it, and return
 * the decompressed data in the destination buffer @dst_start of size @count
 * bytes.
 *
 * If @pl is not NULL, it is the page list in which @dst_start resides with
 * @dst_start beginning at offset zero in the first page of the page list.
 *
 * When @pl is not NULL, we have to check each page in the page list for being
 * valid and if it is we have to skip decompression of its data so that we do
 * not overwrite and dirty but not yet comitted data and even in the read-only
 * driver we want to skip decompression in this case as there is no point in
 * decompressing data we already have decompressed and have in cache.
 *
 * This function allocates a temporary buffer to hold a compression block and
 * reads each compression block in sequence into it using cluster_read() which
 * gives us read-ahead on the raw inode.  Once it has the data it decompresses
 * it straight into the destination buffer @dst_start and stops when @count
 * bytes have been decompressed (usually this will be the end of the
 * compression block).
 *
 * Return 0 on success and errno on error.
 */
errno_t ntfs_read_compressed(ntfs_inode *ni, ntfs_inode *raw_ni, s64 ofs_start,
		int count, u8 *dst_start, upl_page_info_t *pl, int ioflags)
{
	s64 ofs, init_size, raw_size, size;
	ntfs_volume *vol = ni->vol;
	u8 *dst, *cb;
	uio_t uio;
	int err, io_count, pages_per_cb, cb_size, cur_pg, cur_pg_ofs, last_pg;
	int cb_type, zero_end_ofs, dst_ofs_in_cb;

	ntfs_debug("Entering for compressed file inode 0x%llx, offset 0x%llx, "
			"count 0x%x, ioflags 0x%x.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)ofs_start, count, ioflags);
	ofs = ofs_start;
	dst = dst_start;
	cb = NULL;
	uio = NULL;
	zero_end_ofs = last_pg = cur_pg_ofs = cur_pg = 0;
	/*
	 * We can only read from regular files and named streams that are
	 * compressed and non-resident.  We should never be called for anything
	 * else.
	 */
	if (!NInoAttr(raw_ni) || raw_ni->type != AT_DATA ||
			!NInoCompressed(raw_ni) || !NInoNonResident(raw_ni) ||
			!NInoRaw(raw_ni) || NInoEncrypted(raw_ni))
		panic("%s(): Called for incorrect inode type.\n", __FUNCTION__);
	if (ofs & PAGE_MASK || count & PAGE_MASK)
		panic("%s(): Called with offset 0x%llx and count 0x%x at "
				"least one of which is not a multiple of the "
				"system page size 0x%x.\n", __FUNCTION__,
				(unsigned long long)ofs, count, PAGE_SIZE);
	cb_size = raw_ni->compression_block_size;
	/*
	 * Doing this means that we can assume that @pages_per_cb <= 1 implies
	 * that @pl is not NULL in the below code.
	 */
	pages_per_cb = 0xffff;
	if (pl) {
		last_pg = count >> PAGE_SHIFT;
		pages_per_cb = cb_size >> PAGE_SHIFT;
	}
	/*
	 * Zero any completely uninitialized pages thus we are guaranteed only
	 * to have a partial uninitialized page which makes the decompression
	 * code simpler.
	 */
	lck_spin_lock(&ni->size_lock);
	init_size = ni->initialized_size;
	raw_size = ni->allocated_size;
	if (ofs > ni->data_size)
		panic("%s(): Called with offset 0x%llx which is beyond the "
				"data size 0x%llx.\n", __FUNCTION__,
				(unsigned long long)ofs,
				(unsigned long long)ni->data_size);
	lck_spin_unlock(&ni->size_lock);
	size = (init_size + PAGE_MASK) & ~PAGE_MASK_64;
	/* @ofs is page aligned and @count is at least one page in size. */
	if (ofs + count > init_size) {
		int start_count;

		start_count = count;
		/* Do not zero a partial final page yet. */
		count = size - ofs;
		/*
		 * If the beginning of the i/o exceeds the initialized size we
		 * just need to zero the destination and are done.
		 */
		if (ofs > init_size)
			count = 0;
		if (!pl)
			bzero(dst + count, start_count - count);
		else {
			/* Only zero complete, invalid pages. */
			for (cur_pg = count >> PAGE_SHIFT; cur_pg < last_pg;
					cur_pg++) {
				if (!upl_valid_page(pl, cur_pg))
					bzero(dst + (cur_pg << PAGE_SHIFT),
							PAGE_SIZE);
			}
			last_pg = count >> PAGE_SHIFT;
			cur_pg = 0;
		}
		if (!count) {
			ntfs_debug("Done (request in uninitialized region).");
			return 0;
		}
		if (init_size < size)
			zero_end_ofs = init_size - ofs;
	}
	dst_ofs_in_cb = ofs & (cb_size - 1);
do_next_cb:
	/* Truncate the final request if it is partial. */
	io_count = cb_size - dst_ofs_in_cb;
	if (io_count > count)
		io_count = count;
	/*
	 * If a page list is present and a page is larger than or equal to a
	 * compression block and the current page is valid, skip this whole
	 * page.
	 */
	if (pages_per_cb <= 1 && upl_valid_page(pl, cur_pg)) {
		io_count = PAGE_SIZE - cur_pg_ofs;
		cur_pg_ofs = 0;
		cur_pg++;
		goto next_cb;
	}
	/* Determine the type of the current compression block. */
	cb_type = ntfs_get_cb_type(raw_ni, ofs - dst_ofs_in_cb);
	if (cb_type >= 0) {
		err = cb_type;
		goto err;
	}
	/*
	 * If the compression block is sparse, bzero() the destination buffer
	 * (skipping any valid pages) and go to the next compression block.
	 */
	if (cb_type == NTFS_CB_SPARSE) {
		int stop_pg;

		ntfs_debug("Found sparse compression block.");
		/* Only zero invalid pages. */
		if (!pl || pages_per_cb <= 1) {
			bzero(dst, io_count);
			goto pl_next_cb;
		}
		stop_pg = cur_pg + pages_per_cb;
		if (stop_pg > last_pg)
			stop_pg = last_pg;
		for (; cur_pg < stop_pg; cur_pg++) {
			if (!upl_valid_page(pl, cur_pg))
				bzero(dst_start + (cur_pg << PAGE_SHIFT),
						PAGE_SIZE);
		}
		goto next_cb;
	}
	/*
	 * Create a uio or reset the already created one and point it at the
	 * current attribute offset.
	 */
	if (!uio) {
		uio = uio_create(1, ofs, UIO_SYSSPACE32, UIO_READ);
		if (!uio) {
			ntfs_error(vol->mp, "Not enough memory to allocate "
					"uio.");
			err = ENOMEM;
			goto err;
		}
	}
	/*
	 * If the compression block is not compressed use cluster_read() to
	 * read the uncompressed data straight into our destination buffer.
	 */
	if (cb_type == NTFS_CB_UNCOMPRESSED) {
		int stop_pg;

		ntfs_debug("Found uncompressed compression block.");
		 /*
		  * If no page list is present or the only page is invalid use
		  * cluster_read() to read the uncompressed data straight into
		  * our buffer.
		  */
		if (!pl || pages_per_cb <= 1) {
			/*
			 * Add our destination buffer to the uio so we can read
			 * into it using cluster_read().
			 */
			uio_reset(uio, ofs, UIO_SYSSPACE32, UIO_READ);
			err = uio_addiov(uio, CAST_USER_ADDR_T(dst), io_count);
			if (err)
				panic("%s(): uio_addiov() failed.\n",
						__FUNCTION__);
			err = cluster_read(raw_ni->vn, uio, raw_size, ioflags);
			if (err || uio_resid(uio))
				goto cl_err;
			goto pl_next_cb;
		}
		/*
		 * Page list present and multiple pages per compression block.
		 * Iterate over all the pages reading in all invalid page
		 * ranges straight into our buffer.
		 */
		stop_pg = cur_pg + pages_per_cb;
		if (stop_pg > last_pg)
			stop_pg = last_pg;
		for (; cur_pg < stop_pg; cur_pg++) {
			int start_pg, start_ofs;

			/* Skip over valid page ranges. */
			if (upl_valid_page(pl, cur_pg))
				continue;
			/*
			 * We found an invalid page, determine how many
			 * sequential pages are invalid.
			 */
			start_pg = cur_pg;
			while (cur_pg + 1 < stop_pg) {
				if (upl_valid_page(pl, cur_pg + 1))
					break;
				cur_pg++;
			}
			/*
			 * Add our destination buffer to the uio so we can read
			 * into it using cluster_read().
			 */
			start_ofs = start_pg << PAGE_SHIFT;
			uio_reset(uio, ofs_start + start_ofs, UIO_SYSSPACE32,
					UIO_READ);
			err = uio_addiov(uio, CAST_USER_ADDR_T(
					dst_start + start_ofs),
					(cur_pg + 1 - start_pg) << PAGE_SHIFT);
			if (err)
				panic("%s(): uio_addiov() failed.\n",
						__FUNCTION__);
			err = cluster_read(raw_ni->vn, uio, raw_size, ioflags);
			if (err || uio_resid(uio))
				goto cl_err;
		}
		goto next_cb;
	}
	/*
	 * The compression block is compressed.  Read the compressed data into
	 * our temporary buffer, allocating it if we have not done so yet.
	 */
	ntfs_debug("Found compressed compression block.");
	if (!cb) {
		cb = OSMalloc(cb_size, ntfs_malloc_tag);
		if (!cb) {
			ntfs_error(vol->mp, "Not enough memory to allocate "
					"temporary buffer.");
			err = ENOMEM;
			goto err;
		}
	}
	/*
	 * FIXME: As an optimization we could only read the actual allocated
	 * clusters so cluster_read() does not waste time zeroing the sparse
	 * clusters when there are whole pages worth of them.  Probably not
	 * worth the effort as that may mess around with the read-ahead
	 * streaming detection and having read-ahead messed up is likely to
	 * cause a performance hit outweighing the benefit gained from not
	 * doing the zeroing.  Especially so since we would incur overhead in
	 * determining the number of non-sparse clusters.
	 */
	uio_reset(uio, ofs - dst_ofs_in_cb, UIO_SYSSPACE32, UIO_READ);
	err = uio_addiov(uio, CAST_USER_ADDR_T(cb), cb_size);
	if (err)
		panic("%s(): uio_addiov() failed.\n", __FUNCTION__);
	err = cluster_read(raw_ni->vn, uio, raw_size, ioflags);
	if (err || uio_resid(uio))
		goto cl_err;
	/*
	 * We now have the compressed data.  Decompress it into the destination
	 * buffer skipping any valid pages if a page list is present.
	 */
	err = ntfs_decompress(vol, dst, dst_ofs_in_cb, io_count, cb, cb_size,
			pl, cur_pg, pages_per_cb);
	if (err) {
		ntfs_error(vol->mp, "Failed to decompress data (error %d).",
				err);
		goto err;
	}
pl_next_cb:
	if (pl) {
		cur_pg += pages_per_cb;
		if (!pages_per_cb) {
			cur_pg_ofs += io_count;
			if (cur_pg_ofs >= PAGE_SIZE) {
				cur_pg_ofs = 0;
				cur_pg++;
			}
		}
	}
next_cb:
	ofs += io_count;
	dst += io_count;
	count -= io_count;
	dst_ofs_in_cb = 0;
	/* Are we done yet? */
	if (count > 0)
		goto do_next_cb;
	/*
	 * Check if the last page is partially outside the initialized size and
	 * if so zero its uninitialized tail.
	 */
	if (zero_end_ofs) {
		count = PAGE_SIZE - (zero_end_ofs & PAGE_MASK);
		if (!pl || !upl_valid_page(pl, zero_end_ofs >> PAGE_SHIFT))
			bzero(dst_start + zero_end_ofs, count);
	}
	if (uio)
		uio_free(uio);
	if (cb)
		OSFree(cb, cb_size, ntfs_malloc_tag);
	ntfs_debug("Done.");
	return 0;
cl_err:
	if (err)
		ntfs_error(vol->mp, "Failed to read %scompressed compression "
				"block using cluster_read() (error %d).",
				cb_type == NTFS_CB_COMPRESSED ?  "" : "un",
				err);
	else {
		ntfs_error(vol->mp, "Partial read when reading %scompressed "
				"compression block using cluster_read().",
				cb_type == NTFS_CB_COMPRESSED ? "" : "un");
		err = EIO;
	}
err:
	if (uio)
		uio_free(uio);
	if (cb)
		OSFree(cb, cb_size, ntfs_malloc_tag);
	ntfs_error(vol->mp, "Failed (error %d).", err);
	return err;
}
