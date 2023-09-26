#include <copyfile.h>
#include <dirent.h>
#include <errno.h>
#include <os/log.h>
#include <removefile.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_ssize_t.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/syslimits.h>
#include <sys/types.h>
#include <tzfile.h>
#include <unistd.h>

#include "tzlink_internal.h"

char *VOLUME_BASE_DIRECTORY_FORMAT_STRING = "/System/Volumes/Preboot/%s";
#define LOCALTIME_PATH "/etc/localtime"
#define TZ_PATH "/var/db/timezone/tz"
#define TIMEZONE_PATH "/var/db/timezone"
#define USR_TIMEZONE_PATH "/usr/share/zoneinfo.default"
#define TZ_LATEST_PATH "/var/db/timezone/tz_latest"
#define ICUTZ_PATH "/var/db/timezone/icutz"

/*
 * Append tail to head with '/' in between
 * memory must be free'd by the caller, will return NULL on failure
 *
 * @param head
 * The first part of the path for which tail will be appended to.
 *
 * @param tail
 * The second part of the path which will be appended to head.
 *
 * @result
 * The appended string on success NULL on failure
 */
char *file_path_append(const char *head, const char *tail) {
    if (!head | !tail) {
        return NULL;
    }

    char *result = NULL;
    if (-1 == asprintf(&result, "%s/%s", head, tail)) {
        return NULL;
    }

    return result;
}

/*
 * Create intermediate directories specified in path starting from base
 *
 * @param base
 * The directory where path will be created in.
 *
 * @param path
 * The new path to create at base.
 *
 * @result
 * Returns -1 on error, 0 on success, errno might have more specific information
 * about the failure.
 */
static int make_intermediate_dirs(const char *base, const char *path) {
    char *local_path = NULL;
    char *index = NULL;
    int error = 0;

    if (!base || !path) {
        return -1;
    }

    local_path = file_path_append(base, path);
    if (!local_path) {
        return -1;
    }

    size_t base_len = strnlen(base, PATH_MAX);
    if (base_len == PATH_MAX) {
        return -1;
    }

    index = local_path + base_len;
    while (*index) {
        if (*index == '/') {
            *index = '\0';
            int a_ret = access(local_path, F_OK);
            if (a_ret) {
                if (-1 == mkdir(local_path, 0755)) {
                    error = -1;
                    goto done;
                }
            }
            *index = '/';
        }
        index++;
    }

done:
    free(local_path);

    return error;
}

/*
 * Copy files from source to dest.
 *
 * @param source
 * The sources files to copy
 *
 * @param dest
 * The destination to copy files to
 *
 * @param recursive
 * When true does a recursive copy of the contents from source to dest.
 *
 * @result
 * 0 on success less than zero on failure.
 */
static int copy_files(const char *source, const char *dest, bool recursive) {
    int error = 0;
    copyfile_flags_t flags = COPYFILE_ALL | COPYFILE_NOFOLLOW;

    if (!source | !dest) {
        return -1;
    }

    if (recursive) {
        flags |= COPYFILE_RECURSIVE;
    }

    error = copyfile(source, dest, NULL, flags);

    return error;
}

/*
 * Recursively remove files at path
 *
 * @param path
 * The path of files to be removed
 *
 * @result
 * 0 on success less than 0 on error.
 */
int remove_files(const char *path) {
    int error = 0;

    if (!path) {
        return -1;
    }

    error = removefile(path, NULL, REMOVEFILE_RECURSIVE);

    return error;
}

/*
 * Updates the timezone paths in base from source
 *
 * @param source_link
 *
 * @param preboot_volume_base
 *
 * @param recursive
 *
 * For example:
 *     update_timezone_path("/var/db/timezone/icutz",
 * preboot_volume_base_directory, true); Recursive here ensures that
 * icutz44l.dat is copied from: /var/db/timezone/icutz ->
 * /var/db/timezone/tz/2022g.1.0/icutz/
 *
 *  returns -1 on failure 0 on success
 */
static int update_timezone_path(const char *source_link,
                                const char *preboot_volume_base,
                                bool recursive) {
    char *preboot_volume_link = NULL;
    char *preboot_volume_link_target = NULL;
    char source_link_target[PATH_MAX] = "";
    int error = 0;
    ssize_t ret = 0;

    if (!source_link | !preboot_volume_base) {
        return -1;
    }

    ret = readlink(source_link, source_link_target, PATH_MAX);
    if (-1 == ret) {
        error = -1;
        goto done;
    }

    if (PATH_MAX == ret) {
        error = -1;
        goto done;
    }
    source_link_target[ret] = '\0';

    preboot_volume_link = file_path_append(preboot_volume_base, source_link);
    if (!preboot_volume_link) {
        error = -1;
        goto done;
    }

    // If we can access the preboot volume link read it
    if (!access(preboot_volume_link, F_OK)) {
        preboot_volume_link_target = calloc(sizeof(char), PATH_MAX);
        ret = readlink(preboot_volume_link, preboot_volume_link_target, PATH_MAX);
        if (-1 == ret || ret >= PATH_MAX) {
            error = -1;
            goto done;
        }
        preboot_volume_link_target[ret] = '\0';

        // If the preboot volume link exists and points to the same place as the
        // source_link we don't need to do anything
        if (!strncmp(source_link_target, preboot_volume_link_target,
                     PATH_MAX)) {
            goto done;
        }
    } else {
        // If the link doesn't exist we'll create the path it should point to
        preboot_volume_link_target = file_path_append(preboot_volume_base, source_link_target);
        if (!preboot_volume_link) {
            error = -1;
            goto done;
        }
    }

    // If there's content at the preboot volume link location remove it
    if (!access(preboot_volume_link_target, F_OK)) {
        remove_files(preboot_volume_link_target);
    }

    // Create missing intermediate directories in preboot volume for the new
    // link target
    if (make_intermediate_dirs(preboot_volume_base, source_link_target)) {
        os_log_error(OS_LOG_DEFAULT, "Failed to make intermediate directories for path: %s/%s", preboot_volume_base, source_link_target);
        error = -1;
        goto done;
    }

    // Copy the new files
    error = copy_files(source_link_target, preboot_volume_link_target, recursive);
    if (error < 0) {
        os_log_error(OS_LOG_DEFAULT, "Could not copy file: %s to: %s\n", source_link_target, preboot_volume_link_target);
        goto done;
    }

    // Create intermediate directories for symlink if they don't exist
    if (access(preboot_volume_link, F_OK)) {
        make_intermediate_dirs(preboot_volume_base, source_link);

        error = copy_files(source_link, preboot_volume_link, recursive);
        if (error < 0) {
            os_log_error(OS_LOG_DEFAULT, "Could not copy file: %s to : %s\n",
                         source_link, preboot_volume_link);
            goto done;
        }
    }

done:
    free(preboot_volume_link_target);
    free(preboot_volume_link);

    return error;
}

/*
 * Generate the absolute path that the preboot volume's localtime symlink should point to
 *
 * @param base
 * The new beginning of the path
 *
 * @param path_out
 * The path we'll return
 *
 * @param path_out_size
 * The size of the buffer provided in path_out
 *
 * @result
 *
 * returns -1 on failure 0 on success
 */
static int get_corrected_base_path(const char *base, char *path_out, const ssize_t path_out_size) {
    size_t r = 0;

    if (!base || !path_out) {
        return -1;
    }

    r = readlink(TZDIR, path_out, path_out_size);
    if (-1 == r || path_out_size == r) {
        os_log_error(OS_LOG_DEFAULT, "Failed to readlink for TZDIR: %s, error: %s", TZDIR, strerror(errno));
        return -1;
    }
    path_out[r] = '\0';

    if (!path_out) {
        return -1;
    }

    return 0;
}

/*
 * This function gets the version of timezone files used at tz_path and returns
 * it from tz_version The caller is responsible for freeing tz_version
 *
 * @param tz_path
 * Path to check for the version
 *
 * @param tz_version_out
 * Out variable to hold the tz_version
 *
 * @result
 * -1 on error 0 on success. If successful and *tz_version_out is NULL the
 * directory expected to contain the version did not exist
 */
int get_tz_version(const char *tz_path, char **tz_version_out) {
    DIR *tz_dir = NULL;
    char *entry_name = NULL;
    int ret = 0;
    size_t count = 0;
    struct dirent *entry;

    if (!tz_path | !tz_version_out) {
        return -1;
    }

    tz_dir = opendir(tz_path);
    if (!tz_dir) {
        ret = 0;
        os_log_debug(OS_LOG_DEFAULT, "Could not open path for timezone directory: %s\n", tz_path);
        *tz_version_out = NULL;
        goto done;
    }

    while ((entry = readdir(tz_dir)) != NULL) {
        const char *tmp_entry_name = entry->d_name;
        if (!strncmp(".", tmp_entry_name, 2) || !strncmp("..", tmp_entry_name, 3)) {
            continue;
        }
        count++;
        entry_name = strndup(tmp_entry_name, PATH_MAX);
        if (!entry_name) {
            ret = -1;
            goto done;
        } else {
            break;
        }
    }

    if (tz_version_out && entry_name) {
        *tz_version_out = entry_name;
    }

done:
    if (tz_dir) {
        if (-1 == closedir(tz_dir)) {
            os_log_debug(OS_LOG_DEFAULT, "Error closing tz directory: %s, error: %s", tz_path, strerror(errno));
        }
    }

    return ret;
}


/*
 * The localtime link needs special handling in comparison to the other links.
 * this function takes care of making sure that the local time destination files
 * are put in the correct place in the preboot volume
 *
 * @param base
 * The base path we'll be updating localtime at
 */
static int update_localtime_path(const char *base,
                                 const char *localtime_target_path) {
    char *preboot_localtime = NULL;
    char *resolved_localtime_path = NULL;
    char *preboot_volume_resolved_localtime_path = NULL;
    char *localtime_dest_link_path = NULL;
    char *preboot_link_target_base = NULL;
    char corrected_base_path[PATH_MAX] = "";
    int ret = 0;
    size_t tzdir_len = strnlen(TZDIR, PATH_MAX);

    if (!base || !localtime_target_path) {
        ret = -1;
        goto done;
    }

    if (strncmp(localtime_target_path, TZDIR, tzdir_len)) {
        ret = -1;
        os_log_error(OS_LOG_DEFAULT, "localtime path is not prefixed with zoneinfo");
        goto done;
    }

    if (-1 == get_corrected_base_path(base, corrected_base_path, PATH_MAX)) {
        os_log_error(OS_LOG_DEFAULT, "Could not get path for localtime link");
        ret = -1;
        goto done;
    }

    const char *localtime_post_zoneinfo = localtime_target_path + tzdir_len;

    preboot_link_target_base = file_path_append(base, corrected_base_path);
    if (!preboot_link_target_base) {
        ret = -1;
        goto done;
    }

    // Remove old timezones if they exist
    remove_files(preboot_link_target_base);

    if (-1 == make_intermediate_dirs(preboot_link_target_base, localtime_post_zoneinfo)) {
        ret = -1;
        goto done;
    }

    resolved_localtime_path = file_path_append(corrected_base_path, localtime_post_zoneinfo);
    if (!resolved_localtime_path) {
        ret = -1;
        goto done;
    }

    preboot_volume_resolved_localtime_path = file_path_append(base, resolved_localtime_path);
    if (!preboot_volume_resolved_localtime_path) {
        ret = -1;
        goto done;
    }

    int error = copy_files(localtime_target_path, preboot_volume_resolved_localtime_path, true);
    if (error) {
        os_log_error(OS_LOG_DEFAULT, "Could not copy file: %s to : %s\n", localtime_target_path, localtime_post_zoneinfo);
        ret = -1;
        goto done;
    }

    localtime_dest_link_path = file_path_append(base, LOCALTIME_PATH);
    if (!localtime_dest_link_path) {
        ret = -1;
        goto done;
    }

    if (-1 == make_intermediate_dirs(base, LOCALTIME_PATH)) {
        ret = -1;
        goto done;
    }

    if (!access(localtime_dest_link_path, F_OK)) {
        unlink(localtime_dest_link_path);
    }
    error = copy_files(LOCALTIME_PATH, localtime_dest_link_path, true);
    if (error) {
        os_log_error(OS_LOG_DEFAULT, "Could not copy file: %s to : %s\n", LOCALTIME_PATH, preboot_localtime);
        ret = -1;
        goto done;
    }

done:
    free(preboot_link_target_base);
    free(resolved_localtime_path);
    free(preboot_volume_resolved_localtime_path);
    free(localtime_dest_link_path);

    return ret;
}

/*
 * This function checks the timezone db version on the running root and on
 * preboot and removes the timezone directory on the preboot volume if it doesn't
 * match
 *
 * @param base
 * Directory to check timezone version
 *
 * @result
 * 0 on success -1 on failure
 */
static int check_and_remove_timezone_db_version(const char *base) {
    char *preboot_timezone_path = NULL;
    char *preboot_tz_path = NULL;
    char *preboot_tz_version = NULL;
    char *root_tz_version = NULL;
    int ret = 0;

    if (!base) {
        return -1;
    }

    if (get_tz_version(TZ_PATH, &root_tz_version) || !root_tz_version) {
        ret = 0;
        os_log_info(OS_LOG_DEFAULT, "No tz version for path: %s", base);
        goto done;
    }

    preboot_tz_path = file_path_append(base, TZ_PATH);
    if (!preboot_tz_path) {
        os_log_debug(OS_LOG_DEFAULT, "Failed to allocate memeory for filepath");
        ret = -1;
        goto done;
    }

    // If the tz db path doesn't exist we'll create it when we update
    if (access(preboot_tz_path, F_OK)) {
        ret = 0;
        goto done;
    } else {
        // If we have a versioned tz_path we can remove the path from usr/share
        if (access(preboot_tz_path, F_OK)) {
            remove_files(preboot_tz_path);
        }
    }

    // If we can't find the current tz version we'll fill it in when we update
    // paths
    if (get_tz_version(preboot_tz_path, &preboot_tz_version) ||
        !preboot_tz_version) {
        ret = 0;
        goto done;
    }

    /* If the preboot volume timezone db version is not the same as the root
     * timezone db version delete the whole timezone directory from preboot and
     * let the update functions  rebuild it
     */
    if (strncmp(root_tz_version, preboot_tz_version, PATH_MAX)) {
        preboot_timezone_path = file_path_append(base, TIMEZONE_PATH);
        if (!preboot_timezone_path) {
            os_log_debug(OS_LOG_DEFAULT, "Failed to allocate memory for filepath");
            ret = -1;
            goto done;
        }

        remove_files(preboot_timezone_path);
    }

done:
    free(root_tz_version);
    free(preboot_tz_path);
    free(preboot_timezone_path);
    free(preboot_tz_version);

    return ret;
}

/*
 * This function gets the current Preboot UUID volume ID
 *
 * @param uuid_out
 * parameter to retrieve the UUID
 *
 * @returns 0 on success -1 on failure, errno retains value for failed calls
 * made
 */
int get_preboot_volume_uuid(char **uuid_out) {
    char *command[2] = {KERN_APFSPREBOOTUUID, KERN_BOOTUUID};
    char *uuid = NULL;
    int ret = 0;
    size_t len = 0;

    for (int i = 0; i < 2; i++) {
        if (-1 == sysctlbyname(command[i], NULL, &len, NULL, 0)) {
            os_log_error( OS_LOG_DEFAULT, "Could not get size of sysctl %s result. Encountered error: %s", command[i], strerror(errno));
            ret = -1;
            continue;
        }

        uuid = calloc(len, sizeof(char));
        if (!uuid) {
            ret = -1;
            continue;
        }

        errno = 0;
        if (-1 == sysctlbyname(command[i], uuid, &len, NULL, 0)) {
            os_log_error(OS_LOG_DEFAULT, "Could not get %s UUID value. Encountered error: %s", command[i], strerror(errno));
            ret = -1;
            continue;
        }

        *uuid_out = uuid;
        ret = 0;
        break;
    }

    if (-1 == ret) {
        free(uuid);
        *uuid_out = NULL;
    }

    return ret;
}

/*
 * This function updates the preboot_volume's timezone info
 *
 * @param localtime_target_path the path that localtime should be updated to.
 *
 * returns -1 on failure 0 on success
 */
int update_preboot_volume(const char *localtime_target_path) {
    char *uuid = NULL;
    char *preboot_volume_base_directory = NULL;
    int error = 0;

    if (-1 == get_preboot_volume_uuid(&uuid)) {
        error = -1;
        goto done;
    }

    if (-1 == asprintf(&preboot_volume_base_directory, VOLUME_BASE_DIRECTORY_FORMAT_STRING, uuid)) {
        os_log_debug(OS_LOG_DEFAULT, "Could not create preboot volume path");
        error = -1;
        goto done;
    }

    // Get timezonedb version if available
    if (-1 == check_and_remove_timezone_db_version(preboot_volume_base_directory)) {
        os_log_info(OS_LOG_DEFAULT, "Error while checking timezone state in preboot volume");
    }

    // Update the symlinks and copy in the directories they point to if nothing
    // has changed these functions will do nothing
    if (-1 == update_timezone_path(TZDIR, preboot_volume_base_directory, false)) {
        os_log_error(OS_LOG_DEFAULT, "Error while updating zoneinfo file");
        error = -1;
        goto done;
    }
    // BATS has a differnt setup for timezones
    if (!access(TZ_LATEST_PATH, F_OK)) {
        if (-1 == update_timezone_path(TZ_LATEST_PATH, preboot_volume_base_directory, false)) {
            os_log_error(OS_LOG_DEFAULT, "Error while updating preboot tz_latest link");
            error = -1;
            goto done;
        }
    }

    if (!access(ICUTZ_PATH, F_OK)) {
        if (-1 == update_timezone_path(ICUTZ_PATH, preboot_volume_base_directory, true)) {
            os_log_error(OS_LOG_DEFAULT, "Error while updating preboot icutz link");
            error = -1;
            goto done;
        }
    }

    // Update the localtime path, this needs special handling for the
    // intermediate zoneinfo link
    if (-1 == update_localtime_path(preboot_volume_base_directory, localtime_target_path)) {
        os_log_error(OS_LOG_DEFAULT, "Error while updating preboot localtime link");
        error = -1;
        goto done;
    }

done:
    free(uuid);
    free(preboot_volume_base_directory);

    return error;
}
