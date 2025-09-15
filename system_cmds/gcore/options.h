/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */

#include <sys/types.h>
#include <os/log.h>
#include <stdbool.h>

#include <assert.h>

#ifndef _OPTIONS_H
#define _OPTIONS_H

#if defined(__arm__) || defined(__arm64__)
#define RDAR_23744374       1   /* 'true' while not fixed i.e. enable workarounds */
#define RDAR_28040018		1	/* 'true' while not fixed i.e. enable workarounds */
#endif

#define CONFIG_SUBMAP       1   /* include submaps */
#define CONFIG_DEBUG		1	/* support '-d' option */

#ifdef NDEBUG
#define poison(a, p, s)     /* do nothing */
#else
#define poison(a, p, s)     memset(a, p, s) /* scribble on dying memory */
#endif

enum content : uint8_t {
    CONTENT_STACK,      // just stack pages
    CONTENT_COMPACT,    // all unmodified pages
    CONTENT_FULL,       // all content pages
};

extern os_log_t glog;

struct options {
    int corpsify;       // make a corpse to dump from
    int suspend;        // suspend while dumping
    int preserve;       // preserve the core file, even if there are errors
    int verbose;        // be chatty
#ifdef CONFIG_DEBUG
    int debug;          // internal debugging: options accumulate. noisy.
#endif
    off_t sizebound;    // maximum size of the dump
	size_t ncthresh;	// F_NOCACHE enabled *above* this value
    int gzip;           // pipe corefile via gzip -1 compression
    int stream;         // write corefile sequentially e.g. no pwrites
    bool notes;         // if set, dump LC_NOTES for memory analysis tools
    enum content content;   // dump content options
    int quiet;          // log rather than send to stdout/stderr
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
