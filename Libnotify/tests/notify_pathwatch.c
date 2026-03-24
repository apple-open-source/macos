//
//  notify_pathwatch.c
//  tests
//
//  Created by Brycen Wershing on 8/2/18.
//

#include <darwintest.h>
#include <dispatch/dispatch.h>
#include <notify.h>
#include <stdio.h>
#include <stdlib.h>
#include <tzfile.h>
#include <mach/mach.h>
#include <darwintest_multiprocess.h>
#include "notify_private.h"

int dispatch_changer;

#define PATHWATCH_TEST_NAME "com.example.test.pathwatch"
#define PATHWATCH_TEST_FILE "/tmp/notify_pathwatch.test"

T_DECL(notify_pathwatch,
       "notify pathwatch test",
       T_META("owner", "Core Darwin Daemons & Tools"),
       T_META("as_root", "true"))
{
	dispatch_queue_t testQueue = dispatch_queue_create("testQ", DISPATCH_QUEUE_SERIAL);
	uint32_t rc;
	static int check_token = NOTIFY_TOKEN_INVALID;
	static int disp_token = NOTIFY_TOKEN_INVALID;
	int check;
	uint64_t state;
	int fd, bytes_written;

	dispatch_changer = 0;


	// register for check and dispatch

	rc = notify_register_check(PATHWATCH_TEST_NAME, &check_token);
	assert(rc == NOTIFY_STATUS_OK);

	rc = notify_check(check_token, &check);
	assert(rc == NOTIFY_STATUS_OK);
	assert(check == 1);

	rc = notify_register_dispatch(PATHWATCH_TEST_NAME, &disp_token, testQueue, ^(int i){dispatch_changer++;});
	assert(rc == NOTIFY_STATUS_OK);


	// add file monitoring for nonexistant file
	unlink(PATHWATCH_TEST_FILE); // return code ignored because the file probably does not exist


	notify_monitor_file(check_token, PATHWATCH_TEST_FILE, 0x3ff);
	notify_monitor_file(disp_token, PATHWATCH_TEST_FILE, 0x3ff);


	// there should not be a post yet
	dispatch_sync(testQueue, ^{assert(
		dispatch_changer == 0);
	});

	rc = notify_check(check_token, &check);
	assert(rc == NOTIFY_STATUS_OK);
	assert(check == 0);


	// post and fence
	rc = notify_post(PATHWATCH_TEST_NAME);
	assert(rc == NOTIFY_STATUS_OK);

	notify_get_state(check_token, &state); //fence
	sleep(1);

	// check to make sure post was good
	rc = notify_check(check_token, &check);
	assert(rc == NOTIFY_STATUS_OK);
	assert(check == 1);

	dispatch_sync(testQueue, ^{
		assert(dispatch_changer == 1);
		dispatch_changer = 0;
	});




	// create the file
	fd = creat(PATHWATCH_TEST_FILE, 0);
	assert(fd != -1);

	sleep(1);
	notify_get_state(check_token, &state); //fence

	// check to make sure post was good
	rc = notify_check(check_token, &check);
	assert(rc == NOTIFY_STATUS_OK);
	assert(check == 1);

	dispatch_sync(testQueue, ^{
		assert(dispatch_changer == 1);
		dispatch_changer = 0;
	});


	// write something to the file
	bytes_written = write(fd, "this", 4);
	assert(bytes_written == 4);

	sleep(1);
	notify_get_state(check_token, &state); //fence

	// check to make sure post was good
	rc = notify_check(check_token, &check);
	assert(rc == NOTIFY_STATUS_OK);
	assert(check == 1);

	dispatch_sync(testQueue, ^{
		assert(dispatch_changer == 1);
		dispatch_changer = 0;
	});


	// delete the file
	rc = close(fd);
	assert(rc == 0);

	unlink(PATHWATCH_TEST_FILE);

	sleep(1);
	notify_get_state(check_token, &state); //fence

	// check to make sure post was good
	rc = notify_check(check_token, &check);
	assert(rc == NOTIFY_STATUS_OK);
	assert(check == 1);

	dispatch_sync(testQueue, ^{
		assert(dispatch_changer == 1);
		dispatch_changer = 0;
	});

	// cleanup
	rc = notify_cancel(check_token);
	assert(rc == NOTIFY_STATUS_OK);

	rc = notify_cancel(disp_token);
	assert(rc == NOTIFY_STATUS_OK);

	dispatch_release(testQueue);

	fd = creat(PATHWATCH_TEST_FILE, 0);
	assert(fd != -1);

	
	dt_helper_t helpers[50];
	for(int i = 0; i < 50; i++){
		helpers[i] = dt_child_helper("notify_pathwatch_helper");
	}

	dt_run_helpers(helpers, 50, 180);

	rc = close(fd);
	assert(rc == 0);

	unlink(PATHWATCH_TEST_FILE);


	T_PASS("Success");
}


T_HELPER_DECL(notify_pathwatch_helper,
	      "notify pathwatch test helper",
	      T_META("owner", "Core Darwin Daemons & Tools"))
{
	int token;
	char *filename;
	uint32_t block_rc;
	block_rc = notify_register_check(PATHWATCH_TEST_NAME, &token);
	assert(block_rc == NOTIFY_STATUS_OK);
	notify_monitor_file(token, PATHWATCH_TEST_FILE, 0x3ff);
	T_PASS("Helper passed");
	T_END;
	exit(0);
}

#pragma mark - rdar://167445250

static pid_t
get_notifyd_pid(void)
{
	FILE *fp;
	char buf[32];
	pid_t pid = 0;

	fp = popen("pgrep -x notifyd", "r");
	if (fp == NULL) {
		return 0;
	}
	if (fgets(buf, sizeof(buf), fp) != NULL) {
		pid = (pid_t)strtol(buf, NULL, 10);
	}
	pclose(fp);
	return pid;
}

T_DECL(notify_pathwatch_interesting_paths,
       "notifyd should not crash when handling pathwatch on paths")
{
	static const char *protected_paths[] = {
		"/var/Keychains/keychain-2.db",
		"/var/Keychains/ocspcache.sqlite3",
		"/var/mobile/Library/SMS/sms.db",
		"/var/mobile/Library/SMS/sms.db-wal",
		"/var/mobile/Library/Health/healthdb.sqlite",
		"/var/mobile/Library/Health/healthdb_secure.sqlite",
		"/var/mobile/Library/AddressBook/AddressBook.sqlitedb",
		"/var/mobile/Library/CallHistoryDB/CallHistory.storedata",
		"/var/mobile/Library/Safari/History.db",
		"/var/mobile/Library/Safari/Bookmarks.db",
		"/var/mobile/Library/Notes/notes.sqlite",
		"/var/mobile/Library/Voicemail/voicemail.db",
		"/var/mobile/Media/PhotoData/Photos.sqlite",
		"/var/mobile/Library/Mail/Envelope Index",
		"/var/mobile/Library/Caches/locationd/clients.plist",
		"/var/run/utmpx",
		"/private/var/db/dhcpclient/leases",
	};

	static const char *good_paths[] = {
		TZDIR
	};

	static const char *pathwatch_prefix = "com.apple.system.notify.service.path:1023:";
	char name_buf[256];
	int check_token;
	int protected_tokens[sizeof(protected_paths) / sizeof(protected_paths[0])];
	uint32_t rc;
	size_t i;
	pid_t notifyd_pid_before, notifyd_pid_after;

	// Get notifyd PID before test
	notifyd_pid_before = get_notifyd_pid();
	T_ASSERT_GT(notifyd_pid_before, 0, "notifyd (%d) should be running", notifyd_pid_before);

	// Register for pathwatch on protected paths (may fail due to permissions)
	for (i = 0; i < sizeof(protected_paths) / sizeof(protected_paths[0]); i++) {
		protected_tokens[i] = NOTIFY_TOKEN_INVALID;
		snprintf(name_buf, sizeof(name_buf), "%s%s", pathwatch_prefix, protected_paths[i]);
		(void)notify_register_check(name_buf, &protected_tokens[i]);
	}

	sleep(1);

	// Verify notifyd did not crash
	notifyd_pid_after = get_notifyd_pid();
	T_ASSERT_EQ(notifyd_pid_before, notifyd_pid_after,
	    "notifyd should not have crashed after protected path registrations");

	// Cancel protected path registrations
	for (i = 0; i < sizeof(protected_paths) / sizeof(protected_paths[0]); i++) {
		if (protected_tokens[i] != NOTIFY_TOKEN_INVALID) {
			notify_cancel(protected_tokens[i]);
		}
	}

	// Register for pathwatch on good paths
	for (i = 0; i < sizeof(good_paths) / sizeof(good_paths[0]); i++) {
		snprintf(name_buf, sizeof(name_buf), "%s%s", pathwatch_prefix, good_paths[i]);
		rc = notify_register_check(name_buf, &check_token);
		T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_register_check for %s should succeed", good_paths[i]);
		if (check_token != NOTIFY_TOKEN_INVALID) {
			notify_cancel(check_token);
		}
	}

	// Verify notifyd did not crash
	notifyd_pid_after = get_notifyd_pid();
	T_ASSERT_EQ(notifyd_pid_before, notifyd_pid_after,
	    "notifyd should not have crashed after good path registrations");
}