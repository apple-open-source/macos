#include <TargetConditionals.h>
#include <errno.h>
#include <locale.h>
#include <wchar.h>

#include <darwintest.h>

T_DECL(strxfrm_PR_152138335, "Test failed strxfrm()-driven assertion",
    T_META_ENABLED(TARGET_OS_OSX))
{
	size_t out;

	T_ASSERT_EQ_STR("el_GR.UTF-8", setlocale(LC_ALL, "el_GR.UTF-8"),
	    NULL);
	out = strxfrm(NULL, "ό", 0);
	T_ASSERT_EQ(2, out, NULL);
}
