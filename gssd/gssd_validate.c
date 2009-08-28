#include <search.h>
#include <stdint.h>
#include "gssd.h"

void *rootp = (void *)0;

static int
compare(const void *p1, const void *p2)
{
	uintptr_t v1 = (uintptr_t)p1;
	uintptr_t v2 = (uintptr_t)p2;
	if (v1 == v2)
		return (0);
	else if (v1 < v2)
		return (-1);
	return (1);
}

void
gssd_enter(void *ptr)
{
	(void) tsearch(ptr, &rootp, compare);
}

void
gssd_remove(void *ptr)
{
	if (ptr)
		(void) tdelete(ptr, &rootp, compare);
}

int
gssd_check(void *ptr)
{
	if (ptr == (void *)0)
		return (1);
	return (tfind(ptr, &rootp, compare) != (void *)0);
}

#if 0
#include <stdio.h>
#include <stdlib.h>

main()
{
	void *p1 = malloc(1);
	void *p2 = malloc(2);
	void *p3 = malloc(3);
	
	gssd_enter(p1);
	gssd_enter(p2);
	gssd_enter(p3);
	
	if (gssd_check(p1))
		printf("p1 is ok\n");
	if (gssd_check(p2))
		printf("p2 is ok\n");
	if (gssd_check(p3))
		printf("p3 is ok\n");
	
	if (gssd_check((void *)0x100))
		printf("0x100 is bogus\n");
	if (gssd_check(&p1))
		printf("stack is bougs\n");
	gssd_remove(p1);
	if (!gssd_check(p1))
		printf("p1 removed\n");
	gssd_remove(p3);
	if (!gssd_check(p3))
		printf("p3 removed\n");
	if (gssd_check(p2))
		printf("p2 is still here\n");
	gssd_remove(p2);
	if (!gssd_check(p2))
		printf("p2 is gone\n");
	printf("root = %p\n", rootp);
		
}
#endif 
