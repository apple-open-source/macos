/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "options.h"

#include <stdbool.h>

#ifndef _CONVERT_H
#define _CONVERT_H

#ifdef CONFIG_GCORE_FREF
extern int gcore_fref(int);
#endif

#ifdef CONFIG_GCORE_MAP
extern int gcore_map(int);
#endif

#ifdef CONFIG_GCORE_CONV
extern int gcore_conv(int, const char *, bool, int);
#endif

#endif /* _CONVERT_H */
