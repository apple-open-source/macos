#include <stdlib.h>
#include <stdio.h>

int main(int c, char **v)
{
    int i, t;
    t = atoi(v[1]);
    if (t == 3) {
	for (i = 0; i < 1000000; ++i) {
	    putchar('a');
	}
    } else if (t == 2) {
	printf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
    } else {
	printf("abc");
    }
    return(0);
}
