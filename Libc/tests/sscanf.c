#include <darwintest.h>
#include <darwintest_utils.h>

#include "libc_hooks_helper.h"
#include <xlocale.h>

T_DECL(libc_hooks_sscanf_l, "Test libc_hooks for sscanf_l")
{
    // Setup
    T_SETUPBEGIN;
    locale_t loc = duplocale(NULL);
    T_SETUPEND;

    // Signed integers (All lengths)
    int             d;
    signed char   hhd;
    short int      hd;
    long int       ld;
    long long int lld;
    intmax_t       jd;
    size_t         zd;
    ptrdiff_t      td;

    // Test
    char data_d[] = "42, 42, 42, 42, 42, 42, 42, 42";
    char fmt_d[] = "%d, %hhd, %hd, %ld, %lld, %jd, %zd, %td";
    libc_hooks_log_start();
    sscanf_l(data_d, loc, fmt_d, &d, &hhd, &hd, &ld, &lld, &jd, &zd, &td);
    libc_hooks_log_stop(11);

    // Check
    T_LOG("sscanf(\"%s\", loc, \"%s\", &d, &hhd, &hd, &ld, &lld, &jd, &zd, &td) - %s", data_d, fmt_d);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, data_d, strlen(data_d) + 1), "checking data_d");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, loc, SIZE_LOCALE_T), "checking loc");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, fmt_d, strlen(fmt_d) + 1), "checking fmt_d");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &d, sizeof(d)), "checking d");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &hhd, sizeof(hhd)), "checking hhd");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &hd, sizeof(hd)), "checking hd");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &ld, sizeof(ld)), "checking ld");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &lld, sizeof(lld)), "checking lld");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &jd, sizeof(jd)), "checking jd");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &zd, sizeof(zd)), "checking zd");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &td, sizeof(td)), "checking td");

    // Unsigned integers (All lengths)
    int             u;
    signed char   hhu;
    short int      hu;
    long int       lu;
    long long int llu;
    intmax_t       ju;
    size_t         zu;
    ptrdiff_t      tu;

    // Test
    char data_u[] = "42, 42, 42, 42, 42, 42, 42, 42";
    char fmt_u[] = "%u, %hhu, %hu, %lu, %llu, %ju, %zu, %tu";
    libc_hooks_log_start();
    sscanf_l(data_u, loc, fmt_u, &u, &hhu, &hu, &lu, &llu, &ju, &zu, &tu);
    libc_hooks_log_stop(11);

    // Check
    T_LOG("sscanf_l(\"%s\", loc, \"%s\", &u, &hhu, &hu, &lu, &llu, &ju, &zu, &tu)", data_u, fmt_u);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, data_u, strlen(data_u) + 1), "checking data_u");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, loc, SIZE_LOCALE_T), "checking loc");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, fmt_u, strlen(fmt_u) + 1), "checking fmt_u");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &u, sizeof(u)), "checking u");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &hhu, sizeof(hhu)), "checking hhu");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &hu, sizeof(hu)), "checking hu");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &lu, sizeof(lu)), "checking lu");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &llu, sizeof(llu)), "checking llu");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &ju, sizeof(ju)), "checking ju");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &zu, sizeof(zu)), "checking zu");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &tu, sizeof(tu)), "checking tu");

    // Floats (all lengths)
    float f;
    double lf;
    long double Lf;

    // Test
    char data_f[] = "42.0, 42.0, 42.0";
    char fmt_f[] = "%f, %lf, %Lf";
    libc_hooks_log_start();
    sscanf_l(data_f, loc, fmt_f, &f, &lf, &Lf);
    libc_hooks_log_stop(6);

    // Check
    T_LOG("sscanf_l(\"%s\", loc, \"%s\", &f, &lf, &Lf)", data_f, fmt_f);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, data_f, strlen(data_f) + 1), "checking data_f");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, loc, SIZE_LOCALE_T), "checking loc");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, fmt_f, strlen(fmt_f) + 1), "checking fmt_f");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &f, sizeof(f)), "checking f");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &lf, sizeof(lf)), "checking lf");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &Lf, sizeof(Lf)), "checking Lf");

    // Characters and Strings
    char c;
    char s[256];

    // Test
    char data_cs[] = "C, forty_two";
    char fmt_cs[] = "%c, %s";
    libc_hooks_log_start();
    sscanf_l(data_cs, loc, fmt_cs, &c, &s);
    libc_hooks_log_stop(5);

    // Check
    T_LOG("sscanf_l(\"%s\", loc, \"%s\", &c, &s)", data_cs, fmt_cs);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, data_cs, strlen(data_cs) + 1), "checking data_cs");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, loc, SIZE_LOCALE_T), "checking loc");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, fmt_cs, strlen(fmt_cs) + 1), "checking fmt_cs");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &c, sizeof(c)), "checking c");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, &s, strlen(s) + 1), "checking s");

    // Wide characters and Strings
    wint_t lc;
    wint_t ls[256];

    char fmt_wcs[] = "%lc, %ls";
    char data_wcs[] = "É, ÉÉÉ";
    libc_hooks_log_start();
    sscanf_l(data_wcs, loc, fmt_wcs, &lc, &ls);
    libc_hooks_log_stop(3);

    // Check
    T_LOG("sscanf_l(\"%s\", loc, \"%s\", &lc, &ls) - %s", data_wcs, fmt_wcs);

    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, data_wcs, strlen(data_wcs) + 1), "checking data_wcs");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, loc, SIZE_LOCALE_T), "checking loc");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, fmt_wcs, strlen(fmt_wcs) + 1), "checking fmt_wcs");
#if 0 // TBD: Wide characters and strings
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, &lc, sizeof(lc)), "checking lc");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, &ls, wcslen(ls) + 1), "checking ls");
#endif
}
