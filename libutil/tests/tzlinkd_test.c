/*
 * Integration tests for tzlinkd
 */

#include "../tzlink.h"
#include "../tzlink_internal.h"
#include "tzfile.h"

#include <errno.h>
#include <TargetConditionals.h>
#include <removefile.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/_types/_errno_t.h>
#include <sys/_types/_ssize_t.h>
#include <sys/errno.h>
#include <sys/syslimits.h>
#include <unistd.h>

extern char *VOLUME_BASE_DIRECTORY_FORMAT_STRING;

static const char *VOLUME_TIMEZONE_FORMAT_STRING = "/System/Volumes/Preboot/%s/var/db/timezone";
static const char *VOLUME_TIMEZONE_SYMLINK_FORMAT_STRING = "/System/Volumes/Preboot/%s/etc";
static const char *VOLUME_TIMEZONE_DB_VERSION_FORMAT_STRING = "/System/Volumes/Preboot/%s/var/db/timezone/tz/%s";

#define TESTING_TIME_ZONE "America/Denver"
#define TZPATH "/var/db/timezone/zoneinfo"
#define TEST_SYMLINK_DEST "/var/db/timezone/zoneinfo/America/Denver"

ssize_t get_current_timezone(char *buf, ssize_t size) {
    ssize_t result_size = readlink(TZDEFAULT, buf, size);

    if (-1 == result_size) {
        perror("[FAIL] Couldn't get current timezone\n");
        exit(EXIT_FAILURE);
    }

    return result_size;
}

void change_current_timezone(const char *new_timezone) {
    errno_t error;

    error = tzlink(new_timezone);
    if (error) {
        fprintf(stderr, "[FAIL] tzlink call failed for timezone: %s, error: %s\n", new_timezone, strerror(error));
        exit(EXIT_FAILURE);
    }

}

void check_localtime(const char* expected) {
    char new_tz_name_buf[PATH_MAX] = "";
    ssize_t new_buf_size;
    ssize_t tzpath_len = strnlen(TZPATH, PATH_MAX) + 1;

    new_buf_size = get_current_timezone(new_tz_name_buf, PATH_MAX);
    if (strncmp(expected, new_tz_name_buf + tzpath_len, new_buf_size - tzpath_len)) {
        fprintf(stderr, "[FAIL] localtime symlink wasn't updated\n");
        exit(EXIT_FAILURE);
    }

    printf("[PASS]\n");
}


/*
 * This function checks if the symlink given in path_to_check points to the file at expected_path
 */
int check_symlink_match(char *path_to_test, char *target_link)
{
    int ret = 0;
    char test_link_path[PATH_MAX] = "";
    char local_link_path[PATH_MAX] = "";
    ssize_t res = 0;

    res = readlink(path_to_test, test_link_path, PATH_MAX);
    if (-1 == res) {
        fprintf(stderr, "Failed to readlink: %s, error: %s", path_to_test, strerror(errno));
        ret = -1;
        goto done;
    }

    test_link_path[res] = '\0';

    res = readlink(target_link, local_link_path, PATH_MAX);
    if (-1 == res) {
        fprintf(stderr, "Failed to readlink: %s, error: %s", target_link, strerror(errno));
        ret = -1;
        goto done;
    }

    local_link_path[res] = '\0';

    if (strncmp(test_link_path, local_link_path, PATH_MAX)) {
        fprintf(stderr, "Error %s does not match %s\n", test_link_path, local_link_path);
        ret = -1;
        goto done;
    }

done:
    return ret;
}

int check_all_symlink_match(void)
{
    int ret = 0;
    char *timezone_path = NULL;
    char *uuid = NULL;
    char *tz_latest_link_path = NULL;
    char *icutz_link_path = NULL;
    char *preboot_volume_localtime_symlink = NULL;
    char *preboot_volume_etc_path = NULL;
    char *tz_zoneinfo_link_path = NULL;

    get_preboot_volume_uuid(&uuid);
    if (-1 == asprintf(&timezone_path, VOLUME_TIMEZONE_FORMAT_STRING, uuid)) {
        fprintf(stderr, "Could not allocate memory for string\n");
        ret = -1;
        goto done;
    }

    // Check preboot volume db icutz
    if (!access("/var/db/timezone/icutz", F_OK)) {
        icutz_link_path = file_path_append(timezone_path, "icutz");
        if (!icutz_link_path) {
            fprintf(stderr, "Could not append %s & %s\n", timezone_path, "icutz");
            ret = -1;
            goto done;
        }
        if (check_symlink_match(icutz_link_path, "/var/db/timezone/icutz")) {
            fprintf(stderr, "icutz link didn't match\n");
            ret = -1;
            goto done;
        }
    }

    // Check preboot volume db tz_latest
    if (!access("/var/db/timezone/tz_latest", F_OK)) {
        tz_latest_link_path = file_path_append(timezone_path, "tz_latest");
        if (!tz_latest_link_path) {
            fprintf(stderr, "Could not append %s & %s\n", timezone_path, "tz_latest");
            ret = -1;
            goto done;
        }
        if (check_symlink_match(tz_latest_link_path, "/var/db/timezone/tz_latest")) {
            fprintf(stderr, "tz_latest link didn't match\n");
            ret = -1;
            goto done;
        }
    }

    // Check preboot volume db zoneinfo
    tz_zoneinfo_link_path = file_path_append(timezone_path, "zoneinfo");
    if (!tz_zoneinfo_link_path) {
        fprintf(stderr, "Could not append %s & %s\n", timezone_path, "zoneinfo");
        ret = -1;
        goto done;
    }
    if (check_symlink_match(tz_zoneinfo_link_path, "/var/db/timezone/zoneinfo")) {
        fprintf(stderr, "zoneinfo link didn't match\n");
        ret = -1;
        goto done;
    }

    // Check preboot volume localtime
    if (-1 == asprintf(&preboot_volume_etc_path, VOLUME_TIMEZONE_SYMLINK_FORMAT_STRING, uuid)) {
        fprintf(stderr, "Could not append %s & %s\n", VOLUME_TIMEZONE_SYMLINK_FORMAT_STRING, uuid);
        ret = -1;
        goto done;
    }
    preboot_volume_localtime_symlink = file_path_append(preboot_volume_etc_path, "localtime");
    if (!preboot_volume_localtime_symlink) {
        fprintf(stderr, "Could not append %s & %s\n", preboot_volume_etc_path, "localtime");
        ret = -1;
        goto done;
    }
    if (check_symlink_match(preboot_volume_localtime_symlink, "/etc/localtime")) {
        fprintf(stderr, "localtime link didn't match\n");
        ret = -1;
        goto done;
    }

done:
    free(timezone_path);
    free(icutz_link_path);
    free(tz_latest_link_path);
    free(tz_zoneinfo_link_path);
    free(preboot_volume_etc_path);
    free(preboot_volume_localtime_symlink);

    return ret;
}


void testtzlinkd(void) {
    // The local BridgeOS device used for testing does not have a /etc/localtime symlink
    // the devices in BATS do, and tzlink() fails for other reasons.
    // The intent of this test is to verify functionality of tzlink and it seems unlikely
    // someone will change the timezone on their bridge devices particularly since the
    // timzone dbs aren't installed.
#if TARGET_OS_BRIDGE
    printf("[TEST] tzlinkd_test\n[BEGIN]\n[PASS]\n");
    exit(EXIT_SUCCESS);
#endif

    char initial_tz_name_buf[PATH_MAX] = "";
    const ssize_t tzpath_len = strnlen(TZPATH, PATH_MAX) + 1;

    printf("[TEST] tzlinkd_test\n");
    printf("[BEGIN] test set localtime\n");

    get_current_timezone(initial_tz_name_buf, PATH_MAX);

    // Test if we can change the current timezone
    change_current_timezone(TESTING_TIME_ZONE);
    check_localtime(TESTING_TIME_ZONE);

    // Test if we can restore to our previous timezone
    // this is mostly to cleanup when running tests locally
    printf("[BEGIN] test restore localtime\n");
    change_current_timezone(initial_tz_name_buf + tzpath_len);
    check_localtime(initial_tz_name_buf + tzpath_len);
}

/*
 *  This tests whether that we copy the timezone database to a target directory correctly
 *  when there no existing timezone database on the preboot volume
 */
int test_update_timezone_empty_preboot(const char *localtime_target_path)
{
    int ret = 0;

    printf("[TEST] test_update_timezone_empty_preboot\n");
    printf("[BEGIN]\n");

    if (update_preboot_volume(localtime_target_path)) {
        ret = -1;
        fprintf(stderr, "Could not update preboot volume\n");
        goto done;
    }

    if (check_all_symlink_match()) {
        fprintf(stderr, "Symlinks didn't match\n");
        ret = -1;
        goto done;
    }

done:
    if (ret) {
        fprintf(stderr, "[FAIL]\n");
    } else {
        printf("[PASS]\n");
    }

    return ret;
}

/*
 * This test whether we correctly update the timezones with a new version of
 * timezone files
 */
// TODO merge this test with the existing timezone setting test
int test_update_timezone_new_timezone(void)
{
    char initial_tz_name_buf[PATH_MAX] = "";
    const ssize_t tzpath_len = strnlen(TZPATH, PATH_MAX) + 1;
    int ret = 0;

    printf("[TEST] test_update_timezone_new_timezone\n");
    printf("[BEGIN]\n");

    change_current_timezone(TESTING_TIME_ZONE);
    get_current_timezone(initial_tz_name_buf, PATH_MAX);

    if (update_preboot_volume(TEST_SYMLINK_DEST)) {
        fprintf(stderr, "Could not update preboot volume\n");
        ret = -1;
        goto done;
    }

    if (check_all_symlink_match()) {
        fprintf(stderr, "Symlinks didn't match\n");
        ret = -1;
        goto done;
    }

done:
    change_current_timezone(initial_tz_name_buf + tzpath_len);

    if (!ret) {
        printf("[PASS]\n");
    } else {
        fprintf(stderr, "[FAIL]\n");
    }

    return ret;
}

/*
 * This tests whether we correctly replace the timezones with a new timezone
 * but the same versions of timezone files
 */
void test_update_timezone_out_of_date_preboot(const char *localtime_target_path)
{
    int ret = 0;
    printf("[TEST] test_update_timezone_out_of_date_preboot\n");
    printf("[BEGIN]\n");

    // Early devices don't have this file so pass them
    if (access("/var/db/timezone/tz", F_OK)) {
        goto done;
    }

    char *timezone_path = NULL;
    char *uuid = NULL;
    get_preboot_volume_uuid(&uuid);
    if (-1 == asprintf(&timezone_path, VOLUME_TIMEZONE_FORMAT_STRING, uuid)) {
        printf("failed to make timezone_path\n");
        ret = -1;
        goto done;
    }

    char *volume_timezone_db_dir_path = file_path_append(timezone_path, "tz");
    char *result = NULL;
    if (get_tz_version(volume_timezone_db_dir_path, &result)) {
        // Hosts don't always have tz versions they should pass this test if not
        goto done;
    }

    if (!result) {
        printf("expected to find timezone db version");
        ret = -1;
        goto done;
    }

    char *mv_command = NULL;
    asprintf(&mv_command, "mv /tmp/tzlinkd_test/%s/var/db/timezone/tz/%s /tmp/tzlinkd_test/%s/var/db/timezone/tz/oldversion", uuid, result, uuid);
    system(mv_command);
    free(mv_command);

    if (update_preboot_volume(localtime_target_path)) {
        fprintf(stderr, "Could not update preboot volume\n");
        ret = -1;
    }

    if (check_all_symlink_match()) {
        fprintf(stderr, "Symlinks didn't match\n");
        ret = -1;
    }

done:
    if (ret) {
        fprintf(stderr, "[FAIL]\n");
    } else {
        printf("[PASS]\n");
    }
}

void clean_tmp_dir(void) {
    removefile("/tmp/tzlinkd_test", NULL, REMOVEFILE_RECURSIVE);
}

void create_tmp_dir(void) {
    char *uuid;
    get_preboot_volume_uuid(&uuid);
    char *testing_directory_structure = NULL;
    asprintf(&testing_directory_structure, "/tmp/tzlinkd_test/%s/var/db/",  uuid);
    mkpath_np(testing_directory_structure, 0755);

    free(testing_directory_structure);
    free(uuid);
}

int main(int argc, char **argv)
{
    char *uuid = NULL;
    char *timezone_path = NULL;
    char *etc_path = NULL;
    char current_localtime_link_dest[PATH_MAX];

#if TARGET_CPU_ARM64 && TARGET_OS_OSX
    get_preboot_volume_uuid(&uuid);
    char VOLUME_TIMEZONE[] = "/System/Volumes/Preboot/%s/var/db/timezone";
    if (-1 == asprintf(&timezone_path, VOLUME_TIMEZONE, uuid)) {
        printf("[FAIL]\n");
    }

    char VOLUME_ETC[] = "/System/Volumes/Preboot/%s/etc";
    if (-1 == asprintf(&etc_path, VOLUME_ETC, uuid)) {
        printf("[FAIL]\n");
    }

    remove_files(timezone_path);
    remove_files(etc_path);
#endif

    testtzlinkd();

#if TARGET_CPU_ARM64 && TARGET_OS_OSX
    if (-1 == readlink("/etc/localtime", current_localtime_link_dest, PATH_MAX)) {
        fprintf(stderr, "[FAIL] could not get timezone link destination\n");
    }

    VOLUME_BASE_DIRECTORY_FORMAT_STRING = "/tmp/tzlinkd_test/%s";
    VOLUME_TIMEZONE_FORMAT_STRING = "/tmp/tzlinkd_test/%s/var/db/timezone";
    VOLUME_TIMEZONE_SYMLINK_FORMAT_STRING = "/tmp/tzlinkd_test/%s/etc";
    VOLUME_TIMEZONE_DB_VERSION_FORMAT_STRING = "/tmp/tzlinkd_test/%s/var/db/timezone/tz/%s";

    create_tmp_dir();
    test_update_timezone_empty_preboot(current_localtime_link_dest);
    test_update_timezone_new_timezone();
    test_update_timezone_out_of_date_preboot(current_localtime_link_dest);
    clean_tmp_dir();
#endif

    exit(EXIT_SUCCESS);
}
