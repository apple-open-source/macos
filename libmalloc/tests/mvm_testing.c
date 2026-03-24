// When darwintests are all compiled into a single executable, they could result
// in multiply-defined symbols. Avoid this by having this file provide the
// single definition.

#define MVM_INCLUDE_SOURCE
#include "mvm_testing.h"
