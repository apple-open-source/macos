
/*! @struct foo
    @abstract test struct
    @attribute readable false
    @attributeblock accessibility
	This is a royal bloody mess
	that no one in their right minds should use.
    @attributelist other stuff
	people John, James, Ralph, Terry
	places Paris, Cairo, Luxembourg
 */

struct foo {
	int a = 3;
	char *b = "this is a test";		// this should be a comment
	char *c = "this is \" a test";		/* this should be, too */
	char d = 'f';				/* this "should" // still
						   be 'a' comment */
	char e = '\'';
}
