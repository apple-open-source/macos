// © 2023 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <cstring>

#include "fuzzer_utils.h"
#include "unicode/localpointer.h"
#include "unicode/timezone.h"
#include "unicode/vtzone.h"

#if !APPLE_ICU_CHANGES
// rdar://165672453 (ICU-23254 Remove C++ static initialization)
// (Port of ICU-23254: Should be included in ICU 78.2)
IcuEnvironment* env = new IcuEnvironment();
#endif // APPLE_ICU_CHANGES

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {

  // Limit the test for at most 1000 Unicode characters.
  if (size > 2000) {
    size = 2000;
  }
  size_t unistr_size = size/2;
  std::unique_ptr<char16_t[]> fuzzbuff(new char16_t[unistr_size]);
  std::memcpy(fuzzbuff.get(), data, unistr_size * 2);
  icu::UnicodeString fuzzstr(false, fuzzbuff.get(), unistr_size);

  icu::LocalPointer<icu::TimeZone> tz(
      icu::TimeZone::createTimeZone(fuzzstr));

  icu::TimeZone::getEquivalentID(fuzzstr, size);

  icu::UnicodeString output;

  UErrorCode status = U_ZERO_ERROR;
  UBool system;
  icu::TimeZone::getCanonicalID(fuzzstr, output, system, status);

  status = U_ZERO_ERROR;
  icu::TimeZone::getIanaID(fuzzstr, output, status);

  status = U_ZERO_ERROR;
  icu::TimeZone::getWindowsID(fuzzstr, output, status);

  status = U_ZERO_ERROR;
  icu::TimeZone::getRegion(fuzzstr, status);

  tz.adoptInstead(icu::VTimeZone::createVTimeZoneByID(fuzzstr));

  status = U_ZERO_ERROR;
  tz.adoptInstead(icu::VTimeZone::createVTimeZone(fuzzstr, status));
  return 0;
}
