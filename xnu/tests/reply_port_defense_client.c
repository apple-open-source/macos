#include <mach/kern_return.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/task.h>
#include <mach/thread_status.h>
#include <assert.h>
#include <sys/codesign.h>
#include <stdbool.h>
#include "cs_helpers.h"

#define MAX_TEST_NUM 9

#if __arm64__
#define machine_thread_state_t          arm_thread_state64_t
#define EXCEPTION_THREAD_STATE          ARM_THREAD_STATE64
#define EXCEPTION_THREAD_STATE_COUNT    ARM_THREAD_STATE64_COUNT
#elif __x86_64__
#define machine_thread_state_t          x86_thread_state_t
#define EXCEPTION_THREAD_STATE          x86_THREAD_STATE
#define EXCEPTION_THREAD_STATE_COUNT    x86_THREAD_STATE_COUNT
#else
#error Unsupported architecture
#endif

static mach_port_t
alloc_server_port(void)
{
	mach_port_t server_port = MACH_PORT_NULL;
	kern_return_t kr;

	kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &server_port);
	assert(kr == 0);

	kr = mach_port_insert_right(mach_task_self(), server_port, server_port, MACH_MSG_TYPE_MAKE_SEND);
	assert(kr == 0);

	return server_port;
}

static mach_port_t
alloc_provisional_reply_port()
{
	kern_return_t kr;
	mach_port_t reply_port = MACH_PORT_NULL;
	mach_port_t task = mach_task_self();

	mach_port_options_t opts = {
		.flags = MPO_PROVISIONAL_REPLY_PORT | MPO_INSERT_SEND_RIGHT,
	};

	kr = mach_port_construct(mach_task_self(), &opts, 0, &reply_port);
	assert(kr == 0);

	return reply_port;
}

static mach_port_t
alloc_reply_port()
{
	kern_return_t kr;
	mach_port_t reply_port = MACH_PORT_NULL;
	mach_port_t task = mach_task_self();

	mach_port_options_t opts = {
		.flags = MPO_REPLY_PORT | MPO_INSERT_SEND_RIGHT,
	};

	kr = mach_port_construct(mach_task_self(), &opts, 0, &reply_port);
	assert(kr == 0);

	return reply_port;
}

/* The rcv right of the port would be marked immovable. */
static void
test_immovable_receive_right(void)
{
	kern_return_t kr;
	mach_port_t server_port = MACH_PORT_NULL, reply_port = MACH_PORT_NULL;
	struct {
		mach_msg_header_t header;
		mach_msg_body_t body;
		mach_msg_port_descriptor_t desc;
	} msg;

	server_port = alloc_server_port();
	reply_port = alloc_reply_port();

	msg.header.msgh_remote_port = server_port;
	msg.header.msgh_local_port = MACH_PORT_NULL;
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
	msg.header.msgh_size = sizeof msg;

	msg.body.msgh_descriptor_count = 1;

	msg.desc.name = reply_port;
	msg.desc.disposition = MACH_MSG_TYPE_MOVE_RECEIVE;
	msg.desc.type = MACH_MSG_PORT_DESCRIPTOR;
	kr = mach_msg_send(&msg.header);

	printf("[reply_port_defense_client test_immovable_receive_right]: mach_msg2() returned %d\n", kr);
}

/* The only way you could create a send once right is when you send the port in local port of a mach msg with MAKE_SEND_ONCE disposition. */
static void
test_make_send_once_right(void)
{
	kern_return_t kr;
	mach_port_t reply_port = alloc_reply_port();
	kr = mach_port_insert_right(mach_task_self(), reply_port, reply_port, MACH_MSG_TYPE_MAKE_SEND_ONCE);
	printf("[reply_port_defense_client test_make_send_once_right]: mach_port_insert_right() returned %d\n", kr);
}

/* The send right of the port would only used for guarding a name in ipc space, it would not allow to send a message. */
static void
test_using_send_right(void)
{
	kern_return_t kr;
	mach_port_t reply_port = alloc_reply_port();
	struct {
		mach_msg_header_t header;
		mach_msg_body_t body;
	} msg;

	msg.header.msgh_remote_port = reply_port;
	msg.header.msgh_local_port = MACH_PORT_NULL;
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
	msg.header.msgh_size = sizeof msg;

	kr = mach_msg_send(&msg.header);
	printf("[reply_port_defense_client test_using_send_right]: mach_msg2() returned %d\n", kr);
}

/* The send right of the port would only used for guarding a name in ipc space, it would not allowed to get moved. */
static void
test_move_send_right(void)
{
	kern_return_t kr;
	mach_port_t server_port = MACH_PORT_NULL, reply_port = MACH_PORT_NULL;
	struct {
		mach_msg_header_t header;
		mach_msg_body_t body;
		mach_msg_port_descriptor_t desc;
	} msg;

	server_port = alloc_server_port();
	reply_port = alloc_reply_port();

	msg.header.msgh_remote_port = server_port;
	msg.header.msgh_local_port = MACH_PORT_NULL;
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
	msg.header.msgh_size = sizeof msg;

	msg.body.msgh_descriptor_count = 1;

	msg.desc.name = reply_port;
	msg.desc.disposition = MACH_MSG_TYPE_MOVE_SEND;
	msg.desc.type = MACH_MSG_PORT_DESCRIPTOR;

	kr = mach_msg_send(&msg.header);
	printf("[reply_port_defense_client test_move_send_right]: mach_msg2() returned %d\n", kr);
}

static void
test_move_provisional_reply_port(void)
{
	kern_return_t kr;
	mach_port_t server_port = MACH_PORT_NULL, reply_port = MACH_PORT_NULL;
	struct {
		mach_msg_header_t header;
		mach_msg_body_t body;
		mach_msg_port_descriptor_t desc;
	} msg;

	server_port = alloc_server_port();
	reply_port = alloc_provisional_reply_port();

	msg.header.msgh_remote_port = server_port;
	msg.header.msgh_local_port = MACH_PORT_NULL;
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
	msg.header.msgh_size = sizeof msg;

	msg.body.msgh_descriptor_count = 1;

	msg.desc.name = reply_port;
	msg.desc.disposition = MACH_MSG_TYPE_MOVE_RECEIVE;
	msg.desc.type = MACH_MSG_PORT_DESCRIPTOR;

	kr = mach_msg_send(&msg.header);

	printf("[reply_port_defense_client test_immovable_receive_right]: mach_msg2() returned %d\n", kr);
}

static void
test_unentitled_thread_set_state(void)
{
	machine_thread_state_t ts;
	mach_msg_type_number_t count = MACHINE_THREAD_STATE_COUNT;

	/* thread_set_state as a hardened binary should fail */
	kern_return_t kr = thread_get_state(mach_thread_self(), MACHINE_THREAD_STATE, (thread_state_t)&ts, &count);

	kr = thread_set_state(mach_thread_self(), MACHINE_THREAD_STATE, (thread_state_t)&ts, count);
	assert(kr != KERN_SUCCESS);
	exit(-1); /* Should have crashed before here! */
}

static void
unentitled_set_exception_ports_crash(void)
{
	mach_port_t exc_port = alloc_server_port();

	/* thread_set_exception_ports as a hardened binary should fail */
	kern_return_t kr = thread_set_exception_ports(
		mach_thread_self(),
		EXC_MASK_ALL,
		exc_port,
		(exception_behavior_t)((unsigned int)EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES),
		EXCEPTION_THREAD_STATE);

	/* thread_set_exception_ports is supposed to crash, unless the policy is turned off.
	 * Things that disable the policy: AMFI boot-args in use, SIP disabled,
	 * third party plugins in a process. The caller of this client will check
	 * whether the test crashed and correctly adhered to these policies.
	 */
	printf("thread_set_exception_ports did not crash\n");
}

static void
unentitled_set_exception_ports_pass(void)
{
	mach_port_t exc_port = alloc_server_port();

	/* thread_set_exception_ports with state *IDENTITY_PROTECTED should not fail */
	kern_return_t kr = thread_set_exception_ports(
		mach_thread_self(),
		EXC_MASK_ALL,
		exc_port,
		(exception_behavior_t)((unsigned int)EXCEPTION_STATE_IDENTITY_PROTECTED | MACH_EXCEPTION_CODES),
		EXCEPTION_THREAD_STATE);
	assert(kr == 0);

	kr = thread_set_exception_ports(
		mach_thread_self(),
		EXC_MASK_ALL,
		exc_port,
		(exception_behavior_t)((unsigned int)EXCEPTION_IDENTITY_PROTECTED | MACH_EXCEPTION_CODES),
		EXCEPTION_THREAD_STATE);
	assert(kr == 0);

	return;
}

static void
exception_ports_crash(void)
{
	kern_return_t kr;
	mach_port_t exc_port;
	mach_port_options_t opts = {
		.flags = MPO_INSERT_SEND_RIGHT | MPO_EXCEPTION_PORT,
	};

	kr = mach_port_construct(mach_task_self(), &opts, 0ull, &exc_port);
	assert(kr == KERN_SUCCESS);

	kr = task_register_hardened_exception_handler(current_task(),
	    0, EXC_MASK_BAD_ACCESS,
	    EXCEPTION_STATE_IDENTITY_PROTECTED, EXCEPTION_THREAD_STATE, exc_port);

	kr = thread_set_exception_ports(
		mach_thread_self(),
		EXC_MASK_BAD_ACCESS,
		exc_port,
		(exception_behavior_t)((unsigned int)EXCEPTION_STATE_IDENTITY_PROTECTED | MACH_EXCEPTION_CODES),
		EXCEPTION_THREAD_STATE);

	printf("thread_set_exception_ports did not crash: %d\n", kr);
}

int
main(int argc, char *argv[])
{
	uint32_t my_csflags = 0;
	bool thirdparty_hardened = !strcmp(argv[0], "./reply_port_defense_client_3P_hardened");

	/* TODO add some sysctl which disabled platform binary bit here */
	if (my_csflags & CS_PLATFORM_BINARY == thirdparty_hardened) {
		printf("platform binary does not match expected\n");
		return -1;
	}


	void (*tests[MAX_TEST_NUM])(void) = {
		test_immovable_receive_right, /* 0 */
		test_make_send_once_right,
		test_using_send_right, /* 2 */
		test_move_send_right,
		test_move_provisional_reply_port, /* 4 */
		unentitled_set_exception_ports_crash,
		test_unentitled_thread_set_state, /* 6 */
		unentitled_set_exception_ports_pass,
		exception_ports_crash, /* 8 */
	};

	if (argc < 2) {
		printf("[reply_port_defense_client]: Specify a test to run.");
		exit(-1);
	}

	int test_num = atoi(argv[1]);
	printf("[reply_port_defense_client]: My Pid: %d Test num: %d third_party_hardened: %s\n",
	    getpid(), test_num, thirdparty_hardened ? "yes" : "no");
	fflush(stdout);
	if (test_num >= 0 && test_num < MAX_TEST_NUM) {
		(*tests[test_num])();
	} else {
		printf("[reply_port_defense_client]: Invalid test num. Exiting...\n");
		exit(-1);
	}
	printf("Child exiting cleanly!!\n");
	fflush(stdout);
	// return 0;
	exit(0);
}
