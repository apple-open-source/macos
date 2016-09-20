/*
 * Copyright (c) 2015 Apple Inc.  All rights reserved.
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <uuid/uuid.h>
#include <mach/mach_types.h>
#include <sysexits.h>
#include <err.h>

#ifndef _UTILS_H
#define _UTILS_H

extern const char *pgm;

struct region;

extern void err_mach(kern_return_t, const struct region *r, const char *fmt, ...) __printflike(3, 4);
extern void printr(const struct region *r, const char *fmt, ...) __printflike(2, 3);

typedef char hsize_str_t[7]; /* e.g. 1008Mib */

extern const char *str_hsize(hsize_str_t hstr, uint64_t);
extern char *strconcat(const char *, const char *, size_t);

#endif /* _UTILS_H */
