/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
#include "utility.h"
#include <sys/resource.h>

/*******************************************************************************
* createCFString()
*******************************************************************************/
CFStringRef createCFString(char * string)
{
    return CFStringCreateWithCString(kCFAllocatorDefault, string,
        kCFStringEncodingUTF8);
}


/*******************************************************************************
* check_dir()
*
* This function makes sure that a given directory exists, and is writeable.
*******************************************************************************/
Boolean check_dir(const char * dirname, int writeable, int print_err)
{
    int result = true;  // assume success
    struct stat stat_buf;

    if (stat(dirname, &stat_buf) != 0) {
        if (print_err) {
            perror(dirname);
        }
        result = false;
        goto finish;
    }

    if ( !(stat_buf.st_mode & S_IFDIR) ) {
// XXX This could be called on a kext, message should say such
        if (print_err) {
            qerror("%s is not a directory\n", dirname);
        }
        result = false;
        goto finish;
    }

    if (writeable) {
        if (access(dirname, W_OK) != 0) {
            if (print_err) {
                qerror("%s is not writeable\n", dirname);
            }
            result = false;
            goto finish;
        }
    }
finish:
    return result;
}

/*******************************************************************************
* qerror()
*
* Quick wrapper over printing that checks verbose level. Does not append a
* newline like error_log() does.
*******************************************************************************/
void qerror(const char * format, ...)
{
    va_list ap;

    if (g_verbose_level <= kKXKextManagerLogLevelSilent) return;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fflush(stderr);

    return;
}

/*******************************************************************************
* basic_log()
*
* Print a log message prefixed with the name of the program.
*******************************************************************************/
void basic_log(const char * format, ...)
{
    va_list ap;

    fprintf(stdout, "%s: ", progname);

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
    fprintf(stdout, "\n");

    fflush(stdout);

    return;
}

/*******************************************************************************
* verbose_log()
*
* Print a log message prefixed with the name of the program.
*******************************************************************************/
void verbose_log(const char * format, ...)
{
    va_list ap;

    if (g_verbose_level < kKXKextManagerLogLevelDefault) return;

    fprintf(stdout, "%s: ", progname);

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
    fprintf(stdout, "\n");

    fflush(stdout);

    return;
}

/*******************************************************************************
* error_log()
*
* Print an error message prefixed with the name of the program.
*******************************************************************************/
void error_log(const char * format, ...)
{
    va_list ap;

    if (g_verbose_level <= kKXKextManagerLogLevelSilent) return;

    fprintf(stderr, "%s: ", progname);

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    fflush(stderr);

    return;
}

/*******************************************************************************
* addKextsToManager()
*
* Add the kexts named in the kextNames array to the given kext manager, and
* put their names into the kextNamesToUse.
*
* Return values:
*   1: all kexts added successfully
*   0: one or more could not be added
*  -1: program-fatal error; exit as soon as possible
*******************************************************************************/
int addKextsToManager(
    KXKextManagerRef aManager,
    CFArrayRef kextNames,
    CFMutableArrayRef kextNamesToUse,
    Boolean do_tests)
{
    int result = 1;     // assume success
    KXKextManagerError kxresult = kKXKextManagerErrorNone;
    CFIndex i, count;
    OSKextRef theKext = NULL;  // don't release
    CFURLRef kextURL = NULL;   // must release

   /*****
    * Add each kext named to the manager.
    */
    count = CFArrayGetCount(kextNames);
    for (i = 0; i < count; i++) {
        char name_buffer[PATH_MAX];

        CFStringRef kextName = (CFStringRef)CFArrayGetValueAtIndex(
            kextNames, i);

        if (kextURL) {
            CFRelease(kextURL);
            kextURL = NULL;
        }

        kextURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
            kextName, kCFURLPOSIXPathStyle, true);
        if (!kextURL) {
            qerror("memory allocation failure\n");
            result = -1;
            goto finish;
        }

        if (!CFStringGetCString(kextName,
            name_buffer, sizeof(name_buffer) - 1, kCFStringEncodingUTF8)) {

            qerror("memory allocation or string conversion error\n");
            result = -1;
            goto finish;
        }

        kxresult = KXKextManagerAddKextWithURL(aManager, kextURL, true, &theKext);
        if (kxresult != kKXKextManagerErrorNone) {
            result = 0;
            qerror("can't add kernel extension %s (%s)",
                name_buffer, KXKextManagerErrorStaticCStringForError(kxresult));
            qerror(" (run %s on this kext with -t for diagnostic output)\n",
                progname);
#if 0
            if (do_tests && theKext && g_verbose_level >= kKXKextManagerLogLevelErrorsOnly) {
                qerror("kernel extension problems:\n");
                KXKextPrintDiagnostics(theKext, stderr);
            }
            continue;
#endif 0
        }
        if (kextNamesToUse && theKext &&
            (kxresult == kKXKextManagerErrorNone || do_tests)) {

            CFArrayAppendValue(kextNamesToUse, kextName);
        }
    }

finish:
    if (kextURL) CFRelease(kextURL);
    return result;
}

/*******************************************************************************
* Fork a process after a specified delay, and either wait on it to exit or
* leave it to run in the background.
*
* Returns -2 on fork() failure, -1 on other failure, and depending on wait:
* wait:true - exit status of forked program
* wait: false - pid of background process
*******************************************************************************/
int fork_program(const char * argv0, char * const argv[], int delay, Boolean wait)
{
    int result = -2;
    int status;
    pid_t pid;

    switch (pid = fork()) {
        case -1:  // error
            goto finish;
            break;

        case 0:  // child
            if (!wait) {
                // child forks for async/no zombies
                result = daemon(0, 0);
                if (result == -1) {
                    goto finish;
                }
                // XX does this policy survive the exec below?
                setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, IOPOL_THROTTLE);
            }

            if (delay) {
                sleep(delay);
            }

            execv(argv0, argv);
            // if execv returns, we have an error but no clear way to log it
            exit(1);

        default:  // parent
            waitpid(pid, &status, 0);
            status = WEXITSTATUS(status);
            if (wait) {
                result = status;
            } else if (status != 0) {
                result = -1;
            } else {
                result = pid;
            }
            break;
    }

finish:
    return result;
}

