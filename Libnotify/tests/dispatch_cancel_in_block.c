#include <stdlib.h>
#include <notify.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#define KEY "foo.test"
#define COUNT 10
int verbose = 0;

void
doit(dispatch_queue_t q, int n)
{
	int i;
	int status;
		
	for (i = 0; i < n; ++i)
	{
		int t;
		status = notify_register_dispatch(KEY, &t, q, ^(int x){
				printf("handle %d\n", x);
				notify_cancel(x);
		});
		
		assert(status == NOTIFY_STATUS_OK);
		printf("register %d\n", t);
	}
	
	notify_post(KEY);
}

int main(int argc, char *argv[])
{
	int i, n = COUNT;
	int nap = 60;
	dispatch_queue_t q = dispatch_queue_create("Notify", NULL);
	
	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-n")) n = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-z")) nap = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-v")) verbose = 1;
	}
	
	doit(q, n);
	
	dispatch_release(q);
	dispatch_main();
	return 0;
}
