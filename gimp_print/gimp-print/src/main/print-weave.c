/*
 * "$Id: print-weave.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
 *
 *   Softweave calculator for gimp-print.
 *
 *   Copyright 2000 Charles Briscoe-Smith <cpbs@debian.org>
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * This file must include only standard C header files.  The core code must
 * compile on generic platforms that don't support glib, gimp, gtk, etc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gimp-print/gimp-print.h>
#include "gimp-print-internal.h"
#include <gimp-print/gimp-print-intl-internal.h>

#if 0
#define TEST_RAW
#endif
#if 0
#define TEST_COOKED
#endif
#if 0
#define ACCUMULATE
#endif
#if 1
#define ASSERTIONS
#endif

#if defined(TEST_RAW) || defined(TEST_COOKED)
#define TEST
#endif

#ifdef ASSERTIONS
#define assert(x,v)							\
do									\
{									\
  if (!(x))								\
    {									\
      stp_eprintf(v, "Assertion %s failed! file %s, line %d.\n",	\
		  #x, __FILE__, __LINE__);				\
      exit(1);								\
    }									\
} while (0)
#else
#define assert(x,v) do {} while (0)
#endif

#ifdef TEST
#define MAXCOLLECT (1000)
#endif

static int
gcd(int x, int y)
{
	if (y == 0)
		return x;
	while (x != 0) {
		if (y > x) {
			int t = x;
			x = y;
			y = t;
		}
		x %= y;
	}
	return y;
}

/* RAW WEAVE */

typedef struct raw {
	int separation;
	int jets;
	int oversampling;
	int advancebasis;
	int subblocksperpassblock;
	int passespersubblock;
	int strategy;
	stp_vars_t v;
} raw_t;

/*
 * Strategy types currently defined:
 *
 *  0: microweave (intercepted at the escp2 driver level so we never
 *     see it here)
 *  1: zig-zag type pass block filling
 *  2: ascending pass block filling
 *  3: descending pass block filling
 *  4: ascending fill with 2x expansion
 *  5: ascending fill with 3x expansion
 *  6: staggered zig-zag neighbour-avoidance fill
 *
 * In theory, strategy 1 should be optimal; in practice, it can lead
 * to visible areas of banding.  If it's necessary to avoid filling
 * neighbouring rows in neighbouring passes, strategy 6 should be optimal,
 * at least for some weaves.
 */

static void
initialize_raw_weave(raw_t *w,	/* I - weave struct to be filled in */
                     int S,	/* I - jet separation */
                     int J,	/* I - number of jets */
                     int H,	/* I - oversampling factor */
                     int strat,	/* I - weave pattern variation to use */
		     stp_vars_t v)
{
	w->separation = S;
	w->jets = J;
	w->oversampling = H;
	w->advancebasis = J / H;
	if (w->advancebasis == 0)
	  w->advancebasis++;
	w->subblocksperpassblock = gcd(S, w->advancebasis);
	w->passespersubblock = S / w->subblocksperpassblock;
	w->strategy = strat;
	w->v = v;
}

static void
calculate_raw_pass_parameters(raw_t *w,		/* I - weave parameters */
                              int pass,		/* I - pass number ( >= 0) */
                              int *startrow,	/* O - print head position */
                              int *subpass)	/* O - subpass number */
{
	int band, passinband, subpassblock, subpassoffset;

	band = pass / (w->separation * w->oversampling);
	passinband = pass % (w->separation * w->oversampling);
	subpassblock = pass % w->separation
	                 * w->subblocksperpassblock / w->separation;

	switch (w->strategy) {
	case 1:
		if (subpassblock * 2 < w->subblocksperpassblock)
			subpassoffset = 2 * subpassblock;
		else
			subpassoffset = 2 * (w->subblocksperpassblock
			                      - subpassblock) - 1;
		break;
	case 2:
		subpassoffset = subpassblock;
		break;
	case 3:
		subpassoffset = w->subblocksperpassblock - 1 - subpassblock;
		break;
	case 4:
		if (subpassblock * 2 < w->subblocksperpassblock)
			subpassoffset = 2 * subpassblock;
		else
			subpassoffset = 1 + 2 * (subpassblock
			                          - (w->subblocksperpassblock
			                              + 1) / 2);
		break;
	case 5:
		if (subpassblock * 3 < w->subblocksperpassblock)
			subpassoffset = 3 * subpassblock;
		else if (3 * (subpassblock - (w->subblocksperpassblock + 2) / 3)
		          < w->subblocksperpassblock - 2)
			subpassoffset = 2 + 3 * (subpassblock
			                          - (w->subblocksperpassblock
			                              + 2) / 3);
		else
			subpassoffset = 1 + 3 * (subpassblock
			                          - (w->subblocksperpassblock
			                              + 2) / 3
						  - w->subblocksperpassblock
						      / 3);
		break;
	case 6:
		if (subpassblock * 2 < w->subblocksperpassblock)
			subpassoffset = 2 * subpassblock;
		else if (subpassblock * 2 < w->subblocksperpassblock + 2)
			subpassoffset = 1;
		else
			subpassoffset = 2 * (w->subblocksperpassblock
			                      - subpassblock) + 1;
		break;
	default:
		subpassoffset = subpassblock;
		break;
	}

	*startrow = w->separation * w->jets * band
	              + w->advancebasis * passinband + subpassoffset;
	*subpass = passinband / w->separation;
}

static void
calculate_raw_row_parameters(raw_t *w,		/* I - weave parameters */
                             int row,		/* I - row number */
                             int subpass,	/* I - subpass number */
                             int *pass,		/* O - pass number */
                             int *jet,		/* O - jet number in pass */
                             int *startrow)	/* O - starting row of pass */
{
	int subblockoffset, subpassblock, band, baserow, passinband, offset;
	int pass_div_separation;
	int pass_mod_separation;
	int off_mod_separation;

	subblockoffset = row % w->subblocksperpassblock;
	switch (w->strategy) {
	case 1:
		if (subblockoffset % 2 == 0)
			subpassblock = subblockoffset / 2;
		else
			subpassblock = w->subblocksperpassblock
			                 - (subblockoffset + 1) / 2;
		break;
	case 2:
		subpassblock = subblockoffset;
		break;
	case 3:
		subpassblock = w->subblocksperpassblock - 1 - subblockoffset;
		break;
	case 4:
		if (subblockoffset % 2 == 0)
			subpassblock = subblockoffset / 2;
		else
			subpassblock = (subblockoffset - 1) / 2
			               + (w->subblocksperpassblock + 1) / 2;
		break;
	case 5:
		if (subblockoffset % 3 == 0)
			subpassblock = subblockoffset / 3;
		else if (subblockoffset % 3 == 1)
			subpassblock = (subblockoffset - 1) / 3
			                 + (w->subblocksperpassblock + 2) / 3;
		else
			subpassblock = (subblockoffset - 2) / 3
			                 + (w->subblocksperpassblock + 2) / 3
			                 + (w->subblocksperpassblock + 1) / 3;
		break;
	case 6:
		if (subblockoffset % 2 == 0)
			subpassblock = subblockoffset / 2;
		else if (subblockoffset == 1)
			subpassblock = w->subblocksperpassblock / 2;
		else
			subpassblock = w->subblocksperpassblock
			                 - (subblockoffset - 1) / 2;
	default:
		subpassblock = subblockoffset;
		break;
	}

	band = row / (w->separation * w->jets);
	baserow = row - subblockoffset - band * w->separation * w->jets;
	passinband = baserow / w->advancebasis;
	offset = baserow % w->advancebasis;
	pass_div_separation = passinband / w->separation;
	pass_mod_separation = passinband % w->separation;
	off_mod_separation = offset % w->separation;

	while (off_mod_separation != 0
	       || pass_div_separation != subpass
	       || pass_mod_separation / w->passespersubblock
	            != subpassblock)
	  {
	    offset += w->advancebasis;
	    passinband--;
	    if (passinband >= 0)
	      {
		pass_mod_separation--;
		if (pass_mod_separation < 0)
		  {
		    pass_mod_separation += w->separation;
		    pass_div_separation--;
		  }
		if (w->advancebasis < w->separation)
		  {
		    off_mod_separation += w->advancebasis;
		    if (off_mod_separation >= w->separation)
		      off_mod_separation -= w->separation;
		  }
		else if (w->advancebasis > w->separation)
		  off_mod_separation = offset % w->separation;
	      }
	    else
	      {
		const int roundedjets =
		  (w->advancebasis * w->oversampling) % w->jets;
		band--;
		passinband += w->separation * w->oversampling;
		offset += w->separation * (w->jets - roundedjets);
		pass_div_separation = passinband / w->separation;
		pass_mod_separation = passinband % w->separation;
		off_mod_separation = offset % w->separation;
	      }
	  }

	*pass = band * w->oversampling * w->separation + passinband;
	*jet = (offset / w->separation) % w->jets;
	*startrow = row - (*jet * w->separation);
}

/* COOKED WEAVE */

typedef struct cooked {
	raw_t rw;
	int first_row_printed;
	int last_row_printed;

	int first_premapped_pass;	/* First raw pass used by this page */
	int first_normal_pass;
	int first_postmapped_pass;
	int first_unused_pass;

	int *pass_premap;
	int *stagger_premap;
	int *pass_postmap;
	int *stagger_postmap;
} cooked_t;

static void
sort_by_start_row(int *map, int *startrows, int count)
{
	/*
	 * Yes, it's a bubble sort, but we do it no more than 4 times
	 * per page, and we are only sorting a small number of items.
	 */

	int dirty;

	do {
		int x;
		dirty = 0;
		for (x = 1; x < count; x++) {
			if (startrows[x - 1] > startrows[x]) {
				int temp = startrows[x - 1];
				startrows[x - 1] = startrows[x];
				startrows[x] = temp;
				temp = map[x - 1];
				map[x - 1] = map[x];
				map[x] = temp;
				dirty = 1;
			}
		}
	} while (dirty);
}

static void
calculate_stagger(raw_t *w, int *map, int *startrows_stagger, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		int startrow, subpass;
		calculate_raw_pass_parameters(w, map[i], &startrow, &subpass);
		startrow -= w->separation * w->jets;
		startrows_stagger[i] = (startrows_stagger[i] - startrow)
		                         / w->separation;
	}
}

static void
invert_map(int *map, int *stagger, int count, int oldfirstpass,
           int newfirstpass)
{
	int i;
	int *newmap, *newstagger;
	newmap = stp_malloc(count * sizeof(int));
	newstagger = stp_malloc(count * sizeof(int));

	for (i = 0; i < count; i++) {
		newmap[map[i] - oldfirstpass] = i + newfirstpass;
		newstagger[map[i] - oldfirstpass] = stagger[i];
	}

	memcpy(map, newmap, count * sizeof(int));
	memcpy(stagger, newstagger, count * sizeof(int));
	stp_free(newstagger);
	stp_free(newmap);
}

static void
make_passmap(raw_t *w, int **map, int **starts, int first_pass_number,
             int first_pass_to_map, int first_pass_after_map,
             int first_pass_to_stagger, int first_pass_after_stagger,
             int first_row_of_maximal_pass, int separations_to_distribute)
{
	int *passmap, *startrows;
	int passes_to_map = first_pass_after_map - first_pass_to_map;
	int i;

	assert(first_pass_to_map <= first_pass_after_map, w->v);
	assert(first_pass_to_stagger <= first_pass_after_stagger, w->v);

	*map = passmap = stp_malloc(passes_to_map * sizeof(int));
	*starts = startrows = stp_malloc(passes_to_map * sizeof(int));

	for (i = 0; i < passes_to_map; i++) {
		int startrow, subpass;
		int pass = i + first_pass_to_map;
		calculate_raw_pass_parameters(w, pass, &startrow, &subpass);
		passmap[i] = pass;
		if (first_row_of_maximal_pass >= 0)
			startrow = first_row_of_maximal_pass - startrow
			             + w->separation * w->jets;
		else
			startrow -= w->separation * w->jets;
		while (startrow < 0)
			startrow += w->separation;
		startrows[i] = startrow;
	}

	sort_by_start_row(passmap, startrows, passes_to_map);

	separations_to_distribute++;

	for (i = 0; i < first_pass_after_stagger - first_pass_to_stagger; i++) {
		int offset = first_pass_to_stagger - first_pass_to_map;
		if (startrows[i + offset] / w->separation
		      < i % separations_to_distribute)
		{
			startrows[i + offset]
			  = startrows[i + offset] % w->separation
			     + w->separation * (i % separations_to_distribute);
		}
	}

	if (first_row_of_maximal_pass >= 0) {
		for (i = 0; i < passes_to_map; i++) {
			startrows[i] = first_row_of_maximal_pass - startrows[i];
		}
	}

	sort_by_start_row(passmap, startrows, passes_to_map);
	calculate_stagger(w, passmap, startrows, passes_to_map);

	invert_map(passmap, startrows, passes_to_map, first_pass_to_map,
		   first_pass_to_map - first_pass_number);
}

static void
calculate_pass_map(stp_vars_t v,
		   cooked_t *w,		/* I - weave parameters */
                   int pageheight,	/* I - number of rows on page */
                   int firstrow,	/* I - first printed row */
                   int lastrow)		/* I - last printed row */
{
	int startrow, subpass;
	int pass = -1;

	w->first_row_printed = firstrow;
	w->last_row_printed = lastrow;

	if (pageheight <= lastrow)
		pageheight = lastrow + 1;

	do {
		calculate_raw_pass_parameters(&w->rw, ++pass,
		                              &startrow, &subpass);
	} while (startrow - w->rw.separation < firstrow);

	w->first_premapped_pass = pass;

	while (startrow < w->rw.separation * w->rw.jets
	       && startrow - w->rw.separation < pageheight
	       && startrow <= lastrow + w->rw.separation * w->rw.jets)
	{
		calculate_raw_pass_parameters(&w->rw, ++pass,
		                              &startrow, &subpass);
	}
	w->first_normal_pass = pass;

	while (startrow - w->rw.separation < pageheight
	       && startrow <= lastrow + w->rw.separation * w->rw.jets)
	{
		calculate_raw_pass_parameters(&w->rw, ++pass,
		                              &startrow, &subpass);
	}
	w->first_postmapped_pass = pass;

	while (startrow <= lastrow + w->rw.separation * w->rw.jets) {
		calculate_raw_pass_parameters(&w->rw, ++pass,
		                              &startrow, &subpass);
	}
	w->first_unused_pass = pass;

	stp_dprintf(STP_DBG_WEAVE_PARAMS, v,
		    "first premapped %d first normal %d first postmapped %d "
		    "first unused %d\n",
		    w->first_premapped_pass, w->first_normal_pass,
		    w->first_postmapped_pass, w->first_unused_pass);
	/*
	 * FIXME: make sure first_normal_pass doesn't advance beyond
	 * first_postmapped_pass, or first_postmapped_pass doesn't
	 * retreat before first_normal_pass.
	 */

	if (w->first_normal_pass > w->first_premapped_pass) {
		int spread, separations_to_distribute, normal_passes_mapped;
		separations_to_distribute = firstrow / w->rw.separation;
		spread = (separations_to_distribute + 1) * w->rw.separation;
		normal_passes_mapped = (spread + w->rw.advancebasis - 1)
		                            / w->rw.advancebasis;
		w->first_normal_pass += normal_passes_mapped;
		make_passmap(&w->rw, &w->pass_premap, &w->stagger_premap,
		             w->first_premapped_pass,
		             w->first_premapped_pass, w->first_normal_pass,
			     w->first_premapped_pass,
			     w->first_normal_pass - normal_passes_mapped,
		             -1, separations_to_distribute);
	} else {
		w->pass_premap = 0;
		w->stagger_premap = 0;
	}

	if (w->first_unused_pass > w->first_postmapped_pass) {
		int spread, separations_to_distribute, normal_passes_mapped;
		separations_to_distribute = (pageheight - lastrow - 1)
		                                     / w->rw.separation;
		spread = (separations_to_distribute + 1) * w->rw.separation;
		normal_passes_mapped = (spread + w->rw.advancebasis)
		                             / w->rw.advancebasis;
		w->first_postmapped_pass -= normal_passes_mapped;
		make_passmap(&w->rw, &w->pass_postmap, &w->stagger_postmap,
		             w->first_premapped_pass,
		             w->first_postmapped_pass, w->first_unused_pass,
			     w->first_postmapped_pass + normal_passes_mapped,
			     w->first_unused_pass,
		             pageheight - 1
		                 - w->rw.separation * (w->rw.jets - 1),
			     separations_to_distribute);
	} else {
		w->pass_postmap = 0;
		w->stagger_postmap = 0;
	}
}

static void *				/* O - weave parameter block */
initialize_weave_params(int S,		/* I - jet separation */
                        int J,		/* I - number of jets */
                        int H,		/* I - oversampling factor */
                        int firstrow,	/* I - first row number to print */
                        int lastrow,	/* I - last row number to print */
                        int pageheight,	/* I - number of rows on the whole
                        		       page, without using any
                        		       expanded margin facilities */
                        int strategy,	/* I - weave pattern variant to use */
			stp_vars_t v)
{
	cooked_t *w = stp_malloc(sizeof(cooked_t));
	if (w) {
		initialize_raw_weave(&w->rw, S, J, H, strategy, v);
		calculate_pass_map(v, w, pageheight, firstrow, lastrow);
	}
	return w;
}

void
stp_destroy_weave_params(void *vw)
{
	cooked_t *w = (cooked_t *) vw;

	if (w->pass_premap) stp_free(w->pass_premap);
	if (w->stagger_premap) stp_free(w->stagger_premap);
	if (w->pass_postmap) stp_free(w->pass_postmap);
	if (w->stagger_postmap) stp_free(w->stagger_postmap);
	stp_free(w);
}

static void
stp_calculate_row_parameters(void *vw,		/* I - weave parameters */
			     int row,		/* I - row number */
			     int subpass,	/* I - subpass */
			     int *pass,		/* O - pass containing row */
			     int *jetnum,	/* O - jet number of row */
			     int *startingrow,	/* O - phys start of pass */
			     int *ophantomrows,	/* O - missing rows at start */
			     int *ojetsused)	/* O - jets used by pass */
{
	cooked_t *w = (cooked_t *) vw;
	int raw_pass, jet, startrow, phantomrows, jetsused;
	int stagger = 0;
	int extra;

	assert(row >= w->first_row_printed, w->rw.v);
	assert(row <= w->last_row_printed, w->rw.v);
	calculate_raw_row_parameters(&w->rw,
	                             row + w->rw.separation * w->rw.jets,
	                             subpass, &raw_pass, &jet, &startrow);
	startrow -= w->rw.separation * w->rw.jets;
	jetsused = w->rw.jets;
	phantomrows = 0;

	if (raw_pass < w->first_normal_pass) {
	        assert(raw_pass >= w->first_premapped_pass, w->rw.v);
		*pass = w->pass_premap[raw_pass - w->first_premapped_pass];
		stagger = w->stagger_premap[raw_pass - w->first_premapped_pass];
	} else if (raw_pass >= w->first_postmapped_pass) {
	        assert(raw_pass >= w->first_postmapped_pass, w->rw.v);
		*pass = w->pass_postmap[raw_pass - w->first_postmapped_pass];
		stagger = w->stagger_postmap[raw_pass
		                             - w->first_postmapped_pass];
	} else {
		*pass = raw_pass - w->first_premapped_pass;
	}

	startrow += stagger * w->rw.separation;
	*jetnum = jet - stagger;
	if (stagger < 0) {
		stagger = -stagger;
		phantomrows += stagger;
	}
	jetsused -= stagger;

	extra = w->first_row_printed
	             - (startrow + w->rw.separation * phantomrows);
	if (extra > 0) {
		extra = (extra + w->rw.separation - 1) / w->rw.separation;
		jetsused -= extra;
		phantomrows += extra;
	}

	extra = startrow + w->rw.separation * (phantomrows + jetsused - 1)
	          - w->last_row_printed;
	if (extra > 0) {
		extra = (extra + w->rw.separation - 1) / w->rw.separation;
		jetsused -= extra;
	}

	*startingrow = startrow;
	*ophantomrows = phantomrows;
	*ojetsused = jetsused;
}

void
stp_fold(const unsigned char *line,
	   int single_length,
	   unsigned char *outbuf)
{
  int i;
  memset(outbuf, 0, single_length * 2);
  for (i = 0; i < single_length; i++)
    {
      unsigned char l0 = line[0];
      unsigned char l1 = line[single_length];
      if (l0 || l1)
	{
	  outbuf[0] =
	    ((l0 & (1 << 7)) >> 1) +
	    ((l0 & (1 << 6)) >> 2) +
	    ((l0 & (1 << 5)) >> 3) +
	    ((l0 & (1 << 4)) >> 4) +
	    ((l1 & (1 << 7)) >> 0) +
	    ((l1 & (1 << 6)) >> 1) +
	    ((l1 & (1 << 5)) >> 2) +
	    ((l1 & (1 << 4)) >> 3);
	  outbuf[1] =
	    ((l0 & (1 << 3)) << 3) +
	    ((l0 & (1 << 2)) << 2) +
	    ((l0 & (1 << 1)) << 1) +
	    ((l0 & (1 << 0)) << 0) +
	    ((l1 & (1 << 3)) << 4) +
	    ((l1 & (1 << 2)) << 3) +
	    ((l1 & (1 << 1)) << 2) +
	    ((l1 & (1 << 0)) << 1);
	}
      line++;
      outbuf += 2;
    }
}

static void
stp_split_2_1(int length,
		const unsigned char *in,
		unsigned char *outhi,
		unsigned char *outlo)
{
  unsigned char *outs[2];
  int i;
  int row = 0;
  int limit = length;
  outs[0] = outhi;
  outs[1] = outlo;
  memset(outs[1], 0, limit);
  for (i = 0; i < limit; i++)
    {
      unsigned char inbyte = in[i];
      outs[0][i] = 0;
      if (inbyte == 0)
	continue;
      /* For some reason gcc isn't unrolling this, even with -funroll-loops */
      if (inbyte & 1)
	{
	  outs[row][i] |= 1 & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 1))
	{
	  outs[row][i] |= (1 << 1) & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 2))
	{
	  outs[row][i] |= (1 << 2) & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 3))
	{
	  outs[row][i] |= (1 << 3) & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 4))
	{
	  outs[row][i] |= (1 << 4) & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 5))
	{
	  outs[row][i] |= (1 << 5) & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 6))
	{
	  outs[row][i] |= (1 << 6) & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 7))
	{
	  outs[row][i] |= (1 << 7) & inbyte;
	  row = row ^ 1;
	}
    }
}

static void
stp_split_2_2(int length,
		const unsigned char *in,
		unsigned char *outhi,
		unsigned char *outlo)
{
  unsigned char *outs[2];
  int i;
  unsigned row = 0;
  int limit = length * 2;
  outs[0] = outhi;
  outs[1] = outlo;
  memset(outs[1], 0, limit);
  for (i = 0; i < limit; i++)
    {
      unsigned char inbyte = in[i];
      outs[0][i] = 0;
      if (inbyte == 0)
	continue;
      /* For some reason gcc isn't unrolling this, even with -funroll-loops */
      if (inbyte & 3)
	{
	  outs[row][i] |= (3 & inbyte);
	  row = row ^ 1;
	}
      if (inbyte & (3 << 2))
	{
	  outs[row][i] |= ((3 << 2) & inbyte);
	  row = row ^ 1;
	}
      if (inbyte & (3 << 4))
	{
	  outs[row][i] |= ((3 << 4) & inbyte);
	  row = row ^ 1;
	}
      if (inbyte & (3 << 6))
	{
	  outs[row][i] |= ((3 << 6) & inbyte);
	  row = row ^ 1;
	}
    }
}

void
stp_split_2(int length,
	      int bits,
	      const unsigned char *in,
	      unsigned char *outhi,
	      unsigned char *outlo)
{
  if (bits == 2)
    stp_split_2_2(length, in, outhi, outlo);
  else
    stp_split_2_1(length, in, outhi, outlo);
}

static void
stp_split_4_1(int length,
		const unsigned char *in,
		unsigned char *out0,
		unsigned char *out1,
		unsigned char *out2,
		unsigned char *out3)
{
  unsigned char *outs[4];
  int i;
  int row = 0;
  int limit = length;
  outs[0] = out0;
  outs[1] = out1;
  outs[2] = out2;
  outs[3] = out3;
  memset(outs[1], 0, limit);
  memset(outs[2], 0, limit);
  memset(outs[3], 0, limit);
  for (i = 0; i < limit; i++)
    {
      unsigned char inbyte = in[i];
      outs[0][i] = 0;
      if (inbyte == 0)
	continue;
      /* For some reason gcc isn't unrolling this, even with -funroll-loops */
      if (inbyte & 1)
	{
	  outs[row][i] |= 1 & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 1))
	{
	  outs[row][i] |= (1 << 1) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 2))
	{
	  outs[row][i] |= (1 << 2) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 3))
	{
	  outs[row][i] |= (1 << 3) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 4))
	{
	  outs[row][i] |= (1 << 4) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 5))
	{
	  outs[row][i] |= (1 << 5) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 6))
	{
	  outs[row][i] |= (1 << 6) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 7))
	{
	  outs[row][i] |= (1 << 7) & inbyte;
	  row = (row + 1) & 3;
	}
    }
}

static void
stp_split_4_2(int length,
		const unsigned char *in,
		unsigned char *out0,
		unsigned char *out1,
		unsigned char *out2,
		unsigned char *out3)
{
  unsigned char *outs[4];
  int i;
  int row = 0;
  int limit = length * 2;
  outs[0] = out0;
  outs[1] = out1;
  outs[2] = out2;
  outs[3] = out3;
  memset(outs[1], 0, limit);
  memset(outs[2], 0, limit);
  memset(outs[3], 0, limit);
  for (i = 0; i < limit; i++)
    {
      unsigned char inbyte = in[i];
      outs[0][i] = 0;
      if (inbyte == 0)
	continue;
      /* For some reason gcc isn't unrolling this, even with -funroll-loops */
      if (inbyte & 3)
	{
	  outs[row][i] |= 3 & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (3 << 2))
	{
	  outs[row][i] |= (3 << 2) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (3 << 4))
	{
	  outs[row][i] |= (3 << 4) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (3 << 6))
	{
	  outs[row][i] |= (3 << 6) & inbyte;
	  row = (row + 1) & 3;
	}
    }
}

void
stp_split_4(int length,
	      int bits,
	      const unsigned char *in,
	      unsigned char *out0,
	      unsigned char *out1,
	      unsigned char *out2,
	      unsigned char *out3)
{
  if (bits == 2)
    stp_split_4_2(length, in, out0, out1, out2, out3);
  else
    stp_split_4_1(length, in, out0, out1, out2, out3);
}


#if __BYTE_ORDER == __LITTLE_ENDIAN
#define SH20 0
#define SH21 8
#else
#define SH20 8
#define SH21 0
#endif

static void
stp_unpack_2_1(int length,
		 const unsigned char *in,
		 unsigned char *out0,
		 unsigned char *out1)
{
  unsigned char	tempin, bit, temp0, temp1;

  if (length <= 0)
    return;
  for (bit = 128, temp0 = 0, temp1 = 0;
       length > 0;
       length --)
    {
      tempin = *in++;

      if (tempin & 128)
        temp0 |= bit;
      if (tempin & 64)
        temp1 |= bit;
      bit >>= 1;
      if (tempin & 32)
        temp0 |= bit;
      if (tempin & 16)
        temp1 |= bit;
      bit >>= 1;
      if (tempin & 8)
        temp0 |= bit;
      if (tempin & 4)
        temp1 |= bit;
      bit >>= 1;
      if (tempin & 2)
        temp0 |= bit;
      if (tempin & 1)
        temp1 |= bit;

      if (bit > 1)
        bit >>= 1;
      else
      {
        bit     = 128;
	*out0++ = temp0;
	*out1++ = temp1;

	temp0   = 0;
	temp1   = 0;
      }
    }

  if (bit < 128)
    {
      *out0++ = temp0;
      *out1++ = temp1;
    }
}

static void
stp_unpack_2_2(int length,
	       const unsigned char *in,
	       unsigned char *out0,
	       unsigned char *out1)
{
  if (length <= 0)
    return;

  for (;length;length --)
    {
      unsigned char ti0, ti1;
      ti0 = in[0];
      ti1 = in[1];

      *out0++  = (ti0 & 0xc0) << 0
	| (ti0 & 0x0c) << 2
	| (ti1 & 0xc0) >> 4
	| (ti1 & 0x0c) >> 2;
      *out1++  = (ti0 & 0x30) << 2
	| (ti0 & 0x03) << 4
	| (ti1 & 0x30) >> 2
	| (ti1 & 0x03) >> 0;
      in += 2;
    }
}

void
stp_unpack_2(int length,
	       int bits,
	       const unsigned char *in,
	       unsigned char *outlo,
	       unsigned char *outhi)
{
  if (bits == 1)
    stp_unpack_2_1(length, in, outlo, outhi);
  else
    stp_unpack_2_2(length, in, outlo, outhi);
}

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define SH40 0
#define SH41 8
#define SH42 16
#define SH43 24
#else
#define SH40 24
#define SH41 16
#define SH42 8
#define SH43 0
#endif

static void
stp_unpack_4_1(int length,
		 const unsigned char *in,
		 unsigned char *out0,
		 unsigned char *out1,
		 unsigned char *out2,
		 unsigned char *out3)
{
  unsigned char	tempin, bit, temp0, temp1, temp2, temp3;

  if (length <= 0)
    return;
  for (bit = 128, temp0 = 0, temp1 = 0, temp2 = 0, temp3 = 0;
       length > 0;
       length --)
    {
      tempin = *in++;

      if (tempin & 128)
        temp0 |= bit;
      if (tempin & 64)
        temp1 |= bit;
      if (tempin & 32)
        temp2 |= bit;
      if (tempin & 16)
        temp3 |= bit;
      bit >>= 1;
      if (tempin & 8)
        temp0 |= bit;
      if (tempin & 4)
        temp1 |= bit;
      if (tempin & 2)
        temp2 |= bit;
      if (tempin & 1)
        temp3 |= bit;

      if (bit > 1)
        bit >>= 1;
      else
      {
        bit     = 128;
	*out0++ = temp0;
	*out1++ = temp1;
	*out2++ = temp2;
	*out3++ = temp3;

	temp0   = 0;
	temp1   = 0;
	temp2   = 0;
	temp3   = 0;
      }
    }

  if (bit < 128)
    {
      *out0++ = temp0;
      *out1++ = temp1;
      *out2++ = temp2;
      *out3++ = temp3;
    }
}

static void
stp_unpack_4_2(int length,
		 const unsigned char *in,
		 unsigned char *out0,
		 unsigned char *out1,
		 unsigned char *out2,
		 unsigned char *out3)
{
  unsigned char	tempin,
		shift,
		temp0,
		temp1,
		temp2,
		temp3;

  length *= 2;

  for (shift = 0, temp0 = 0, temp1 = 0, temp2 = 0, temp3 = 0;
       length > 0;
       length --)
    {
     /*
      * Note - we can't use (tempin & N) >> (shift - M) since negative
      * right-shifts are not always implemented.
      */

      tempin = *in++;

      if (tempin & 192)
        temp0 |= (tempin & 192) >> shift;
      if (tempin & 48)
        temp1 |= ((tempin & 48) << 2) >> shift;
      if (tempin & 12)
        temp2 |= ((tempin & 12) << 4) >> shift;
      if (tempin & 3)
        temp3 |= ((tempin & 3) << 6) >> shift;

      if (shift < 6)
        shift += 2;
      else
      {
        shift   = 0;
	*out0++ = temp0;
	*out1++ = temp1;
	*out2++ = temp2;
	*out3++ = temp3;

	temp0   = 0;
	temp1   = 0;
	temp2   = 0;
	temp3   = 0;
      }
    }

  if (shift)
    {
      *out0++ = temp0;
      *out1++ = temp1;
      *out2++ = temp2;
      *out3++ = temp3;
    }
}

void
stp_unpack_4(int length,
	       int bits,
	       const unsigned char *in,
	       unsigned char *out0,
	       unsigned char *out1,
	       unsigned char *out2,
	       unsigned char *out3)
{
  if (bits == 1)
    stp_unpack_4_1(length, in, out0, out1, out2, out3);
  else
    stp_unpack_4_2(length, in, out0, out1, out2, out3);
}

static void
stp_unpack_8_1(int length,
		 const unsigned char *in,
		 unsigned char *out0,
		 unsigned char *out1,
		 unsigned char *out2,
		 unsigned char *out3,
		 unsigned char *out4,
		 unsigned char *out5,
		 unsigned char *out6,
		 unsigned char *out7)
{
  unsigned char	tempin, bit, temp0, temp1, temp2, temp3, temp4, temp5, temp6,
    temp7;

  if (length <= 0)
    return;

  for (bit = 128, temp0 = 0, temp1 = 0, temp2 = 0,
       temp3 = 0, temp4 = 0, temp5 = 0, temp6 = 0, temp7 = 0;
       length > 0;
       length --)
    {
      tempin = *in++;

      if (tempin & 128)
        temp0 |= bit;
      if (tempin & 64)
        temp1 |= bit;
      if (tempin & 32)
        temp2 |= bit;
      if (tempin & 16)
        temp3 |= bit;
      if (tempin & 8)
        temp4 |= bit;
      if (tempin & 4)
        temp5 |= bit;
      if (tempin & 2)
        temp6 |= bit;
      if (tempin & 1)
        temp7 |= bit;

      if (bit > 1)
        bit >>= 1;
      else
      {
        bit     = 128;
	*out0++ = temp0;
	*out1++ = temp1;
	*out2++ = temp2;
	*out3++ = temp3;
	*out4++ = temp4;
	*out5++ = temp5;
	*out6++ = temp6;
	*out7++ = temp7;

	temp0   = 0;
	temp1   = 0;
	temp2   = 0;
	temp3   = 0;
	temp4   = 0;
	temp5   = 0;
	temp6   = 0;
	temp7   = 0;
      }
    }

  if (bit < 128)
    {
      *out0++ = temp0;
      *out1++ = temp1;
      *out2++ = temp2;
      *out3++ = temp3;
      *out4++ = temp4;
      *out5++ = temp5;
      *out6++ = temp6;
      *out7++ = temp7;
    }
}

static void
stp_unpack_8_2(int length,
		 const unsigned char *in,
		 unsigned char *out0,
		 unsigned char *out1,
		 unsigned char *out2,
		 unsigned char *out3,
		 unsigned char *out4,
		 unsigned char *out5,
		 unsigned char *out6,
		 unsigned char *out7)
{
  unsigned char	tempin,
		shift,
		temp0,
		temp1,
		temp2,
		temp3,
		temp4,
		temp5,
		temp6,
		temp7;


  for (shift = 0, temp0 = 0, temp1 = 0,
       temp2 = 0, temp3 = 0, temp4 = 0, temp5 = 0, temp6 = 0, temp7 = 0;
       length > 0;
       length --)
    {
     /*
      * Note - we can't use (tempin & N) >> (shift - M) since negative
      * right-shifts are not always implemented.
      */

      tempin = *in++;

      if (tempin & 192)
        temp0 |= (tempin & 192) >> shift;
      if (tempin & 48)
        temp1 |= ((tempin & 48) << 2) >> shift;
      if (tempin & 12)
        temp2 |= ((tempin & 12) << 4) >> shift;
      if (tempin & 3)
        temp3 |= ((tempin & 3) << 6) >> shift;

      tempin = *in++;

      if (tempin & 192)
        temp4 |= (tempin & 192) >> shift;
      if (tempin & 48)
        temp5 |= ((tempin & 48) << 2) >> shift;
      if (tempin & 12)
        temp6 |= ((tempin & 12) << 4) >> shift;
      if (tempin & 3)
        temp7 |= ((tempin & 3) << 6) >> shift;

      if (shift < 6)
        shift += 2;
      else
      {
        shift   = 0;
	*out0++ = temp0;
	*out1++ = temp1;
	*out2++ = temp2;
	*out3++ = temp3;
	*out4++ = temp4;
	*out5++ = temp5;
	*out6++ = temp6;
	*out7++ = temp7;

	temp0   = 0;
	temp1   = 0;
	temp2   = 0;
	temp3   = 0;
	temp4   = 0;
	temp5   = 0;
	temp6   = 0;
	temp7   = 0;
      }
    }

  if (shift)
    {
      *out0++ = temp0;
      *out1++ = temp1;
      *out2++ = temp2;
      *out3++ = temp3;
      *out4++ = temp4;
      *out5++ = temp5;
      *out6++ = temp6;
      *out7++ = temp7;
    }
}

void
stp_unpack_8(int length,
	       int bits,
	       const unsigned char *in,
	       unsigned char *out0,
	       unsigned char *out1,
	       unsigned char *out2,
	       unsigned char *out3,
	       unsigned char *out4,
	       unsigned char *out5,
	       unsigned char *out6,
	       unsigned char *out7)
{
  if (bits == 1)
    stp_unpack_8_1(length, in, out0, out1, out2, out3,
		     out4, out5, out6, out7);
  else
    stp_unpack_8_2(length, in, out0, out1, out2, out3,
		     out4, out5, out6, out7);
}

int
stp_pack_uncompressed(const unsigned char *line,
		      int length,
		      unsigned char *comp_buf,
		      unsigned char **comp_ptr)
{
  int i;
  memcpy(comp_buf, line, length);
  *comp_ptr = comp_buf + length;
  for (i = 0; i < length; i++)
    if (line[i])
      return 1;
  return 0;
}

int
stp_pack_tiff(const unsigned char *line,
	      int length,
	      unsigned char *comp_buf,
	      unsigned char **comp_ptr)
{
  const unsigned char *start;		/* Start of compressed data */
  unsigned char repeat;			/* Repeating char */
  int count;			/* Count of compressed bytes */
  int tcount;			/* Temporary count < 128 */
  int active = 0;		/* Have we found data? */
  register const unsigned char *xline = line;
  register int xlength = length;

  /*
   * Compress using TIFF "packbits" run-length encoding...
   */

  (*comp_ptr) = comp_buf;

  while (xlength > 0)
    {
      /*
       * Get a run of non-repeated chars...
       */

      start  = xline;
      xline   += 2;
      xlength -= 2;

      while (xlength > 0 && (xline[-2] != xline[-1] || xline[-1] != xline[0]))
	{
	  if (! active && (xline[-2] || xline[-1] || xline[0]))
	    active = 1;
	  xline ++;
	  xlength --;
	}

      xline   -= 2;
      xlength += 2;

      /*
       * Output the non-repeated sequences (max 128 at a time).
       */

      count = xline - start;
      while (count > 0)
	{
	  tcount = count > 128 ? 128 : count;

	  (*comp_ptr)[0] = tcount - 1;
	  memcpy((*comp_ptr) + 1, start, tcount);

	  (*comp_ptr) += tcount + 1;
	  start    += tcount;
	  count    -= tcount;
	}

      if (xlength <= 0)
	break;

      /*
       * Find the repeated sequences...
       */

      start  = xline;
      repeat = xline[0];
      if (repeat)
	active = 1;

      xline ++;
      xlength --;

      if (xlength > 0)
	{
	  int ylength = xlength;
	  while (ylength && *xline == repeat)
	    {
	      xline ++;
	      ylength --;
	    }
	  xlength = ylength;
	}

      /*
       * Output the repeated sequences (max 128 at a time).
       */

      count = xline - start;
      while (count > 0)
	{
	  tcount = count > 128 ? 128 : count;

	  (*comp_ptr)[0] = 1 - tcount;
	  (*comp_ptr)[1] = repeat;

	  (*comp_ptr) += 2;
	  count    -= tcount;
	}
    }
  return active;
}

/*
 * "Soft" weave
 *
 * The Epson Stylus Color/Photo printers don't have memory to print
 * using all of the nozzles in the print head.  For example, the Stylus Photo
 * 700/EX has 32 nozzles.  At 720 dpi, with an 8" wide image, a single line
 * requires (8 * 720 * 6 / 8) bytes, or 4320 bytes (because the Stylus Photo
 * printers have 6 ink colors).  To use 32 nozzles would require 138240 bytes.
 * It's actually worse than that, though, because the nozzles are spaced 8
 * rows apart.  Therefore, in order to store enough data to permit sending the
 * page as a simple raster, the printer would require enough memory to store
 * 256 rows, or 1105920 bytes.  Considering that the Photo EX can print
 * 11" wide, we're looking at more like 1.5 MB.  In fact, these printers are
 * capable of 1440 dpi horizontal resolution.  This would require 3 MB.  The
 * printers actually have 64K-256K.
 *
 * With the newer (740/750 and later) printers it's even worse, since these
 * printers support multiple dot sizes.  But that's neither here nor there.
 *
 * Older Epson printers had a mode called MicroWeave (tm).  In this mode, the
 * host fed the printer individual rows of dots, and the printer bundled them
 * up and sent them to the print head in the correct order to achieve high
 * quality.  This MicroWeave mode still works in new printers, but the
 * implementation is very minimal: the printer uses exactly one nozzle of
 * each color (the first one).  This makes printing extremely slow (more than
 * 30 minutes for one 8.5x11" page), although the quality is extremely high
 * with no visible banding whatsoever.  It's not good for the print head,
 * though, since no ink is flowing through the other nozzles.  This leads to
 * drying of ink and possible permanent damage to the print head.
 *
 * By the way, although the Epson manual says that microweave mode should be
 * used at 720 dpi, 360 dpi continues to work in much the same way.  At 360
 * dpi, data is fed to the printer one row at a time on all Epson printers.
 * The pattern that the printer uses to print is very prone to banding.
 * However, 360 dpi is inherently a low quality mode; if you're using it,
 * presumably you don't much care about quality.
 *
 * Printers from roughly the Stylus Color 600 and later do not have the
 * capability to do MicroWeave correctly.  Instead, the host must arrange
 * the output in the order that it will be sent to the print head.  This
 * is a very complex process; the jets in the print head are spaced more
 * than one row (1/720") apart, so we can't simply send consecutive rows
 * of dots to the printer.  Instead, we have to pass e. g. the first, ninth,
 * 17th, 25th... rows in order for them to print in the correct position on
 * the paper.  This interleaving process is called "soft" weaving.
 *
 * This decision was probably made to save money on memory in the printer.
 * It certainly makes the driver code far more complicated than it would
 * be if the printer could arrange the output.  Is that a bad thing?
 * Usually this takes far less CPU time than the dithering process, and it
 * does allow us more control over the printing process, e. g. to reduce
 * banding.  Conceivably, we could even use this ability to map out bad
 * jets.
 *
 * Interestingly, apparently the Windows (and presumably Macintosh) drivers
 * for most or all Epson printers still list a "microweave" mode.
 * Experiments have demonstrated that this does not in fact use the
 * "microweave" mode of the printer.  Possibly it does nothing, or it uses
 * a different weave pattern from what the non-"microweave" mode does.
 * This is unnecessarily confusing.
 *
 * What makes this interesting is that there are many different ways of
 * of accomplishing this goal.  The naive way would be to divide the image
 * up into groups of 256 rows, and print all the mod8=0 rows in the first pass,
 * mod8=1 rows in the second, and so forth.  The problem with this approach
 * is that the individual ink jets are not perfectly uniform; some emit
 * slightly bigger or smaller drops than others.  Since each group of 8
 * adjacent rows is printed with the same nozzle, that means that there will
 * be distinct streaks of lighter and darker bands within the image (8 rows
 * is 1/90", which is visible; 1/720" is not).  Possibly worse is that these
 * patterns will repeat every 256 rows.  This creates banding patterns that
 * are about 1/3" wide.
 *
 * So we have to do something to break up this patterning.
 *
 * Epson does not publish the weaving algorithms that they use in their
 * bundled drivers.  Indeed, their developer web site
 * (http://www.ercipd.com/isv/edr_docs.htm) does not even describe how to
 * do this weaving at all; it says that the only way to achieve 720 dpi
 * is to use MicroWeave.  It does note (correctly) that 1440 dpi horizontal
 * can only be achieved by the driver (i. e. in software).  The manual
 * actually makes it fairly clear how to do this (it requires two passes
 * with horizontal head movement between passes), and it is presumably
 * possible to do this with MicroWeave.
 *
 * The information about how to do this is apparently available under NDA.
 * It's actually easy enough to reverse engineer what's inside a print file
 * with a simple Perl script.  There are presumably other printer commands
 * that are not documented and may not be as easy to reverse engineer.
 *
 * I considered a few algorithms to perform the weave.  The first one I
 * devised let me use only (jets - distance_between_jets + 1) nozzles, or
 * 25.  This is OK in principle, but it's slower than using all nozzles.
 * By playing around with it some more, I came up with an algorithm that
 * lets me use all of the nozzles, except near the top and bottom of the
 * page.
 *
 * This still produces some banding, though.  Even better quality can be
 * achieved by using multiple nozzles on the same line.  How do we do this?
 * In 1440x720 mode, we're printing two output lines at the same vertical
 * position.  However, if we want four passes, we have to effectively print
 * each line twice.  Actually doing this would increase the density, so
 * what we do is print half the dots on each pass.  This produces near-perfect
 * output, and it's far faster than using (pseudo) "MicroWeave".
 *
 * The current algorithm is not completely general.  The number of passes
 * is limited to (nozzles / gap).  On the Photo EX class printers, that limits
 * it to 4 -- 32 nozzles, an inter-nozzle gap of 8 lines.  Furthermore, there
 * are a number of routines that are only coded up to 8 passes.  Fortunately,
 * this is enough passes to get rid of most banding.  What's left is a very
 * fine pattern that is sometimes described as "corduroy", since the pattern
 * looks like that kind of fabric.
 *
 * Newer printers (those that support variable dot sizes, such as the 740,
 * 1200, etc.) have an additional complication: when used in softweave mode,
 * they operate at 360 dpi horizontal resolution.  This requires FOUR passes
 * to achieve 1440x720 dpi.  Thus, to enable us to break up each row
 * into separate sub-rows, we have to actually print each row eight times.
 * Fortunately, all such printers have 48 nozzles and a gap of 6 rows,
 * except for the high-speed 900, which uses 96 nozzles and a gap of 2 rows.
 *
 * I cannot let this entirely pass without commenting on the Stylus Color 440.
 * This is a very low-end printer with 21 (!) nozzles and a separation of 8.
 * The weave routine works correctly with single-pass printing, which is enough
 * to minimally achieve 720 dpi output (it's physically a 720 dpi printer).
 * However, the routine does not work correctly at more than one pass per row.
 * Therefore, this printer bands badly.
 *
 * Yet another complication is how to get near the top and bottom of the page.
 * This algorithm lets us print to within one head width of the top of the
 * page, and a bit more than one head width from the bottom.  That leaves a
 * lot of blank space.  Doing the weave properly outside of this region is
 * increasingly difficult as we get closer to the edge of the paper; in the
 * interior region, any nozzle can print any line, but near the top and
 * bottom edges, only some nozzles can print.  We've handled this for now by
 * using the naive way mentioned above near the borders, and switching over
 * to the high quality method in the interior.  Unfortunately, this means
 * that the quality is quite visibly degraded near the top and bottom of the
 * page.  Algorithms that degrade more gracefully are more complicated.
 * Epson does not advertise that the printers can print at the very top of the
 * page, although in practice most or all of them can.  I suspect that the
 * quality that can be achieved very close to the top is poor enough that
 * Epson does not want to allow printing there.  That is a valid decision,
 * although we have taken another approach.
 *
 * To compute the weave information, we need to start with the following
 * information:
 *
 * 1) The number of jets the print head has for each color;
 *
 * 2) The separation in rows between the jets;
 *
 * 3) The horizontal resolution of the printer;
 *
 * 4) The desired horizontal resolution of the output;
 *
 * 5) The desired extra passes to reduce banding.
 *
 * As discussed above, each row is actually printed in one or more passes
 * of the print head; we refer to these as subpasses.  For example, if we're
 * printing at 1440(h)x720(v) on a printer with true horizontal resolution of
 * 360 dpi, and we wish to print each line twice with different nozzles
 * to reduce banding, we need to use 8 subpasses.  The dither routine
 * will feed us a complete row of bits for each color; we have to split that
 * up, first by round robining the bits to ensure that they get printed at
 * the right micro-position, and then to split up the bits that are actually
 * turned on into two equal chunks to reduce banding.
 *
 * Given the above information, and the desired row index and subpass (which
 * together form a line number), we can compute:
 *
 * 1) Which pass this line belongs to.  Passes are numbered consecutively,
 *    and each pass must logically (see #3 below) start at no smaller a row
 *    number than the previous pass, as the printer cannot advance by a
 *    negative amount.
 *
 * 2) Which jet will print this line.
 *
 * 3) The "logical" first line of this pass.  That is, what line would be
 *    printed by jet 0 in this pass.  This number may be less than zero.
 *    If it is, there are ghost lines that don't actually contain any data.
 *    The difference between the logical first line of this pass and the
 *    logical first line of the preceding pass tells us how many lines must
 *    be advanced.
 *
 * 4) The "physical" first line of this pass.  That is, the first line index
 *    that is actually printed in this pass.  This information lets us know
 *    when we must prepare this pass.
 *
 * 5) The last line of this pass.  This lets us know when we must actually
 *    send this pass to the printer.
 *
 * 6) The number of ghost rows this pass contains.  We must still send the
 *    ghost data to the printer, so this lets us know how much data we must
 *    fill in prior to the start of the pass.
 *
 * The bookkeeping to keep track of all this stuff is quite hairy, and needs
 * to be documented separately.
 *
 * The routine initialize_weave calculates the basic parameters, given
 * the number of jets and separation between jets, in rows.
 *
 * -- Robert Krawitz <rlk@alum.mit.edu) November 3, 1999
 */

static stp_lineoff_t *
allocate_lineoff(int count, int ncolors)
{
  int i;
  stp_lineoff_t *retval = stp_malloc(count * sizeof(stp_lineoff_t));
  for (i = 0; i < count; i++)
    {
      retval[i].ncolors = ncolors;
      retval[i].v = stp_zalloc(ncolors * sizeof(unsigned long));
    }
  return (retval);
}

static stp_lineactive_t *
allocate_lineactive(int count, int ncolors)
{
  int i;
  stp_lineactive_t *retval = stp_malloc(count * sizeof(stp_lineactive_t));
  for (i = 0; i < count; i++)
    {
      retval[i].ncolors = ncolors;
      retval[i].v = stp_zalloc(ncolors * sizeof(char));
    }
  return (retval);
}

static stp_linecount_t *
allocate_linecount(int count, int ncolors)
{
  int i;
  stp_linecount_t *retval = stp_malloc(count * sizeof(stp_linecount_t));
  for (i = 0; i < count; i++)
    {
      retval[i].ncolors = ncolors;
      retval[i].v = stp_zalloc(ncolors * sizeof(int));
    }
  return (retval);
}

static stp_linebufs_t *
allocate_linebuf(int count, int ncolors)
{
  int i;
  stp_linebufs_t *retval = stp_malloc(count * sizeof(stp_linebufs_t));
  for (i = 0; i < count; i++)
    {
      retval[i].ncolors = ncolors;
      retval[i].v = stp_zalloc(ncolors * sizeof(unsigned char *));
    }
  return (retval);
}

/*
 * Initialize the weave parameters
 *
 * Rules:
 *
 * 1) Currently, osample * v_subpasses * v_subsample <= 8, and no one
 *    of these variables may exceed 4.
 *
 * 2) first_line >= 0
 *
 * 3) line_height < physlines
 *
 * 4) phys_lines >= 2 * jets * sep
 */
void *
stp_initialize_weave(int jets,	/* Width of print head */
		     int sep,	/* Separation in rows between jets */
		     int osample,	/* Horizontal oversample */
		     int v_subpasses, /* Vertical passes */
		     int v_subsample, /* Vertical oversampling */
		     int ncolors,
		     int width,	/* bits/pixel */
		     int linewidth,	/* Width of a line, in pixels */
		     int lineheight, /* Number of lines that will be printed */
		     int first_line, /* First line that will be printed on page */
		     int phys_lines, /* Total height of the page in rows */
		     int weave_strategy, /* Which weaving pattern to use */
		     int *head_offset,
		     stp_vars_t v,
		     void (*flushfunc)(stp_softweave_t *sw, int passno,
				       int model, int width, int hoffset,
				       int ydpi, int xdpi, int physical_xdpi,
				       int vertical_subpass),
		     void (*fill_start)(stp_softweave_t *sw, int row,
					int subpass, int width,
					int missingstartrows,
					int vertical_subpass),
		     int (*pack)(const unsigned char *in, int bytes,
				 unsigned char *out, unsigned char **optr),
		     int (*compute_linewidth)(const stp_softweave_t *sw,
					      int n))
{
  int i;
  int last_line, maxHeadOffset;
  stp_softweave_t *sw = stp_zalloc(sizeof (stp_softweave_t));

  if (jets < 1)
    jets = 1;
  if (jets == 1 || sep < 1)
    sep = 1;
  if (v_subpasses < 1)
    v_subpasses = 1;
  if (v_subsample < 1)
    v_subsample = 1;

  sw->v = v;
  sw->separation = sep;
  sw->jets = jets;
  sw->horizontal_weave = osample;
  sw->oversample = osample * v_subpasses * v_subsample;
  if (sw->oversample > jets)
    {
      int found = 0;
      for (i = 2; i <= osample; i++)
	{
	  if ((osample % i == 0) && (sw->oversample / i <= jets))
	    {
	      sw->repeat_count = i;
	      osample /= i;
	      found = 1;
	      break;
	    }
	}
      if (!found)
	{
	  stp_eprintf(v, "Weave error: oversample (%d) > jets (%d)\n",
		      sw->oversample, jets);
	  stp_free(sw);
	  return 0;
	}
    }
  else
    sw->repeat_count = 1;

  sw->vertical_oversample = v_subsample;
  sw->vertical_subpasses = v_subpasses;
  sw->oversample = osample * v_subpasses * v_subsample;
  sw->firstline = first_line;
  sw->lineno = first_line;
  sw->flushfunc = flushfunc;

  if (sw->oversample > jets)
    {
      stp_eprintf(v, "Weave error: oversample (%d) > jets (%d)\n",
		  sw->oversample, jets);
      stp_free(sw);
      return 0;
    }

  /*
   * setup printhead offsets.
   * for monochrome (bw) printing, the offsets are 0.
   */
  sw->head_offset = stp_zalloc(ncolors * sizeof(int));
  if (ncolors > 1)
    for(i = 0; i < ncolors; i++)
      sw->head_offset[i] = head_offset[i];

  maxHeadOffset = 0;
  for (i = 0; i < ncolors; i++)
    if (sw->head_offset[i] > maxHeadOffset)
      maxHeadOffset = sw->head_offset[i];

  sw->virtual_jets = sw->jets;
  if (maxHeadOffset > 0)
    sw->virtual_jets += (maxHeadOffset + sw->separation - 1) / sw->separation;
  last_line = first_line + lineheight - 1 + maxHeadOffset;

  sw->weaveparm = initialize_weave_params(sw->separation, sw->jets,
                                          sw->oversample, first_line, last_line,
                                          phys_lines, weave_strategy, v);
  /*
   * The value of vmod limits how many passes may be unfinished at a time.
   * If pass x is not yet printed, pass x+vmod cannot be started.
   *
   * The multiplier of 2: 1 for the normal passes, 1 for the special passes
   * at the start or end.
   */
  sw->vmod = 2 * sw->separation * sw->oversample * sw->repeat_count;
  if (sw->virtual_jets > sw->jets)
    sw->vmod *= (sw->virtual_jets + sw->jets - 1) / sw->jets;

  sw->bitwidth = width;
  sw->last_pass_offset = 0;
  sw->last_pass = -1;
  sw->current_vertical_subpass = 0;
  sw->ncolors = ncolors;
  sw->linewidth = linewidth;
  sw->vertical_height = lineheight;
  sw->lineoffsets = allocate_lineoff(sw->vmod, ncolors);
  sw->lineactive = allocate_lineactive(sw->vmod, ncolors);
  sw->linebases = allocate_linebuf(sw->vmod, ncolors);
  sw->passes = stp_zalloc(sw->vmod * sizeof(stp_pass_t));
  sw->linecounts = allocate_linecount(sw->vmod, ncolors);
  sw->rcache = -2;
  sw->vcache = -2;
  sw->fill_start = fill_start;
  sw->compute_linewidth = compute_linewidth;
  sw->pack = pack;
  sw->horizontal_width =
    (sw->compute_linewidth)(sw, ((sw->linewidth + sw->horizontal_weave - 1) /
				 sw->horizontal_weave));
  sw->horizontal_width = ((sw->horizontal_width + 7) / 8);

  for (i = 0; i < sw->vmod; i++)
    {
      int j;
      sw->passes[i].pass = -1;
      for (j = 0; j < sw->ncolors; j++)
	{
	  sw->linebases[i].v[j] = NULL;
	}
    }
  return (void *) sw;
}

void
stp_destroy_weave(void *vsw)
{
  int i, j;
  stp_softweave_t *sw = (stp_softweave_t *) vsw;
  stp_free(sw->passes);
  if (sw->fold_buf)
    stp_free(sw->fold_buf);
  if (sw->comp_buf)
    stp_free(sw->comp_buf);
  for (i = 0; i < MAX_WEAVE; i++)
    if (sw->s[i])
      stp_free(sw->s[i]);
  for (i = 0; i < sw->vmod; i++)
    {
      for (j = 0; j < sw->ncolors; j++)
	{
	  if (sw->linebases[i].v[j])
	    stp_free(sw->linebases[i].v[j]);
	}
      stp_free(sw->linecounts[i].v);
      stp_free(sw->linebases[i].v);
      stp_free(sw->lineactive[i].v);
      stp_free(sw->lineoffsets[i].v);
    }
  stp_free(sw->linecounts);
  stp_free(sw->lineactive);
  stp_free(sw->lineoffsets);
  stp_free(sw->linebases);
  stp_free(sw->head_offset);
  stp_destroy_weave_params(sw->weaveparm);
  stp_free(vsw);
}

static void
weave_parameters_by_row(const stp_softweave_t *sw, int row,
			int vertical_subpass, stp_weave_t *w)
{
  int jetsused;
  int sub_repeat_count = vertical_subpass % sw->repeat_count;
  /*
   * Conceptually, this does not modify the softweave state.  We cache
   * the results, but this cache is considered hidden.
   */
  stp_softweave_t *wsw = (stp_softweave_t *)sw;
  vertical_subpass /= sw->repeat_count;

  if (sw->rcache == row && sw->vcache == vertical_subpass)
    {
      memcpy(w, &sw->wcache, sizeof(stp_weave_t));
      w->pass = (w->pass * sw->repeat_count) + sub_repeat_count;
      return;
    }
  wsw->rcache = row;
  wsw->vcache = vertical_subpass;

  w->row = row;
  stp_calculate_row_parameters(sw->weaveparm, row, vertical_subpass,
			       &w->pass, &w->jet, &w->logicalpassstart,
			       &w->missingstartrows, &jetsused);

  w->physpassstart = w->logicalpassstart + sw->separation * w->missingstartrows;
  w->physpassend = w->physpassstart + sw->separation * (jetsused - 1);

  memcpy(&(wsw->wcache), w, sizeof(stp_weave_t));
  w->pass = (w->pass * sw->repeat_count) + sub_repeat_count;
  stp_dprintf(STP_DBG_WEAVE_PARAMS, sw->v, "row %d, jet %d of pass %d "
	      "(pos %d, start %d, end %d, missing rows %d)\n",
	      w->row, w->jet, w->pass, w->logicalpassstart, w->physpassstart,
	      w->physpassend, w->missingstartrows);
}

void
stp_weave_parameters_by_row(const stp_softweave_t *sw, int row,
			int vertical_subpass, stp_weave_t *w)
{
  weave_parameters_by_row(sw, row, vertical_subpass, w);
}


static stp_lineoff_t *
stp_get_lineoffsets(const stp_softweave_t *sw, int row, int subpass, int offset)
{
  stp_weave_t w;
  weave_parameters_by_row(sw, row + offset, subpass, &w);
  return &(sw->lineoffsets[w.pass % sw->vmod]);
}

static stp_lineactive_t *
stp_get_lineactive(const stp_softweave_t *sw, int row, int subpass, int offset)
{
  stp_weave_t w;
  weave_parameters_by_row(sw, row + offset, subpass, &w);
  return &(sw->lineactive[w.pass % sw->vmod]);
}

static stp_linecount_t *
stp_get_linecount(const stp_softweave_t *sw, int row, int subpass, int offset)
{
  stp_weave_t w;
  weave_parameters_by_row(sw, row + offset, subpass, &w);
  return &(sw->linecounts[w.pass % sw->vmod]);
}

static const stp_linebufs_t *
stp_get_linebases(const stp_softweave_t *sw, int row, int subpass, int offset)
{
  stp_weave_t w;
  weave_parameters_by_row(sw, row + offset, subpass, &w);
  return &(sw->linebases[w.pass % sw->vmod]);
}

static stp_pass_t *
stp_get_pass_by_row(const stp_softweave_t *sw, int row, int subpass,int offset)
{
  stp_weave_t w;
  weave_parameters_by_row(sw, row + offset, subpass, &w);
  return &(sw->passes[w.pass % sw->vmod]);
}

stp_lineoff_t *
stp_get_lineoffsets_by_pass(const stp_softweave_t *sw, int pass)
{
  return &(sw->lineoffsets[pass % sw->vmod]);
}

stp_lineactive_t *
stp_get_lineactive_by_pass(const stp_softweave_t *sw, int pass)
{
  return &(sw->lineactive[pass % sw->vmod]);
}

stp_linecount_t *
stp_get_linecount_by_pass(const stp_softweave_t *sw, int pass)
{
  return &(sw->linecounts[pass % sw->vmod]);
}

const stp_linebufs_t *
stp_get_linebases_by_pass(const stp_softweave_t *sw, int pass)
{
  return &(sw->linebases[pass % sw->vmod]);
}

stp_pass_t *
stp_get_pass_by_pass(const stp_softweave_t *sw, int pass)
{
  return &(sw->passes[pass % sw->vmod]);
}

static void
check_linebases(stp_softweave_t *sw, int row, int cpass, int head_offset,
		int color)
{
  stp_linebufs_t *bufs =
    (stp_linebufs_t *) stp_get_linebases(sw, row, cpass, head_offset);
  if (!(bufs[0].v[color]))
    bufs[0].v[color] =
      stp_zalloc (sw->virtual_jets * sw->bitwidth * sw->horizontal_width);
}

/*
 * If there are phantom rows at the beginning of a pass, fill them in so
 * that the printer knows exactly what it doesn't have to print.  We're
 * using RLE compression here.  Each line must be specified independently,
 * so we have to compute how many full blocks (groups of 128 bytes, or 1024
 * "off" pixels) and how much leftover is needed.  Note that we can only
 * RLE-encode groups of 2 or more bytes; single bytes must be specified
 * with a count of 1.
 */

void
stp_fill_tiff(stp_softweave_t *sw, int row, int subpass,
	      int width, int missingstartrows, int color)
{
  stp_lineoff_t *lineoffs;
  stp_linecount_t *linecount;
  const stp_linebufs_t *bufs;
  int i = 0;
  int k = 0;

  width = sw->bitwidth * width * 8;
  for (k = 0; k < missingstartrows; k++)
    {
      int bytes_to_fill = width;
      int full_blocks = bytes_to_fill / (128 * 8);
      int leftover = (7 + (bytes_to_fill % (128 * 8))) / 8;
      int l = 0;
      bufs = stp_get_linebases(sw, row, subpass, sw->head_offset[color]);

      while (l < full_blocks)
	{
	  (bufs[0].v[color][2 * i]) = 129;
	  (bufs[0].v[color][2 * i + 1]) = 0;
	  i++;
	  l++;
	}
      if (leftover == 1)
	{
	  (bufs[0].v[color][2 * i]) = 1;
	  (bufs[0].v[color][2 * i + 1]) = 0;
	  i++;
	}
      else if (leftover > 0)
	{
	  (bufs[0].v[color][2 * i]) = 257 - leftover;
	  (bufs[0].v[color][2 * i + 1]) = 0;
	  i++;
	}
    }

  lineoffs = stp_get_lineoffsets(sw, row, subpass, sw->head_offset[color]);
  linecount = stp_get_linecount(sw, row, subpass, sw->head_offset[color]);
  lineoffs[0].v[color] = 2 * i;
  linecount[0].v[color] = missingstartrows;
}

void
stp_fill_uncompressed(stp_softweave_t *sw, int row, int subpass,
		      int width, int missingstartrows, int color)
{
  stp_lineoff_t *lineoffs;
  stp_linecount_t *linecount;
  const stp_linebufs_t *bufs;

  bufs = stp_get_linebases(sw, row, subpass, sw->head_offset[color]);
  lineoffs = stp_get_lineoffsets(sw, row, subpass, sw->head_offset[color]);
  linecount = stp_get_linecount(sw, row, subpass, sw->head_offset[color]);
  width *= sw->bitwidth * missingstartrows;
  memset(bufs[0].v[color], 0, width);
  lineoffs[0].v[color] = width;
  linecount[0].v[color] = missingstartrows;
}

int
stp_compute_tiff_linewidth(const stp_softweave_t *sw, int n)
{
  /*
   * It's possible for the "compression" to actually expand the line by
   * roughly one part in 128.
   */
  return ((n + 128 + 7) * 129 / 128);
}

int
stp_compute_uncompressed_linewidth(const stp_softweave_t *sw, int n)
{
  return (8 * ((n + 7) / 8));
}

static void
initialize_row(stp_softweave_t *sw, int row, int width,
	       unsigned char *const cols[])
{
  stp_weave_t w;
  int i, j, jj;
  stp_pass_t *pass;

  for (i = 0; i < sw->oversample; i++)
    {
      for (j = 0; j < sw->ncolors; j++)
	{
	  if (cols[j])
	    {
	      stp_lineoff_t *lineoffs =
		stp_get_lineoffsets(sw, row, i, sw->head_offset[j]);
	      stp_lineactive_t *lineactive =
		stp_get_lineactive(sw, row, i, sw->head_offset[j]);
	      stp_linecount_t *linecount =
		stp_get_linecount(sw, row, i, sw->head_offset[j]);
	      check_linebases(sw, row, i, sw->head_offset[j], j);
	      weave_parameters_by_row(sw, row+sw->head_offset[j], i, &w);
	      pass = stp_get_pass_by_row(sw, row, i, sw->head_offset[j]);

	      /* initialize pass if not initialized yet */
	      if (pass->pass < 0)
		{
		  pass->pass = w.pass;
		  pass->missingstartrows = w.missingstartrows;
		  pass->logicalpassstart = w.logicalpassstart;
		  pass->physpassstart = w.physpassstart;
		  pass->physpassend = w.physpassend;
		  pass->subpass = i;

		  for(jj=0; jj<sw->ncolors; jj++)
		    {
		      if (lineoffs[0].v[jj] != 0)
			stp_eprintf(sw->v,
				    "WARNING: pass %d subpass %d row %d: "
				    "lineoffs %ld should be zero!\n",
				    w.pass, i, row, lineoffs[0].v[jj]);
		      lineoffs[0].v[jj] = 0;
		      lineactive[0].v[jj] = 0;
		      if (linecount[0].v[jj] != 0)
			stp_eprintf(sw->v,
				    "WARNING: pass %d subpass %d row %d: "
				    "linecount %d should be zero!\n",
				    w.pass, i, row, linecount[0].v[jj]);
		      linecount[0].v[jj] = 0;
		    }
		}

	      if((linecount[0].v[j] == 0) && (w.jet > 0))
		{
		  (sw->fill_start)(sw, row, i, width, w.jet, j);
		}
	    }
	}
    }
}

static void
add_to_row(stp_softweave_t *sw, int row, unsigned char *buf, size_t nbytes,
	   int color, int setactive, stp_lineoff_t *lineoffs,
	   stp_lineactive_t *lineactive, stp_linecount_t *linecount,
	   const stp_linebufs_t *bufs)
{
  size_t place = lineoffs[0].v[color];
  size_t count = linecount[0].v[color];
  if (place + nbytes > sw->virtual_jets * sw->bitwidth * sw->horizontal_width)
    {
      stp_eprintf(sw->v, "Buffer overflow: limit %d, actual %d, count %d\n",
		  sw->virtual_jets * sw->bitwidth * sw->horizontal_width,
		  place + nbytes, count);
      exit(1);
    }
  memcpy(bufs[0].v[color] + lineoffs[0].v[color], buf, nbytes);
  lineoffs[0].v[color] += nbytes;
  if (setactive)
    lineactive[0].v[color] = 1;
}

static void
stp_flush(void *vsw, int model, int width, int hoffset,
	    int ydpi, int xdpi, int physical_xdpi)
{
  stp_softweave_t *sw = (stp_softweave_t *) vsw;
  while (1)
    {
      stp_pass_t *pass = stp_get_pass_by_pass(sw, sw->last_pass + 1);
      /*
       * This ought to be   pass->physpassend >  sw->lineno
       * but that causes rubbish to be output for some reason.
       */
      if (pass->pass < 0 || pass->physpassend >= sw->lineno)
	return;
      (sw->flushfunc)(sw, pass->pass, model, width, hoffset, ydpi, xdpi,
		      physical_xdpi, pass->subpass);
    }
}

void
stp_flush_all(void *vsw, int model, int width, int hoffset,
	      int ydpi, int xdpi, int physical_xdpi)
{
  stp_softweave_t *sw = (stp_softweave_t *) vsw;
  while (1)
    {
      stp_pass_t *pass = stp_get_pass_by_pass(sw, sw->last_pass + 1);
      /*
       * This ought to be   pass->physpassend >  sw->lineno
       * but that causes rubbish to be output for some reason.
       */
      if (pass->pass < 0)
	return;
      (sw->flushfunc)(sw, pass->pass, model, width, hoffset, ydpi, xdpi,
		      physical_xdpi, pass->subpass);
    }
}

static void
finalize_row(stp_softweave_t *sw, int row, int model, int width,
	     int hoffset, int ydpi, int xdpi, int physical_xdpi)
{
  int i,j;
  stp_dprintf(STP_DBG_ROWS, sw->v, "Finalizing row %d...\n", row);
  for (i = 0; i < sw->oversample; i++)
    {
      stp_weave_t w;
      stp_linecount_t *lines;

      for(j=0; j<sw->ncolors; j++)
        {
        lines = stp_get_linecount(sw, row, i, sw->head_offset[j]);
        lines[0].v[j]++;
        }

      weave_parameters_by_row(sw, row, i, &w);
      if (w.physpassend == row)
	{
	  stp_dprintf(STP_DBG_ROWS, sw->v,
		      "Pass=%d, physpassend=%d, row=%d, lineno=%d, flush...\n",
		      w.pass, w.physpassend, row, sw->lineno);
	  stp_flush(sw, model, width, hoffset, ydpi, xdpi, physical_xdpi);
       }
    }
}

void
stp_write_weave(void *        vsw,
		int           length,	/* I - Length of bitmap data */
		int           ydpi,	/* I - Vertical resolution */
		int           model,	/* I - Printer model */
		int           width,	/* I - Printed width */
		int           offset,	/* I - Offset from left side of page */
		int		xdpi,
		int		physical_xdpi,
		unsigned char *const cols[])
{
  stp_softweave_t *sw = (stp_softweave_t *) vsw;
  stp_lineoff_t *lineoffs[8];
  stp_lineactive_t *lineactives[8];
  stp_linecount_t *linecounts[8];
  const stp_linebufs_t *bufs[8];
  int xlength = (length + sw->horizontal_weave - 1) / sw->horizontal_weave;
  int ylength = xlength * sw->horizontal_weave;
  unsigned char *comp_ptr;
  int i, j;
  int setactive;
  int h_passes = sw->horizontal_weave * sw->vertical_subpasses;
  int cpass = sw->current_vertical_subpass * h_passes;

  if (!sw->fold_buf)
    sw->fold_buf = stp_zalloc(sw->bitwidth * ylength);
  if (!sw->comp_buf)
    sw->comp_buf = stp_zalloc(sw->bitwidth *(sw->compute_linewidth)(sw,ylength));
  if (sw->current_vertical_subpass == 0)
    initialize_row(sw, sw->lineno, xlength, cols);

  for (j = 0; j < sw->ncolors; j++)
    {
      if (cols[j])
	{
        const unsigned char *in;

        for (i = 0; i < h_passes; i++)
	  {
	    if (!sw->s[i])
	      sw->s[i] = stp_zalloc(sw->bitwidth *
				    (sw->compute_linewidth)(sw, ylength));
	    lineoffs[i] = stp_get_lineoffsets(sw, sw->lineno, cpass + i,
					      sw->head_offset[j]);
	    linecounts[i] = stp_get_linecount(sw, sw->lineno, cpass + i,
					      sw->head_offset[j]);
	    lineactives[i] = stp_get_lineactive(sw, sw->lineno, cpass + i,
						sw->head_offset[j]);
	    bufs[i] = stp_get_linebases(sw, sw->lineno, cpass + i,
					sw->head_offset[j]);
	  }

	  if (sw->bitwidth == 2)
	    {
	      stp_fold(cols[j], length, sw->fold_buf);
	      in = sw->fold_buf;
	    }
	  else
	    in = cols[j];
	  if (h_passes > 1)
	    {
	      switch (sw->horizontal_weave)
		{
		case 2:
		  stp_unpack_2(length, sw->bitwidth, in, sw->s[0], sw->s[1]);
		  break;
		case 4:
		  stp_unpack_4(length, sw->bitwidth, in,
			       sw->s[0], sw->s[1], sw->s[2], sw->s[3]);
		  break;
		case 8:
		  stp_unpack_8(length, sw->bitwidth, in,
			       sw->s[0], sw->s[1], sw->s[2], sw->s[3],
			       sw->s[4], sw->s[5], sw->s[6], sw->s[7]);
		  break;
		}
	      switch (sw->vertical_subpasses)
		{
		case 4:
		  switch (sw->horizontal_weave)
		    {
		    case 1:
		      stp_split_4(length, sw->bitwidth, in,
				  sw->s[0], sw->s[1], sw->s[2], sw->s[3]);
		      break;
		    case 2:
		      stp_split_4(length, sw->bitwidth, sw->s[0],
				  sw->s[0], sw->s[2], sw->s[4], sw->s[6]);
		      stp_split_4(length, sw->bitwidth, sw->s[1],
				  sw->s[1], sw->s[3], sw->s[5], sw->s[7]);
		      break;
		    }
		  break;
		case 2:
		  switch (sw->horizontal_weave)
		    {
		    case 1:
		      stp_split_2(xlength, sw->bitwidth, in, sw->s[0], sw->s[1]);
		      break;
		    case 2:
		      stp_split_2(xlength, sw->bitwidth, sw->s[0], sw->s[0], sw->s[2]);
		      stp_split_2(xlength, sw->bitwidth, sw->s[1], sw->s[1], sw->s[3]);
		      break;
		    case 4:
		      stp_split_2(xlength, sw->bitwidth, sw->s[0], sw->s[0], sw->s[4]);
		      stp_split_2(xlength, sw->bitwidth, sw->s[1], sw->s[1], sw->s[5]);
		      stp_split_2(xlength, sw->bitwidth, sw->s[2], sw->s[2], sw->s[6]);
		      stp_split_2(xlength, sw->bitwidth, sw->s[3], sw->s[3], sw->s[7]);
		      break;
		    }
		  break;
		  /* case 1 is taken care of because the various unpack */
		  /* functions will do the trick themselves */
		}
	      for (i = 0; i < h_passes; i++)
		{
		  setactive = (sw->pack)(sw->s[i], sw->bitwidth * xlength,
					 sw->comp_buf, &comp_ptr);
		  add_to_row(sw, sw->lineno, sw->comp_buf,
			     comp_ptr - sw->comp_buf, j, setactive,
			     lineoffs[i], lineactives[i], linecounts[i], bufs[i]);
		}
	    }
	  else
	    {
	      setactive = (sw->pack)(in, length * sw->bitwidth,
				   sw->comp_buf, &comp_ptr);
	      add_to_row(sw, sw->lineno, sw->comp_buf, comp_ptr - sw->comp_buf,
			 j, setactive, lineoffs[0], lineactives[0], linecounts[0], bufs[0]);
	    }
	}
    }
  sw->current_vertical_subpass++;
  if (sw->current_vertical_subpass >= sw->vertical_oversample)
    {
      finalize_row(sw, sw->lineno, model, width, offset, ydpi, xdpi,
		   physical_xdpi);
      sw->lineno++;
      sw->current_vertical_subpass = 0;
    }
}

#ifdef TEST_COOKED
static void
calculate_pass_parameters(cooked_t *w,		/* I - weave parameters */
                          int pass,		/* I - pass number ( >= 0) */
                          int *startrow,	/* O - print head position */
                          int *subpass,		/* O - subpass number */
			  int *phantomrows,	/* O - missing rows */
			  int *jetsused)	/* O - jets used to print */
{
	int raw_pass = pass + w->first_premapped_pass;
	int stagger = 0;
	int extra;

	if (raw_pass < w->first_normal_pass) {
		int i = 0;
		while (i + w->first_premapped_pass < w->first_normal_pass) {
			if (w->pass_premap[i] == pass) {
				raw_pass = i + w->first_premapped_pass;
				stagger = w->stagger_premap[i];
				break;
			}
			i++;
		}
	} else if (raw_pass >= w->first_postmapped_pass) {
		int i = 0;
		while (i + w->first_postmapped_pass < w->first_unused_pass) {
			if (w->pass_postmap[i] == pass) {
				raw_pass = i + w->first_postmapped_pass;
				stagger = w->stagger_postmap[i];
				break;
			}
			i++;
		}
	}

	calculate_raw_pass_parameters(&w->rw, raw_pass, startrow, subpass);
	*startrow -= w->rw.separation * w->rw.jets;
	*jetsused = w->rw.jets;
	*phantomrows = 0;

	*startrow += stagger * w->rw.separation;
	if (stagger < 0) {
		stagger = -stagger;
		*phantomrows += stagger;
	}
	*jetsused -= stagger;

	extra = w->first_row_printed - (*startrow + w->rw.separation * *phantomrows);
	extra = (extra + w->rw.separation - 1) / w->rw.separation;
	if (extra > 0) {
		*jetsused -= extra;
		*phantomrows += extra;
	}

	extra = *startrow + w->rw.separation * (*phantomrows + *jetsused - 1)
	          - w->last_row_printed;
	extra = (extra + w->rw.separation - 1) / w->rw.separation;
	if (extra > 0) {
		*jetsused -= extra;
	}
}
#endif /* TEST_COOKED */

#ifdef TEST

#ifdef ACCUMULATE
#define PUTCHAR(x) /* nothing */
#else
#define PUTCHAR(x) putchar(x)
#endif

static void
plotpass(int startrow, int phantomjets, int jetsused, int totaljets,
         int separation, int subpass, int *collect, int *prints)
{
	int hpos, i;

	for (hpos = 0; hpos < startrow; hpos++)
		PUTCHAR(' ');

	for (i = 0; i < phantomjets; i++) {
		int j;
		PUTCHAR('X');
		hpos++;
		for (j = 1; j < separation; j++) {
			PUTCHAR(' ');
			hpos++;
		}
	}

	for (; i < phantomjets + jetsused; i++) {
		if (i > phantomjets) {
			int j;
			for (j = 1; j < separation; j++) {
				PUTCHAR('-');
				hpos++;
			}
		}
		if (hpos < MAXCOLLECT) {
			if (collect[hpos] & 1 << subpass)
				PUTCHAR('^');
			else if (subpass < 10)
				PUTCHAR('0' + subpass);
			else
				PUTCHAR('A' + subpass - 10);
			collect[hpos] |= 1 << subpass;
			prints[hpos]++;
		} else {
			PUTCHAR('0' + subpass);
		}
		hpos++;
	}

	while (i++ < totaljets) {
		int j;
		for (j = 1; j < separation; j++) {
			PUTCHAR(' ');
			hpos++;
		}
		PUTCHAR('X');
	}
#ifdef ACCUMULATE
	for (i=0; i<=MAXCOLLECT; i++) {
		if (collect[i] == 0)
			putchar(' ');
		else if (collect[i] < 10)
			putchar('0'+collect[i]);
		else
			putchar('A'+collect[i]-10);
	}
#endif
	putchar('\n');
}
#endif /* TEST */

#ifdef TEST_COOKED
int
main(int ac, char *av[])
{
	int S         =ac>1 ? atoi(av[1]) : 4;
	int J         =ac>2 ? atoi(av[2]) : 12;
	int H         =ac>3 ? atoi(av[3]) : 1;
	int firstrow  =ac>4 ? atoi(av[4]) : 1;
	int lastrow   =ac>5 ? atoi(av[5]) : 100;
	int pageheight=ac>6 ? atoi(av[6]) : 1000;
	int strategy  =ac>7 ? atoi(av[7]) : 1;
	cooked_t *weave;
	int passes;

	int pass, x;
	int collect[MAXCOLLECT];
	int prints[MAXCOLLECT];

	memset(collect, 0, MAXCOLLECT*sizeof(int));
	memset(prints, 0, MAXCOLLECT*sizeof(int));
	printf("S=%d  J=%d  H=%d  firstrow=%d  lastrow=%d  "
	       "pageheight=%d  strategy=%d\n",
	       S, J, H, firstrow, lastrow, pageheight, strategy);

	weave = initialize_weave_params(S, J, H, firstrow, lastrow,
	                                pageheight, strategy);
	passes = weave->first_unused_pass - weave->first_premapped_pass;

	for (pass = 0; pass < passes; pass++) {
		int startrow, subpass, phantomjets, jetsused;

		calculate_pass_parameters(weave, pass, &startrow, &subpass,
		                          &phantomjets, &jetsused);

		plotpass(startrow, phantomjets, jetsused, J, S, subpass,
		         collect, prints);
	}

	for (pass=MAXCOLLECT - 1; prints[pass] == 0; pass--)
		;

	for (x=0; x<=pass; x++) {
		if (collect[x] < 10)
			putchar('0'+collect[x]);
		else
			putchar('A'+collect[x]-10);
	}
	putchar('\n');

	for (x=0; x<=pass; x++) {
		if (prints[x] < 10)
			putchar('0'+prints[x]);
		else
			putchar('A'+prints[x]-10);
	}
	putchar('\n');

	return 0;
}
#endif /* TEST_COOKED */

#ifdef TEST_RAW
int
main(int ac, char *av[])
{
	int S     =ac>1 ? atoi(av[1]) : 4;
	int J     =ac>2 ? atoi(av[2]) : 12;
	int h_pos =ac>3 ? atoi(av[3]) : 1;
	int h_over=ac>4 ? atoi(av[4]) : 1;
	int v_over=ac>5 ? atoi(av[5]) : 1;
	int H = h_pos * h_over * v_over;

	int pass, passes, x;
	int collect[MAXCOLLECT];
	int prints[MAXCOLLECT];
	raw_t raw;

	memset(collect, 0, MAXCOLLECT*sizeof(int));
	memset(prints, 0, MAXCOLLECT*sizeof(int));
	printf("S=%d  J=%d  H=%d\n", S, J, H);

	if (H > J) {
		printf("H > J, so this weave will not work!\n");
	}
	passes = S * H * 3;

	initialize_raw_weave(&raw, S, J, H);

	for (pass=0; pass<passes + S * H; pass++) {
		int startrow, subpass, phantomjets, jetsused;

		calculate_raw_pass_parameters(&raw, pass, &startrow, &subpass);
		phantomjets = 0;
		jetsused = J;

		plotpass(startrow, phantomjets, jetsused, J, S, subpass,
		         collect, prints);
	}
	for (pass=MAXCOLLECT - 1; prints[pass] == 0; pass--)
		;

	for (x=0; x<=pass; x++) {
		if (collect[x] < 10)
			putchar('0'+collect[x]);
		else
			putchar('A'+collect[x]-10);
	}
	putchar('\n');

	for (x=0; x<=pass; x++) {
		if (prints[x] < 10)
			putchar('0'+prints[x]);
		else
			putchar('A'+prints[x]-10);
	}
	putchar('\n');

	printf("  A  first row given by pass lookup doesn't match row lookup\n"
	       "  B  jet out of range\n"
	       "  C  given jet number of pass doesn't print this row\n"
	       "  D  subpass given by reverse lookup doesn't match requested subpass\n");

	for (x = S * J; x < S * J * 20; x++) {
		int h;
		for (h = 0; h < H; h++) {
			int pass, jet, start, first, subpass, z;
			int a=0, b=0, c=0, d=0;
			calculate_raw_row_parameters(&raw, x, h, &pass, &jet, &start);
			for (z=0; z < pass; z++) {
				putchar(' ');
			}
			printf("%d", jet);
			calculate_raw_pass_parameters(&raw, pass, &first, &subpass);
			if (first != start)
				a=1;
			if (jet < 0 || jet >= J)
				b=1;
			if (x != first + jet * S)
				c=1;
			if (subpass != h)
				d=1;
			if (a || b || c || d) {
				printf("    (");
				if (a) putchar('A');
				if (b) putchar('B');
				if (c) putchar('C');
				if (d) putchar('D');
				putchar(')');
				printf("\npass=%d first=%d start=%d jet=%d subpass=%d", pass, first, start, jet, subpass);
			}
			putchar('\n');
		}
		/* putchar('\n'); */
	}

	return 0;
}

#endif /* TEST_RAW */
