#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <sysexits.h>
#include <sys/stat.h>
#import <CrashReporterClient.h>
#import <TargetConditionals.h>

#if TARGET_OS_OSX
#define PREFIX "/usr"
#define SSHDIR "/etc/ssh"
#else
#define PREFIX "/usr/local"
#define SSHDIR "/private/var/db/ssh"
#endif

int check_and_gen_key(const char *key_type, char *envp[])
{
    char path[PATH_MAX];
    char *key = strdup(key_type);
    struct stat s;

    snprintf(path, sizeof(path), "%s/ssh_host_%s_key", SSHDIR, key_type);
    if (0 != stat(path, &s)) {
        pid_t child;

        char *argv[] = { PREFIX "/bin/ssh-keygen",
                          "-q",
                          "-t", key,
                          "-f", path,
                          "-N", "",
                          "-C", "",
                          0
                       };

        child = fork();
        if (child == 0) {
            int ret;
            char msg[1024];

            if (!freopen("/dev/null", "w", stdout)) {
                syslog(LOG_ERR, "failed to redirect stdout for ssh-keygen operation");
            }
            ret = execve(argv[0], &argv[0], envp);
#if TARGET_OS_OSX
            snprintf(msg, sizeof(msg), "execve failed on error %d %d: %s: "
                     PREFIX "/bin/sshd-keygen -q -t %s -f %s -N \"\" -C \"\"",
                     ret, errno, strerror(errno), key, path);
            CRSetCrashLogMessage(msg);
#endif
            abort();
        } else {
            pid_t tpid;
            int stat_loc;
            do {
                tpid = waitpid(child, &stat_loc, 0);
            } while (tpid != child);
        }
    }

    free(key);
    return 0;
}

void usage() {
    fprintf(stderr, "Usage: sshd-keygen-wrapper [-s]\n");
    exit(EX_USAGE);
}
extern int optind;

int main(int argc, char **argv_, char **envp) {
    char *argv[] = { PREFIX "/sbin/sshd", "-i", 0, 0 }; /* note placeholder */
    int endarg = 2; /* penultimate position in argv above */
    int ch = -1;
    int password_auth = 1;

    while ((ch = getopt(argc, argv_, "s")) != -1) {
        switch (ch) {
            case 's':
                password_auth = 0;
                break;
            default:
                usage();
        }
    }
    argc -= optind;
    argv_ += optind;

#if TARGET_OS_OSX
    check_and_gen_key("rsa", envp);
    check_and_gen_key("dsa", envp);
#endif
    check_and_gen_key("ecdsa", envp);
    check_and_gen_key("ed25519", envp);

    if (!password_auth) {
        argv[endarg++] = "-oPasswordAuthentication=no";
    }
    execve(argv[0], &argv[0], envp);
}
