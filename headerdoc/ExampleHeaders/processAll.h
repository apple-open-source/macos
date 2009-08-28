/*
     This is a header containing a mixture of API with and without
     HeaderDoc comments for testing the new flag to process all content
     regardless of tagging.
 */


void foo(char *a);

/*! This is the documentation for bar. */
/* Initial comment */
int bar(char *b);

#if APPLE_HEADER
#define foo bar

void multiline(int a,
	char *b,
	void *c);

#endif

char *baz;

/*! This is the documentation for bat. */
void *bat;

char *tail;

