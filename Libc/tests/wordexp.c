/*
* Copyright (c) 2024 Apple Inc. All rights reserved.
*
* @APPLE_LICENSE_HEADER_START@
*
* This file contains Original Code and/or Modifications of Original Code
* as defined in and that are subject to the Apple Public Source License
* Version 2.0 (the 'License'). You may not use this file except in
* compliance with the License. Please obtain a copy of the License at
* http://www.opensource.apple.com/apsl/ and read it before using this
* file.
*
* The Original Code and all software distributed under the License are
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
* Please see the License for the specific language governing rights and
* limitations under the License.
*
* @APPLE_LICENSE_HEADER_END@
*/

#include <wordexp.h>
#include <stdbool.h>
#include <TargetConditionals.h>

#include <darwintest.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

#if !TARGET_OS_IPHONE
static void check(const char *words, int expected_res, const char *expected_word,
				  const char *label)
{
	wordexp_t we;
	int res = wordexp(words, &we, WRDE_NOCMD | WRDE_SHOWERR);
	T_EXPECT_EQ(res, expected_res, "%s: error code | input: %s |", label, words);
	if (res == 0 && expected_res == 0) {
		T_EXPECT_EQ(we.we_wordc, (size_t)1,
					"%s: we_wordc | input: %s |", label, words);
		if (we.we_wordc == 1) {
			T_EXPECT_EQ_STR(we.we_wordv[0], expected_word,
							"%s: we_wordv[0] | input: %s |", label, words);
		}
	}
	if (res == 0) {
		wordfree(&we);
	}
}
#endif

T_DECL(wordexp_backtick,
	   "Check that wordexp blocks backtick only when it should")
{
#if TARGET_OS_IPHONE
	T_SKIP("wordexp doesn't exist on this OS");
#else /* !TARGET_OS_IPHONE */
	check("`",               WRDE_CMDSUB, NULL,     "unquoted backtick");
	check("\\`",             0,           "`",      "unquoted escaped backtick");
	check("\\\\`",           WRDE_CMDSUB, NULL,     "unquoted escaped backslash + backtick");
	check("'`'",             0,           "`",      "single quoted backtick");
	check("\"`\"",           WRDE_CMDSUB, NULL,     "double quoted backtick");
	check("\"\\`\"",         0,           "`",      "double quoted escaped backtick");
	check("'`",              WRDE_SYNTAX, NULL,     "single quoted backtick w/o ending quote");

	check("\\`$(oh no) # `", WRDE_CMDSUB, NULL,     "previously exploitable");

	check("$(foo)",          WRDE_CMDSUB, NULL,     "unquoted $()");
	check("\\$\\(foo\\)",    0,           "$(foo)", "unquoted escaped $()");
	check("'$(foo)'",        0,           "$(foo)", "single quoted $()");
	check("\"$(foo)\"",      WRDE_CMDSUB, NULL,     "double quoted $()");
	check("\"\\$(foo)\"",    0,           "$(foo)", "double quoted escaped $()");

#endif /* !TARGET_OS_IPHONE */
}

