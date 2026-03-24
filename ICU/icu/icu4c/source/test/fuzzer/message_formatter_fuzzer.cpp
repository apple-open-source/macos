// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <cstring>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


#include "fuzzer_utils.h"
#include "unicode/messageformat2.h"
#include "unicode/messagepattern.h"
#include "unicode/msgfmt.h"

#if !APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
IcuEnvironment* env = new IcuEnvironment();
#endif // APPLE_ICU_CHANGES

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  UParseError pe = { 0, 0, {0}, {0} };
  UErrorCode status = U_ZERO_ERROR;

  size_t unistr_size = size/2;
  std::unique_ptr<char16_t[]> fuzzbuff(new char16_t[unistr_size]);
  std::memcpy(fuzzbuff.get(), data, unistr_size * 2);
  icu::UnicodeString fuzzstr(false, fuzzbuff.get(), unistr_size);

  icu::MessageFormat mfmt(fuzzstr, status);

  status = U_ZERO_ERROR;
  icu::MessagePattern mpat(fuzzstr, &pe, status);
  pe = { 0, 0, {0}, {0} };

  status = U_ZERO_ERROR;
  icu::message2::MessageFormatter msgfmt2 =
      icu::message2::MessageFormatter::Builder(status)
      .setPattern(fuzzstr, pe, status)
      .build(status);
  return 0;
}
