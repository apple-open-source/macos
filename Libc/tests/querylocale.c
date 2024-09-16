/*
* Copyright (c) 2023 Apple Inc. All rights reserved.
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

#include <locale.h>
#include <xlocale.h>

#include <darwintest.h>
#include <TargetConditionals.h>

#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

/*
 * We don't install the necessary localedata on embedded platforms to be able to
 * usefully run this tests, so just limit it to macOS.
 */
T_DECL(querylocale_names, "Check that querylocale() returns names",
    T_META_ENABLED(TARGET_OS_OSX))
{
	const char *lcat;
	int mask = LC_ALL_MASK;

	T_ASSERT_EQ_STR("en_US.UTF-8", setlocale(LC_ALL, "en_US.UTF-8"), NULL);

	while (mask != 0) {
		lcat = querylocale(mask, NULL);

		T_ASSERT_EQ_STR("en_US.UTF-8", lcat, NULL);
		mask &= ~(1 << (ffs(mask) - 1));
	}
}

T_DECL(querylocale_newlocale_names,
    "Check that querylocale() returns names for newlocale() locales",
    T_META_ENABLED(TARGET_OS_OSX))
{
	const char *lcat;
	locale_t nlocale;
	int mask = LC_ALL_MASK;

	nlocale = newlocale(LC_ALL_MASK, "en_US.UTF-8", NULL);

	while (mask != 0) {
		lcat = querylocale(mask, nlocale);

		T_ASSERT_EQ_STR("en_US.UTF-8", lcat, NULL);
		mask &= ~(1 << (ffs(mask) - 1));
	}

	freelocale(nlocale);
}

/* We expect alphabetical order. */
static int order_mapping[] = {
	[0] = LC_COLLATE,
	[1] = LC_CTYPE,
	[2] = LC_MESSAGES,
	[3] = LC_MONETARY,
	[4] = LC_NUMERIC,
	[5] = LC_TIME,
};

T_DECL(querylocale_order, "Check the querylocale() mask mapping",
    T_META_ENABLED(TARGET_OS_OSX))
{
	const char *lcat;
	int cat;

	for (size_t i = 0; i < nitems(order_mapping); i++) {
		cat = order_mapping[i];

		T_QUIET;
		T_ASSERT_EQ_STR("C", setlocale(cat, NULL), NULL);

		T_ASSERT_EQ_STR("en_US.UTF-8", setlocale(cat, "en_US.UTF-8"),
		    NULL);

		lcat = querylocale(1 << i, NULL);
		T_ASSERT_EQ_STR("en_US.UTF-8", lcat, NULL);

		T_ASSERT_EQ_STR("C", setlocale(cat, "C"), NULL);
	}
}
