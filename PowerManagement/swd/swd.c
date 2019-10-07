/*
 * Copyright (c) 2013 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdio.h>
#include <asl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/dir.h>
#include <System/sys/stackshot.h>
#include <string.h>
#include <sys/sysctl.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/IOKitLib.h>
#include <zlib.h>
#include <os/log.h>
#include <CoreFoundation/CoreFoundation.h>

#define SLEEP_WAKE_FILENAMETOKEN "Sleep Wake Failure"

#define PANIC_FILE "/var/db/PanicReporter/current.panic"

#define STACKSHOT_TEMP_FILE "/tmp/sw_stackshot"

#define STACKSHOT_KCDATA_FORMAT  0x10000

ssize_t uncompress_stackshot(char *gz_fname, char *fname_out)
{
    char buf[512];
    int fd = -1;
    int bytes_read = -1;
    ssize_t bytes_saved, total_bytes;

    total_bytes = 0;
    gzFile gz = gzopen(gz_fname, "r");
    if (gz == NULL) {
        os_log_error(OS_LOG_DEFAULT, "Failed to open compressed file %s\n", gz_fname);
        goto exit;
    }

    fd = open(fname_out, O_CREAT|O_RDWR, 0600);
    if (fd == -1) {
        os_log_error(OS_LOG_DEFAULT, "Failed to open temp file to save uncompressed stackshot. errno:%d\n", errno);
        goto exit;
    }
    while((bytes_read = gzread(gz, buf, sizeof(buf))) > 0) {
        bytes_saved = write(fd, buf, bytes_read);
        if (bytes_saved != bytes_read) {
            os_log_error(OS_LOG_DEFAULT, "Failed to save uncompressed data to temp file. error:%d\n", errno);
            break;
        }
        total_bytes += bytes_saved;
        os_log_debug(OS_LOG_DEFAULT, "Saved %ld uncompressed bytes to temp file %s\n", bytes_saved, fname_out);
    }
    if (bytes_read == -1) {
        int errnum = 0;
        const char *errstr = gzerror(gz, &errnum);
        os_log_error(OS_LOG_DEFAULT, "Failed to uncompress stackshot data due to error:%s(%d)",
                errstr, errnum);
    }

exit:
    if (gz) {
        gzclose(gz);
    }
    if (fd != -1) {
        close(fd);
    }
    return total_bytes;
}

void process_logfiles(char *gz_stacksfile, char *logfile, int compressed)
{
    struct stat stacks;
    struct stat logs;
    char cmd[200];
    DIR *dirp;
    struct dirent *dp;
    char fname[128], swfile[128];
    long crtime = 0;
    struct stat fstats, pfstat;
    struct timeval sleeptime, boottime;
    size_t size;
    bool stacksExist = true;
    char stacksfile[128];


    if ((lstat(logfile, &logs) != 0) || !S_ISREG(logs.st_mode) || (logs.st_size == 0))
        return;

    if ((lstat(gz_stacksfile, &stacks) != 0) || !S_ISREG(stacks.st_mode) || (stacks.st_size == 0)) {
        stacksExist = false;
    }
    else {
        if (compressed) {
            snprintf(stacksfile, sizeof(stacksfile), "%s.%ld", STACKSHOT_TEMP_FILE, time(NULL));
            if (uncompress_stackshot(gz_stacksfile, stacksfile) == 0) {
                stacksExist = false;
            }
        } else {
            snprintf(stacksfile, sizeof(stacksfile), "%s", gz_stacksfile);
            unlink(kSleepWakeStacksFilename);
        }
    }
    
    /*
     * We need to differentiate Sleep/Wake failure from AppleOSXWatchdog failure. Indeed,
     * currently spindump has a different interface for both watchdogs.
     */
    
    snprintf(cmd, sizeof(cmd),
             "/usr/sbin/spindump %s %s -sleepwakefailure_data_file %s",
             (stacksExist) ? "-sleepwakefailure_stackshot_file" : "",
             (stacksExist) ? stacksfile : "",
             logfile);
    
    // Invoke spindump.
    int err = system(cmd);
    if (err) {
        os_log_error(OS_LOG_DEFAULT, "spindump cmd %s returned non zero exit code %d\n", cmd, err);
    }


    unlink(gz_stacksfile);
    unlink(stacksfile);
    unlink(logfile);


    // Due diligence check to make sure this file is not generated due to system sleep
    size = sizeof(sleeptime);
    if ((sysctlbyname("kern.sleeptime", &sleeptime, &size, NULL, 0) != 0) || (sleeptime.tv_sec != 0))
        return;

    // For RD_LOG files, trigger a pop-up dialog to report that
    // system has rebooted due to Sleep Wake failure.
    //
    // Look for most recently created Sleep Wake Failure file
    dirp = opendir("/Library/Logs/DiagnosticReports");
    if (dirp == NULL) {
        return ;
    }

    while ((dp = readdir(dirp)) != NULL) {
        char *filename      = SLEEP_WAKE_FILENAMETOKEN;
        size_t filename_len = sizeof(SLEEP_WAKE_FILENAMETOKEN);
        
        if ((strnstr(dp->d_name, filename, filename_len) == dp->d_name)
                    && (dp->d_type == DT_REG)) {
            snprintf(fname, sizeof(fname), "/Library/Logs/DiagnosticReports/%s", dp->d_name);
            stat(fname, &fstats);

            if (S_ISREG(fstats.st_mode) && (crtime < fstats.st_birthtime)) {
                crtime = fstats.st_birthtime;
                strncpy(swfile, fname, sizeof(swfile));
            }
        }
    }

    // Check for existence of SleepWakeFailure log file
    if (crtime == 0)
        return;     

    // Due diligence check to make sure this file is generated after system boot
    size = sizeof(boottime);
    if ((sysctlbyname("kern.boottime", &boottime, &size, NULL, 0) != 0) || (boottime.tv_sec > crtime))
        return;

    // If panic file link already exists, skip linking this Sleep Wake failure file
    if (lstat(PANIC_FILE, &pfstat) == 0)
        return;

    symlink(swfile, PANIC_FILE);

}

ssize_t take_stackshot(char *stackshot_filename) {
    void *buffer = NULL;
    uint32_t buffer_size = 0;
    ssize_t bytes_saved = 0;
    pid_t pid = -1;
    int fd = -1;
    int ret = 0;

    stackshot_config_t *config = stackshot_config_create();
    if (!config) {
        os_log_error(OS_LOG_DEFAULT, "Unable to create stackshot config\n");
        return bytes_saved;
    }

    // stackshot format flag needs to be set
    stackshot_config_set_flags(config, STACKSHOT_KCDATA_FORMAT);
    pid = getpid();
    ret = stackshot_config_set_pid(config, pid);
    if (ret) {
        os_log_error(OS_LOG_DEFAULT, "Unable to set config with pid %d. Return %d\n", pid, ret);
    }
    ret = stackshot_capture_with_config(config);
    if (ret) {
        os_log_error(OS_LOG_DEFAULT, "stackshot capture returned error %d for pid %d\n", ret, pid);
        return bytes_saved;
    }

    buffer = stackshot_config_get_stackshot_buffer(config);
    if (!buffer) {
        os_log_error(OS_LOG_DEFAULT, "stackshot buffer returned NULL\n");
        return bytes_saved;
    }
    buffer_size = stackshot_config_get_stackshot_size(config);
    if (!buffer_size) {
        os_log_error(OS_LOG_DEFAULT, "stackshot buffer size returned %d\n", buffer_size);
        return bytes_saved;
    }
    fd = open(stackshot_filename, O_CREAT|O_RDWR, 0600);
    if (fd == -1) {
        os_log_error(OS_LOG_DEFAULT, "Failed to open temp file to save stackshot. errno: %d\n", errno);
        return bytes_saved;
    }

    bytes_saved = write(fd, buffer, buffer_size);
    os_log_info(OS_LOG_DEFAULT, "Saved %ld bytes to stackshot file %s\n", bytes_saved, stackshot_filename);
    return bytes_saved;

}

int main(int argc, char **argv)
{
    /*
     * If sleep wake failure in EFI (0x1f)
     * take a stackshot of swd to create a
     * "Sleep Wake Failure" report
     */
    uint64_t status_code = 0;
    uint32_t phase_data = 0;
    uint32_t phase_detail = 0;
    io_registry_entry_t root_domain = MACH_PORT_NULL;

    root_domain = IORegistryEntryFromPath(kIOMasterPortDefault, kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
    CFNumberRef status_code_cf = (CFNumberRef)IORegistryEntryCreateCFProperty(root_domain, CFSTR(kIOPMSleepWakeFailureCodeKey), kCFAllocatorDefault, 0);
    if (status_code_cf) {

        // get sleep wake failure code
        CFNumberGetValue(status_code_cf, kCFNumberLongLongType, &status_code);

        // failure phase and detail
        phase_detail = (status_code >> 32) & 0xFFFFFFFF;
        phase_data = (status_code & 0xFFFFFFFF) & 0xFF;
        if (phase_data == 0x1f) {

            // failure in EFI
            os_log_error(OS_LOG_DEFAULT, "EFI failure\n");

            // take a stackshot
            char temp_filename[128];
            ssize_t bytes_saved = 0;
            snprintf(temp_filename, sizeof(temp_filename), "%s.%ld", STACKSHOT_TEMP_FILE, time(NULL));
            bytes_saved = take_stackshot(temp_filename);
            if (bytes_saved) {
                process_logfiles(temp_filename, kSleepWakeFailureStringFile, 0);
            }
        } else {
            process_logfiles(kSleepWakeStacksFilename, kSleepWakeFailureStringFile, 1);
        }
        CFRelease(status_code_cf);
    }
    return 0;
}
