#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

#include <libcharset.h>

int
main(int argc, char *argv[])
{

	setlocale(LC_ALL, "");

	if (argc == 1) {
		/*
		 * With no arguments, we just print the current
		 * locale_charset().
		 */
		printf("%s\n", locale_charset());
	} else {
		/*
		 * With one or more arguments, each argument is assumed to be a
		 * CHARSETALIASDIR and we'll try each one.  This is really just
		 * used to demonstrate that the mapping is fixed after the first
		 * call.
		 */
		for (int i = 1; i < argc; i++) {
			if (argv[i][0] == '\0')
				unsetenv("CHARSETALIASDIR");
			else
				setenv("CHARSETALIASDIR", argv[i], 1);
			printf("%s\n", locale_charset());
		}
	}

	return (0);
}
