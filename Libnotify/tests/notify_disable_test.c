#include <stdlib.h>
#include <stdio.h>
#include <notify.h>
#include <notify_private.h>

#define KEY1 "com.apple.notify.test.disable"
#define KEY2 "com.apple.notify.test.disable.fail"

int main(int argc, char *argv[])
{
	int token1, token2, status, fd;
	uint64_t state;

	token1 = NOTIFY_TOKEN_INVALID;
	token2 = NOTIFY_TOKEN_INVALID;
	fd = -1;

	status = notify_register_file_descriptor(KEY1, &fd, 0, &token1);
	if (status != NOTIFY_STATUS_OK)
	{
		fprintf(stderr, "notify_register_file_descriptor failed status %d\n", status);
		return -1;
	}

	state = 123454321;
	status = notify_set_state(token1, state);
	if (status != NOTIFY_STATUS_OK)
	{
		fprintf(stderr, "notify_set_state failed status %d\n", status);
		return -1;
	}
	
	state = 0;
	status = notify_get_state(token1, &state);
	if (status != NOTIFY_STATUS_OK)
	{
		fprintf(stderr, "notify_set_state failed status %d\n", status);
		return -1;
	}

	if (state != 123454321)
	{
		fprintf(stderr, "notify_get_state returned %llu expected 123454321\n", state);
		return -1;
	}

	notify_set_options(NOTIFY_OPT_DISABLE);
	
	status = notify_register_check(KEY2, &token2);
	if (status == NOTIFY_STATUS_OK)
	{
		fprintf(stderr, "notify_register_check %s succeeded after notify_set_options(NOTIFY_OPT_DISABLE)\n", KEY2);
		return -1;
	}

	state = 0;
	status = notify_get_state(token1, &state);
	if (status == NOTIFY_STATUS_OK)
	{
		fprintf(stderr, "notify_get_state succeeded after notify_set_options(NOTIFY_OPT_DISABLE)\n");
		return -1;
	}

	notify_set_options(NOTIFY_OPT_ENABLE);
	
	state = 0;
	status = notify_get_state(token1, &state);
	if (status != NOTIFY_STATUS_OK)
	{
		fprintf(stderr, "notify_set_state (2) failed status %d\n", status);
		return -1;
	}
	
	if (state != 123454321)
	{
		fprintf(stderr, "notify_get_state (2) returned %llu expected 123454321\n", state);
		return -1;
	}

	printf("Test succeeded\n");
	notify_cancel(token1);
	return 0;
}
