/* ----------------------------------------------------------------------------
Copyright (c) 2018-2022, Microsoft Research, Daan Leijen
Copyright © 2025 Apple Inc.
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" in the same directory as this file.
-----------------------------------------------------------------------------*/

#if CONFIG_XZONE_MALLOC

#ifndef _MALLOC_XZONE_INTROSPECT_H_
#define _MALLOC_XZONE_INTROSPECT_H_

#include <malloc/_ptrcheck.h>
__ptrcheck_abi_assume_single()

#include <malloc/malloc.h>

extern const struct malloc_introspection_t xzm_malloc_zone_introspect;

#endif // _MALLOC_XZONE_INTROSPECT_H_

#endif // CONFIG_XZONE_MALLOC
