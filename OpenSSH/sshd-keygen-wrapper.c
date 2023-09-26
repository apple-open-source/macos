#include <os/log.h>
#include <printf.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sysexits.h>
#include <sys/stat.h>
#include <sys/wait.h>

#import <CrashReporterClient.h>

#define PREFIX "/usr"
#define SSHDIR "/etc/ssh"

static os_log_t logger;
static printf_domain_t xp = NULL;

static int
fprintf_shell(FILE *stream, const struct printf_info *info, const void *const *args)
{
    static const char safechars[] = "_-/:%,.0123456789"
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char *s = *(const char **)args[0];
    if (s[0] != '\0' && strlen(s) == strspn(s, safechars)) {
        return fprintf(stream, "%s", s);
    }
    int count = 2; // leading & trailing single quotes
    fputc('\'', stream);
    for (; *s != '\0'; s++) {
        switch (*s) {
        case '\'':
            count += fprintf(stream, "'\\''");
            break;
        default:
            fputc(*s, stream);
            count++;
            break;
        }
    }
    fputc('\'', stream);
    return count;
}

static int
fprintf_shell_arginfo(const struct printf_info *info, size_t n, int *argtypes)
{
    argtypes[0] = PA_STRING;
    return 1;
}

static void
setup_printf(void)
{
    xp = new_printf_domain();
    register_printf_domain_function(xp, 'S', fprintf_shell, fprintf_shell_arginfo, NULL);
}

static int
fmt_argv(char *buf, size_t buflen, int argc, char **argv)
{
    char *end = &buf[buflen];
    char *p = buf;
    for (int i = 0; i < argc && argv[i] != NULL; ++i) {
        int n = sxprintf(p, end - p, xp, NULL, "%s%S", i ? " " : "", argv[i]);
        if (n < 0 || n >= end - p) {
            return 1;
        }
        p += n;
    }
    return 0;
}


static int
check_and_gen_key(const char *key_type, char *envp[])
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
                         0};
        os_log_info(logger, "Generating %s key at %s", key, path);
        char msg[1024] = { 0 };
        fmt_argv(msg, sizeof(msg), sizeof(argv) / sizeof(argv[0]), argv);
        child = fork();
        if (child == 0) {
            logger = os_log_create("com.apple.sshd-keygen-wrapper", "default");
            if (!freopen("/dev/null", "w", stdout)) {
                os_log_error(logger, "failed to redirect stdout for ssh-keygen operation");
            }

            execve(argv[0], &argv[0], envp);
            os_log_fault(logger, "execve: %{darwin.errno}d: `%s`", errno, msg);
            CRSetCrashLogMessage(msg);
            abort();
        } else {
            int stat_loc;
            (void)waitpid(child, &stat_loc, 0);
            if (WIFEXITED(stat_loc)) {
                if (WEXITSTATUS(stat_loc)) {
                    os_log_fault(logger, "`%s` exited with status %d",
                        msg, WEXITSTATUS(stat_loc));
                }
            } else if (WIFSTOPPED(stat_loc)) {
                os_log_fault(logger, "`%s` stopped on signal %d",
                    msg, WSTOPSIG(stat_loc));
            } else if (WIFSIGNALED(stat_loc)) {
                os_log_fault(logger, "`%s` terminated on signal %d",
                    msg, WTERMSIG(stat_loc));
            }
        }
    }

    free(key);
    return 0;
}

static void
usage(void)
{
    fprintf(stderr, "Usage: sshd-keygen-wrapper [-s]\n");
    exit(EX_USAGE);
}
extern int optind;

int
main(int argc, char **argv_, char **envp)
{
    setup_printf();
    char *argv[] = { PREFIX "/sbin/sshd", "-i", 0 };
    int ch = -1;

    logger = os_log_create("com.apple.sshd-keygen-wrapper", "default");

    while ((ch = getopt(argc, argv_, "")) != -1) {
        switch (ch) {
        default:
            usage();
        }
    }
    argc -= optind;
    argv_ += optind;

    check_and_gen_key("rsa", envp);
    check_and_gen_key("dsa", envp);
    check_and_gen_key("ecdsa", envp);
    check_and_gen_key("ed25519", envp);

    char msg[1024] = { 0 };
    fmt_argv(msg, sizeof(msg), sizeof(argv) / sizeof(argv[0]), argv);
    os_log_debug(logger, "execve: `%s`", msg);
    execve(argv[0], &argv[0], envp);
    os_log_fault(logger, "execve: %{darwin.errno}d: `%s`", errno, msg);
    abort();
}
