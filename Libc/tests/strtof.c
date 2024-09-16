#include <fenv.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdlib.h>

#include <darwintest.h>

#pragma STDC FENV_ACCESS ON

static void strtof_verify_with_rounding_mode(const char *src, int mode, float e, int expected_errno, const char *expected_end) {
  union {
    uint32_t raw;
    float f;
  } actual, expected;
  errno = 0;
  expected.f = e;

  fesetround(mode);
  const char *mode_name =
    (mode == FE_TONEAREST) ? "TONEAREST" :
    (mode == FE_DOWNWARD) ? "DOWNWARD" :
    (mode == FE_UPWARD) ? "UPWARD" :
    (mode == FE_TOWARDZERO) ? "TOWARDZERO" :
    "<UNKNOWN MODE>";

  char *end = NULL;
  actual.f = strtof(src, &end);
  int actual_errno = errno;
  int failed = 0;
  
  // Note: Verify the bits, not the FP value
  // In particular, this correctly validates NaNs and signed zeros
  if (actual.raw != expected.raw) {
    T_FAIL("Parsed value mismatch: "
           "Input \"%s\", mode = %s, actual = %.17g %a (0x%08x), expected = %.17g %a (x%08x)\n",
           src, mode_name,
           (double)actual.f, (double)actual.f, actual.raw,
           (double)expected.f, (double)expected.f, expected.raw);
    failed = 1;
  }

  switch (fpclassify(expected.f)) {
  case FP_SUBNORMAL:
    expected_errno = ERANGE;
  }

  if (actual_errno != expected_errno) {
    T_FAIL("Errno mismatch: "
           "input \"%s\", mode= %s, "
           "result = %.15g %a (0x%08x), "
           "actual errno = %d, expected errno = %d",
           src, mode_name,
           (double)actual.f, (double)actual.f, actual.raw,
           actual_errno, expected_errno);
    failed = 1;
  }
  if (end != expected_end) {
    T_FAIL("End did not match end of string: input \"%s\", mode = %s, end = \"%s\", expected end = \"%s\"\n",
           src, mode_name, end, expected_end);
    failed = 1;
  }

  {
    // "errno should never be set to zero by any library function"
    errno = 999;
    (void)strtof(src, NULL);
    if (errno == 0) {
      T_FAIL("Errno forced to zero for input \"%s\", mode = %s", src, mode_name);
      failed = 1;
    }
  }

  // Adding non-relevant characters to the end should not affect how it got parsed.
  // In particular, the updated `end` should point to the same offset.
  char *buff = malloc(strlen(src) + 10);
  const char *extras = strchr(src, '.') != NULL ? "Q+%-&!x,." : "Q+%-&!x,";
  for (size_t i = 0; i < strlen(extras); i++) {
    strcpy(buff, src);
    size_t s = strlen(src);
    buff[s] = extras[i];
    buff[s + 1] = '\0';
    expected.f = actual.f; // We expect the same value as before
    end = NULL;
    actual.f = strtof(buff, &end);
    if (actual.raw != expected.raw) {
      T_FAIL("Adding %c to end of \"%s\" changed result from "
             "%.17g %a (0x%08x) to %.17g %a (0x%08x)",
             extras[i], buff,
             (double)expected.f, (double)expected.f, expected.raw,
             (double)actual.f, (double)actual.f, actual.raw);
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

static void strtof_verify(const char *src, float expected_nearest, float expected_down, float expected_up) {
  // Note that the `src` here is never an explicit zero or infinity,
  // so any zero or subnormal result is an underflow, and any infinite
  // result is an overflow.
  
  int expected_errno = isnormal(expected_nearest) ? 0 : ERANGE;
  strtof_verify_with_rounding_mode(src, FE_TONEAREST, expected_nearest, expected_errno, src + strlen(src));
  expected_errno = isnormal(expected_down) ? 0 : ERANGE;
  strtof_verify_with_rounding_mode(src, FE_DOWNWARD, expected_down, expected_errno, src + strlen(src));
  expected_errno = isnormal(expected_up) ? 0 : ERANGE;
  strtof_verify_with_rounding_mode(src, FE_UPWARD, expected_up, expected_errno, src + strlen(src));
  float expected = signbit(expected_nearest) ? expected_up : expected_down;
  expected_errno = isnormal(expected) ? 0 : ERANGE;
  strtof_verify_with_rounding_mode(src, FE_TOWARDZERO, expected, expected_errno, src + strlen(src));
}

static void strtof_verify_overflow(const char *src, float expected_nearest, float expected_down, float expected_up) {
  int expected_errno = ERANGE;
  strtof_verify_with_rounding_mode(src, FE_TONEAREST, expected_nearest, expected_errno, src + strlen(src));
  strtof_verify_with_rounding_mode(src, FE_DOWNWARD, expected_down, expected_errno, src + strlen(src));
  strtof_verify_with_rounding_mode(src, FE_UPWARD, expected_up, expected_errno, src + strlen(src));
  float expected = signbit(expected_nearest) ? expected_up : expected_down;
  strtof_verify_with_rounding_mode(src, FE_TOWARDZERO, expected, expected_errno, src + strlen(src));
}

// Explicit zero, infinity, or Nan inputs never trigger ERANGE...
static void strtof_verify_no_overunder(const char *src, float expected) {
  strtof_verify_with_rounding_mode(src, FE_TONEAREST, expected, 0, src + strlen(src));
  strtof_verify_with_rounding_mode(src, FE_DOWNWARD, expected, 0, src + strlen(src));
  strtof_verify_with_rounding_mode(src, FE_UPWARD, expected, 0, src + strlen(src));
  strtof_verify_with_rounding_mode(src, FE_TOWARDZERO, expected, 0, src + strlen(src));
}
static void strtof_verify_infinity(const char *src, float expected) {
  strtof_verify_no_overunder(src, expected);
}
static void strtof_verify_zero(const char *src, float expected) {
  strtof_verify_no_overunder(src, expected);
}
static void strtof_verify_nan(const char *src, float expected) {
  strtof_verify_no_overunder(src, expected);
}

T_DECL(strtof, "strtof(3)")
{
  static const float min_subnormal = 0x1p-149f;
  // static const float min_subnormal_succ = 0x2p-149f;
  static const float max_subnormal_pred = 0x0.fffffcp-126f;
  static const float max_subnormal = 0x0.fffffep-126f;
  static const float min_normal = 0x1p-126f;
  static const float min_normal_succ = 0x1.000002p-126f;
  static const float max_normal_pred = 0x1.fffffcp+127f;
  static const float max_normal = 0x1.fffffep+127f;
  static const float infinity = HUGE_VALF;

  static const float pi_pred = 0x1.921fb4p+1;
  static const float pi = 0x1.921fb6p+1;
  static const float pi_succ = 0x1.921fb8p+1;

  // Sanity-test the environment
  T_EXPECT_FALSE(isnormal(max_subnormal), "isnormal(max_subnormal) should be false");
  T_EXPECT_TRUE(isnormal(min_normal), "isnormal(min_normal) should be true");
  T_EXPECT_TRUE(isnormal(max_normal), "isnormal(max_normal) should be true");
  T_EXPECT_TRUE(isinf(infinity), "isinf(infinity) should be true");
  T_EXPECT_FALSE(isinf(min_normal), "isinf(min_normal) should be false");

  strtof_verify("1.0", 1.0f, 1.0f, 1.0f);
  strtof_verify("+1.0", 1.0f, 1.0f, 1.0f);
  strtof_verify(" 1.0", 1.0f, 1.0f, 1.0f);
  strtof_verify("  1.0", 1.0f, 1.0f, 1.0f);
  strtof_verify("\t1.0", 1.0f, 1.0f, 1.0f);
  strtof_verify("\n1.0", 1.0f, 1.0f, 1.0f);
  strtof_verify("\v1.0", 1.0f, 1.0f, 1.0f);
  strtof_verify("\f1.0", 1.0f, 1.0f, 1.0f);
  strtof_verify("\r1.0", 1.0f, 1.0f, 1.0f);
  strtof_verify(" \t\n\v\f\r1.0", 1.0f, 1.0f, 1.0f);
  strtof_verify("1e-45", min_subnormal, 0.0f, min_subnormal);
  strtof_verify("-1e-45", -min_subnormal, -min_subnormal, -0.0f);
  strtof_verify("11754942e-45", max_subnormal, max_subnormal_pred, max_subnormal);
  strtof_verify("117549432e-46", min_normal, max_subnormal, min_normal);
  strtof_verify("11754944e-45", min_normal, min_normal, min_normal_succ);
  strtof_verify("34028235e+31", max_normal, max_normal, infinity);
  strtof_verify("-34028235e+31", -max_normal, -infinity, -max_normal);

  // 1 less than max_normal
  strtof_verify("340282346638528859811704183484516925439", max_normal, max_normal_pred, max_normal);
  // Exact max_normal
  strtof_verify("340282346638528859811704183484516925440", max_normal, max_normal, max_normal);
  // 1 more than max_normal
  strtof_verify("340282346638528859811704183484516925441", max_normal, max_normal, infinity);

  // 1 less than exact midpoint between max_normal and max_normal + 1 ULP
  // (largest integer that rounds-to-nearest to max_normal)
  strtof_verify("340282356779733661637539395458142568447", max_normal, max_normal, infinity);
  // Even closer to exact midpoint, but still below
  strtof_verify("340282356779733661637539395458142568447.9999999999", max_normal, max_normal, infinity);
  // Exact midpoint between max_normal and max_normal + 1 ULP (rounds even to overflow)
  strtof_verify("340282356779733661637539395458142568448", infinity, max_normal, infinity);
  // 1 more than exact midpoint between max_normal and max_normal + 1 ULP
  strtof_verify("340282356779733661637539395458142568449", infinity, max_normal, infinity);
  strtof_verify("340282356779733661637539395458142568449.00", infinity, max_normal, infinity);

  // 1 less than max_normal + 1 ULP
  // (Largest integer less than overflow threshold == largest integer that rounds down to max_normal)
  strtof_verify("340282366920938463463374607431768211455", infinity, max_normal, infinity);
  //  Very, very close to the overflow threshold, but still rounds down to max_normal
  strtof_verify("340282366920938463463374607431768211455."
                "9999999999999999999999999999999999999999"
                "9999999999999999999999999999999999999999"
                "9999999999999999999999999999999999999999",
                infinity, max_normal, infinity);
  // max_normal + 1 ULP == 2^128 == overflow threshold
  // This is treated as overflow in all rounding modes.
  strtof_verify_overflow("340282366920938463463374607431768211456", infinity, max_normal, infinity);
  strtof_verify_overflow("340282366920938463463374607431768211456.00000", infinity, max_normal, infinity);

  // Successively larger than max normal
  strtof_verify("3.4028235e+38", max_normal, max_normal, infinity);
  strtof_verify("3.4028236e+38", infinity, max_normal, infinity);
  strtof_verify_overflow("3.4028237e+38", infinity, max_normal, infinity);
  strtof_verify_overflow("3.402824e+38", infinity, max_normal, infinity);
  strtof_verify_overflow("3.40283e+38", infinity, max_normal, infinity);
  strtof_verify_overflow("3.4029e+38", infinity, max_normal, infinity);
  strtof_verify_overflow("3.403e+38", infinity, max_normal, infinity);
  strtof_verify_overflow("3.41e+38", infinity, max_normal, infinity);
  strtof_verify_overflow("3.5e+38", infinity, max_normal, infinity);
  strtof_verify_overflow("4e38", infinity, max_normal, infinity);

  strtof_verify("3.1415927", pi, pi_pred, pi);
  strtof_verify("3.141592653589793238462643383279502884197169399"
         "375105820974944592307816406286208998628034825342117", pi, pi_pred, pi);
  strtof_verify("0x1.921fb6p+1", pi, pi, pi);
  strtof_verify("0x1.921fb7p+1", pi_succ, pi, pi_succ);
  strtof_verify("0x1.921fb600000000000000000000000p+1", pi, pi, pi);
  strtof_verify("0x1.921fb60000000000000000000001p+1", pi, pi, pi_succ);
  strtof_verify("-0x1.921fb6p+1", -pi, -pi, -pi);
  strtof_verify("-0x1.921fb60000000000000000000001p+1", -pi, -pi_succ, -pi);

  strtof_verify("20768701875e-4", 0x1.fb0c64p+20f, 0x1.fb0c62p+20f, 0x1.fb0c64p+20f);
  strtof_verify("115417771911e-23", 0x1.44df46p-40f, 0x1.44df44p-40f, 0x1.44df46p-40f);
  strtof_verify("148848731621e20", 0x1.77bf3ep103f, 0x1.77bf3cp103f, 0x1.77bf3ep103f);
  strtof_verify("161046205767e-46", 0x1.5681e6p-116f, 0x1.5681e4p-116f, 0x1.5681e6p-116f);
  strtof_verify("71.40359474881643", 0x1.1d9d48p+6f, 0x1.1d9d46p+6f, 0x1.1d9d48p+6f);

  strtof_verify("123456.7890123e-4789", 0.0, 0.0, min_subnormal);
  strtof_verify("-8e-9999999999999999999999999999999999", -0.0, -min_subnormal, -0.0);
  strtof_verify("0xfp-999999999999999999999999999999999", 0.0, 0.0, min_subnormal);
  strtof_verify("-0xfedcba987654321p-987654", -0.0, -min_subnormal, -0.0);

  // Overflow cases
  strtof_verify_overflow("123456.7890123e+4789", infinity, max_normal, infinity);
  strtof_verify_overflow("1e309", infinity, max_normal, infinity);
  strtof_verify_overflow("-1e+99999999999999999999999999999999", -infinity, -infinity, -max_normal);
  strtof_verify_overflow("-1e309", -infinity, -infinity, -max_normal);
  strtof_verify_overflow("0x123456789abcdefp9999999999999999999999999999", infinity, max_normal, infinity);
  strtof_verify_overflow("0x123456789abcdefp123456789", infinity, max_normal, infinity);
  strtof_verify_overflow("-0x123456789abcdefp9999999999999999999999999999", -infinity, -infinity, -max_normal);
  strtof_verify_overflow("-0x123456789abcdefp123456789", -infinity, -infinity, -max_normal);
  strtof_verify_overflow("999999999999999999999999999999999999999.999999999999999999999999999", infinity, max_normal, infinity);
  strtof_verify_overflow("7674047411400702925974988342550565582448.117", infinity, max_normal, infinity);

  // Nan parsing
  strtof_verify_nan("NaN", nanf(""));
  strtof_verify_nan("-NaN", -nanf(""));
  strtof_verify_nan("nan", nanf(""));
  strtof_verify_nan("NAN", nanf(""));
  strtof_verify_nan("nAn", nanf(""));
  strtof_verify_nan("NaN()", nanf(""));
  strtof_verify_nan("nan(1)", nanf("1"));
  strtof_verify_nan("NaN(011)", nanf("9"));
  strtof_verify_nan("NaN(0x11)", nanf("0x11"));
  strtof_verify_nan("NaN(11)", nanf("0xb"));
  strtof_verify_nan("nan(0xffffffffffffffffffffff9)", nanf("0x7fffffffffff9"));

  // Spellings of infinity
  strtof_verify_infinity("inf", infinity);
  strtof_verify_infinity("InF", infinity);
  strtof_verify_infinity("iNf", infinity);
  strtof_verify_infinity("+inf", infinity);
  strtof_verify_infinity("-InF", -infinity);

  strtof_verify_infinity("InFiNiTy", infinity);
  strtof_verify_infinity("iNfInItY", infinity);
  strtof_verify_infinity("-infinity", -infinity);
  strtof_verify_infinity("+infinity", infinity);
  const char *infinite_string = "infinite";
  strtof_verify_with_rounding_mode(infinite_string, FE_TONEAREST, infinity, 0, infinite_string + 3);

  // Spellings of zero
  strtof_verify_zero("0", 0.0f);
  strtof_verify_zero("+0", 0.0f);
  strtof_verify_zero("+0000000.0000000e0000000", 0.0f);
  strtof_verify_zero("0e99999999999999999999999999", 0.0f);
  strtof_verify_zero("-0.0e0", -0.0f);
  strtof_verify_zero("-0", -0.0f);
  strtof_verify_zero("-.0", -0.0f);
  strtof_verify_zero("-.0e+0", -0.0f);
  strtof_verify_zero("-.0e-0", -0.0f);
}

T_DECL(strtof_locale, "strtof(3) locale support")
{
  // Literals that we'll test against:
  //  Include values with a decimal point in the beginning, middle, and end.
  //  Include values for C/en_US, fr_FR, and a synthesized locale that uses a multi-byte decimal point
  //  Include values with a malformed multi-byte decimal point
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
  strtof_verify_with_rounding_mode(us_123_456, FE_TONEAREST, 123.456f, 0, us_123_456 + 7);
  strtof_verify_with_rounding_mode(fr_123_456, FE_TONEAREST, 123.0f, 0, fr_123_456 + 3);
  strtof_verify_with_rounding_mode(syn_123_456, FE_TONEAREST, 123.0f, 0, syn_123_456 + 3);
  strtof_verify_with_rounding_mode(syn_trunc_123_456, FE_TONEAREST, 123.0f, 0, syn_trunc_123_456 + 3);
  strtof_verify_with_rounding_mode(us__279, FE_TONEAREST, 0.279f, 0, us__279 + 4);
  strtof_verify_with_rounding_mode(fr__279, FE_TONEAREST, 0.0f, 0, fr__279);
  strtof_verify_with_rounding_mode(syn__279, FE_TONEAREST, 0.0f, 0, syn__279);
  strtof_verify_with_rounding_mode(syn_trunc__279, FE_TONEAREST, 0.0f, 0, syn_trunc__279);
  strtof_verify_with_rounding_mode(us_843_, FE_TONEAREST, 843.0f, 0, us_843_ + 4);
  strtof_verify_with_rounding_mode(fr_843_, FE_TONEAREST, 843.0f, 0, fr_843_ + 3);
  strtof_verify_with_rounding_mode(syn_843_, FE_TONEAREST, 843.0f, 0, syn_843_ + 3);
  strtof_verify_with_rounding_mode(syn_trunc_843_, FE_TONEAREST, 843.0f, 0, syn_trunc_843_ + 3);

  if (setlocale(LC_ALL, "en_US") != NULL || setlocale(LC_ALL, "en_US.UTF-8") != NULL) {
    lc = localeconv();
    T_EXPECT_EQ_STR(lc->decimal_point, ".", "Expected en_US locale to have '.' as decimal point");
    strtof_verify_with_rounding_mode(us_123_456, FE_TONEAREST, 123.456f, 0, us_123_456 + 7);
    strtof_verify_with_rounding_mode(fr_123_456, FE_TONEAREST, 123.0f, 0, fr_123_456 + 3);
    strtof_verify_with_rounding_mode(syn_123_456, FE_TONEAREST, 123.0f, 0, syn_123_456 + 3);
    strtof_verify_with_rounding_mode(syn_trunc_123_456, FE_TONEAREST, 123.0f, 0, syn_trunc_123_456 + 3);
    strtof_verify_with_rounding_mode(us__279, FE_TONEAREST, 0.279f, 0, us__279 + 4);
    strtof_verify_with_rounding_mode(fr__279, FE_TONEAREST, 0.0f, 0, fr__279);
    strtof_verify_with_rounding_mode(syn__279, FE_TONEAREST, 0.0f, 0, syn__279);
    strtof_verify_with_rounding_mode(syn_trunc__279, FE_TONEAREST, 0.0f, 0, syn_trunc__279);
    strtof_verify_with_rounding_mode(us_843_, FE_TONEAREST, 843.0f, 0, us_843_ + 4);
    strtof_verify_with_rounding_mode(fr_843_, FE_TONEAREST, 843.0f, 0, fr_843_ + 3);
    strtof_verify_with_rounding_mode(syn_843_, FE_TONEAREST, 843.0f, 0, syn_843_ + 3);
    strtof_verify_with_rounding_mode(syn_trunc_843_, FE_TONEAREST, 843.0f, 0, syn_trunc_843_ + 3);
  }

  if (setlocale(LC_ALL, "fr_FR") != NULL || setlocale(LC_ALL, "fr_FR.UTF-8") != NULL) {
    lc = localeconv();
    T_EXPECT_EQ_STR(lc->decimal_point, ",", "Expected fr_FR locale to have ',' as decimal point");
    strtof_verify_with_rounding_mode(us_123_456, FE_TONEAREST, 123.0f, 0, us_123_456 + 3);
    strtof_verify_with_rounding_mode(fr_123_456, FE_TONEAREST, 123.456f, 0, fr_123_456 + 7);
    strtof_verify_with_rounding_mode(syn_123_456, FE_TONEAREST, 123.0f, 0, syn_123_456 + 3);
    strtof_verify_with_rounding_mode(syn_trunc_123_456, FE_TONEAREST, 123.0f, 0, syn_trunc_123_456 + 3);
    strtof_verify_with_rounding_mode(us__279, FE_TONEAREST, 0.0f, 0, us__279);
    strtof_verify_with_rounding_mode(fr__279, FE_TONEAREST, 0.279f, 0, fr__279 + 4);
    strtof_verify_with_rounding_mode(syn__279, FE_TONEAREST, 0.0f, 0, syn__279);
    strtof_verify_with_rounding_mode(syn_trunc__279, FE_TONEAREST, 0.0f, 0, syn_trunc__279);
    strtof_verify_with_rounding_mode(us_843_, FE_TONEAREST, 843.0f, 0, us_843_ + 3);
    strtof_verify_with_rounding_mode(fr_843_, FE_TONEAREST, 843.0f, 0, fr_843_ + 4);
    strtof_verify_with_rounding_mode(syn_843_, FE_TONEAREST, 843.0f, 0, syn_843_ + 3);
    strtof_verify_with_rounding_mode(syn_trunc_843_, FE_TONEAREST, 843.0f, 0, syn_trunc_843_ + 3);
  }

  if (setlocale(LC_ALL, "en_US") != NULL || setlocale(LC_ALL, "en_US.UTF-8") != NULL) {
    lc = localeconv();
    lc->decimal_point = "%$";
    lc = localeconv();
    T_EXPECT_EQ_STR(lc->decimal_point, "%$", "Expected to be able to configure locale with '%%$' as decimal point");
    strtof_verify_with_rounding_mode(us_123_456, FE_TONEAREST, 123.0f, 0, us_123_456 + 3);
    strtof_verify_with_rounding_mode(fr_123_456, FE_TONEAREST, 123.0f, 0, fr_123_456 + 3);
    strtof_verify_with_rounding_mode(syn_123_456, FE_TONEAREST, 123.456f, 0, syn_123_456 + 8);
    strtof_verify_with_rounding_mode(syn_trunc_123_456, FE_TONEAREST, 123.0f, 0, syn_trunc_123_456 + 3);
    strtof_verify_with_rounding_mode(us__279, FE_TONEAREST, 0.0f, 0, us__279);
    strtof_verify_with_rounding_mode(fr__279, FE_TONEAREST, 0.0f, 0, fr__279);
    strtof_verify_with_rounding_mode(syn__279, FE_TONEAREST, 0.279f, 0, syn__279 + 5);
    strtof_verify_with_rounding_mode(syn_trunc__279, FE_TONEAREST, 0.0f, 0, syn_trunc__279);
    strtof_verify_with_rounding_mode(us_843_, FE_TONEAREST, 843.0f, 0, us_843_ + 3);
    strtof_verify_with_rounding_mode(fr_843_, FE_TONEAREST, 843.0f, 0, fr_843_ + 3);
    strtof_verify_with_rounding_mode(syn_843_, FE_TONEAREST, 843.0f, 0, syn_843_ + 5);
    strtof_verify_with_rounding_mode(syn_trunc_843_, FE_TONEAREST, 843.0f, 0, syn_trunc_843_ + 3);
    lc->decimal_point = ".";
  }

  (void)setlocale(LC_ALL, "");
  (void)setlocale(LC_ALL, "POSIX");
  (void)setlocale(LC_ALL, "C");
}
