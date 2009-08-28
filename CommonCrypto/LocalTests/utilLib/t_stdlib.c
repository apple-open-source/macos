#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <security_bsafe/bsafe.h>

void T_free(POINTER block)
{
    if (block != NULL_PTR) {
		free(block);
    }
}

POINTER T_malloc(unsigned int len)
{
    return (POINTER) malloc(len ? len : 1);
}

/* these are not needed - they are in system.c in security_bsafe */
#if 0
int T_memcmp(POINTER firstBlock, POINTER secondBlock, unsigned int len)
{
    if (len == 0) {
		return 0;
    }
    return memcmp(firstBlock, secondBlock, len);
}

void T_memcpy(POINTER output, POINTER input, unsigned int len)
{
    if (len != 0) {
		memcpy(output, input, len);
    }
}

void T_memmove(POINTER output, POINTER input, unsigned int len)
{
    if (len != 0) {
		memmove(output, input, len);
    }
}

void T_memset(POINTER output, int value, unsigned int len)
{
    if (len != 0) {
		memset(output, value, len);
    }
}
#endif

POINTER T_realloc(POINTER block, unsigned int len)
{
    if (block == NULL_PTR)
		return (POINTER) malloc(len ? len : 1);
	
	return (POINTER)realloc(block, len);
}
