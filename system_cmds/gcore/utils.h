/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <uuid/uuid.h>
#include <mach/mach_types.h>
#include <sysexits.h>
#include <fcntl.h>

#ifndef _UTILS_H
#define _UTILS_H

extern const char *pgm;

struct vm_range;
struct region;

extern void err_mach(kern_return_t, const struct region *, const char *, ...) __printflike(3, 4);
extern void printvr(const struct vm_range *, const char *, ...) __printflike(2, 3);
extern void printr(const struct region *, const char *, ...) __printflike(2, 3);

typedef char hsize_str_t[7]; /* e.g. 1008Mib */

extern const char *str_hsize(hsize_str_t hstr, uint64_t);
extern const char *str_prot(vm_prot_t);
extern const char *str_shared(int);
extern const char *str_purgable(int, int);

typedef char tag_str_t[40];

extern const char *str_tag(tag_str_t, const struct region *);

extern int bounded_pwrite(int, const void *, size_t, off_t, bool *, ssize_t *);
extern int bounded_write(int, const void *, size_t, ssize_t *);
extern int bounded_write_zero(int, size_t, ssize_t *);

extern bool task_port_is_corpse(mach_port_t);

extern int virtual_address_size(uint32_t *);

#endif /* _UTILS_H */
