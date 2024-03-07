#include <fenv.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <xlocale.h>

#include <darwintest.h>

#pragma STDC FENV_ACCESS ON

static void strtod_verify_with_rounding_mode(const char *src, int mode, double e, int expected_errno, const char *expected_end, const char *detail) {
  union {
    uint64_t raw;
    double d;
  } actual, expected;
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
  actual.d = strtod(src, &end);
  int actual_errno = errno;
  int failed = 0;

  // Note: Verify the bits, not the FP value!
  // In particular, this correctly validates NaNs and signed zeros
  if (actual.raw != expected.raw) {
    T_FAIL("Parsed value mismatch: "
           "Input \"%s\", mode = %s, "
           "actual = %.25g %a (0x%016llx), "
           "expected = %.25g %a (0x%016llx) %s",
           src, mode_name,
           actual.d, actual.d, actual.raw,
           expected.d, expected.d, expected.raw,
           detail);
    failed = 1;
  }

  if (actual_errno != expected_errno) {
    T_FAIL("Errno mismatch: "
           "input \"%s\", mode = %s, "
           "result = %.25g %a (0x%016llx), "
           "actual errno = %d, expected errno = %d %s",
           src, mode_name,
           actual.d, actual.d, actual.raw,
           actual_errno, expected_errno,
           detail);
    failed = 1;
  }

  if (end != expected_end) {
    T_FAIL("End did not match: "
           "input \"%s\", mode = %s, "
           "end = \"%s\", expected end = \"%s\" %s",
           src, mode_name, end, expected_end, detail);
    failed = 1;
  }

  {
    // "errno should never be set to zero by any library function"
    errno = 999;
    (void)strtod(src, NULL);
    if (errno == 0) {
      T_FAIL("Errno forced to zero for input \"%s\", mode = %s %s", src, mode_name, detail);
      failed = 1;
    }
  }

  // Adding non-relevant characters to the end should not affect how it got parsed.
  // In particular, the updated `end` should point to the same offset.
  char *buff = malloc(strlen(src) + 10);
  // Redundant '.' should be ignored if there's already a '.'
  const char *extras = strchr(src, '.') != NULL ? "Q+%-&!x.," : "Q+%-&!x," ;
  for (size_t i = 0; i < strlen(extras); i++) {
    strcpy(buff, src);
    size_t s = strlen(src);
    buff[s] = extras[i];
    buff[s + 1] = '\0';
    expected.d = actual.d; // We expect the same value as before
    end = NULL;
    actual.d = strtod(buff, &end);
    if (actual.raw != expected.raw) {
      T_FAIL("Adding %c to end of \"%s\" changed result from %.25g %a (0x%016llx) to %.25g %a (0x%016llx)",
             extras[i], buff, expected.d, expected.d, expected.raw, actual.d, actual.d, actual.raw);
      failed = 1;
    }
    if (end != buff + (expected_end - src)) {
      T_FAIL("End did not update correctly for input \"%s\", end = \"%s\" (buff + %d) %s",
             buff, end, (int)(end - buff), detail);
      failed = 1;
    }
  }
  free(buff);

  if (!failed) {
    T_PASS("%s: %s %s", mode_name, src, detail);
  }
}

static void strtod_verify(const char *src, double expected_nearest, double expected_down, double expected_up) {
  // Literal infinities get tested with strtod_verify_infinity below, so any infinity here is really an overflow
  int expected_errno = isnormal(expected_nearest) ? 0 : ERANGE;
  strtod_verify_with_rounding_mode(src, FE_TONEAREST, expected_nearest, expected_errno, src + strlen(src), "");
  expected_errno = isnormal(expected_down) ? 0 : ERANGE;
  strtod_verify_with_rounding_mode(src, FE_DOWNWARD, expected_down, expected_errno, src + strlen(src), "");
  expected_errno = isnormal(expected_up) ? 0 : ERANGE;
  strtod_verify_with_rounding_mode(src, FE_UPWARD, expected_up, expected_errno, src + strlen(src), "");
  double expected = signbit(expected_up) ? expected_up : expected_down;
  expected_errno = isnormal(expected) ? 0 : ERANGE;
  strtod_verify_with_rounding_mode(src, FE_TOWARDZERO, expected, expected_errno, src + strlen(src), "");
}

static void strtod_verify_noerange(const char *src, double expected_nearest, double expected_down, double expected_up) {
  int expected_errno = 0;
  strtod_verify_with_rounding_mode(src, FE_TONEAREST, expected_nearest, expected_errno, src + strlen(src), "");
  strtod_verify_with_rounding_mode(src, FE_DOWNWARD, expected_down, expected_errno, src + strlen(src), "");
  strtod_verify_with_rounding_mode(src, FE_UPWARD, expected_up, expected_errno, src + strlen(src), "");
  double expected = signbit(expected_up) ? expected_up : expected_down;
  strtod_verify_with_rounding_mode(src, FE_TOWARDZERO, expected, expected_errno, src + strlen(src), "");
}

// Explicit zero, infinity, or Nan inputs never trigger ERANGE...
static void strtod_verify_no_overunder(const char *src, double expected) {
  strtod_verify_with_rounding_mode(src, FE_TONEAREST, expected, 0, src + strlen(src), "");
  strtod_verify_with_rounding_mode(src, FE_DOWNWARD, expected, 0, src + strlen(src), "");
  strtod_verify_with_rounding_mode(src, FE_UPWARD, expected, 0, src + strlen(src), "");
  strtod_verify_with_rounding_mode(src, FE_TOWARDZERO, expected, 0, src + strlen(src), "");
}
static void strtod_verify_infinity(const char *src, double expected) {
  strtod_verify_no_overunder(src, expected);
}
static void strtod_verify_zero(const char *src, double expected) {
  strtod_verify_no_overunder(src, expected);
}
static void strtod_verify_nan(const char *src, double expected) {
  strtod_verify_no_overunder(src, expected);
}

// TODO: test hexfloat parsing of large integers

T_DECL(strtod, "strtod(3)")
{
  static const double min_subnormal = 0x1p-1074;
  static const double min_subnormal_succ = 0x2p-1074;
  static const double max_subnormal = 0x0.fffffffffffffp-1022;
  static const double min_normal = 0x1p-1022;
  static const double max_normal_pred = 0x1.ffffffffffffep+1023;
  static const double max_normal = 0x1.fffffffffffffp+1023;
  static const double infinity = HUGE_VAL;

  static const double pi_pred = 0x1.921fb54442d17p+1;
  static const double pi = 0x1.921fb54442d18p+1;
  static const double pi_succ = 0x1.921fb54442d19p+1;

  // Sanity-test the environment
  T_EXPECT_FALSE(isnormal(max_subnormal), "isnormal(max_subnormal) should be false");
  T_EXPECT_TRUE(isnormal(min_normal), "isnormal(min_normal) should be true");
  T_EXPECT_TRUE(isnormal(max_normal), "isnormal(max_normal) should be true");
  T_EXPECT_TRUE(isinf(infinity), "isinf(infinity) should be true");
  T_EXPECT_FALSE(isinf(min_normal), "isinf(min_normal) should be false");

  strtod_verify("1.0", 1.0, 1.0, 1.0);
  strtod_verify(" 1.0", 1.0, 1.0, 1.0);
  strtod_verify("  1.0", 1.0, 1.0, 1.0);
  strtod_verify("\t1.0", 1.0, 1.0, 1.0);
  strtod_verify("\n1.0", 1.0, 1.0, 1.0);
  strtod_verify("\v1.0", 1.0, 1.0, 1.0);
  strtod_verify("\f1.0", 1.0, 1.0, 1.0);
  strtod_verify("\r1.0", 1.0, 1.0, 1.0);
  strtod_verify(" \t\n\v\f\r1.0", 1.0, 1.0, 1.0);
  strtod_verify("5e-324", min_subnormal, min_subnormal, min_subnormal_succ);
  strtod_verify("-5e-324", -min_subnormal, -min_subnormal_succ, -min_subnormal);
  strtod_verify("1.11253692925360069154511635866620203211e-308", min_normal / 2, min_normal / 2, 0x1.0000000000002p-1023);
  strtod_verify("2225073858507201e-323", max_subnormal, max_subnormal, min_normal);
  strtod_verify("2.225073858507201e-308", max_subnormal, max_subnormal, min_normal);
  strtod_verify("2.225073858507201137e-308", min_normal, max_subnormal, min_normal);
  strtod_verify("2.22507385850720114e-308", min_normal, max_subnormal, min_normal);
  strtod_verify("2.2250738585072012e-308", min_normal, max_subnormal, min_normal);
  strtod_verify("2.2250738585072013e-308", min_normal, max_subnormal, min_normal);
  strtod_verify("2.2250738585072014e-308", min_normal, min_normal, 0x1.0000000000001p-1022);
  strtod_verify("17976931348623157e292", max_normal, max_normal_pred, max_normal);
  // Tiny bit less than max normal
  strtod_verify("1.7976931348623157e308", max_normal, max_normal_pred, max_normal);
  // Tiny bit larger than max normal
  strtod_verify("1.7976931348623158e308", max_normal, max_normal, infinity);
  // Larger than midpoint between max normal and max normal + 1 ULP
  strtod_verify("1.7976931348623159e308", infinity, max_normal, infinity);
  // Larger than max normal + 1 ULP
  strtod_verify("1.797693134862316e308", infinity, infinity, infinity);
  strtod_verify("1.79769313486232e308", infinity, infinity, infinity);
  strtod_verify("1.7976931348624e308", infinity, infinity, infinity);
  strtod_verify("1.797693134863e308", infinity, infinity, infinity);
  strtod_verify("1.79769313487e308", infinity, infinity, infinity);
  strtod_verify("1.7976931349e308", infinity, infinity, infinity);
  strtod_verify("1.797693135e308", infinity, infinity, infinity);
  strtod_verify("1.79769314e308", infinity, infinity, infinity);
  strtod_verify("1.7976932e308", infinity, infinity, infinity);
  strtod_verify("1.797694e308", infinity, infinity, infinity);
  strtod_verify("1.7977e308", infinity, infinity, infinity);
  strtod_verify("1.798e308", infinity, infinity, infinity);
  strtod_verify("1.8e308", infinity, infinity, infinity);
  strtod_verify("1.9e308", infinity, infinity, infinity);
  strtod_verify("2e308", infinity, infinity, infinity);

  strtod_verify("3.141592653589793", pi, pi_pred, pi);
  strtod_verify("3.141592653589793238462643383279502884197169399"
         "375105820974944592307816406286208998628034825342117",
         pi, pi, pi_succ);
  strtod_verify("0x1.921fb54442d18p+1", pi, pi, pi);
  strtod_verify("0x1.921fb54442d188p+1", pi, pi, pi_succ);
  strtod_verify("0x1.921fb54442d1800000000000000000000000p+1", pi, pi, pi);
  strtod_verify("0x1.921fb54442d1800000000000000000000001p+1", pi, pi, pi_succ);
  strtod_verify("-0x1.921fb54442d18p+1", -pi, -pi, -pi);
  strtod_verify("-0x1.921fb54442d1800000000000000000000001p+1", -pi, -pi_succ, -pi);

  strtod_verify("2.47032822920623272088284396434110686182529901307162382212792841"
         "2503377536351043759326499181808179961898982823477228588654633283"
         "5517796989819938739800539093906315035659515570226392290858392449"
         "1051844359318028499365361525003193704576782492193656236698636584"
         "8075700158576926990370631192827955855133292783433840935197801553"
         "1246597263579574622766465272827220056374006485499977096599470454"
         "0208281662262378573934507363390079677619305775067401763246736009"
         "6895134053553745851666113422376667860416215968046191446729184030"
         "0530057530849048765391711386591646239524912623653881879636239373"
         "2804238910186723484976682350898633885879256283027559956575244555"
         "0725518931369083625477918694866799496832404970582102851318545139"
         "6213837722826145437693412532098591327667236328125e-324",
         0.0, 0.0, min_subnormal);
  strtod_verify("2.47032822920623272088284396434110686182529901307162382212792841"
         "2503377536351043759326499181808179961898982823477228588654633283"
         "5517796989819938739800539093906315035659515570226392290858392449"
         "1051844359318028499365361525003193704576782492193656236698636584"
         "8075700158576926990370631192827955855133292783433840935197801553"
         "1246597263579574622766465272827220056374006485499977096599470454"
         "0208281662262378573934507363390079677619305775067401763246736009"
         "6895134053553745851666113422376667860416215968046191446729184030"
         "0530057530849048765391711386591646239524912623653881879636239373"
         "2804238910186723484976682350898633885879256283027559956575244555"
         "0725518931369083625477918694866799496832404970582102851318545139"
         "6213837722826145437693412532098591327667236328125000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000"
         "00000000000000000000000000000000000000000000000000000000000e-324",
         0.0, 0.0, min_subnormal);
  strtod_verify("2.47032822920623272088284396434110686182529901307162382212792841"
         "2503377536351043759326499181808179961898982823477228588654633283"
         "5517796989819938739800539093906315035659515570226392290858392449"
         "1051844359318028499365361525003193704576782492193656236698636584"
         "8075700158576926990370631192827955855133292783433840935197801553"
         "1246597263579574622766465272827220056374006485499977096599470454"
         "0208281662262378573934507363390079677619305775067401763246736009"
         "6895134053553745851666113422376667860416215968046191446729184030"
         "0530057530849048765391711386591646239524912623653881879636239373"
         "2804238910186723484976682350898633885879256283027559956575244555"
         "0725518931369083625477918694866799496832404970582102851318545139"
         "6213837722826145437693412532098591327667236328125000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000"
         "0000000000000000000000000000000000000000000000000000000000000000"
         "00000000000000000000000000000000000000000000000000000000001e-324",
         min_subnormal, 0.0, min_subnormal);
  strtod_verify("2.47032822920623272088284396434110686182529901307162382212792841"
         "2503377536351043759326499181808179961898982823477228588654633283"
         "5517796989819938739800539093906315035659515570226392290858392449"
         "1051844359318028499365361525003193704576782492193656236698636584"
         "8075700158576926990370631192827955855133292783433840935197801553"
         "1246597263579574622766465272827220056374006485499977096599470454"
         "0208281662262378573934507363390079677619305775067401763246736009"
         "6895134053553745851666113422376667860416215968046191446729184030"
         "0530057530849048765391711386591646239524912623653881879636239373"
         "2804238910186723484976682350898633885879256283027559956575244555"
         "0725518931369083625477918694866799496832404970582102851318545139"
         "6213837722826145437693412532098591327667236328124999999999999999"
         "99999999999999999999999999999999999999999999999999999999999e-324",
         0.0, 0.0, min_subnormal);
  strtod_verify("7.41098468761869816264853189302332058547589703921487146638378523"
         "7510132609053131277979497545424539885696948470431685765963899850"
         "6553390969459816219401617281718945106978546710679176872575177347"
         "3155533077954085498096084575009581113730347476580968710095909754"
         "4227100475730780971111893578483867565399878350301522805593404659"
         "3739791790738723868299395818481660169122019456499931289798411362"
         "0624844986787135721803522090170239032857917325202205289740208029"
         "0685402160661237554998340267130003581248647904138574340187552090"
         "1590172592547146296175134159774938718574737870961645638908718119"
         "8412716730560170454930047052695901657637768849082679869725733665"
         "2176556794107250876433756084600398490497214911746308553955635418"
         "8641513168478436313080237596295773983001708984375e-324",
         min_subnormal_succ, min_subnormal, min_subnormal_succ);
  strtod_verify("7.41098468761869816264853189302332058547589703921487146638378523"
         "7510132609053131277979497545424539885696948470431685765963899850"
         "6553390969459816219401617281718945106978546710679176872575177347"
         "3155533077954085498096084575009581113730347476580968710095909754"
         "4227100475730780971111893578483867565399878350301522805593404659"
         "3739791790738723868299395818481660169122019456499931289798411362"
         "0624844986787135721803522090170239032857917325202205289740208029"
         "0685402160661237554998340267130003581248647904138574340187552090"
         "1590172592547146296175134159774938718574737870961645638908718119"
         "8412716730560170454930047052695901657637768849082679869725733665"
         "2176556794107250876433756084600398490497214911746308553955635418"
         "8641513168478436313080237596295773983001708984374999999999999999"
         "99999999999999999999999999999999999999999999999999999999999e-324",
         min_subnormal, min_subnormal, min_subnormal_succ);
  strtod_verify("7.41098468761869816264853189302332058547589703921487146638378523"
         "7510132609053131277979497545424539885696948470431685765963899850"
         "6553390969459816219401617281718945106978546710679176872575177347"
         "3155533077954085498096084575009581113730347476580968710095909754"
         "4227100475730780971111893578483867565399878350301522805593404659"
         "3739791790738723868299395818481660169122019456499931289798411362"
         "0624844986787135721803522090170239032857917325202205289740208029"
         "0685402160661237554998340267130003581248647904138574340187552090"
         "1590172592547146296175134159774938718574737870961645638908718119"
         "8412716730560170454930047052695901657637768849082679869725733665"
         "2176556794107250876433756084600398490497214911746308553955635418"
         "8641513168478436313080237596295773983001708984375000000000000000"
         "00000000000000000000000000000000000000000000000000000000001e-324",
         min_subnormal_succ, min_subnormal, min_subnormal_succ);

  // Subnormal hexfloats
  // rdar://108539918 - Only set erange for inexact subnormal hexfloats
  strtod_verify_noerange("0x0.8p-1022", 0x0.8p-1022, 0x0.8p-1022, 0x0.8p-1022);
  strtod_verify_noerange("0x0.5555555555555p-1022",
		0x0.5555555555555p-1022, 0x0.5555555555555p-1022, 0x0.5555555555555p-1022);
  strtod_verify_noerange("0x0.fffffffffffffp-1022", max_subnormal, max_subnormal, max_subnormal);
  // This is inexact, so should set ERANGE
  strtod_verify("0x0.555555555555555555p-1022",
		0x1.5555555555554p-1024, 0x1.5555555555554p-1024, 0x1.5555555555558p-1024);
  // Rounds up to min normal (no error), down to max subnormal (erange)
  static const char *hexfloat1 = "0x0.fffffffffffff8p-1022";
  strtod_verify_with_rounding_mode(hexfloat1, FE_TONEAREST, min_normal, 0, hexfloat1 + strlen(hexfloat1), "");
  strtod_verify_with_rounding_mode(hexfloat1, FE_UPWARD, min_normal, 0, hexfloat1 + strlen(hexfloat1), "");
  strtod_verify_with_rounding_mode(hexfloat1, FE_DOWNWARD, max_subnormal, ERANGE, hexfloat1 + strlen(hexfloat1), "");

  strtod_verify("123456.7890123e-4789", 0.0, 0.0, min_subnormal);
  strtod_verify("-8e-9999999999999999999999999999999999", -0.0, -min_subnormal, -0.0);
  strtod_verify("0xfp-999999999999999999999999999999999", 0.0, 0.0, min_subnormal);
  strtod_verify("-0xfedcba987654321p-987654", -0.0, -min_subnormal, -0.0);

  // Obvious overflows
  strtod_verify("123456.7890123e+4789", infinity, infinity, infinity);
  strtod_verify("1e309", infinity, infinity, infinity);
  strtod_verify("-1e+99999999999999999999999999999999", -infinity, -infinity, -infinity);
  strtod_verify("-1e309", -infinity, -infinity, -infinity);
  strtod_verify("0x123456789abcdefp9999999999999999999999999999", infinity, infinity, infinity);
  strtod_verify("0x123456789abcdefp123456789", infinity, infinity, infinity);
  // Borderline overflows
  // Exact 2^1024 is the max normal double + 1 ULP.  Standard rounding rounds
  // up in all cases, so we return infinity, so it must be considered overflow
  strtod_verify("1797693134862315907729305190789024733617976978942306572734300811"
              "5773267580550096313270847732240753602112011387987139335765878976"
              "8814416622492847430639474124377767893424865485276302219601246094"
              "1194530829520850057688381506823424628814739131105408272371633505"
              "10684586298239947245938479716304835356329624224137216",
              infinity, infinity, infinity);
  // 1 less than 2^1024 rounds down to the max normal double (so is not overflow
  // in that case).
  strtod_verify("1797693134862315907729305190789024733617976978942306572734300811"
              "5773267580550096313270847732240753602112011387987139335765878976"
              "8814416622492847430639474124377767893424865485276302219601246094"
              "1194530829520850057688381506823424628814739131105408272371633505"
              "10684586298239947245938479716304835356329624224137215",
              infinity, max_normal, infinity);
  // Exact midpoint between max_normal and (max normal + 1 ULP)
  // Standard rounding takes this up for nearest, which then becomes overflow
  strtod_verify("1797693134862315807937289714053034150799341327100378269361737789"
              "8044496829276475094664901797758720709633028641669288791094655554"
              "7851940402630657488671505820681908902000708383676273854845817711"
              "5317644757302700698555713669596228429148198608349364752927190741"
              "68444365510704342711559699508093042880177904174497792",
              infinity, max_normal, infinity);
  // 1 less than the above
  // Standard rounding takes this down for nearest, which is not overflow
  strtod_verify("1797693134862315807937289714053034150799341327100378269361737789"
              "8044496829276475094664901797758720709633028641669288791094655554"
              "7851940402630657488671505820681908902000708383676273854845817711"
              "5317644757302700698555713669596228429148198608349364752927190741"
              "68444365510704342711559699508093042880177904174497791",
              max_normal, max_normal, infinity);

  // Nan parsing
  strtod_verify_nan("NaN", nan(""));
  strtod_verify_nan("-NaN", -nan(""));
  strtod_verify_nan("nan", nan(""));
  strtod_verify_nan("+NAN", nan(""));
  strtod_verify_nan("nAn", nan(""));
  strtod_verify_nan("NaN()", nan(""));
  strtod_verify_nan("nan(1)", nan("1"));
  strtod_verify_nan("NaN(011)", nan("9"));
  strtod_verify_nan("NaN(0x11)", nan("0x11"));
  strtod_verify_nan("NaN(11)", nan("0xb"));
  strtod_verify_nan("nan(0xffffffffffffffffffffff9)", nan("0x7fffffffffff9"));

  // Spellings of explicit infinity
  strtod_verify_infinity("inf", infinity);
  strtod_verify_infinity("InF", infinity);
  strtod_verify_infinity("iNf", infinity);
  strtod_verify_infinity("+inf", infinity);
  strtod_verify_infinity("-InF", -infinity);

  strtod_verify_infinity("InFiNiTy", infinity);
  strtod_verify_infinity("iNfInItY", infinity);
  strtod_verify_infinity("-infinity", -infinity);
  strtod_verify_infinity("+infinity", infinity);
  const char *infinite_string = "infinite";
  strtod_verify_with_rounding_mode(infinite_string, FE_TONEAREST, infinity, 0, infinite_string + 3, "");

  // Spellings of zero
  strtod_verify_zero("0", 0.0);
  strtod_verify_zero("+0", 0.0);
  strtod_verify_zero("+0000000.0000000e0000000", 0.0);
  strtod_verify_zero("0e99999999999999999999999999", 0.0);
  strtod_verify_zero("-0.0e0", -0.0);
  strtod_verify_zero("-0", -0.0);
  strtod_verify_zero("-.0", -0.0);
  strtod_verify_zero("-.0e+0", -0.0);
  strtod_verify_zero("-.0e-0", -0.0);
}

T_DECL(strtod_locale, "strtod(3) locale support")
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
  strtod_verify_with_rounding_mode(us_123_456, FE_TONEAREST, 123.456, 0, us_123_456 + 7, "C locale");
  strtod_verify_with_rounding_mode(fr_123_456, FE_TONEAREST, 123.0, 0, fr_123_456 + 3, "C locale");
  strtod_verify_with_rounding_mode(syn_123_456, FE_TONEAREST, 123.0, 0, syn_123_456 + 3, "C locale");
  strtod_verify_with_rounding_mode(syn_trunc_123_456, FE_TONEAREST, 123.0, 0, syn_trunc_123_456 + 3, "C locale");
  strtod_verify_with_rounding_mode(us__279, FE_TONEAREST, 0.279, 0, us__279 + 4, "C locale");
  strtod_verify_with_rounding_mode(fr__279, FE_TONEAREST, 0.0, 0, fr__279, "C locale");
  strtod_verify_with_rounding_mode(syn__279, FE_TONEAREST, 0.0, 0, syn__279, "C locale");
  strtod_verify_with_rounding_mode(syn_trunc__279, FE_TONEAREST, 0.0, 0, syn_trunc__279, "C locale");
  strtod_verify_with_rounding_mode(us_843_, FE_TONEAREST, 843.0, 0, us_843_ + 4, "C locale");
  strtod_verify_with_rounding_mode(fr_843_, FE_TONEAREST, 843.0, 0, fr_843_ + 3, "C locale");
  strtod_verify_with_rounding_mode(syn_843_, FE_TONEAREST, 843.0, 0, syn_843_ + 3, "C locale");
  strtod_verify_with_rounding_mode(syn_trunc_843_, FE_TONEAREST, 843.0, 0, syn_trunc_843_ + 3, "C locale");

  if (setlocale(LC_ALL, "en_US") != NULL || setlocale(LC_ALL, "en_US.UTF-8") != NULL) {
    // Skip this if "en_US" locale can't be found
    lc = localeconv();
    T_EXPECT_EQ_STR(lc->decimal_point, ".", "Expected en_US locale to have '.' as decimal point");
    strtod_verify_with_rounding_mode(us_123_456, FE_TONEAREST, 123.456, 0, us_123_456 + 7, "en_US locale");
    strtod_verify_with_rounding_mode(fr_123_456, FE_TONEAREST, 123.0, 0, fr_123_456 + 3, "en_US locale");
    strtod_verify_with_rounding_mode(syn_123_456, FE_TONEAREST, 123.0, 0, syn_123_456 + 3, "en_US locale");
    strtod_verify_with_rounding_mode(syn_trunc_123_456, FE_TONEAREST, 123.0, 0, syn_trunc_123_456 + 3, "en_US locale");
    strtod_verify_with_rounding_mode(us__279, FE_TONEAREST, 0.279, 0, us__279 + 4, "en_US locale");
    strtod_verify_with_rounding_mode(fr__279, FE_TONEAREST, 0.0, 0, fr__279, "en_US locale");
    strtod_verify_with_rounding_mode(syn__279, FE_TONEAREST, 0.0, 0, syn__279, "en_US locale");
    strtod_verify_with_rounding_mode(syn_trunc__279, FE_TONEAREST, 0.0, 0, syn_trunc__279, "en_US locale");
    strtod_verify_with_rounding_mode(us_843_, FE_TONEAREST, 843.0, 0, us_843_ + 4, "en_US locale");
    strtod_verify_with_rounding_mode(fr_843_, FE_TONEAREST, 843.0, 0, fr_843_ + 3, "en_US locale");
    strtod_verify_with_rounding_mode(syn_843_, FE_TONEAREST, 843.0, 0, syn_843_ + 3, "en_US locale");
    strtod_verify_with_rounding_mode(syn_trunc_843_, FE_TONEAREST, 843.0, 0, syn_trunc_843_ + 3, "en_US locale");
  }

  if (setlocale(LC_ALL, "fr_FR") != NULL || setlocale(LC_ALL, "fr_FR.UTF-8") != NULL) {
    // Skip this if "fr_FR" locale can't be found
    lc = localeconv();
    T_EXPECT_EQ_STR(lc->decimal_point, ",", "Expected fr_FR locale to have ',' as decimal point");
    strtod_verify_with_rounding_mode(us_123_456, FE_TONEAREST, 123.0, 0, us_123_456 + 3, "fr_FR locale");
    strtod_verify_with_rounding_mode(fr_123_456, FE_TONEAREST, 123.456, 0, fr_123_456 + 7, "fr_FR locale");
    strtod_verify_with_rounding_mode(syn_123_456, FE_TONEAREST, 123.0, 0, syn_123_456 + 3, "fr_FR locale");
    strtod_verify_with_rounding_mode(syn_trunc_123_456, FE_TONEAREST, 123.0, 0, syn_trunc_123_456 + 3, "fr_FR locale");
    strtod_verify_with_rounding_mode(us__279, FE_TONEAREST, 0.0, 0, us__279, "fr_FR locale");
    strtod_verify_with_rounding_mode(fr__279, FE_TONEAREST, 0.279, 0, fr__279 + 4, "fr_FR locale");
    strtod_verify_with_rounding_mode(syn__279, FE_TONEAREST, 0.0, 0, syn__279, "fr_FR locale");
    strtod_verify_with_rounding_mode(syn_trunc__279, FE_TONEAREST, 0.0, 0, syn_trunc__279, "fr_FR locale");
    strtod_verify_with_rounding_mode(us_843_, FE_TONEAREST, 843.0, 0, us_843_ + 3, "fr_FR locale");
    strtod_verify_with_rounding_mode(fr_843_, FE_TONEAREST, 843.0, 0, fr_843_ + 4, "fr_FR locale");
    strtod_verify_with_rounding_mode(syn_843_, FE_TONEAREST, 843.0, 0, syn_843_ + 3, "fr_FR locale");
    strtod_verify_with_rounding_mode(syn_trunc_843_, FE_TONEAREST, 843.0, 0, syn_trunc_843_ + 3, "fr_FR locale");
  }

  if (setlocale(LC_ALL, "en_US") != NULL || setlocale(LC_ALL, "en_US.UTF-8") != NULL) {
    lc = localeconv();
    lc->decimal_point = "%$";
    lc = localeconv();
    T_EXPECT_EQ_STR(lc->decimal_point, "%$", "Expected to be able to configure locale with '%%$' as decimal point");
    strtod_verify_with_rounding_mode(us_123_456, FE_TONEAREST, 123.0, 0, us_123_456 + 3, "Synthetic locale");
    strtod_verify_with_rounding_mode(fr_123_456, FE_TONEAREST, 123.0, 0, fr_123_456 + 3, "Synthetic locale");
    strtod_verify_with_rounding_mode(syn_123_456, FE_TONEAREST, 123.456, 0, syn_123_456 + 8, "Synthetic locale");
    strtod_verify_with_rounding_mode(syn_trunc_123_456, FE_TONEAREST, 123.0, 0, syn_trunc_123_456 + 3, "Synthetic locale");
    strtod_verify_with_rounding_mode(us__279, FE_TONEAREST, 0.0, 0, us__279, "Synthetic locale");
    strtod_verify_with_rounding_mode(fr__279, FE_TONEAREST, 0.0, 0, fr__279, "Synthetic locale");
    strtod_verify_with_rounding_mode(syn__279, FE_TONEAREST, 0.279, 0, syn__279 + 5, "Synthetic locale");
    strtod_verify_with_rounding_mode(syn_trunc__279, FE_TONEAREST, 0.0, 0, syn_trunc__279, "Synthetic locale");
    strtod_verify_with_rounding_mode(us_843_, FE_TONEAREST, 843.0, 0, us_843_ + 3, "Synthetic locale");
    strtod_verify_with_rounding_mode(fr_843_, FE_TONEAREST, 843.0, 0, fr_843_ + 3, "Synthetic locale");
    strtod_verify_with_rounding_mode(syn_843_, FE_TONEAREST, 843.0, 0, syn_843_ + 5, "Synthetic locale");
    strtod_verify_with_rounding_mode(syn_trunc_843_, FE_TONEAREST, 843.0, 0, syn_trunc_843_ + 3, "Synthetic locale");
    lc->decimal_point = ".";
  }

  // Try hard to restore a sane default locale after the above
  (void)setlocale(LC_ALL, "");
  (void)setlocale(LC_ALL, "POSIX");
  (void)setlocale(LC_ALL, "C");
  (void)uselocale(LC_GLOBAL_LOCALE);
}

// Based on bug report from
// https://stackoverflow.com/questions/76133503/strtod-does-not-respect-locale-on-macos-13-3-1
// also see rdar://111449210 (SEED: Web: strtod() function does not respect locale in macOS 13)
T_DECL(strtod_thread_locale, "strtod(3) thread-local locale support")
{
  if (setlocale(LC_ALL, "fr_FR") != NULL || setlocale(LC_ALL, "fr_FR.UTF-8") != NULL) {
    // Skip these tests if "fr_FR" locale can't be found
    double d1 = strtod("123.25", NULL);
    if (d1 != 123.0) {
      T_FAIL("\"123.25\" in fr_FR locale should parse to 123.0, not %g", d1);
    } else {
      T_PASS("fr_FR global locale ignores '.'");
    }

    double d2 = strtod("123,25", NULL);
    if (d2 != 123.25) {
      T_FAIL("\"123,25\" in fr_FR locale should parse to 123.25, not %g", d2);
    } else {
      T_PASS("fr_FR global locale recognizes ','");
    }
  }

  locale_t c_locale = newlocale(LC_NUMERIC_MASK, "C", NULL);
  if (c_locale != NULL) {
    // Now set the thread-specific locale to use '.' and verify that works correctly
    // Caveat:  These are less interesting if selecting the global "fr_FR" locale
    // above failed, but they should still pass in that case.
    uselocale(c_locale);
    double d3 = strtod("123.25", NULL);
    if (d3 != 123.25) {
      T_FAIL("\"123.25\" in fr_FR global locale with C thread locale should parse to 123.25, not %g", d3);
    } else {
      T_PASS("fr_FR global locale with C thread locale recognizes '.'");
    }
    double d4 = strtod("123,25", NULL);
    if (d4 != 123.0) {
      T_FAIL("\"123,25\" in fr_FR global locale with C thread locale should parse to 123.0, not %g", d4);
    } else {
      T_PASS("fr_FR global locale with C thread locale ignores ','");
    }
    uselocale(LC_GLOBAL_LOCALE);
    freelocale(c_locale);
  }

  // Try hard to restore a sane default locale after the above
  (void)setlocale(LC_ALL, "");
  (void)setlocale(LC_ALL, "POSIX");
  (void)setlocale(LC_ALL, "C");
  (void)uselocale(LC_GLOBAL_LOCALE);
}
