#ifndef SecDbInternal_h
#define SecDbInternal_h

#include "SecDb.h"

static const size_t kSecDbMaxReaders = 6; // Increased from 5: index 0 reserved for backup, 1-5 for normal readers

// Do not increase this without changing lock types in SecDb
static const size_t kSecDbMaxWriters = 1;

// maxreaders + maxwriters
static const size_t kSecDbMaxIdleHandles = 7; // Updated from 6 to match kSecDbMaxReaders + kSecDbMaxWriters

// Trustd's databases pass in this constant instead in order
// to reduce trustd's inactive memory footprint by having
// fewer cached open sqlite connections.
static const size_t kSecDbTrustdMaxIdleHandles = 2;

#endif /* SecDbInternal_h */
