#ifndef SecDbInternal_h
#define SecDbInternal_h

#include "SecDb.h"

static const size_t kSecDbMaxReaders = 5;

// Do not increase this without changing lock types in SecDb
static const size_t kSecDbMaxWriters = 1;

// maxreaders + maxwriters
static const size_t kSecDbMaxIdleHandles = 6;

// Trustd's databases pass in this constant instead in order
// to reduce trustd's inactive memory footprint by having
// fewer cached open sqlite connections.
static const size_t kSecDbTrustdMaxIdleHandles = 2;

#endif /* SecDbInternal_h */
