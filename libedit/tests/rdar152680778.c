#include <histedit.h>
#include <stdlib.h>
#include <string.h>

#include <darwintest_utils.h>

#ifndef EL_BUFSIZ
#define EL_BUFSIZ 1024
#endif

#define INSIZE (EL_BUFSIZ * 2)

T_DECL(rdar152680778, "Test that parse__string() does not overflow its output buffer",
    T_META_NAMESPACE("libedit.regression"))
{
	EditLine *el;
	const char *args[4] = { "bind", "-r", NULL, NULL };
	char *in;

	T_ASSERT_NOTNULL(el = el_init(T_NAME, stdin, stdout, stderr), NULL);
	T_ASSERT_NOTNULL(in = malloc(INSIZE + 1), NULL);
	memset(in, 'A', INSIZE);
	in[INSIZE] = '\0';
	args[2] = in;
	T_ASSERT_EQ(1, el_parse(el, 3, args), NULL);
	free(in);
}
