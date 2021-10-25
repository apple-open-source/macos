#include "test.h"
__FBSDID("$FreeBSD$");

#include <spawn.h>

extern char **environ;

DEFINE_TEST(test_leaks)
{
    char execString[16] = {'\0'};
    snprintf(execString, sizeof(execString), "%d", getpid());
    char *args[] = { "/usr/bin/leaks", execString, NULL };
    const char *memgraphError = "Leaks found, but error occurred while generating memgraph";
    
    pid_t pid = -1;
    int status = 0;
    int rv = 0;
    int exitCode = -1;
    
    rv = posix_spawn(&pid, args[0], NULL, NULL, args, environ);
    assert(!rv);
    
    do {
        rv = waitpid(pid, &status, 0);
    } while (rv < 0 && errno == EINTR);
    
    if(WIFEXITED(status)) {
        exitCode = WEXITSTATUS(status);
    }
    
    if (!exitCode) {
        // No leaks found
        return;
    }
    
    // Leaks found. Generate memgraph.
    char *memgraphArgs[] = { "/usr/bin/leaks", execString, "-outputGraph=leaks-libarchive.memgraph", NULL };
    rv = posix_spawn(&pid, memgraphArgs[0], NULL, NULL, memgraphArgs, environ);
    failure("%s", memgraphError);
    assert(!rv);
    
    do {
        rv = waitpid(pid, &status, 0);
    } while (rv < 0 && errno == EINTR);
    
    if(WIFEXITED(status)) {
        failure("%s", memgraphError);
        assert(WEXITSTATUS(status) < 1);
    }
    
    failure("Leaks found");
    assert(!exitCode);
}
