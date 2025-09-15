#include <readline/history.h>
#include <stdlib.h>

#include <darwintest_utils.h>

T_DECL(rdar152680481, "Test that history_expand() does not use strcpy() incorrectly",
    T_META_NAMESPACE("libedit.regression"))
{
	char in[161], *out;
	int i;

	for (i = 0; i < 80; i++) {
		in[2*i]   = '\\';
		in[2*i+1] = '!';
	}
	in[160] = '\0';
	T_ASSERT_EQ(-1, history_expand(in, &out), NULL);
	free(out);
}
