#include <stdio.h>
#include <stdlib.h>

int main() {
	fputs("To install emacs on PPC you must run the following command\nsudo /usr/libexec/dumpemacs\n", stderr);
	exit(1);
}
