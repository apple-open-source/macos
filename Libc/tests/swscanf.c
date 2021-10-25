#include <stdio.h>
#include <wchar.h>
#include <darwintest.h>
#include <darwintest_utils.h>

T_DECL(swscanf, "input conversion")
{
    wchar_t arg [] = L"abcd efgh ik";
    wchar_t s[50];
    int ret = 0;

    (void)wcscpy(s,L"\0");
    ret = swscanf(s,L"%[Zto]",arg);
    T_ASSERT_EQ(ret, EOF, "swscanf returned %d", ret);
}
