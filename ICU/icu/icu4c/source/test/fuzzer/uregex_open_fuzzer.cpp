// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <stddef.h>
#include <stdint.h>
#include <string.h>


#include "fuzzer_utils.h"
#include "unicode/regex.h"

#if !APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
IcuEnvironment* env = new IcuEnvironment();
#endif // APPLE_ICU_CHANGES

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  UParseError pe = { 0, 0, {0}, {0} };
  UErrorCode status = U_ZERO_ERROR;

  URegularExpression* re = uregex_open(reinterpret_cast<const char16_t*>(data),
                                       static_cast<int>(size) / sizeof(char16_t),
                                       0, &pe, &status);

  if (re)
    uregex_close(re);
 
  return 0;
}
