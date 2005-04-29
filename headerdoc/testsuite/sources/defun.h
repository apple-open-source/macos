

/*! @function foo
	@param a arg1
	@param b arg2
 */

#define foo(a, b) { printf(a, b) }

/*! @function bar
	@param c arg3
	@param d arg4
 */

#define bar(c, d) { \
	printf(c, d) \
	}

