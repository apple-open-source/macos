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
#endif

#define CONFIG_REFSC        1   /* create shared cache reference segment (-R) */
//#define   CONFIG_PURGABLE 1   /* record purgability */
//#define   CONFIG_SUBMAP   1   /* include submaps (debugging) */

#ifdef NDEBUG
#define poison(a, p, s)     /* do nothing */
#else
#define poison(a, p, s)     memset(a, p, s) /* scribble on dying memory */
#endif

struct options {
    int corpse;         // dump from a corpse
    int suspend;        // suspend while dumping
    int preserve;       // preserve the core file, even if there are errors
    int verbose;        // be chatty
    int debug;          // internal debugging: options accumulate.  very noisy.
    int dryrun;         // do all the work, but throw the dump away
    int sparse;         // use dyld's data about dylibs to reduce the size of the dump
    off_t sizebound;    // maximum size of the dump
    int coreinfo;       // create a (currently experimental) 'coreinfo' section
#ifdef CONFIG_REFSC
    int scfileref;      // create "reference" segments that point at the shared cache
#endif
    int compress;       // compress the dump
    size_t chunksize;   // max size of the compressed segment
    compression_algorithm calgorithm; // algorithm in use
};

extern const struct options *opt;

#endif /* _OPTIONS_H */
