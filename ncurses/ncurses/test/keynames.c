/*
 * $Id: keynames.c,v 1.1.1.1 2001/11/29 20:40:59 jevans Exp $
 */

#include <test.priv.h>

int
main(int argc GCC_UNUSED, char *argv[]GCC_UNUSED)
{
    int n;
    for (n = -1; n < 512; n++) {
	printf("%d(%5o):%s\n", n, n, keyname(n));
    }
    ExitProgram(EXIT_SUCCESS);
}
