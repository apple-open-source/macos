#include <TargetConditionals.h>
#include <errno.h>
#include <locale.h>
#include <wchar.h>

#include <darwintest.h>

T_DECL(wcscoll_PR_142386677, "Test failed wcscoll() assertion",
    T_META_ENABLED(TARGET_OS_OSX))
{
	int ret;

	setlocale(LC_COLLATE, "en_US.UTF-8");
	ret = wcscoll(L"ở", L"ở");
	T_ASSERT_EQ(ret, 0, NULL);
}
