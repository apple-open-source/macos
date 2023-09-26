#include <string.h>

#include <darwintest.h>

#define S1 "Squeamish"
#define S2 "Ossifrage"

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

/*
 * Common test procedure for strncat(): measure the inputs, verify that we
 * will not overflow our buffer, populate the destination buffer, call
 * strncat(), verify that the destination buffer contains exactly what we
 * expect.
 */
static void
t_strncat(char *s1, char *s2, size_t n)
{
	char buf[1024];
	char *result;
	size_t l1, l2, l;

	memset(buf, '@', sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	l1 = strlen(s1);
	l2 = strlen(s2);
	l = l2 < n ? l2 : n;
	assert(l1 + l < sizeof(buf));
	memcpy(buf, s1, l1);
	buf[l1] = '\0';
	T_EXPECT_NOTNULL(result = strncat(buf, s2, n),
	    "Return value should not be NULL");
	T_LOG("strncat(\"%s\", \"%s\", %zu) = \"%s\"", s1, s2, n, buf);
	/* cast to void to suppress warning in darwintest macro */
	T_EXPECT_EQ_PTR((void *)result, (void *)buf,
	    "Return value should be equal to destination");
	T_EXPECT_EQ(0, memcmp(buf, s1, l1),
	    "Output should contain first input");
	T_EXPECT_EQ(0, memcmp(buf + l1, s2, l),
	    "Output should contain second input");
	T_EXPECT_EQ('\0', buf[l1 + l],
	    "Output should be terminated");
}

T_DECL(strncat_empty_empty_zero,
    "Append zero characters from an empty string to an empty string")
{
	t_strncat("", "", 0);
}

T_DECL(strncat_empty_empty_unlimited,
    "Append an empty string to an empty string")
{
	t_strncat("", "", SIZE_MAX);
}

T_DECL(strncat_empty_nonempty_zero,
    "Append zero characters from a non-empty string to an empty string")
{
	t_strncat("", S2, 0);
}

T_DECL(strncat_empty_nonempty_limited,
    "Append part of a non-empty string to an empty string")
{
	t_strncat("", S2, sizeof(S2) / 2);
}

T_DECL(strncat_empty_nonempty_unlimited,
    "Append a non-empty string to an empty string")
{
	t_strncat("", S2, SIZE_MAX);
}

T_DECL(strncat_nonempty_empty_unlimited,
    "Append an empty string to a non-empty string")
{
	t_strncat(S1, "", SIZE_MAX);
}

T_DECL(strncat_nonempty_nonempty_zero,
    "Append zero characters from a non-empty string to a non-empty string")
{
	t_strncat(S1, S2, sizeof(S2) / 2);
}

T_DECL(strncat_nonempty_nonempty_limited,
    "Append part of a non-empty string to a non-empty string")
{
	t_strncat(S1, S2, sizeof(S2) / 2);
}

T_DECL(strncat_nonempty_nonempty_unlimited,
    "Append a non-empty string to a non-empty string")
{
	t_strncat(S1, S2, SIZE_MAX);
}
