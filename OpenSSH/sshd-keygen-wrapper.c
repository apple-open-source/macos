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
#if !TARGET_OS_OSX
        char *ssh_keygen_path = calloc(PATH_MAX, 1);
        char *cryptex_mount_path = getenv("CRYPTEX_MOUNT_PATH");
        if (cryptex_mount_path == NULL) {
            cryptex_mount_path = "";
        }
        int path_len_needed = snprintf(ssh_keygen_path, PATH_MAX, "%s/%s%s", cryptex_mount_path, PREFIX, "/bin/ssh-keygen");
        if (path_len_needed < 0 || (size_t)path_len_needed >= PATH_MAX) {
            syslog(LOG_ERR, "Failed to generate path to ssh-keygen. Perhaps the cryptex mount path is too long?");
            exit(EX_CONFIG);
        }

        char *argv[] = { ssh_keygen_path,
                          "-q",
                          "-t", key,
                          "-f", path,
                          "-N", "",
                          "-C", "",
                          0
                       };
#else
        char *argv[] = { PREFIX "/bin/ssh-keygen",
                          "-q",
                          "-t", key,
                          "-f", path,
                          "-N", "",
                          "-C", "",
                          0
                       };
#endif
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
#if !TARGET_OS_OSX
        free(ssh_keygen_path);
#endif
    }

    free(key);
    return 0;
}

void usage() {
#if !TARGET_OS_OSX
    fprintf(stderr, "Usage: sshd-keygen-wrapper [-s] [-f <path to custom sshd_config>]\n");
#else
    fprintf(stderr, "Usage: sshd-keygen-wrapper [-s]\n");
#endif
    exit(EX_USAGE);
}
extern int optind;

int main(int argc, char **argv_, char **envp) {
#if !TARGET_OS_OSX
    char *sshd_path = calloc(PATH_MAX, 1);
    char *cryptex_mount_path = getenv("CRYPTEX_MOUNT_PATH");
    if (cryptex_mount_path == NULL) {
        cryptex_mount_path = "";
    }
    int ret = snprintf(sshd_path, PATH_MAX, "%s/%s%s", cryptex_mount_path, PREFIX, "/sbin/sshd");
    if (ret < 0 || (size_t)ret >= PATH_MAX) {
        syslog(LOG_ERR, "Failed to generate path to sshd. Perhaps the cryptex mount path is too long?");
        exit(EX_CONFIG);
    }
    char *config_file = "";
    char *options = "sf:";
    char *argv[] = { sshd_path, "-i", 0, 0, 0, 0 }; /* note placeholder */
#else
    char *options = "s";
    char *argv[] = { PREFIX "/sbin/sshd", "-i", 0, 0}; /* note placeholder */
#endif

    int endarg = 2; /* penultimate position in argv above */
    int ch = -1;
    int password_auth = 1;

    while ((ch = getopt(argc, argv_, options)) != -1) {
        switch (ch) {
            case 's':
                password_auth = 0;
                break;
#if !TARGET_OS_OSX
            case 'f':
                config_file = optarg;
                break;
#endif
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
#if !TARGET_OS_OSX
    if (strcmp(config_file, "") != 0) {
        argv[endarg++] = "-f";
        argv[endarg++] = config_file;
    }
#endif
    execve(argv[0], &argv[0], envp);
#if !TARGET_OS_OSX
    free(sshd_path);
#endif
}
