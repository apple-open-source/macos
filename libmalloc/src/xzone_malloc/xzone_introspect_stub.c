/* ----------------------------------------------------------------------------
Copyright (c) 2018-2022, Microsoft Research, Daan Leijen
Copyright © 2025 Apple Inc.
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" in the same directory as this file.
-----------------------------------------------------------------------------*/

#include "../internal.h"

#if !defined(__x86_64__)
# error "This file should only be built for exclaves introspection on x86_64!"
#endif // !defined(__x86_64__)

