#include <darwintest.h>
#include <darwintest_utils.h>

#include "libc_hooks_helper.h"
#include <xlocale.h>

// Testing fprintf_l instead of fprintf() so we can test locale_tests as well
T_DECL(libc_hooks_fprintf_l, "Test libc_hooks for fprintf_l")
{
    // Setup
    T_SETUPBEGIN;
    locale_t loc = duplocale(NULL);
    FILE *f = fopen("/dev/null", "w");
    T_SETUPEND;

    // Signed integers (All lengths)
    int             d = 42;
    signed char   hhd = 42;
    short int      hd = 42;
    long int       ld = 42;
    long long int lld = 42;
    intmax_t       jd = 42;
    size_t         zd = 42;
    ptrdiff_t      td = 42;

    // Test
    char fmt_d[] = "%d, %hhd, %hd, %ld, %lld, %jd, %zd, %td";
    libc_hooks_log_start();
    fprintf_l(f, loc, fmt_d, d, hhd, hd, ld, lld, jd, zd, td);
    libc_hooks_log_stop(3);

    // Check
    T_LOG("fprintf_l(f, loc, \"%s\")", fmt_d);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, f, sizeof(*f)), "checking f");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read, loc, SIZE_LOCALE_T), "checking loc");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, fmt_d, strlen(fmt_d) + 1), "checking fmt_d");

    // Unsigned integers (All lengths)
    unsigned int             u = 42;
    unsigned char          hhu = 42;
    unsigned short int      hu = 42;
    unsigned long int       lu = 42;
    unsigned long long int llu = 42;
    uintmax_t               ju = 42;

    // Test
    char fmt_u[] = "%u, %hhu, %hu, %lu, %llu, %ju";
    libc_hooks_log_start();
    fprintf_l(f, loc, fmt_u, u, hhu, hu, lu, llu, ju);
    libc_hooks_log_stop(3);

    // Check
    T_LOG("fprintf_l(f, loc, \"%s\")", fmt_u);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, f, sizeof(*f)), "checking f");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read, loc, SIZE_LOCALE_T), "checking loc");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, fmt_u, strlen(fmt_u) + 1), "checking fmt_u");

    // Float points (All length)
    float       fx = 42.0;
    double      lf = 42.0;
    long double LF = 42.0L;

    // Test
    char fmt_f[] = "%f, %lf, %Lf";
    libc_hooks_log_start();
    fprintf_l(f, loc, fmt_f, fx, lf, LF);
    libc_hooks_log_stop(3);

    // Check
    T_LOG("fprintf_l(f, loc, \"%s\")", fmt_f);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, f, sizeof(*f)), "checking f");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read, loc, SIZE_LOCALE_T), "checking loc");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, fmt_f, strlen(fmt_f) + 1), "checking fmt_f");

    // Characters and strings
    char c = 'C';
    wchar_t lc = L'W';
    char s[] = "foo";
    wchar_t ls[] = L"foo";

    // Test
    char fmt_s[] = "%c, %lc, %s, %ls";
    libc_hooks_log_start();
    fprintf_l(f, loc, fmt_s, c, lc, s, ls);
    libc_hooks_log_stop(5);

    // Check
    T_LOG("fprintf_l(f, loc, \"%s\")", fmt_s);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, f, sizeof(*f)), "checking f");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read, loc, SIZE_LOCALE_T), "checking loc");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, fmt_s, strlen(fmt_s) + 1), "checking fmt_s");
#if 0 // TBD: Investigate where these are coming from
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, ?, 3), "checking ?");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, ?, 3), "checking ?");
#endif

    // Special "%n" format specifier (all lengths)
    int             n;
    signed char   hhn;
    short int      hn;
    long int       ln;
    long long int lln;
    intmax_t       jn;
    size_t         zn;
    ptrdiff_t      tn;

    // Test
    char fmt_n[] = "%n, %hhn, %hn, %ln, %lln, %jn, %zn, %tn";
    libc_hooks_log_start();
    fprintf_l(f, loc, "%n, %hhn, %hn, %ln, %lln, %jn, %zn, %tn", &n, &hhn, &hn, &ln, &lln, &jn, &zn, &tn);
    libc_hooks_log_stop(11);

    // Check
    T_LOG("fprintf_l(f, loc, \"%s\")", fmt_n);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, f, sizeof(*f)), "checking f");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read, loc, SIZE_LOCALE_T), "checking loc");
#if 0 // Can't use a non-literal string for %n family for security reasons
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, fmt_n, strlen(fmt_n) + 1), "checking fmt_n");
#else
    libc_hooks_log.check++;
#endif
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &n, sizeof(n)), "checking n");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &hhn, sizeof(hhn)), "checking hhn");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &hn, sizeof(hn)), "checking hn");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &ln, sizeof(ln)), "checking ln");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &lln, sizeof(lln)), "checking lln");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &jn, sizeof(jn)), "checking jn");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &zn, sizeof(zn)), "checking zn");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &tn, sizeof(tn)), "checking tn");

    // Cleanup
    fclose(f);
}
