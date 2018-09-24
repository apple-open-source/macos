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
#include <string.h>
#include <sys/sysctl.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <zlib.h>
#include <os/log.h>

#define APPLEOSXWATCHDOG_FILENAMETOKEN "progress watchdog"
#define SLEEP_WAKE_FILENAMETOKEN "System Sleep Wake"

#define PANIC_FILE "/var/db/PanicReporter/current.panic"

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

void process_logfiles(char *gz_stacksfile, char *logfile)
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
    bool isAppleOSXWatchdog = false;
    bool stacksExist = true;
    char stacksfile[128];


    if ((lstat(logfile, &logs) != 0) || !S_ISREG(logs.st_mode) || (logs.st_size == 0))
        return;

    if ((lstat(gz_stacksfile, &stacks) != 0) || !S_ISREG(stacks.st_mode) || (stacks.st_size == 0)) {
        stacksExist = false;
    }
    else {
        snprintf(stacksfile, sizeof(stacksfile), "/tmp/sw_stackshot.%ld", time(NULL));
        if (uncompress_stackshot(gz_stacksfile, stacksfile) == 0) {
            stacksExist = false;
        }
    }
    
    /*
     * We need to differentiate Sleep/Wake failure from AppleOSXWatchdog failure. Indeed,
     * currently spindump has a different interface for both watchdogs.
     */
    isAppleOSXWatchdog = (stacksfile == kOSWatchdogStacksFilename || logfile == kOSWatchdogFailureStringFile);
    
    if (isAppleOSXWatchdog) {
        snprintf(cmd, sizeof(cmd),
                 "/usr/sbin/spindump %s %s -progress_watchdog_data_file %s",
                 (stacksExist) ? "-progress_watchdog_stackshot_file" : "",
                 (stacksExist) ? stacksfile : "",
                 logfile);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "/usr/sbin/spindump %s %s -sleepwakefailure_data_file %s",
                 (stacksExist) ? "-sleepwakefailure_stackshot_file" : "",
                 (stacksExist) ? stacksfile : "",
                 logfile);
    }
    
    // Invoke spindump.
    system(cmd);


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
        char *filename      = isAppleOSXWatchdog ? APPLEOSXWATCHDOG_FILENAMETOKEN : SLEEP_WAKE_FILENAMETOKEN;
        size_t filename_len = isAppleOSXWatchdog ? sizeof(APPLEOSXWATCHDOG_FILENAMETOKEN) : sizeof(SLEEP_WAKE_FILENAMETOKEN);
        
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

int main(int argc, char **argv)
{

    process_logfiles(kSleepWakeStacksFilename, kSleepWakeFailureStringFile);
    
    /* AppleOSXWatchdog */
    process_logfiles(kOSWatchdogStacksFilename, kOSWatchdogFailureStringFile);

    return 0;
}
