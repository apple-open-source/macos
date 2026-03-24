//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include <dispatch/dispatch.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <cstdlib> // for abort

// FIXME: The declaration for swift_reportError below was extracted from swift/Runtime/Debug.h.
namespace swift {
extern "C" void swift_reportError(uint32_t flags, const char *message);
}

extern "C" void
_swift_dispatch_source_create_abort(void)
{
  swift::swift_reportError(0,
      "dispatch_source_create returned NULL, invalid parameters passed to dispatch_source_create");
  abort();
}
