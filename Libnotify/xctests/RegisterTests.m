//
//  xctests.m
//  xctests
//

#include <dispatch/dispatch.h>
#include <notify.h>
#include <stdatomic.h>
#include <unistd.h>

#include "notify_internal.h"

#import <XCTest/XCTest.h>

//  atomic types so we can use ++
static atomic_long register_count = -1; // notify_lib_init calls register, so ignore that one
static atomic_long cancel_count = 0;
static atomic_uint_fast64_t nid = 0;

// mock the server calls so we can make sure we are balancing register/cancel - we are not using the server at all for this test module

kern_return_t
_notify_server_register_mach_port_2(mach_port_t server, caddr_t name, int token, mach_port_t port)
{
	register_count++;
	return KERN_SUCCESS;
}

kern_return_t
_notify_server_cancel_2(mach_port_t server, int token)
{
	cancel_count++;
	return KERN_SUCCESS;
}

kern_return_t
_notify_server_post_2(mach_port_t server, caddr_t name, uint64_t *name_id, int *status, boolean_t claim_root_access)
{
    *name_id = nid++;
    *status = NOTIFY_STATUS_OK;
    return KERN_SUCCESS;
}

kern_return_t
_notify_server_post_3(mach_port_t server, uint64_t name_id, boolean_t claim_root_access)
{
    return KERN_SUCCESS;
}

kern_return_t
_notify_server_post_4(mach_port_t server, caddr_t name, boolean_t claim_root_access)
{
    return KERN_SUCCESS;
}

kern_return_t
_notify_server_register_plain_2(mach_port_t server, caddr_t name, int token)
{
	register_count++;
	return KERN_SUCCESS;
}

kern_return_t
_notify_server_register_check_2(mach_port_t server, caddr_t name, int token, int *size, int *slot, uint64_t *name_id, int *status)
{
	register_count++;
	*name_id = nid++;
	*status = NOTIFY_STATUS_OK;
	*size = getpagesize();
	return KERN_SUCCESS;
}

kern_return_t
_notify_server_register_signal_2(mach_port_t server, caddr_t name, int token, int sig)
{
	register_count++;
	return KERN_SUCCESS;
}

kern_return_t
_notify_server_register_file_descriptor_2(mach_port_t server, caddr_t name, int token, mach_port_t fileport)
{
	register_count++;
	return KERN_SUCCESS;
}

kern_return_t
_notify_server_register_plain(mach_port_t server, caddr_t name, int *token, int *status)
{
    *status = NOTIFY_STATUS_OK;
    return KERN_SUCCESS;
}

kern_return_t
_notify_server_cancel(mach_port_t server, int token, int *status)
{
    *status = NOTIFY_STATUS_OK;
    return KERN_SUCCESS;
}

kern_return_t
_notify_server_get_state(mach_port_t server, int token, uint64_t *state, int *status)
{
    *state = ((uint64_t)getpid() << 32) | NOTIFY_IPC_VERSION;
    *status = NOTIFY_STATUS_OK;
    return KERN_SUCCESS;
}

kern_return_t
_notify_server_checkin(mach_port_t server, uint32_t *version, uint32_t *server_pid, int *status)
{
	*version = NOTIFY_IPC_VERSION;
	*status = NOTIFY_STATUS_OK;
	*server_pid = getpid();
	return KERN_SUCCESS;
}

@interface RegisterTests : XCTestCase

@end

@implementation RegisterTests

static dispatch_queue_t noteQueue;

+ (void)setUp
{
	noteQueue = dispatch_queue_create("noteQ", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
}

+ (void)tearDown
{
	noteQueue = nil;
}

- (void)tearDown
{
	XCTAssert(register_count == cancel_count, @"register_count %ld != cancel_count %ld", register_count, cancel_count);
	register_count = cancel_count = 0;
	[super tearDown];
}

- (void)test00RegisterSimple
{	
	static int token;
	XCTAssert(notify_register_dispatch("com.example.test.simple", &token, noteQueue, ^(int i){}) == NOTIFY_STATUS_OK, @"notify_register_dispatch failed");
	XCTAssert(notify_cancel(token) == NOTIFY_STATUS_OK, @"notify_cancel failed");
}

- (void)test01RegisterNested
{	
	for (int i = 0; i < 100000; i++) {
		static int token1, token2;
		XCTAssert(notify_register_dispatch("com.example.test.multiple", &token1, noteQueue, ^(int i){}) == NOTIFY_STATUS_OK, @"notify_register_dispatch failed");
		XCTAssert(notify_register_dispatch("com.example.test.multiple", &token2, noteQueue, ^(int i){}) == NOTIFY_STATUS_OK, @"notify_register_dispatch failed");
		XCTAssert(notify_cancel(token1) == NOTIFY_STATUS_OK, @"notify_cancel failed");
		XCTAssert(notify_cancel(token2) == NOTIFY_STATUS_OK, @"notify_cancel failed");
	}
}

- (void)test02RegisterInterleaved
{
	for (int i = 0; i < 100000; i++) {
		static int token1, token2;
		XCTAssert(notify_register_dispatch("com.example.test.interleaved", &token1, noteQueue, ^(int i){}) == NOTIFY_STATUS_OK, @"notify_register_dispatch failed");
		XCTAssert(notify_cancel(token1) == NOTIFY_STATUS_OK, @"notify_cancel failed");
		XCTAssert(notify_register_dispatch("com.example.test.interleaved", &token2, noteQueue, ^(int i){}) == NOTIFY_STATUS_OK, @"notify_register_dispatch failed");
		XCTAssert(notify_cancel(token2) == NOTIFY_STATUS_OK, @"notify_cancel failed");
	}
}

- (void)test03RegisterRaceWithDealloc
{	
    static int tokens[1000000];
    dispatch_apply(1000000, DISPATCH_APPLY_AUTO, ^(size_t i) {
        XCTAssert(notify_register_check("com.example.test.race", &tokens[i]) == NOTIFY_STATUS_OK, @"notify_register_check failed");
		XCTAssert(notify_cancel(tokens[i]) == NOTIFY_STATUS_OK, @"notify_cancel failed");
		XCTAssert(notify_register_dispatch("com.example.test.race", &tokens[i], noteQueue, ^(int i){}) == NOTIFY_STATUS_OK, @"notify_register_dispatch failed");
		XCTAssert(notify_cancel(tokens[i]) == NOTIFY_STATUS_OK, @"notify_cancel failed");
        XCTAssert(notify_post("com.example.test.race") == NOTIFY_STATUS_OK, @"notify_post failed");
	});
}

- (void)test04RegisterManyTokens
{	
    static int tokens[100000];
	dispatch_apply(100000, DISPATCH_APPLY_AUTO, ^(size_t i) {
		XCTAssert(notify_register_dispatch("com.example.test.many", &tokens[i], noteQueue, ^(int i){}) == NOTIFY_STATUS_OK, @"notify_register_dispatch failed");
		XCTAssert(notify_cancel(tokens[i]) == NOTIFY_STATUS_OK, @"notify_cancel failed");
	});
}

- (void)test05RegisterBulkCancel
{	
    static int tokens[100000];
	dispatch_apply(100000, DISPATCH_APPLY_AUTO, ^(size_t i) {
		XCTAssert(notify_register_dispatch("com.example.test.bulk", &tokens[i], noteQueue, ^(int i){}) == NOTIFY_STATUS_OK, @"notify_register_dispatch failed");
	});
	
	dispatch_apply(100000, DISPATCH_APPLY_AUTO, ^(size_t i) {
		XCTAssert(notify_cancel(tokens[i]) == NOTIFY_STATUS_OK, @"notify_cancel failed");
	});
}

@end
