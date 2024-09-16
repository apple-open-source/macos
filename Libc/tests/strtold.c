#include <fenv.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdlib.h>

#include <darwintest.h>

#pragma STDC FENV_ACCESS ON

// strtold parses to long double, which varies between
// platforms.

// First, let's determine what long double is supposed to be:

#if (defined(__i386__) || defined(__x86_64)) && defined(__APPLE__) && defined(__MACH__)
// MacOS on x86 uses long double == float80
#define LONG_DOUBLE_IS_FLOAT80 1
#endif

#if (defined(__arm) || defined(__aarch64__)) && defined(__APPLE__) && defined(__MACH__)
#define LONG_DOUBLE_IS_BINARY64 1
#endif

// TODO: Set LONG_DOUBLE_IS_* appropriately for other platforms?

#if !defined(LONG_DOUBLE_IS_BINARY64) && !defined(LONG_DOUBLE_IS_FLOAT80) && !defined(LONG_DOUBLE_IS_BINARY128)
#error Unable to determine long double format used by this platform
#endif


// Now that we know what long double is, we can check that strtold
// is the right one.  If strtold is parsing to the wrong format
// (and then converting to long double), then that will show up
// as anomalies in the parsing:

T_DECL(strtold_sanity, "strtold(3) is appropriate for this platform")
{
#if defined(LONG_DOUBLE_IS_BINARY128)
  // TODO: Check that strtold is parsing binary128, not float80 or binary64
#endif

#if defined(LONG_DOUBLE_IS_FLOAT80)
  // If strtold is parsing to double internally, this will underflow to zero.
  T_EXPECT_TRUE(strtold("1e-1000", NULL) != 0.0L, "No unexpected underflow");
  // TODO: Check that strtold isn't parsing to binary128 internally...
  // This requires identifying an input where double-rounding changes the result
#endif

#if defined(LONG_DOUBLE_IS_BINARY64)
  // TODO: Fix this.  This is true even if strtold is parsing to float80 and
  // then rounding to binary64.
  T_EXPECT_TRUE(strtold("1e-1000", NULL) == 0.0L, "Should not underflow float80");
#endif
}

// Dump a multi-byte integer as a hex string using the local endianness
// (Currently assumes little-endian)
static void hexdump(char *dest, const uint8_t *p, size_t len) {
  *dest++ = '0';
  *dest++ = 'x';
  for (int i = 0; i < (int)len; i++) {
    sprintf(dest, "%02x", p[(int)len - i - 1]);
    dest += 2;
  }
}

static void strtold_verify_with_rounding_mode(const char *src,
                                      int mode,
                                      long double e,
                                      int expected_errno,
                                      const char *expected_end)
{
  union {
    uint8_t raw[sizeof(long double)];
    long double d;
  } actual, expected;
  memset(&actual, 0, sizeof(actual));
  memset(&expected, 0, sizeof(expected));
  errno = 0;
  expected.d = e;

  fesetround(mode);
  const char *mode_name =
    (mode == FE_TONEAREST) ? "TONEAREST" :
    (mode == FE_DOWNWARD) ? "DOWNWARD" :
    (mode == FE_UPWARD) ? "UPWARD" :
    (mode == FE_TOWARDZERO) ? "TOWARDZERO" :
    "<UNKNOWN MODE>";

  char *end = NULL;
  actual.d = strtold(src, &end);
  int actual_errno = errno;
  int failed = 0;
  char actual_hexdump[40];
  hexdump(actual_hexdump, actual.raw, sizeof(actual.raw));
  char expected_hexdump[40];
  hexdump(expected_hexdump, expected.raw, sizeof(expected.raw));

  // Note: Verify the bits, not the FP value!
  // In particular, this correctly validates NaNs and signed zeros
  if (memcmp(actual.raw, expected.raw, sizeof(actual.raw)) != 0) {
    T_FAIL("Parsed value mismatch: Input \"%s\", mode = %s, "
           "actual = %.25Lg %La (%s), "
           "expected = %.25Lg %La (%s) (s(ld)=%d) ",
           src, mode_name,
           actual.d, actual.d, actual_hexdump,
           expected.d, expected.d, expected_hexdump, (int)sizeof(long double));
    failed = 1;
  }

  // We also expect ERANGE if the return value was subnormal or infinite.
  switch (fpclassify(expected.d)) {
  case FP_SUBNORMAL:
    expected_errno = ERANGE;
  }

  if (actual_errno != expected_errno) {
    T_FAIL("Errno mismatch: "
           "input \"%s\", mode = %s, "
           "result = %.25Lg %La (%s), "
           "actual errno = %d, expected errno = %d",
           src, mode_name,
           actual.d, actual.d, actual_hexdump,
           actual_errno, expected_errno);
    failed = 1;
  }

  if (end != expected_end) {
    T_FAIL("End did not match end of string: input \"%s\", mode = %s, end = \"%s\", expected end = \"%s\"",
           src, mode_name, end, expected_end);
    failed = 1;
  }

  {
    // "errno should never be set to zero by any library function"
    errno = 999;
    (void)strtold(src, NULL);
    if (errno == 0) {
      T_FAIL("Errno forced to zero for input \"%s\", mode = %s", src, mode_name);
      failed = 1;
    }
  }

  // Adding non-relevant characters to the end should not affect how it got parsed.
  // In particular, the updated `end` should point to the same offset.
  char *buff = malloc(strlen(src) + 10);
  const char *extras = strchr(src, '.') ? "Q+%-&!x.," : "Q+%-&!x,";
  for (size_t i = 0; i < strlen(extras); i++) {
    strcpy(buff, src);
    size_t s = strlen(src);
    buff[s] = extras[i];
    buff[s + 1] = '\0';
    expected.d = actual.d; // We expect the same value as before
    end = NULL;
    actual.d = strtold(buff, &end);
    if (memcmp(actual.raw, expected.raw, sizeof(actual.raw)) != 0) {
      hexdump(actual_hexdump, actual.raw, sizeof(actual.raw));
      T_FAIL("Adding %c to end of \"%s\" changed result from "
             "%.25Lg %La (%s) "
             "to %.25Lg %La (%s)",
             extras[i], buff, expected.d, expected.d, expected_hexdump,
             actual.d, actual.d, actual_hexdump);
      failed = 1;
    }
    if (end != buff + (expected_end - src)) {
      T_FAIL("End did not update correctly for input \"%s\", end = \"%s\" (buff + %d)", buff, end, (int)(end - buff));
      failed = 1;
    }
  }
  free(buff);

  if (!failed) {
    T_PASS("%s: %s", mode_name, src);
  }
}

#define issubnormal(d) (isfinite(d) && !isnormal(d) && ((d) != 0.0L))
#define is_inf_or_subnormal(d) (isinf(d) || issubnormal(d))
static void strtold_verify(const char *src,
                           long double expected_nearest,
                           long double expected_down,
                           long double expected_up) {
  int expected_errno = 0;
  if ((expected_down == 0.0L) != (expected_up == 0.0L)) {
    // (== rounds to zero in one rounding mode but not the other)
    expected_errno = ERANGE;
  }

  int e_errno = is_inf_or_subnormal(expected_nearest) ? ERANGE : expected_errno;
  strtold_verify_with_rounding_mode(src, FE_TONEAREST, expected_nearest, e_errno, src + strlen(src));
  e_errno = is_inf_or_subnormal(expected_down) ? ERANGE : expected_errno;
  strtold_verify_with_rounding_mode(src, FE_DOWNWARD, expected_down, e_errno, src + strlen(src));
  e_errno = is_inf_or_subnormal(expected_up) ? ERANGE : expected_errno;
  strtold_verify_with_rounding_mode(src, FE_UPWARD, expected_up, e_errno, src + strlen(src));
  long double expected = signbit(expected_up) ? expected_up : expected_down;
  e_errno = is_inf_or_subnormal(expected) ? ERANGE : expected_errno;
  strtold_verify_with_rounding_mode(src, FE_TOWARDZERO, expected, e_errno, src + strlen(src));
}

static void strtold_verify_overflow(const char *src, long double expected_nearest, long double expected_down, long double expected_up) {
  int expected_errno = ERANGE;
  strtold_verify_with_rounding_mode(src, FE_TONEAREST, expected_nearest, expected_errno, src + strlen(src));
  strtold_verify_with_rounding_mode(src, FE_DOWNWARD, expected_down, expected_errno, src + strlen(src));
  strtold_verify_with_rounding_mode(src, FE_UPWARD, expected_up, expected_errno, src + strlen(src));
  long double expected = signbit(expected_nearest) ? expected_up : expected_down;
  strtold_verify_with_rounding_mode(src, FE_TOWARDZERO, expected, expected_errno, src + strlen(src));
}

static void strtold_verify_infinity(const char *src, long double expected) {
  strtold_verify_with_rounding_mode(src, FE_TONEAREST, expected, 0, src + strlen(src));
  strtold_verify_with_rounding_mode(src, FE_DOWNWARD, expected, 0, src + strlen(src));
  strtold_verify_with_rounding_mode(src, FE_UPWARD, expected, 0, src + strlen(src));
  strtold_verify_with_rounding_mode(src, FE_TOWARDZERO, expected, 0, src + strlen(src));
}


T_DECL(strtold_common, "strtold(3) (format-independent)")
{
  // The following should all work regardless of the exact format of 'long double'
  static const long double infinity = HUGE_VALL;

  // Spellings of zero
  long double zero = 0.0L;
  strtold_verify("0", zero, zero, zero);
  strtold_verify("0e0", zero, zero, zero);
  strtold_verify("0e1", zero, zero, zero);
  strtold_verify("0e9999999999999", zero, zero, zero);
  strtold_verify("0.0e0", zero, zero, zero);
  strtold_verify("00000000000000.0000000000000000000000000000", zero, zero, zero);
  strtold_verify("0x0", zero, zero, zero);
  strtold_verify("0x0.0p0", zero, zero, zero);

  // 1 with leading whitespace
  long double one = 1.0L;
  strtold_verify("1.0", one, one, one);
  strtold_verify("+1.0", 1.0L, 1.0L, 1.0L);
  strtold_verify(" 1.0", one, one, one);
  strtold_verify("  1.0", one, one, one);
  strtold_verify("\t1.0", one, one, one);
  strtold_verify("\n1.0", one, one, one);
  strtold_verify("\v1.0", one, one, one);
  strtold_verify("\f1.0", one, one, one);
  strtold_verify("\r1.0", one, one, one);
  strtold_verify(" \t\n\v\f\r1.0", one, one, one);

  // NaN forms
  strtold_verify("NaN", nanl(""), nanl(""), nanl(""));
  strtold_verify("-NaN", -nanl(""), -nanl(""), -nanl(""));
  strtold_verify("nan", nanl(""), nanl(""), nanl(""));
  strtold_verify("+NAN", nanl(""), nanl(""), nanl(""));
  strtold_verify("nAn", nanl(""), nanl(""), nanl(""));
  strtold_verify("NaN()", nanl(""), nanl(""), nanl(""));
  strtold_verify("nan(1)", nanl("1"), nanl("1"), nanl("1"));
  strtold_verify("NaN(011)", nanl("9"), nanl("9"), nanl("9"));
  strtold_verify("NaN(0x11)", nanl("0x11"), nanl("0x11"), nanl("0x11"));
  strtold_verify("NaN(11)", nanl("0xb"), nanl("0xb"), nanl("0xb"));
  strtold_verify("nan(0xffffffffffffffffffffff9)",
         nanl("0x3fffffffffffffff9"), nanl("0x3fffffffffffffff9"), nanl("0x3fffffffffffffff9"));
  strtold_verify("nan(0x1fffffffffffffff9)",
         nanl("0x1fffffffffffffff9"), nanl("0x1fffffffffffffff9"), nanl("0x1fffffffffffffff9"));
  strtold_verify("nan(0xffffffffffffff9)",
         nanl("0xffffffffffffff9"), nanl("0xffffffffffffff9"), nanl("0xffffffffffffff9"));
  strtold_verify("nan(0xfffffffffffff9)",
         nanl("0xfffffffffffff9"), nanl("0xfffffffffffff9"), nanl("0xfffffffffffff9"));

  // Explicit infinities
  strtold_verify_infinity("inf", infinity);
  strtold_verify_infinity("InF", infinity);
  strtold_verify_infinity("iNf", infinity);
  strtold_verify_infinity("+inf", infinity);
  strtold_verify_infinity("-InF", -infinity);

  // "inf" is parsed, rest is not relevant
  const char *infinite = "infinite";
  strtold_verify_with_rounding_mode(infinite, FE_TONEAREST, infinity, 0, infinite + 3);
  strtold_verify_with_rounding_mode(infinite, FE_TOWARDZERO, infinity, 0, infinite + 3);

  strtold_verify_infinity("InFiNiTy", infinity);
  strtold_verify_infinity("iNfInItY", infinity);
  strtold_verify_infinity("-infinity", -infinity);
  strtold_verify_infinity("+infinity", infinity);
}

#if defined(LONG_DOUBLE_IS_FLOAT80)

T_DECL(strtold_float80, "strtold(3) for long double == float80")
{
  static const long double min_subnormal = 0x1p-16445L;
  static const long double min_subnormal_succ = 0x2p-16445L;
  static const long double max_subnormal = 0x1.fffffffffffffffcp-16383L;
  static const long double min_normal = 0x1p-16382L;
  static const long double min_normal_succ = 0x1.0000000000000002p-16382L;
  static const long double max_normal_pred = 0x1.fffffffffffffffcp+16383L;
  static const long double max_normal = 0x1.fffffffffffffffep+16383L;
  static const long double infinity = HUGE_VALL;

  static const long double pi_pred = 0x1.921fb54442d18468p+1L;
  static const long double pi =      0x1.921fb54442d1846ap+1L;

  // Optimal forms of various special values
  strtold_verify("4e-4951", min_subnormal, min_subnormal, min_subnormal_succ);
  strtold_verify("3362103143112093506e-4950", max_subnormal, max_subnormal, min_normal);
  strtold_verify("33621031431120935063e-4951", min_normal, min_normal, min_normal_succ);
  strtold_verify("1189731495357231765e4914", max_normal, max_normal_pred, max_normal);

  // Tad less than max_normal
  strtold_verify("1189731495357231765021263853030970205169063322294624200440323733e4869", max_normal, max_normal_pred, max_normal);
  // Tad more than max_normal
  strtold_verify("1189731495357231765021263853030970205169063322294624200440323734e4869", max_normal, max_normal, infinity);

  // Just a tad less than max_normal + 1/2 ULP == midway between max_normal and overflow threshold
  strtold_verify("1189731495357231765053511589829488667966254004695567218956499277e4869", max_normal, max_normal, infinity);
  // A tad more than max_normal + 1/2 ULP
  strtold_verify("1189731495357231765053511589829488667966254004695567218956499278e4869", infinity, max_normal, infinity);

  // Just a bit less than 2 ** 16384 (== max_normal + 1 ULP == overflow threshold)
  strtold_verify("1189731495357231765085759326628007130763444687096510237472674821e4869", infinity, max_normal, infinity);
  // Just a bit more than 2 ** 16384 (== max_normal + 1 ULP == overflow threshold)
  strtold_verify_overflow("1189731495357231765085759326628007130763444687096510237472674822e4869", infinity, max_normal, infinity);
  strtold_verify_overflow("11897314953572317650857593266280071307634446870965103e4880", infinity, max_normal, infinity);
  strtold_verify_overflow("1189731495357231765085759326628007130763445e4890", infinity, max_normal, infinity);
  strtold_verify_overflow("118973149535723176508575932662801e4900", infinity, max_normal, infinity);
  strtold_verify_overflow("11897314953572317650858e4910", infinity, max_normal, infinity);
  strtold_verify_overflow("1189731495358e4920", infinity, max_normal, infinity);
  strtold_verify_overflow("1189732e4926", infinity, max_normal, infinity);
  strtold_verify_overflow("2e4932", infinity, max_normal, infinity);
  
  strtold_verify("3.1415926535897932385", pi, pi_pred, pi);
  strtold_verify("3.141592653589793238462643383279502884197169399"
                 "375105820974944592307816406286208998628034825342117",
                 pi, pi_pred, pi);


}

#endif

T_DECL(strtold_locale, "strtold(3) locale support")
{
  // Literals that we'll test against:
  //  Include values with a decimal point in the beginning, middle, and end.
  //  Include values for C/en_US, fr_FR, and a synthesized locale that uses a multi-byte decimal point
  //  Include values with a malformed multi-byte decimal point
  // Note that these should all work identically regardless of what format is used by long double
  const char *us_123_456 = "123.456";
  const char *fr_123_456 = "123,456";
  const char *syn_123_456 = "123%$456";
  const char *syn_trunc_123_456 = "123%456";
  const char *us__279 = ".279";
  const char *fr__279 = ",279";
  const char *syn__279 = "%$279";
  const char *syn_trunc__279 = "%279";
  const char *us_843_ = "843.";
  const char *fr_843_ = "843,";
  const char *syn_843_ = "843%$";
  const char *syn_trunc_843_ = "843%";

  (void)setlocale(LC_ALL, "C");
  struct lconv *lc = localeconv();
  T_EXPECT_EQ_STR(lc->decimal_point, ".", "Expected C locale to have '.' as decimal point");
  strtold_verify_with_rounding_mode(us_123_456, FE_TONEAREST, 123.456L, 0, us_123_456 + 7);
  strtold_verify_with_rounding_mode(fr_123_456, FE_TONEAREST, 123.0L, 0, fr_123_456 + 3);
  strtold_verify_with_rounding_mode(syn_123_456, FE_TONEAREST, 123.0L, 0, syn_123_456 + 3);
  strtold_verify_with_rounding_mode(syn_trunc_123_456, FE_TONEAREST, 123.0L, 0, syn_trunc_123_456 + 3);
  strtold_verify_with_rounding_mode(us__279, FE_TONEAREST, 0.279L, 0, us__279 + 4);
  strtold_verify_with_rounding_mode(fr__279, FE_TONEAREST, 0.0L, 0, fr__279);
  strtold_verify_with_rounding_mode(syn__279, FE_TONEAREST, 0.0L, 0, syn__279);
  strtold_verify_with_rounding_mode(syn_trunc__279, FE_TONEAREST, 0.0L, 0, syn_trunc__279);
  strtold_verify_with_rounding_mode(us_843_, FE_TONEAREST, 843.0L, 0, us_843_ + 4);
  strtold_verify_with_rounding_mode(fr_843_, FE_TONEAREST, 843.0L, 0, fr_843_ + 3);
  strtold_verify_with_rounding_mode(syn_843_, FE_TONEAREST, 843.0L, 0, syn_843_ + 3);
  strtold_verify_with_rounding_mode(syn_trunc_843_, FE_TONEAREST, 843.0L, 0, syn_trunc_843_ + 3);

  if (setlocale(LC_ALL, "en_US") != NULL || setlocale(LC_ALL, "en_US.UTF-8") != NULL) {
    lc = localeconv();
    T_EXPECT_EQ_STR(lc->decimal_point, ".", "Expected en_US locale to have '.' as decimal point");
    strtold_verify_with_rounding_mode(us_123_456, FE_TONEAREST, 123.456L, 0, us_123_456 + 7);
    strtold_verify_with_rounding_mode(fr_123_456, FE_TONEAREST, 123.0L, 0, fr_123_456 + 3);
    strtold_verify_with_rounding_mode(syn_123_456, FE_TONEAREST, 123.0L, 0, syn_123_456 + 3);
    strtold_verify_with_rounding_mode(syn_trunc_123_456, FE_TONEAREST, 123.0L, 0, syn_trunc_123_456 + 3);
    strtold_verify_with_rounding_mode(us__279, FE_TONEAREST, 0.279L, 0, us__279 + 4);
    strtold_verify_with_rounding_mode(fr__279, FE_TONEAREST, 0.0L, 0, fr__279);
    strtold_verify_with_rounding_mode(syn__279, FE_TONEAREST, 0.0L, 0, syn__279);
    strtold_verify_with_rounding_mode(syn_trunc__279, FE_TONEAREST, 0.0L, 0, syn_trunc__279);
    strtold_verify_with_rounding_mode(us_843_, FE_TONEAREST, 843.0L, 0, us_843_ + 4);
    strtold_verify_with_rounding_mode(fr_843_, FE_TONEAREST, 843.0L, 0, fr_843_ + 3);
    strtold_verify_with_rounding_mode(syn_843_, FE_TONEAREST, 843.0L, 0, syn_843_ + 3);
    strtold_verify_with_rounding_mode(syn_trunc_843_, FE_TONEAREST, 843.0L, 0, syn_trunc_843_ + 3);
  }

  if (setlocale(LC_ALL, "fr_FR") != NULL || setlocale(LC_ALL, "fr_FR.UTF-8") != NULL) {
    lc = localeconv();
    T_EXPECT_EQ_STR(lc->decimal_point, ",", "Expected fr_FR locale to have ',' as decimal point");
    strtold_verify_with_rounding_mode(us_123_456, FE_TONEAREST, 123.0L, 0, us_123_456 + 3);
    strtold_verify_with_rounding_mode(fr_123_456, FE_TONEAREST, 123.456L, 0, fr_123_456 + 7);
    strtold_verify_with_rounding_mode(syn_123_456, FE_TONEAREST, 123.0L, 0, syn_123_456 + 3);
    strtold_verify_with_rounding_mode(syn_trunc_123_456, FE_TONEAREST, 123.0L, 0, syn_trunc_123_456 + 3);
    strtold_verify_with_rounding_mode(us__279, FE_TONEAREST, 0.0L, 0, us__279);
    strtold_verify_with_rounding_mode(fr__279, FE_TONEAREST, 0.279L, 0, fr__279 + 4);
    strtold_verify_with_rounding_mode(syn__279, FE_TONEAREST, 0.0L, 0, syn__279);
    strtold_verify_with_rounding_mode(syn_trunc__279, FE_TONEAREST, 0.0L, 0, syn_trunc__279);
    strtold_verify_with_rounding_mode(us_843_, FE_TONEAREST, 843.0L, 0, us_843_ + 3);
    strtold_verify_with_rounding_mode(fr_843_, FE_TONEAREST, 843.0L, 0, fr_843_ + 4);
    strtold_verify_with_rounding_mode(syn_843_, FE_TONEAREST, 843.0L, 0, syn_843_ + 3);
    strtold_verify_with_rounding_mode(syn_trunc_843_, FE_TONEAREST, 843.0L, 0, syn_trunc_843_ + 3);
  }

  if (setlocale(LC_ALL, "en_US") != NULL || setlocale(LC_ALL, "en_US.UTF-8") != NULL) {
    lc = localeconv();
    lc->decimal_point = "%$";
    lc = localeconv();
    T_EXPECT_EQ_STR(lc->decimal_point, "%$", "Expected to be able to configure locale with '%%$' as decimal point");
    strtold_verify_with_rounding_mode(us_123_456, FE_TONEAREST, 123.0L, 0, us_123_456 + 3);
    strtold_verify_with_rounding_mode(fr_123_456, FE_TONEAREST, 123.0L, 0, fr_123_456 + 3);
    strtold_verify_with_rounding_mode(syn_123_456, FE_TONEAREST, 123.456L, 0, syn_123_456 + 8);
    strtold_verify_with_rounding_mode(syn_trunc_123_456, FE_TONEAREST, 123.0L, 0, syn_trunc_123_456 + 3);
    strtold_verify_with_rounding_mode(us__279, FE_TONEAREST, 0.0L, 0, us__279);
    strtold_verify_with_rounding_mode(fr__279, FE_TONEAREST, 0.0L, 0, fr__279);
    strtold_verify_with_rounding_mode(syn__279, FE_TONEAREST, 0.279L, 0, syn__279 + 5);
    strtold_verify_with_rounding_mode(syn_trunc__279, FE_TONEAREST, 0.0L, 0, syn_trunc__279);
    strtold_verify_with_rounding_mode(us_843_, FE_TONEAREST, 843.0L, 0, us_843_ + 3);
    strtold_verify_with_rounding_mode(fr_843_, FE_TONEAREST, 843.0L, 0, fr_843_ + 3);
    strtold_verify_with_rounding_mode(syn_843_, FE_TONEAREST, 843.0L, 0, syn_843_ + 5);
    strtold_verify_with_rounding_mode(syn_trunc_843_, FE_TONEAREST, 843.0L, 0, syn_trunc_843_ + 3);
    lc->decimal_point = ".";
  }

  (void)setlocale(LC_ALL, "");
  (void)setlocale(LC_ALL, "POSIX");
  (void)setlocale(LC_ALL, "C");
}
