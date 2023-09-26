#include <stdio.h>
#include <wchar.h>

#include <darwintest.h>
#include <paths.h>

#define	FWIDE_TEST_PATH	_PATH_DEV "zero"

/*
 * These mostly test fgets/fgetwc, but they can point to issues with the
 * underlying orientation tracking.
 */

T_DECL(fwide_fgets,
    "Test that fgets(3) sets the stream orientation to byte-oriented")
{
	FILE *fp;
	char buf[1];

	fp = fopen(FWIDE_TEST_PATH, "r");
	T_WITH_ERRNO;
	T_ASSERT_NOTNULL(fp, NULL);

	T_ASSERT_EQ(fgets(&buf[0], sizeof(buf), fp), &buf[0], NULL);
	T_ASSERT_LT(fwide(fp, 0), 0, NULL);
}

T_DECL(fwide_fgetwc,
    "Test that fgetwc(3) sets the stream orientation to wide-oriented")
{
	FILE *fp;

	fp = fopen(FWIDE_TEST_PATH, "r");
	T_WITH_ERRNO;
	T_ASSERT_NOTNULL(fp, NULL);

	T_ASSERT_EQ(fgetwc(fp), 0, NULL);
	T_ASSERT_GT(fwide(fp, 0), 0, NULL);
}

T_DECL(fwide_nop, "Test that fwide(3) is a nop after orientation is set")
{
	FILE *fp;

	fp = fopen(FWIDE_TEST_PATH, "r");
	T_WITH_ERRNO;
	T_ASSERT_NOTNULL(fp, NULL);

	T_ASSERT_EQ(fwide(fp, 0), 0, NULL);
	T_ASSERT_EQ(fgetwc(fp), 0, NULL);
	T_ASSERT_GT(fwide(fp, -1), 0, NULL);
}
