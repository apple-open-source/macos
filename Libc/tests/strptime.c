#include <time.h>

#include <darwintest.h>
#include <darwintest_utils.h>

T_DECL(PR_27004626, "strptime() should fail when a %t doesn't match anything")
{
	struct tm tm;
	T_ASSERT_NULL(strptime("there'snotemplateforthis", "%t", &tm), NULL);
}
