//  Black-box tests for notifyd-testing-lib.
//  These tests directly exercise the internal _notify_lib_* APIs.
//
//  NOTE: These tests use Objective-C/XCTest instead of Swift Testing because:
//  - The notify_state_t struct uses complex C types (BSD LIST_HEAD macros,
//    os_unfair_lock, os_map_64_t, table_t, etc.) that don't import into Swift.
//  - Swift's bridging header cannot properly expose these opaque C types.
//  - Objective-C can directly call C functions and use C types without issues.
//
//  Future consideration: If libnotify adds Swift-friendly wrapper types or
//  a module map that properly exports types, tests could be migrated to Swift.
//

#import <XCTest/XCTest.h>
#include "libnotify.h"
#include "notifyd.h"
#include <notify.h>

@interface NotifyStateTests : XCTestCase
@end

@implementation NotifyStateTests

- (void)setUp {
	self.continueAfterFailure = false;
}

#pragma mark - State Initialization Tests

/// Verify _notify_lib_notify_state_init initializes state correctly
- (void)testInitializeState {
    notify_state_t state = {0};
    _notify_lib_notify_state_init(&state, 0);

    // Verify state is initialized
    XCTAssertEqual(state.name_id, 0ULL, @"Initial name_id should be 0");
    XCTAssertEqual(state.stat_name_alloc, 0U, @"Initial stat_name_alloc should be 0");
    XCTAssertEqual(state.stat_client_alloc, 0U, @"Initial stat_client_alloc should be 0");
}

#pragma mark - Registration Tests

/// Register a notification and verify state updates
- (void)testRegisterPlain {
    notify_state_t state = {0};
    _notify_lib_notify_state_init(&state, 0);

    const char *name = "com.apple.test.notifyd-lib.plain";
    pid_t pid = getpid();
    int token = 1;
    uint64_t outNid = 0;

    uint32_t status = _notify_lib_register_plain(
        &state,
        name,
        pid,
        token,
        SLOT_NONE,  // No shared memory slot
        0,          // uid
        0,          // gid
        &outNid
    );

    XCTAssertEqual(status, NOTIFY_STATUS_OK, @"Registration should succeed");
    // Note: Name IDs start at 0 and increment, so 0 is valid for the first name
    XCTAssertEqual(state.name_id, 1ULL, @"name_id counter should have incremented");
    XCTAssertEqual(state.stat_name_alloc, 1U, @"One name should be allocated");
    XCTAssertEqual(state.stat_client_alloc, 1U, @"One client should be allocated");
}

/// Multiple registrations for the same name share the same name_id
- (void)testMultipleRegistrationsForSameName {
    notify_state_t state = {0};
    _notify_lib_notify_state_init(&state, 0);

    const char *name = "com.apple.test.notifyd-lib.multiple";
    pid_t pid = getpid();
    uint64_t outNid1 = 0;
    uint64_t outNid2 = 0;

    // Register twice for the same name
    uint32_t status1 = _notify_lib_register_plain(&state, name, pid, 1, SLOT_NONE, 0, 0, &outNid1);
    uint32_t status2 = _notify_lib_register_plain(&state, name, pid, 2, SLOT_NONE, 0, 0, &outNid2);

    XCTAssertEqual(status1, NOTIFY_STATUS_OK);
    XCTAssertEqual(status2, NOTIFY_STATUS_OK);
    XCTAssertEqual(outNid1, outNid2, @"Same name should have same name ID");
    XCTAssertEqual(state.stat_name_alloc, 1U, @"Only one name should be allocated");
    XCTAssertEqual(state.stat_client_alloc, 2U, @"Two clients should be allocated");
}

#pragma mark - Post and Check Tests

/// Post a notification and verify _notify_lib_check detects it
- (void)testPostAndCheck {
    notify_state_t state = {0};
    _notify_lib_notify_state_init(&state, 0);

    const char *name = "com.apple.test.notifyd-lib.postcheck";
    pid_t pid = getpid();
    int token = 100;
    uint64_t outNid = 0;

    // Register
    uint32_t regStatus = _notify_lib_register_plain(&state, name, pid, token, SLOT_NONE, 0, 0, &outNid);
    XCTAssertEqual(regStatus, NOTIFY_STATUS_OK);

    // Post
    uint32_t postStatus = _notify_lib_post(&state, name, 0, 0);
    XCTAssertEqual(postStatus, NOTIFY_STATUS_OK, @"Post should succeed");

    // Check
    int check = 0;
    uint32_t checkStatus = _notify_lib_check(&state, pid, token, &check);
    XCTAssertEqual(checkStatus, NOTIFY_STATUS_OK, @"Check should succeed");
    XCTAssertNotEqual(check, 0, @"Check should indicate notification was posted");
}

/// Post using name ID (efficient path) instead of name string
- (void)testPostByNameId {
    notify_state_t state = {0};
    _notify_lib_notify_state_init(&state, 0);

    const char *name = "com.apple.test.notifyd-lib.postbynid";
    pid_t pid = getpid();
    int token = 400;
    uint64_t outNid = 0;

    // Register
    uint32_t regStatus = _notify_lib_register_plain(&state, name, pid, token, SLOT_NONE, 0, 0, &outNid);
    XCTAssertEqual(regStatus, NOTIFY_STATUS_OK);

    // Post by name ID (more efficient than by name)
    uint32_t postStatus = _notify_lib_post_nid(&state, outNid, 0, 0);
    XCTAssertEqual(postStatus, NOTIFY_STATUS_OK, @"Post by NID should succeed");

    // Verify notification was received
    int check = 0;
    uint32_t checkStatus = _notify_lib_check(&state, pid, token, &check);
    XCTAssertEqual(checkStatus, NOTIFY_STATUS_OK);
    XCTAssertNotEqual(check, 0, @"Notification should be received");
}

#pragma mark - State Get/Set Tests

/// Set and retrieve notification state value
- (void)testGetAndSetState {
    notify_state_t state = {0};
    _notify_lib_notify_state_init(&state, 0);

    const char *name = "com.apple.test.notifyd-lib.state";
    pid_t pid = getpid();
    int token = 200;
    uint64_t outNid = 0;

    // Register to get a name ID
    uint32_t regStatus = _notify_lib_register_plain(&state, name, pid, token, SLOT_NONE, 0, 0, &outNid);
    XCTAssertEqual(regStatus, NOTIFY_STATUS_OK);

    // Set state
    uint64_t testValue = 0xCAFEBABE;
    uint32_t setStatus = _notify_lib_set_state(&state, outNid, testValue, 0, 0);
    XCTAssertEqual(setStatus, NOTIFY_STATUS_OK, @"Set state should succeed");

    // Get state
    uint64_t retrievedValue = 0;
    uint32_t getStatus = _notify_lib_get_state(&state, outNid, &retrievedValue, 0, 0);
    XCTAssertEqual(getStatus, NOTIFY_STATUS_OK, @"Get state should succeed");
    XCTAssertEqual(retrievedValue, testValue, @"Retrieved state should match set value");
}

#pragma mark - Cancel Tests

/// Cancel a registration and verify cleanup
- (void)testCancelRegistration {
    notify_state_t state = {0};
    _notify_lib_notify_state_init(&state, 0);

    const char *name = "com.apple.test.notifyd-lib.cancel";
    pid_t pid = getpid();
    int token = 300;
    uint64_t outNid = 0;

    // Register
    uint32_t regStatus = _notify_lib_register_plain(&state, name, pid, token, SLOT_NONE, 0, 0, &outNid);
    XCTAssertEqual(regStatus, NOTIFY_STATUS_OK);
    XCTAssertEqual(state.stat_client_alloc, 1U);

    // Cancel
    _notify_lib_cancel(&state, pid, token);
    XCTAssertEqual(state.stat_client_free, 1U, @"Client should be freed after cancel");
}

#pragma mark - Common Port Regeneration (rdar://163900566)

/// Simulates the production workflow of: register clients, then regenerate common port.
/// Verifies the state of clients list.
///
/// Bug caught - iterating common_port_data->clients with wrong linkage.
///
/// The bug: clients are added via client_port_entry, but were cleaned up using client_pid_entry
/// traversal.
///
/// To expose this, we create:
///   - A "decoy" PLAIN client: only in proc->clients (via client_pid_entry)
///   - A common port client: in both lists
///
/// List state after setup:
///   proc->clients (client_pid_entry):           common -> decoy
///   common_port_data->clients (client_port_entry): common
///
/// With BUGGY code (iterates via client_pid_entry):
///   common -> decoy -> NULL  (decoy incorrectly cancelled!)
///
/// With FIXED code (iterates via client_port_entry):
///   common -> NULL  (decoy untouched, correct)
- (void)testCleanUpCommonPortClients {
    // Setup
    struct call_statistics_s mock_stats = {0};
    struct global_s mock_global = {0};
    mock_global.shared_memory_refcount = NULL;
    mock_global.call_stats = &mock_stats;
    _notify_lib_notify_state_init(&mock_global.notify_state, 0);
    notify_state_t *ns = &mock_global.notify_state;

    pid_t test_pid = 99999;
    proc_data_t *proc = _proc_data_alloc(ns, test_pid);
    proc->common_port_data = _common_port_data_alloc(ns, MACH_PORT_NULL);

    // Step 1: Create a "decoy" PLAIN client - ONLY in proc->clients
    uint64_t decoy_nid;
    int decoy_token = 1000;
    _notify_lib_register_plain(ns, "decoy.plain.client", test_pid, decoy_token,
                               SLOT_NONE, 0, 0, &decoy_nid);
    uint64_t decoy_cid = make_client_id(test_pid, decoy_token);
    client_t *decoy = _nc_table_find_64(&ns->client_table, decoy_cid);
    XCTAssertNotEqual(decoy, NULL, @"Decoy client should exist");

    // Manually add decoy to proc->clients (simulating proc_register behavior)
    LIST_INSERT_HEAD(&proc->clients, decoy, client_pid_entry);

    // Step 2: Create 1 common port client (in both lists)
    uint32_t status = 0;
    client_t *common = _register_common_port_client(
        ns, proc, "common.client", test_pid, 100, 0, 0, &status);
    XCTAssertNotEqual(common, NULL, @"Common client should be created");

    // Verify setup: decoy in proc->clients but NOT in common_port_data->clients
    BOOL decoy_in_proc = NO, decoy_in_port = NO;
    client_t *c;
    LIST_FOREACH(c, &proc->clients, client_pid_entry) {
        if (c == decoy) decoy_in_proc = YES;
    }
    LIST_FOREACH(c, &proc->common_port_data->clients, client_port_entry) {
        if (c == decoy) decoy_in_port = YES;
    }
    XCTAssertTrue(decoy_in_proc, @"Decoy should be in proc->clients");
    XCTAssertFalse(decoy_in_port, @"Decoy should NOT be in common_port_data->clients");

    // Execute: cleanup common port clients
    _cleanup_common_port_clients(&mock_global, proc->common_port_data);

    // Verify:
    // With BUGGY code: decoy incorrectly cancelled (traversed via wrong linkage)
    // With FIXED code: decoy still exists
    client_t *decoy_after = _nc_table_find_64(&ns->client_table, decoy_cid);
    XCTAssertNotEqual(decoy_after, NULL,
        @"Decoy should NOT be cancelled - it's not a common port client! "
        @"If this fails, the bug is present (wrong list linkage).");

    // Common port list should be empty
    XCTAssertTrue(LIST_EMPTY(&proc->common_port_data->clients),
        @"Common port client list should be empty after cleanup");

    // Only 1 common port client should be freed (not the decoy)
    XCTAssertEqual(ns->stat_client_free, 1U,
        @"Only 1 common port client should be freed, not the decoy");
}

@end
