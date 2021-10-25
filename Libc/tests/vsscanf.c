#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <darwintest.h>
#include <darwintest_utils.h>

static wchar_t arg1[45] = L"Sierra";

static char s[50] = "\0";

int read_this(char *, ...);

T_DECL(test_vsscanf, "vsscanf should not modify the output string if there is a character mismatch")
{
    (void)strcpy(s,"Yosemite");
    (void)wcscpy(arg1,L"FooBarBaz");
    wprintf(L"Before vsscanf: arg1 = %S", arg1);
    (void)read_this("%l[QZxp]",arg1);

    wprintf(L"After vsscanf: arg1 = %S", arg1);
    if (wcscmp(arg1,L"FooBarBaz")) {
        T_LOG("vsscanf assigned something with %%l[] and ");
        T_LOG("input did not match.");
        T_FAIL("output string was modified");
    } else {
        T_PASS("output string is intact");
    }
}

int read_this(char *format, ...)
{
        int ret = 0;
        va_list args;

        va_start(args, format);
        ret = vsscanf(s, format, args);
        va_end(args);
        return(ret);
}
