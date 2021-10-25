#include <darwintest.h>

#if !defined(DARWIN_API_AVAILABLE_20190830)
#define DARWIN_API_AVAILABLE_20190830
#endif

#include "../libdarwin/bsd.c"

static struct test_case {
	const char *args;
	const char *argname;
	const char *argvalue;
} test_cases[] = {
	{"-x -a b=3 y=42", "-a", ""},
	{"-x -a b=3 y=42", "b", "3"},
	{"-x -a b=2 ba=3 y=42", "b", "2"},
	{"-x -a ba=3 b=2 y=42", "b", "2"},
	{"-x -a b=2 ba=3 y=42", "ba", "3"},
	{"-x -a ba=3 b=2 y=42", "ba", "3"},
	{"-x -ab -aa y=42", "-a", NULL},
	{"-x b=96 y=42", "bx", NULL},
	{"-x ab=96 y=42", "a", NULL},
};

T_DECL(parse_boot_arg_value, "Parsing boot args")
{
	for (int i = 0; i < (int)(sizeof(test_cases)/sizeof(test_cases[0])); i++) {
		struct test_case *test_case = &test_cases[i];
		T_LOG("\"%s\": Looking for \"%s\", expecting \"%s\"",
				test_case->args, test_case->argname, test_case->argvalue);

		char *argbuff = strdup(test_case->args);

		char result[256] = "NOT_FOUND";
		bool found = _parse_boot_arg_value(argbuff, test_case->argname,
				result,sizeof(result),NULL);

		if (test_case->argvalue) {
			T_EXPECT_EQ(found, true, "Should find argument");
			T_EXPECT_EQ_STR(result, test_case->argvalue, "Should find correct result");
		} else {
			T_EXPECT_EQ(found, false, "Should not find argument");
		}

		free(argbuff);
	}
}

T_DECL(os_parse_boot_arg, "Getting boot args")
{
	int64_t value = 0;
	T_EXPECT_EQ(os_parse_boot_arg_int("notarealthing", &value), false, NULL);

	T_MAYFAIL;
	T_EXPECT_EQ(os_parse_boot_arg_int("debug", &value), true, NULL);
	T_EXPECT_GT(value, 0LL, "non-zero debug= value");

	char buf[64] = {};

	T_EXPECT_EQ(os_parse_boot_arg_string("notarealthing", buf, sizeof(buf)), false, NULL);

	T_MAYFAIL;
	T_EXPECT_EQ(os_parse_boot_arg_string("debug", buf, sizeof(buf)), true, NULL);
	T_EXPECT_GT(strlen(buf), 0UL, "non-empty debug= value");
}

T_DECL(os_parse_boot_arg_from_buffer_int, "Getting boot args from a buffer as integers")
{
	const char *boot_args = "hello=world -foobar abc debug=0xBAADF00D";

	int64_t value = 0xCAFEFACE;
	T_EXPECT_EQ(os_parse_boot_arg_from_buffer_int(boot_args, "notarealthing", &value), false, NULL);

	value = 0xCAFEFACE;
	T_EXPECT_EQ(os_parse_boot_arg_from_buffer_int(boot_args, "hello", &value), false, NULL);
	T_EXPECT_EQ(value, 0xCAFEFACELL, "output not overwritten when value unparseable as integer");

	value = 0xCAFEFACE;
	T_EXPECT_EQ(os_parse_boot_arg_from_buffer_int(boot_args, "debug", &value), true, NULL);
	T_EXPECT_EQ(value, 0xBAADF00DLL, "debug= value parsed exactly");

	value = 0xCAFEFACE;
	T_EXPECT_EQ(os_parse_boot_arg_from_buffer_int(boot_args, "-foobar", &value), true, NULL);
	T_EXPECT_EQ(value, 1LL, "-foobar value parsed as 1 (as if foobar=1)");

	value = 0xCAFEFACE;
	T_EXPECT_EQ(os_parse_boot_arg_from_buffer_int(boot_args, "abc", &value), true, NULL);
	T_EXPECT_EQ(value, 1LL, "abc value parsed as 1 (as if abc=1)");
}

T_DECL(os_parse_boot_arg_from_buffer_string, "Getting boot args from a buffer as strings")
{
	const char *boot_args = "hello=world -foobar abc debug=0xBAADF00D";

	char buf[64] = {};

	T_EXPECT_EQ(os_parse_boot_arg_from_buffer_string(boot_args, "notarealthing", buf, sizeof(buf)), false, NULL);

	T_EXPECT_EQ(os_parse_boot_arg_from_buffer_string(boot_args, "hello", buf, sizeof(buf)), true, NULL);
	T_EXPECT_EQ_STR(buf, "world", "parsed hello=world");

	T_EXPECT_EQ(os_parse_boot_arg_from_buffer_string(boot_args, "debug", buf, sizeof(buf)), true, NULL);
	T_EXPECT_EQ_STR(buf, "0xBAADF00D", "parsed debug=0xBAADF00D");

	// XNU currently does NOT convert a flag to the string "1", but it
	// does convert it to the integer 1.
	T_EXPECT_EQ(os_parse_boot_arg_from_buffer_string(boot_args, "-foobar", buf, sizeof(buf)), true, NULL);
	T_EXPECT_EQ_STR(buf, "", "did NOT parse -foobar as foobar=1 (intentional)");

	T_EXPECT_EQ(os_parse_boot_arg_from_buffer_string(boot_args, "abc", buf, sizeof(buf)), true, NULL);
	T_EXPECT_EQ_STR(buf, "1", "parsed abc as abc=1");
}
