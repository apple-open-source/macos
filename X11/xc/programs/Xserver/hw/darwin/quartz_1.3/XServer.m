//
//  XServer.m
//
//  This class handles the interaction between the Cocoa front-end
//  and the Darwin X server thread.
//
//  Created by Andreas Monitzer on January 6, 2001.
//
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/XServer.m,v 1.2 2002/06/19 18:12:02 torrey Exp $ */

#import "XServer.h"
#import "Preferences.h"
#import "XWindow.h"
#include "quartzCommon.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/syslimits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>

// Types of shells
enum {
    shell_Unknown,
    shell_Bourne,
    shell_C
};

typedef struct {
    char *name;
    int type;
} shellList_t;

static shellList_t const shellList[] = {
    { "csh",    shell_C },		// standard C shell
    { "tcsh",   shell_C },		// ... needs no introduction
    { "sh",     shell_Bourne },		// standard Bourne shell
    { "zsh",    shell_Bourne },		// Z shell
    { "bash",   shell_Bourne },		// GNU Bourne again shell
    { NULL,	shell_Unknown }
};

extern int argcGlobal;
extern char **argvGlobal;
extern char **envpGlobal;
extern int main(int argc, char *argv[], char *envp[]);
extern void HideMenuBar(void);
extern void ShowMenuBar(void);
static void childDone(int sig);

static NSPortMessage *signalMessage;
static pid_t clientPID;
static XServer *oneXServer;
static NSRect aquaMenuBarBox;


@implementation XServer

- (id)init
{
    self = [super init];
    oneXServer = self;

    serverLock = [[NSRecursiveLock alloc] init];
    clientPID = 0;
    sendServerEvents = NO;
    serverVisible = NO;
    rootlessMenuBarVisible = YES;
    appQuitting = NO;
    mouseState = 0;
    eventWriteFD = quartzEventWriteFD;
    windowClass = [XWindow class];

    // set up a port to safely send messages to main thread from server thread
    signalPort = [[NSPort port] retain];
    signalMessage = [[NSPortMessage alloc] initWithSendPort:signalPort
                    receivePort:signalPort components:nil];

    // set up receiving end
    [signalPort setDelegate:self];
    [[NSRunLoop currentRunLoop] addPort:signalPort
                                forMode:NSDefaultRunLoopMode];
    [[NSRunLoop currentRunLoop] addPort:signalPort
                                forMode:NSModalPanelRunLoopMode];

    return self;
}

- (NSApplicationTerminateReply)
        applicationShouldTerminate:(NSApplication *)sender
{
    // Quit if the X server is not running
    if ([serverLock tryLock]) {
        appQuitting = YES;
        if (clientPID != 0)
            kill(clientPID, SIGINT);
        return NSTerminateNow;
    }

    if (clientPID != 0 || !quartzStartClients) {
        int but;

        // Hide the X server and stop sending it events
        [self hide];
        sendServerEvents = NO;

        but = NSRunAlertPanel(NSLocalizedString(@"Quit X server?",@""),
                              NSLocalizedString(@"Quitting the X server will terminate any running X Window System programs.",@""),
                              NSLocalizedString(@"Quit",@""),
                              NSLocalizedString(@"Cancel",@""),
                              nil);

        switch (but) {
            case NSAlertDefaultReturn:		// quit
                break;
            case NSAlertAlternateReturn:	// cancel
                sendServerEvents = YES;
                return NSTerminateCancel;
        }
    }

    appQuitting = YES;
    if (clientPID != 0)
        kill(clientPID, SIGINT);
    [self killServer];
    return NSTerminateNow;
}

// Ensure that everything has quit cleanly
- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    // Make sure the client process has finished
    if (clientPID != 0) {
        NSLog(@"Waiting on client process...");
        sleep(2);

        // If the client process hasn't finished yet, kill it off
        if (clientPID != 0) {
            int clientStatus;
            NSLog(@"Killing client process...");
            killpg(clientPID, SIGKILL);
            waitpid(clientPID, &clientStatus, 0);
        }
    }

    // Wait until the X server thread quits
    [serverLock lock];
}

// returns YES when event was handled
- (BOOL)translateEvent:(NSEvent *)anEvent
{
    NXEvent ev;
    static BOOL mouse1Pressed = NO;
    BOOL onScreen;

    if (!sendServerEvents) {
        return NO;
    }

    ev.type  = [anEvent type];
    ev.flags = [anEvent modifierFlags];

    if (!quartzRootless) {
        // Check for switch keypress
        if ((ev.type == NSKeyDown) && (![anEvent isARepeat]) &&
            ([anEvent keyCode] == [Preferences keyCode]))
        {
            unsigned int switchFlags = [Preferences modifiers];

            // Switch if all the switch modifiers are pressed, while none are
            // pressed that should not be, except for caps lock.
            if (((ev.flags & switchFlags) == switchFlags) &&
                ((ev.flags & ~(switchFlags | NSAlphaShiftKeyMask)) == 0))
            {
                [self toggle];
                return YES;
            }
        }

        if (!serverVisible)
            return NO;
    }

    // If the mouse is not on the valid X display area,
    // we don't send the X server key events.
    onScreen = [self getNXMouse:&ev];

    switch (ev.type) {
        case NSLeftMouseUp:
            if (quartzRootless && !mouse1Pressed) {
                // MouseUp after MouseDown in menu - ignore
                return NO;
            }
            mouse1Pressed = NO;
            break;
        case NSLeftMouseDown:
            if (quartzRootless &&
                ! ([anEvent window] &&
                   [[anEvent window] isKindOfClass:windowClass])) {
                // Click in non X window - ignore
                return NO;
            }
            mouse1Pressed = YES;
        case NSMouseMoved:
            break;
        case NSLeftMouseDragged:
        case NSRightMouseDragged:
        case 27:        // undocumented high button MouseDragged event
            ev.type=NSMouseMoved;
            break;
        case NSSystemDefined:
            if (![anEvent subtype]==7)
                return NO; // we only use multibutton mouse events
            if ([anEvent data1] & 1)
                return NO; // skip mouse button 1 events
            if (mouseState==[anEvent data2])
                return NO; // ignore double events
            ev.data.compound.subType=[anEvent subtype];
            ev.data.compound.misc.L[0]=[anEvent data1];
            ev.data.compound.misc.L[1]=mouseState=[anEvent data2];
            break;
        case NSScrollWheel:
            ev.data.scrollWheel.deltaAxis1=[anEvent deltaY];
            break;
        case NSKeyDown:
        case NSKeyUp:
            if (!onScreen)
                return NO;
            ev.data.key.keyCode = [anEvent keyCode];
            ev.data.key.repeat = [anEvent isARepeat];
            break;
        case NSFlagsChanged:
            ev.data.key.keyCode = [anEvent keyCode];
            break;
        case 25:        // undocumented MouseDown
        case 26:        // undocumented MouseUp
            // Hide these from AppKit to avoid its log messages
            return YES;
        default:
            return NO;
    }

    [self sendNXEvent:&ev];

    // Rootless: Send first NSLeftMouseDown to windows and views so window
    // ordering can be suppressed.
    // Don't pass further events - they (incorrectly?) bring the window
    // forward no matter what.
    if (quartzRootless  &&
        (ev.type == NSLeftMouseDown || ev.type == NSLeftMouseUp) &&
        [anEvent clickCount] == 1 &&
        [[anEvent window] isKindOfClass:windowClass])
    {
        return NO;
    }

    return YES;
}

// Fill in NXEvent with mouse coordinates, inverting y coordinate.
// For rootless mode, the menu bar is treated as not part of the usable
// X display area and the cursor position is adjusted accordingly.
// Returns YES if the cursor is not in the menu bar.
- (BOOL)getNXMouse:(NXEvent*)ev
{
    NSPoint pt = [NSEvent mouseLocation];

    ev->location.x = (int)(pt.x);

    if (quartzRootless && NSMouseInRect(pt, aquaMenuBarBox, NO)) {
        // mouse in menu bar - tell X11 that it's just below instead
        ev->location.y = aquaMenuBarHeight;
        return NO;
    } else {
        ev->location.y = NSHeight([[NSScreen mainScreen] frame]) - (int)(pt.y);
        return YES;
    }
}

// Append a string to the given enviroment variable
+ (void)append:(NSString*)value toEnv:(NSString*)name
{
    setenv([name cString],
        [[[NSString stringWithCString:getenv([name cString])]
            stringByAppendingString:value] cString],1);
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Block SIGPIPE
    // SIGPIPE repeatably killed the (rootless) server when closing a
    // dozen xterms in rapid succession. Those SIGPIPEs should have been
    // sent to the X server thread, which ignores them, but somehow they
    // ended up in this thread instead.
    {
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGPIPE);
        // pthread_sigmask not implemented yet
        // pthread_sigmask(SIG_BLOCK, &set, NULL);
        sigprocmask(SIG_BLOCK, &set, NULL);
    }

    if (quartzRootless == -1) {
        // The display mode was not set from the command line.
        // Show mode pick panel?
        if ([Preferences modeWindow]) {
            if ([Preferences rootless])
                [startRootlessButton setKeyEquivalent:@"\r"];
            else
                [startFullScreenButton setKeyEquivalent:@"\r"];
            [modeWindow makeKeyAndOrderFront:nil];
        } else {
            // Otherwise use default mode
            quartzRootless = [Preferences rootless];
            [self startX];
        }
    } else {
        [self startX];
    }
}

// Start the X server thread and the client process
- (void)startX
{
    NSDictionary *appDictionary;
    NSString *appVersion;

    [modeWindow close];

    // Calculate the height of the menu bar so rootless mode can avoid it
    if (quartzRootless) {
        aquaMenuBarHeight = NSHeight([[NSScreen mainScreen] frame]) -
                            NSMaxY([[NSScreen mainScreen] visibleFrame]) - 1;
        aquaMenuBarBox =
            NSMakeRect(0, NSMaxY([[NSScreen mainScreen] visibleFrame]) + 1,
                       NSWidth([[NSScreen mainScreen] frame]),
                       aquaMenuBarHeight);
    }

    // Write the XDarwin version to the console log
    appDictionary = [[NSBundle mainBundle] infoDictionary];
    appVersion = [appDictionary objectForKey:@"CFBundleShortVersionString"];
    if (appVersion)
        NSLog(@"\n%@", appVersion);
    else
        NSLog(@"No version");

    // Start the X server thread
    [NSThread detachNewThreadSelector:@selector(run) toTarget:self
              withObject:nil];
    sendServerEvents = YES;

    // Start the X clients if started from GUI
    if (quartzStartClients) {
        [self startXClients];
    }

    if (quartzRootless) {
        // There is no help window for rootless; just start
        [helpWindow close];
        helpWindow = nil;
        if ([NSApp isActive])
            [self sendShowHide:YES];
        else
            [self sendShowHide:NO];
    } else {
        // Show the X switch window if not using dock icon switching
        if (![Preferences dockSwitch])
            [switchWindow orderFront:nil];

        if ([Preferences startupHelp]) {
            // display the full screen mode help
            [self sendShowHide:NO];
            [helpWindow makeKeyAndOrderFront:nil];
        } else {
            // start running full screen and make sure X is visible
            ShowMenuBar();
            [self closeHelpAndShow:nil];
        }
    }
}

// Start the first X clients in a separate process
- (BOOL)startXClients
{
    struct passwd *passwdUser;
    NSString *shellPath, *dashShellName, *commandStr, *startXPath;
    NSMutableString *safeStartXPath;
    NSRange aRange;
    NSBundle *thisBundle;
    const char *shellPathStr, *newargv[3], *shellNameStr;
    int fd[2], outFD, length, shellType, i;

    // Register to catch the signal when the client processs finishes
    signal(SIGCHLD, childDone);

    // Get user's password database entry
    passwdUser = getpwuid(getuid());

    // Find the shell to use
    if ([Preferences useDefaultShell])
        shellPath = [NSString stringWithCString:passwdUser->pw_shell];
    else
        shellPath = [Preferences shellString];

    dashShellName = [NSString stringWithFormat:@"-%@",
                            [shellPath lastPathComponent]];
    shellPathStr = [shellPath cString];
    shellNameStr = [[shellPath lastPathComponent] cString];

    if (access(shellPathStr, X_OK)) {
        NSLog(@"Shell %s is not valid!", shellPathStr);
        return NO;
    }

    // Find the type of shell
    for (i = 0; shellList[i].name; i++) {
        if (!strcmp(shellNameStr, shellList[i].name))
            break;
    }
    shellType = shellList[i].type;

    newargv[0] = [dashShellName cString];
    if (shellType == shell_Bourne) {
        // Bourne shells need to be told they are interactive to make
        // sure they read all their initialization files.
        newargv[1] = "-i";
        newargv[2] = NULL;
    } else {
        newargv[1] = NULL;
    }

    // Create a pipe to communicate with the X client process
    NSAssert(pipe(fd) == 0, @"Could not create new pipe.");

    // Open a file descriptor for writing to stdout and stderr
    outFD = open("/dev/console", O_WRONLY, 0);
    if (outFD == -1) {
        outFD = open("/dev/null", O_WRONLY, 0);
        NSAssert(outFD != -1, @"Could not open shell output.");
    }

    // Fork process to start X clients in user's default shell
    // Sadly we can't use NSTask because we need to start a login shell.
    // Login shells are started by passing "-" as the first character of
    // argument 0. NSTask forces argument 0 to be the shell's name.
    clientPID = vfork();
    if (clientPID == 0) {

        // Inside the new process:
        if (fd[0] != STDIN_FILENO) {
            dup2(fd[0], STDIN_FILENO);	// Take stdin from pipe
            close(fd[0]);
        }
        close(fd[1]);			// Close write end of pipe
        if (outFD == STDOUT_FILENO) {	// Setup stdout and stderr
            dup2(outFD, STDERR_FILENO);
        } else if (outFD == STDERR_FILENO) {
            dup2(outFD, STDOUT_FILENO);
        } else {
            dup2(outFD, STDERR_FILENO);
            dup2(outFD, STDOUT_FILENO);
            close(outFD);
        }

        // Setup environment
        setenv("HOME", passwdUser->pw_dir, 1);
        setenv("SHELL", shellPathStr, 1);
        setenv("LOGNAME", passwdUser->pw_name, 1);
        setenv("USER", passwdUser->pw_name, 1);
        setenv("TERM", "unknown", 1);
        if (chdir(passwdUser->pw_dir))	// Change to user's home dir
            NSLog(@"Could not change to user's home directory.");

        execv(shellPathStr, newargv);	// Start user's shell

        NSLog(@"Could not start X client process with errno = %i.", errno);
        _exit(127);
    }

    // In parent process:
    close(fd[0]);	// Close read end of pipe
    close(outFD);	// Close output file descriptor

    thisBundle = [NSBundle bundleForClass:[self class]];
    startXPath = [thisBundle pathForResource:@"startXClients" ofType:nil];
    if (!startXPath) {
        NSLog(@"Could not find startXClients in application bundle!");
        return NO;
    }

    // We will run the startXClients script with the path in single quotes
    // in case there are problematic characters in the path. We still have
    // to worry about there being single quotes in the path. So, replace
    // all instances of the ' character in startXPath with '\''.
    safeStartXPath = [NSMutableString stringWithString:startXPath];
    aRange = NSMakeRange(0, [safeStartXPath length]);
    while (aRange.length) {
        aRange = [safeStartXPath rangeOfString:@"'" options:0 range:aRange];
        if (!aRange.length)
            break;
        [safeStartXPath replaceCharactersInRange:aRange
                        withString:@"\'\\'\'"];
        aRange.location += 4;
        aRange.length = [safeStartXPath length] - aRange.location;
    }

    if ([Preferences addToPath]) {
        commandStr = [NSString stringWithFormat:@"'%@' :%d %@\n",
                        safeStartXPath, [Preferences display],
                        [Preferences addToPathString]];
    } else {
        commandStr = [NSString stringWithFormat:@"'%@' :%d\n",
                        safeStartXPath, [Preferences display]];
    }

    length = [commandStr cStringLength];
    if (write(fd[1], [commandStr cString], length) != length) {
        NSLog(@"Write to X client process failed.");
        return NO;
    }

    // Close the pipe so that shell will terminate when xinit quits
    close(fd[1]);

    return YES;
}

// Run the X server thread
- (void)run
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    [serverLock lock];
    main(argcGlobal, argvGlobal, envpGlobal);
    serverVisible = NO;
    [pool release];
    [serverLock unlock];
    QuartzMessageMainThread(kQuartzServerDied);
}

// Full screen mode was picked in the mode pick panel
- (IBAction)startFullScreen:(id)sender
{
    [Preferences setModeWindow:[startupModeButton intValue]];
    [Preferences saveToDisk];
    quartzRootless = FALSE;
    [self startX];
}

// Rootless mode was picked in the mode pick panel
- (IBAction)startRootless:(id)sender
{
    [Preferences setModeWindow:[startupModeButton intValue]];
    [Preferences saveToDisk];
    quartzRootless = TRUE;
    [self startX];
}

// Close the help splash screen and show the X server
- (IBAction)closeHelpAndShow:(id)sender
{
    if (sender) {
        int helpVal = [startupHelpButton intValue];
        [Preferences setStartupHelp:helpVal];
        [Preferences saveToDisk];
    }
    [helpWindow close];
    helpWindow = nil;

    serverVisible = YES;
    [self sendShowHide:YES];
    [NSApp activateIgnoringOtherApps:YES];
}

// Show the X server when sent message from GUI
- (IBAction)showAction:(id)sender
{
    if (sendServerEvents)
        [self sendShowHide:YES];
}

// Show or hide the X server or menu bar in rootless mode
- (void)toggle
{
    if (quartzRootless) {
#if 0
        // FIXME: Remove or add option to not dodge menubar
        if (rootlessMenuBarVisible)
            HideMenuBar();
        else
            ShowMenuBar();
        rootlessMenuBarVisible = !rootlessMenuBarVisible;
#endif
    } else {
        if (serverVisible)
            [self hide];
        else
            [self show];
    }
}

// Show the X server on screen
- (void)show
{
    if (!serverVisible && sendServerEvents) {
        [self sendShowHide:YES];
    }
}

// Hide the X server from the screen
- (void)hide
{
    if (serverVisible && sendServerEvents) {
        [self sendShowHide:NO];
    }
}

// Kill the X server thread
- (void)killServer
{
    NXEvent ev;

    if (serverVisible)
        [self hide];

    ev.type = NX_APPDEFINED;
    ev.data.compound.subType = kXDarwinQuit;
    [self sendNXEvent:&ev];
}

// Tell the X server to show or hide itself.
// This ignores the current X server visible state.
//
// In full screen mode, the order we do things is important and must be
// preserved between the threads. X drawing operations have to be performed
// in the X server thread. It appears that we have the additional
// constraint that we must hide and show the menu bar in the main thread.
//
// To show the X server:
//   1. Capture the displays. (Main thread)
//   2. Hide the menu bar. (Must be in main thread)
//   3. Send event to X server thread to redraw X screen.
//   4. Redraw the X screen. (Must be in X server thread)
//
// To hide the X server:
//   1. Send event to X server thread to stop drawing.
//   2. Stop drawing to the X screen. (Must be in X server thread)
//   3. Message main thread that drawing is stopped.
//   4. If main thread still wants X server hidden:
//     a. Release the displays. (Main thread)
//     b. Unhide the menu bar. (Must be in main thread)
//   Otherwise we have already queued an event to start drawing again.
//
- (void)sendShowHide:(BOOL)show
{
    NXEvent ev;

    [self getNXMouse:&ev];
    ev.type = NX_APPDEFINED;

    if (show) {
        if (!quartzRootless) {
            QuartzFSCapture();
            HideMenuBar();
        }
        ev.data.compound.subType = kXDarwinShow;
        [self sendNXEvent:&ev];

        // inform the X server of the current modifier state
        ev.flags = [[NSApp currentEvent] modifierFlags];
        ev.data.compound.subType = kXDarwinUpdateModifiers;
        [self sendNXEvent:&ev];

        // put the pasteboard into the X cut buffer
        [self readPasteboard];
    } else {
        // put the X cut buffer on the pasteboard
        [self writePasteboard];

        ev.data.compound.subType = kXDarwinHide;
        [self sendNXEvent:&ev];
    }

    serverVisible = show;
}

// Tell the X server to read from the pasteboard into the X cut buffer
- (void)readPasteboard
{
    NXEvent ev;

    ev.type = NX_APPDEFINED;
    ev.data.compound.subType = kXDarwinReadPasteboard;
    [self sendNXEvent:&ev];
}

// Tell the X server to write the X cut buffer into the pasteboard
- (void)writePasteboard
{
    NXEvent ev;

    ev.type = NX_APPDEFINED;
    ev.data.compound.subType = kXDarwinWritePasteboard;
    [self sendNXEvent:&ev];
}

- (void)sendNXEvent:(NXEvent*)ev
{
    int bytesWritten;

    if (quartzRootless  &&
        (ev->type == NSLeftMouseDown  ||  ev->type == NSLeftMouseUp  ||
        (ev->type == NSSystemDefined && ev->data.compound.subType == 7)))
    {
        // mouse button event - send mouseMoved to this position too
        // X gets confused if it gets a click that isn't at the last
        // reported mouse position.
        NXEvent moveEvent = *ev;
        moveEvent.type = NSMouseMoved;
        [self sendNXEvent:&moveEvent];
    }

    bytesWritten = write(eventWriteFD, ev, sizeof(*ev));
    if (bytesWritten == sizeof(*ev))
        return;
    NSLog(@"Bad write to event pipe.");
    // FIXME: handle bad writes better?
}

// Handle messages from the X server thread
- (void)handlePortMessage:(NSPortMessage *)portMessage
{
    unsigned msg = [portMessage msgid];

    switch(msg) {
        case kQuartzServerHidden:
            // Make sure the X server wasn't queued to be shown again while
            // the hide was pending.
            if (!quartzRootless && !serverVisible) {
                QuartzFSRelease();
                ShowMenuBar();
            }

            // FIXME: This hack is necessary (but not completely effective)
            // since Mac OS X 10.0.2
            [NSCursor unhide];
            break;

        case kQuartzServerDied:
            sendServerEvents = NO;
            if (!appQuitting) {
                [NSApp terminate:nil];	// quit if we aren't already
            }
            break;

        default:
            NSLog(@"Unknown message from server thread.");
    }
}

// Quit the X server when the X client process finishes
- (void)clientProcessDone:(int)clientStatus
{
    if (WIFEXITED(clientStatus)) {
        int exitStatus = WEXITSTATUS(clientStatus);
        if (exitStatus != 0)
            NSLog(@"X client process terminated with status %i.", exitStatus);
    } else {
        NSLog(@"X client process terminated abnormally.");
    }

    if (!appQuitting) {
        [NSApp terminate:nil];	// quit if we aren't already
    }
}

// Called when the user clicks the application icon,
// but not when Cmd-Tab is used.
// Rootless: Don't switch until applicationWillBecomeActive.
- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication
            hasVisibleWindows:(BOOL)flag
{
    if ([Preferences dockSwitch] && !quartzRootless) {
        [self show];
    }
    return NO;
}

- (void)applicationWillResignActive:(NSNotification *)aNotification
{
    [self hide];
}

- (void)applicationWillBecomeActive:(NSNotification *)aNotification
{
    if (quartzRootless)
        [self show];
}

@end

// Send a message to the main thread, which calls handlePortMessage in
// response. Must only be called from the X server thread because
// NSPort is not thread safe.
void QuartzMessageMainThread(unsigned msg)
{
    [signalMessage setMsgid:msg];
    [signalMessage sendBeforeDate:[NSDate distantPast]];
}

// Handle SIGCHLD signals
static void childDone(int sig)
{
    int clientStatus;

    if (clientPID == 0)
        return;

    // Make sure it was the client task that finished
    if (waitpid(clientPID, &clientStatus, WNOHANG) == clientPID) {
        if (WIFSTOPPED(clientStatus))
            return;
        clientPID = 0;
        [oneXServer clientProcessDone:clientStatus];
    }
}
