#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <sys/stat.h>
#import <CrashReporterClient.h>
#define SSHDIR "/etc/ssh"

int check_and_gen_key(const char *key_type, char *envp[])
{
    char path[PATH_MAX];
    char *key = strdup(key_type);
    struct stat s;

    snprintf(path, sizeof(path), "%s/ssh_host_%s_key", SSHDIR, key_type);
    if (0 != stat(path, &s)) {
        pid_t child;

        char *argv[] = { "/usr/bin/ssh-keygen",
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
            snprintf(msg, sizeof(msg), "execve failed on error %d %d: %s: /usr/bin/sshd-keygen -q -t %s -f %s -N \"\" -C \"\"", ret, errno, strerror(errno), key, path);
            CRSetCrashLogMessage(msg);
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

int main(int argc, char **argv_, char **envp) {
    char *argv[] = { "/usr/sbin/sshd", "-i", 0 };

    check_and_gen_key("rsa", envp);
    check_and_gen_key("dsa", envp);
    check_and_gen_key("ecdsa", envp);
    check_and_gen_key("ed25519", envp);

    execve(argv[0], &argv[0], envp);
}
