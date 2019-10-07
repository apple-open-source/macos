#include <darwintest.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <limits.h>
 
T_DECL(versionstring, "Apple specific version string") {
	char version[128];
	FILE *sortfile = popen("/usr/bin/sort --version", "r");
	T_ASSERT_NOTNULL(sortfile, "Getting version string");
	T_ASSERT_NOTNULL(fgets(version, sizeof(version), sortfile), "Reading version string");
	pclose(sortfile);
	T_ASSERT_NOTNULL(strstr(version, "-Apple"), "Apple in version string");

	char *num = strstr(version, "(");
	char *endnum = strstr(version, ")");
	T_ASSERT_NOTNULL(num, "Locating parens start");
	T_ASSERT_NOTNULL(endnum, "Locating parens end");
	T_ASSERT_GT(endnum, num, "end is after the start");
	long applevers = strtol(num+1, &endnum, 10);
	T_ASSERT_GT(applevers, 0, "Version greater than zero");
	T_ASSERT_LT(applevers, LONG_MAX, "Version less than LONG_MAX");
}
