//
//  testleaks.c
//  coretls
//

#include "testleaks.h"

#include <stdio.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <errno.h>
#include <AssertMacros.h>

extern char **environ;


int testleaks(void)
{
    int err;
    pid_t leaks_pid;
    char pidstr[8];
    int status;

    snprintf(pidstr, sizeof(pidstr), "%d", getpid());

    char *const argv[] = {
        "leaks",
        pidstr,
        NULL
    };

    char leaksoutname[] = "/tmp/testleaks.out.XXXXXX";


    int leaksout = mkstemp(leaksoutname);

    posix_spawn_file_actions_t file_actions;

    posix_spawn_file_actions_init(&file_actions);
    posix_spawn_file_actions_adddup2(&file_actions, leaksout, 1);

    require_noerr(err = posix_spawnp(&leaks_pid, "leaks", &file_actions, NULL, argv, NULL), errOut);

    while(1) {
        pid_t waited_pid = waitpid(leaks_pid, &status, 0);

        if(waited_pid == -1) {
            if(errno==EINTR) {
                continue;
            }
            perror("waitpid failed");
            err = errno;
            goto errOut;
        }
        if(waited_pid != leaks_pid) {
            fprintf(stderr, "waited_pid didn't match leaks_pid\n");
            err = -1;
            goto errOut;
        } else {
            break;
        }
    }

    if(!WIFEXITED(status)) {
        fprintf(stderr, "leaks didnt exit properly\n");
        err = status;
        goto errOut;
    }

    int leaks_status = WEXITSTATUS(status);

    switch(leaks_status) {
        case 0:
            fprintf(stderr, "NO LEAKS!\n");
            break;
        case 1:
            fprintf(stderr, "Leaks found some LEAKS! (See details in %s)\n", leaksoutname);
            break;
        default:
            fprintf(stderr, "Leaks encountered some error! (%d - See details in %s)\n", leaks_status, leaksoutname);
            break;
    }

    err = leaks_status;

errOut:
    posix_spawn_file_actions_destroy(&file_actions);
    close(leaksout);


    return err;
}

