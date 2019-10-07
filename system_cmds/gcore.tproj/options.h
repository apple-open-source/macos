/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include <sys/types.h>
#include <compression.h>

#include <assert.h>

#ifndef _OPTIONS_H
#define _OPTIONS_H

#if defined(__arm__) || defined(__arm64__)
#define RDAR_23744374       1   /* 'true' while not fixed i.e. enable workarounds */
#define RDAR_28040018		1	/* 'true' while not fixed i.e. enable workarounds */
#endif

//#define   CONFIG_SUBMAP   1   /* include submaps (debugging output) */
#define CONFIG_GCORE_MAP	1	/* support 'gcore map' */
#define CONFIG_GCORE_CONV	1	/* support 'gcore conv' - new -> old core files */
#define CONFIG_GCORE_FREF	1	/* support 'gcore fref' - referenced file list */
#define CONFIG_DEBUG		1	/* support '-d' option */

#ifdef NDEBUG
#define poison(a, p, s)     /* do nothing */
#else
#define poison(a, p, s)     memset(a, p, s) /* scribble on dying memory */
#endif

struct options {
    int corpsify;       // make a corpse to dump from
    int suspend;        // suspend while dumping
    int preserve;       // preserve the core file, even if there are errors
    int verbose;        // be chatty
#ifdef CONFIG_DEBUG
    int debug;          // internal debugging: options accumulate. noisy.
#endif
	int extended;		// avoid writing out ro mapped files, compress regions
    off_t sizebound;    // maximum size of the dump
    size_t chunksize;   // max size of a compressed subregion
    compression_algorithm calgorithm; // algorithm in use
	size_t ncthresh;	// F_NOCACHE enabled *above* this value
    int allfilerefs;    // if set, every mapped file on the root fs is a fileref
	int dsymforuuid;   // Try dsysForUUID to retrieve symbol-rich executable
};

extern const struct options *opt;

/*
 * == 0 - not verbose
 * >= 1 - verbose plus chatty
 * >= 2 - tabular summaries
 * >= 3 - all
 */

#ifdef CONFIG_DEBUG
#define OPTIONS_DEBUG(opt, lvl)	((opt)->debug && (opt)->debug >= (lvl))
#else
#define OPTIONS_DEBUG(opt, lvl)	0
#endif

#endif /* _OPTIONS_H */
