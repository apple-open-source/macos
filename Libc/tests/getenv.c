#include <stdlib.h>

#include <darwintest.h>
#include <libc_private.h>

T_DECL(setenv_getenv, "getenv returns value set by setenv")
{
	char *name = "foo";
	char *value = "bar";
	int setenv_rc = setenv(name, value, 0);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	char *getenv_result = getenv(name);
	T_EXPECT_EQ_STR(value, getenv_result, "getenv must return setenv argument");
}

T_DECL(setenv_overwrite, "getenv returns the latest setenv argument")
{
	char *name = "foo";
	char *first_value = "bar";
	char *second_value = "baz";
	int setenv_rc = 0;
	setenv_rc = setenv(name, first_value, 0);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	setenv_rc = setenv(name, second_value, 1);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	char *getenv_result = getenv(name);
	T_EXPECT_EQ_STR(second_value, getenv_result, "getenv must return the latest setenv argument");
}

T_DECL(setenv_dont_overwrite, "setenv respects overwrite")
{
	char *name = "foo";
	char *first_value = "bar";
	char *second_value = "baz";
	int setenv_rc = 0;
	setenv_rc = setenv(name, first_value, 0);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	setenv_rc = setenv(name, second_value, 0);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	char *getenv_result = getenv(name);
	T_EXPECT_EQ_STR(first_value, getenv_result, "the second setenv must not overwrite the first one");
}

/* There are tests for leading '=' in values because BSDs used to strip them off: rdar://problem/19342460 */

T_DECL(setenv_accepts_leading_eq_sign, "setenv accepts values starting with '='")
{
	char *name = "foo";
	char *value = "=bar";
	int setenv_rc = setenv(name, value, 0);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	char *getenv_result = getenv(name);
	T_EXPECT_EQ_STR(value, getenv_result, "getenv must return setenv argument");
}

T_DECL(setenv_accepts_leading_eq_sign_overwrite, "setenv accepts values starting with '=' when overwriting an existing value")
{
	char *name = "foo";
	char *first_value = "bar";
	char *second_value = "=baz";
	int setenv_rc = 0;
	setenv_rc = setenv(name, first_value, 0);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	setenv_rc = setenv(name, second_value, 1);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	char *getenv_result = getenv(name);
	T_EXPECT_EQ_STR(second_value, getenv_result, "getenv must return the latest setenv argument");
}

#pragma mark getenv_copy_np tests

T_DECL(getenv_copy_np_returns_null, "getenv_copy_np returns NULL for non-existant variable")
{
	char *name = "alskfjapoeifhapoeifnaslkedfhjalsdkjfalsdkjf";
	int unsetenv_rc = unsetenv(name);
	T_EXPECT_EQ(0, unsetenv_rc, "unsetenv must succeed with 0 return code");
	errno = 0;
	char * getenv_copy_np_result = getenv_copy_np(name);
	int save_errno = errno;
	T_EXPECT_NULL(getenv_copy_np_result, "getenv_copy_np reurns NULL for unset environment variable");
	T_EXPECT_EQ(0, save_errno, "getenv_copy_np must succeed without setting errno");
}

T_DECL(getenv_copy_np_empty, "getenv_copy_np returns empty string")
{
	char *name = "foo";
	char *value = "";
	int setenv_rc = setenv(name, value, 0);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	char * getenv_copy_np_result = getenv_copy_np(name);
	T_EXPECT_EQ_STR(value, getenv_copy_np_result, "getenv_copy_np must return setenv argument");
	free(getenv_copy_np_result);
}

T_DECL(setenv_getenv_copy_np, "getenv_copy_np returns value set by setenv")
{
	char *name = "foo";
	char *value = "bar";
	int setenv_rc = setenv(name, value, 0);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	char * getenv_copy_np_result = getenv_copy_np(name);
	T_EXPECT_EQ_STR(value, getenv_copy_np_result, "getenv_copy_np must return setenv argument");
	free(getenv_copy_np_result);
}

T_DECL(setenv_overwrite_getenv_copy_np, "getenv_copy_np returns the latest setenv argument")
{
	char *name = "foo";
	char *first_value = "bar";
	char *second_value = "baz";
	int setenv_rc = 0;
	setenv_rc = setenv(name, first_value, 0);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	setenv_rc = setenv(name, second_value, 1);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	char * getenv_copy_np_result = getenv_copy_np(name);
	T_EXPECT_EQ_STR(second_value, getenv_copy_np_result, "getenv_copy_np must return the latest setenv argument");
	free(getenv_copy_np_result);
}

T_DECL(setenv_dont_overwrite_getenv_copy_np, "setenv respects overwrite")
{
	char *name = "foo";
	char *first_value = "bar";
	char *second_value = "baz";
	int setenv_rc = 0;
	setenv_rc = setenv(name, first_value, 0);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	setenv_rc = setenv(name, second_value, 0);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	char * getenv_copy_np_result = getenv_copy_np(name);
	T_EXPECT_EQ_STR(first_value, getenv_copy_np_result, "the second setenv must not overwrite the first one");
	free(getenv_copy_np_result);
}

/* There are tests for leading '=' in values because BSDs used to strip them off: rdar://problem/19342460 */

T_DECL(setenv_accepts_leading_eq_sign_getenv_copy_np, "setenv accepts values starting with '='")
{
	char *name = "foo";
	char *value = "=bar";
	int setenv_rc = setenv(name, value, 0);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	char * getenv_copy_np_result = getenv_copy_np(name);
	T_EXPECT_EQ_STR(value, getenv_copy_np_result, "getenv_copy_np must return setenv argument");
	free(getenv_copy_np_result);
}

T_DECL(setenv_accepts_leading_eq_sign_overwrite_getenv_copy_np, "setenv accepts values starting with '=' when overwriting an existing value")
{
	char *name = "foo";
	char *first_value = "bar";
	char *second_value = "=baz";
	int setenv_rc = 0;
	setenv_rc = setenv(name, first_value, 0);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	setenv_rc = setenv(name, second_value, 1);
	T_EXPECT_EQ(0, setenv_rc, "setenv must succeed with 0 return code");
	char * getenv_copy_np_result = getenv_copy_np(name);
	T_EXPECT_EQ_STR(second_value, getenv_copy_np_result, "getenv_copy_np must return the latest setenv argument");
	free(getenv_copy_np_result);
}
