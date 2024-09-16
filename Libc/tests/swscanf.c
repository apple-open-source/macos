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

T_DECL(swscanf_53347577, "rdar://53347577")
{
	int a = 0, b = 0, n = 0;
	T_EXPECT_EQ_INT(swscanf(L"23 19", L"%d %d%n", &a, &b, &n), 2, NULL);
	T_EXPECT_EQ_INT(a, 23, NULL);
	T_EXPECT_EQ_INT(b, 19, NULL);
	T_EXPECT_EQ_INT(n, 5, NULL);
}
