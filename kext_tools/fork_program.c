/*
 *  fork_program.c
 *  kext_tools
 *
 *  Created by nik on 5/11/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

#include "fork_program.h"
#include "kext_tools_util.h"
#include <spawn.h>
#include <libc.h>

/*******************************************************************************
* Fork a process after a specified delay, and either wait on it to exit or
* leave it to run in the background.
*
* Returns -2 on fork() failure, -1 on other failure, and depending on wait:
* wait: true - exit status of forked program
* wait: false - pid of background process
*******************************************************************************/
int fork_program(const char * argv0, char * const argv[], Boolean wait)
{
    int            result          = -2;
    int            spawn_result    = 0;
    pid_t          child_pid       = -1;
    int            child_status    = 0;
    int            normal_iopolicy = getiopolicy_np(IOPOL_TYPE_DISK,
                       IOPOL_SCOPE_PROCESS);
    extern char ** environ;

    if (!wait) {
        setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, IOPOL_THROTTLE);
    }

    spawn_result = posix_spawn(&child_pid, argv0, /* file_actions */ NULL,
        /* spawnattrs */ NULL, argv, environ);

   /* If we couldn't spawn the process, return the default error (-2).
    */
    if (spawn_result < 0) {
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel,
            "posix_spawn failed for %s.", argv0);
        goto finish;
    }

    OSKextLog(/* kext */ NULL, kOSKextLogDetailLevel,
        "started child process %s[%d] (%ssynchronous).",
        argv0, child_pid, wait ? "" : "a");

    if (wait) {
        OSKextLogSpec logSpec = kOSKextLogDetailLevel;

        waitpid(child_pid, &child_status, 0);
        result = WEXITSTATUS(child_status);
        if (result) {
            logSpec = kOSKextLogErrorLevel;
        }
        OSKextLog(/* kext */ NULL, logSpec,
            "Child process %s[%d] exited with status %d.",
            argv0, child_pid, WEXITSTATUS(child_status));
    } else {
        result = child_pid;
    }

finish:
    setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, normal_iopolicy);
    
    return result;
}
