
#import <sys/types.h>
#import <sys/time.h>
#import <sys/wait.h>
#import <unistd.h>
#import <errno.h>
#import "BootCache.h"

#import "plugin.h"

// Use BootCacheControl's autostop mode to defer the actual cache stop
// for an additional N seconds.
//
#define DEFERRED_STOP	15

@implementation BootCacheLoginPlugin

- (id)init
{
    if ((self = [super init]) != nil)
    {
        //  perform initialization here
    }

    return self;
}

- (void)dealloc
{
    //  perform cleanup here

    [super dealloc];
}

//
// LoginWindow has started, about to ask the user to log in.
//
// If this is the first time, we should try to stop the boot cache
// and save its playlist.
//
// Note that since this entire process is meant to be invisible to
// the user, we do not attempt to alert the user of any errors.
//
- (void)didStartup
{
	struct BC_statistics *ss;
	char	*argv[4], tbuf[16];
	int	result;
	pid_t	child;

	// check that the cache has not already been stopped
	if (BC_fetch_statistics(&ss) ||	// can't get stats
	    (ss == NULL) ||		// something wrong
	    (ss->ss_cache_flags == 0))	// cache all done
		return;

	argv[0] = BC_CONTROL_TOOL;
	argv[1] = "autostop";
	sprintf(tbuf, "%d", DEFERRED_STOP);
	argv[2] = tbuf;
	argv[3] = NULL;

	// fork and exec the control tool
	child = fork();
	switch (child) {
	case -1:
		/* fork failed, just bail */
		break;
	case 0:
		/* we are the child */
		result = execve(BC_CONTROL_TOOL, argv, NULL);
		exit((result != 0) ? 1 : 0);
		break;
	default:
		/*
		 * We are the parent, wait for the child 
		 * which should exit almost immediately.
		 * XXX bad karma if it does not...
		 */
		waitpid(child, &result, 0);
		break;
	}
}

// XXX do we need these default implementations?

- (BOOL)isLoginAllowedForUserID:(uid_t)userID
{
    return YES;
}

- (void)didLogin
{
}

- (void)willLogout
{
}

- (void)willTerminate
{
}

@end
