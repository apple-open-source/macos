//
//  main.m
//  pythonwrapper
//
//  Created by Andy Kaplan on 1/14/21.
//

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <CoreAnalytics/CoreAnalytics.h>
#import <CoreFoundation/CFPreferences.h>
#import <CoreServices/CoreServicesPriv.h>
#import <AppKit/AppKit.h>
#import "pythonprompt.h"
#import <libproc.h>
#import <sys/sysctl.h>
#import <os/variant_private.h>
#import <sys/file.h>

#define ALLOWED_PREFIXES @[@"/AppleInternal", @"/Applications", @"/System", @"/bin", @"/opt", @"/sbin", @"/usr", @"/SWE", @"/Library"]
#define PYTHON_NAMES @"python", @"python2.7", @"python2"
#define DO_NOT_PROMPT_LOWERCASE_APP_NAMES @[@"pythonwrapper", @"pythonprompt", PYTHON_NAMES]
#define ALERT_DURATION_SECONDS 0

#define DISABLE_PYTHON_ALERT_PREFERENCES_KEY "DisablePythonAlert"
#define DISABLE_PYTHON_ALERT_FILE "/var/db/disablepythonalert"
#define PYTHON_PREFERENCES_APP_ID "com.apple.python"
#define PYTHON_INTERNAL_BIN "/AppleInternal/Library/Frameworks/PythonLegacy.framework/Versions/2.7/bin"
#define PYTHON_UNWRAPPED_BIN "/System/Library/Frameworks/Python.framework/Versions/2.7/bin/unwrapped"
#define PROMPTED_APPS_PLIST_DIR "~/Library/PythonWrapper"
#define PROMPTED_APPS_PLIST PROMPTED_APPS_PLIST_DIR"/promptedapps.plist"
#define LEARN_MORE_URL "https://www.python.org/doc/sunset-python-2"
#define REDACTED_STRING "<redacted>"
#define INTERNAL_ONLY_STRING "[Internal only] "
#define LOCK_PATH "/private/var/tmp/.pythonwrapper.lock"
#define PROMPT_LIMIT_SECONDS (10U)
#define PYTHON_ALERT_ARGS_MAX_LEN (100U)

static BOOL isAppleInternal(void);

void sendAnalytics(NSDictionary *analyticsdict) {
    AnalyticsSendEventLazy(@"com.apple.python.pythonwrapper", ^ {
        return analyticsdict;
    });
}

#define CREATE_ERROR(code, message, ...) _CreateError(__FUNCTION__, __LINE__, @"PythonWrapperErrorDomain", code, nil, nil, message, ## __VA_ARGS__)

typedef NS_OPTIONS(NSInteger, ErrorCodes) {
    GetProcInfoError = 1,
};

NSError *_CreateErrorV(const char *function, int line, NSString *domain, NSInteger code, NSError *underlyingError, NSDictionary *userInfo, NSString *message, va_list ap) {
    NSMutableDictionary *mutableUserInfo = nil;
    if(userInfo) {
        mutableUserInfo = [userInfo mutableCopy];
    } else {
        mutableUserInfo = [NSMutableDictionary new];
    }

    if(message) {
        mutableUserInfo[NSLocalizedDescriptionKey] = [[NSString alloc] initWithFormat:message arguments:ap];
    }

    if(underlyingError) {
        mutableUserInfo[NSUnderlyingErrorKey] = underlyingError;
    }

    mutableUserInfo[@"FunctionName"] = @(function);
    mutableUserInfo[@"SourceFileLine"] = @(line);

    return [NSError errorWithDomain:domain code:code userInfo:mutableUserInfo];
}

NSError *_CreateError(const char *function, int line, NSString *domain, NSInteger code, NSError *underlyingError, NSDictionary *userInfo, NSString *message, ...)
{
    va_list ap;
    va_start(ap, message);
    NSError *error = _CreateErrorV(function, line, domain, code, underlyingError, userInfo, message, ap);
    va_end(ap);
    return error;
}

static BOOL getApplicationFromPid(int pid, NSString **namePt, NSString **executablePathPt, NSString **bundleIDPt) {
    NSString *executablePath = nil;
    LSApplicationProxy *proxy = nil;
    BOOL foundProxy = NO;
    uint8_t path[PROC_PIDPATHINFO_MAXSIZE];
    int length = proc_pidpath(pid, path, PROC_PIDPATHINFO_MAXSIZE);
    if (length > 0) {
        NSString *rawPath = [[NSString alloc] initWithBytes:path length:length encoding:NSUTF8StringEncoding];
        executablePath = [rawPath stringByStandardizingPath];
        if (executablePath) {
            CFBundleRef bundle = _CFBundleCreateWithExecutableURLIfLooksLikeBundle(NULL, (__bridge CFURLRef)[NSURL fileURLWithPath: executablePath]);
            if (bundle) {
                NSURL *bundleURL = CFBridgingRelease(CFBundleCopyBundleURL(bundle));
                if (bundleURL) {
                    proxy = [LSApplicationProxy applicationProxyForBundleURL:bundleURL];
                    if (namePt) {
                        *namePt = proxy.localizedShortName;
                    }
                    if (executablePathPt) {
                        *executablePathPt = proxy.canonicalExecutablePath;
                    }
                    if (bundleIDPt) {
                        *bundleIDPt = proxy.bundleIdentifier;
                    }
                    foundProxy = YES;
                }
                CFRelease(bundle);
            }
        }
    }
    return foundProxy;
}

NSString *filterPath(NSString *path) {
    if (path == nil) {
        return nil;
    }
    NSURL *url = [NSURL fileURLWithPath:path];
    for (NSString *prefix in ALLOWED_PREFIXES) {
        if ([url.path hasPrefix:prefix]) {
            return url.path;
        }
    }
    return @REDACTED_STRING;
}

NSString *constructAndFilterPath(NSString *path, NSString *cwd) {
    if ([path hasPrefix:@"./"] && cwd) {
        path = [NSString stringWithFormat:@"%@/%@", cwd, path];
    }
    return filterPath(path);
}

NSString *truncateString(NSString *string) {
    if (!string) {
        return nil;
    }
    
    NSUInteger length = string.length;
    if (length > PYTHON_ALERT_ARGS_MAX_LEN) {
        length = PYTHON_ALERT_ARGS_MAX_LEN;
    }
    NSRange range = {0, length};
    range = [string rangeOfComposedCharacterSequencesForRange:range];
    return [string substringWithRange:range];
}

void appendFilteredPathsAndNamesCSV(NSString *path, NSString *name, NSString **pathsPt, NSString **namesPt) {
    NSString *filteredpath = filterPath(path);
    NSString *filteredname = nil;
    if (![filteredpath isEqualToString:@REDACTED_STRING]) {
        filteredname = name;
    } else {
        filteredname = @REDACTED_STRING;
    }
    if (pathsPt) {
        *pathsPt = [*pathsPt stringByAppendingFormat:@"%@, ", filteredpath];;
    }
    if (namesPt) {
        *namesPt = [*namesPt stringByAppendingFormat:@"%@, ", filteredname];;
    }
}

// Code adopted from adv_cmds/ps print.c
BOOL getProcInfo(int pid, NSMutableDictionary **dictPt, BOOL *finished, NSError **errorPt) {
    NSError *error = nil;
    int mib[3];
    int argmax, nargs;
    size_t size;
    char *procargs, *savedprocargs, *execpath, *cp, *argPt, *envPt;
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    NSMutableArray<NSString *> *argsArr = nil;
    NSString *execPathString = nil;
    
    procargs = savedprocargs = execpath = NULL;
    
    // Get the maximum process arguments size
    mib[0] = CTL_KERN;
    mib[1] = KERN_ARGMAX;
    size = sizeof(argmax);
    if (sysctl(mib, 2, &argmax, &size, NULL, 0) == -1) {
        error = CREATE_ERROR(GetProcInfoError, @"sysctl argmax failed");
        goto done;
    }
    
    // Allocate space for the arguments
    procargs = (char *)malloc(argmax);
    if (procargs == NULL) {
        error = CREATE_ERROR(GetProcInfoError, @"malloc failed");
        goto done;
    }
    savedprocargs = procargs;

    // Read the process data
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROCARGS2;
    mib[2] = pid;
    size = (size_t)argmax;
    if (sysctl(mib, 3, procargs, &size, NULL, 0) == -1) {
        error = CREATE_ERROR(GetProcInfoError, @"sysctl procargs2 failed");
        goto done;
    }
    
    // Read and skip number of args
    memcpy(&nargs, procargs, sizeof(nargs));
    procargs += sizeof(nargs);
  
    // Read and skip execpath
    execpath = procargs;
    execPathString = [NSString stringWithUTF8String:execpath];
    if ([execPathString isEqualToString:@"/usr/bin/login"]) {
        *finished = YES;
        goto done;
    }
    dict[@"execpath"] = execPathString;
    
    cp = execpath;
    for (; cp < &procargs[size]; cp++) {
        if (*cp == '\0') {
            /* End of execpath reached. */
            break;
        }
    }
    // Skip trailing '\0' characters
    for (; cp < &procargs[size]; cp++) {
        if (*cp != '\0') {
            /* Beginning of first argument reached. */
            break;
        }
    }
    if (cp == &procargs[size]) {
        error = CREATE_ERROR(GetProcInfoError, @"sysctl corrupt execpath");
    }

    // Read '\0' separated args
    int argsRead = 0;
    argPt = NULL;
    argsArr = [NSMutableArray arrayWithCapacity:nargs];
    while (argsRead < nargs) {
        argPt = cp;
        NSString *arg = [NSString stringWithUTF8String:argPt];
        if (!arg) {
            error = CREATE_ERROR(GetProcInfoError, @"sysctl corrupt arg%d cannot be nil", argsRead);
            goto done;
        }
        [argsArr addObject:arg];
        while (cp < &procargs[size] && *cp++ != '\0');
        if (cp >= &procargs[size]) {
            error = CREATE_ERROR(GetProcInfoError, @"sysctl corrupt arg%d", argsRead);
            goto done;
        }
        argsRead++;
    }
    dict[@"args"] = argsArr;

    // Read any environment vars
    envPt = cp;
    size_t envLen = 0;
    int numFound = 0;
    while (cp < &procargs[size]) {
        if (*cp++ != '\0') {
            envLen++;
            continue;
        } else if (envLen > 0) {
            NSString *envVar = [NSString stringWithUTF8String:envPt];
            /* From https://tldp.org/LDP/Bash-Beginners-Guide/html/sect_03_02.html:
               The underscore variable is set at shell startup and contains the absolute file name of the shell or script being executed as passed in
               the argument list. Subsequently, it expands to the last argument to the previous command, after expansion. It is also set to the full
               pathname of each command executed and placed in the environment exported to that command. When checking mail, this parameter holds the
               name of the mail file. */
            if ([envVar hasPrefix:@"_="] || [envVar hasPrefix:@"PWD="] || [envVar hasPrefix:@"SUDO_COMMAND="]) {
                // When we find the parent of the process chain that calls the python #!, this will contain the absolute path that we are looking for
                numFound++;
                NSRange range = [envVar rangeOfString:@"="];
                NSString *prefix = [envVar substringWithRange:NSMakeRange(0, range.location)];
                dict[prefix] = [envVar substringWithRange:NSMakeRange(range.location+1, envVar.length-range.location-1)];
            }
            envLen = 0;
        }
        envPt = cp;
    }

done:
    free(savedprocargs);
    if (error) {
        if (errorPt) {
            *errorPt = error;
        }
        return NO;
    } else if (dictPt) {
        *dictPt = dict;
    }
    return YES;
}

NSMutableDictionary *getInfo(int pid, NSString **appNamePt, NSString **appPathPt, NSString **appBundleIDPt, NSString **appPromptInternalReasonPt) {
    NSMutableDictionary *analyticsdict = [NSMutableDictionary dictionary];
    NSMutableArray<NSDictionary *> *analyticsarr = [NSMutableArray arrayWithCapacity:1];
    NSString *defaultInternalReason = @"Reason: It's using Python 2.7, which has reached sunset.";
    
    if (appNamePt) {
        *appNamePt = nil;
    }
    if (appPathPt) {
        *appPathPt = nil;
    }
    if (appBundleIDPt) {
        *appBundleIDPt = nil;
    }
    if (appPromptInternalReasonPt) {
        *appPromptInternalReasonPt = nil;
    }
    
    int errors = 0;
    BOOL finished = NO;
    NSString *appNames = @"";
    NSString *appPaths = @"";
    NSArray<NSString *> *pythonnames = @[PYTHON_NAMES];
    NSString *scriptpath = nil;
    NSString *processes = @"";
    NSString *cwd = nil;
    NSString *internalReason = defaultInternalReason;
    int i = 1;
    while (1) {
        NSError *error = nil;
        
        NSString *name = nil;
        NSString *path = nil;
        NSString *bundleID = nil;
        BOOL lsPython = NO;
        if (getApplicationFromPid(pid, &name, &path, &bundleID)) {
            
            if ([pythonnames containsObject:name.lowercaseString]) {
                // Don't trust LSApplicationProxy if it returns "Python"
                lsPython = YES;
            } else if (![name.lowercaseString isEqualToString:@"pythonwrapper"]) {
                // We got a real name, e.g., "Visual Studio Code"
                // Don't prompt the user unless we hit this case.
                if (appNamePt) {
                    *appNamePt = name;
                }
                if (appPathPt) {
                    *appPathPt = path;
                }
                if (appBundleIDPt) {
                    *appBundleIDPt = bundleID;
                }
            }
            appendFilteredPathsAndNamesCSV(path, name, &appPaths, &appNames);
        }
        
        NSMutableDictionary *procdict = [NSMutableDictionary dictionary];
        if (getProcInfo(pid,  &procdict, &finished, &error)) {
            if (finished) {
                break;
            }
            [analyticsarr addObject:procdict];
        } else {
            analyticsdict[@"numErrors"] = [NSNumber numberWithInteger:++errors];
        }
        
        BOOL runningDotPyScript = NO;
        BOOL runningPythonCArg = NO;
        BOOL terminalpython = NO;
        NSString *scriptext = nil;
        NSString *execpath = procdict[@"execpath"];
        if ([pythonnames containsObject:execpath.lastPathComponent.lowercaseString]) {
            terminalpython = YES;
        }
        if (lsPython || terminalpython) {
            NSArray<NSString *> *args = procdict[@"args"];
            NSUInteger cIdx = [args indexOfObject:@"-c"];
            scriptext = (args.count >= 2) ? args[1].lowercaseString.pathExtension : nil;
            runningPythonCArg = (cIdx == NSNotFound) ? NO : (cIdx + 1) < args.count;
            runningDotPyScript = [scriptext isEqualToString:@"py"] && !runningPythonCArg;
            if (runningDotPyScript) {
                if (!scriptpath) {
                    scriptpath = (args.count >= 2) ? args[1] : nil;
                    if (scriptpath) {
                        internalReason = [NSString stringWithFormat:@"%@: %@.", @"Reason: Python 2.7 has reached sunset, and the caller is running the Python 2.7 script", scriptpath];
                    } else {
                        // Reason is set at the top of this function
                    }
                }
            } else {
                NSUInteger numargs = args.count;
                if (!scriptpath && (numargs >= 2)) {
                    NSArray<NSString *> *pythonArgs = [args subarrayWithRange:(NSRange){1, args.count-1}];
                    NSString *pythonArgsString = truncateString([pythonArgs componentsJoinedByString:@" "]);
                    internalReason = [NSString stringWithFormat:@"%@: %@.", @"Reason: Python 2.7 has reached sunset, and the caller is running Python 2.7 with args: ", pythonArgsString];
                }
            }
        }
        
        NSString *pwd = procdict[@"PWD"];
        if (pwd) {
            cwd = pwd;
        }
        // SUDO_COMMAND is " " delimited and contains both the command path and command arguments
        NSString *processpath = procdict[@"_"] ? procdict[@"_"]: procdict[@"SUDO_COMMAND"];
        processpath = constructAndFilterPath(processpath, cwd);
        if (processpath || execpath) {
            NSString *process = [NSString stringWithFormat:@"process%d: path:%@ execpath: %@, ", i++, processpath, filterPath(execpath)];
            processes = [processes stringByAppendingString:process];
        }
        
        // Get and repeat for the parent process
        struct proc_bsdinfo bsdinfo;
        proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsdinfo, sizeof(bsdinfo));
        if (pid == bsdinfo.pbi_ppid) {
            break;
        }
        pid = bsdinfo.pbi_ppid;
    }
    
    if (appPromptInternalReasonPt && internalReason) {
        *appPromptInternalReasonPt = internalReason;
    }
    
    if (scriptpath) {
        processes = [processes stringByAppendingFormat:@" script: %@", filterPath(scriptpath)];
    }
    analyticsdict[@"processes"] = processes;
    analyticsdict[@"appnames"] = appNames;
    analyticsdict[@"apppaths"] = appPaths;
    
    return analyticsdict;
}

static BOOL isAppleInternal(void) {
    static BOOL isAppleInternal = NO;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        isAppleInternal = os_variant_has_internal_content("com.apple.python.pythonwrapper");
    });
    return isAppleInternal;
}

static void learnMore(void)
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@LEARN_MORE_URL]];
}

static BOOL avoidMultiplePrompts(void)
{
    NSFileManager* fm = [NSFileManager defaultManager];
    NSDictionary* attrs = [fm attributesOfItemAtPath:@LOCK_PATH error:nil];
    if (!attrs) {
        // File doesn't exist, meaning we should prompt
        return NO;
    }
    NSDate *modificationDate = [attrs fileModificationDate];
    NSTimeInterval elapsed = [[NSDate date] timeIntervalSinceDate:modificationDate];
    if (isnan(elapsed) || elapsed > PROMPT_LIMIT_SECONDS) {
        return NO;
    } else {
        return YES;
    }
}

static int lockPlistLock(BOOL *createdPt)
{
    errno_t savedErrno = 0;
    int oflag = O_CREAT | O_RDWR | O_NOFOLLOW_ANY;
    mode_t mode = 0744;
    BOOL created = NO;
    
    // Check if we are creating the lock file, because otherwise avoidMultiplePrompts suppresses the first prompt
    int fd = open(LOCK_PATH, oflag | O_EXCL, mode);
    if (fd != -1) {
        created = YES;
    } else if (errno == EEXIST) {
        fd = open(LOCK_PATH, oflag, mode);
    }
    if (fd == -1) {
        savedErrno = errno;
        goto error;
    }
    
    if (!flock(fd, LOCK_EX | LOCK_NB)) {
        goto done;
    } else {
        errno_t savedErrno = errno;
        if (savedErrno == EWOULDBLOCK) {
            goto nolock;
        } else {
            goto error;
        }
    }

error:
    NSLog(@"Unable to open lock with error: %s", strerror(savedErrno));
    
nolock:
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
    
done:
    if (createdPt) {
        *createdPt = created;
    }
    
    return fd;
}

static BOOL avoidKnownApp(NSString *appName, NSString *appPath, NSString *appBundleID, NSString *appPromptInternalReason, BOOL *internalAlertPt, NSString **newNamePt, NSString **newReasonPt)
{
    BOOL avoid = NO;
    BOOL internal = NO;
    NSArray<NSString *> *noPromptApps = DO_NOT_PROMPT_LOWERCASE_APP_NAMES;
    NSString *appNameLower = appName.lowercaseString;
    NSString *appPathLower = appPath.lowercaseString;
    
    if ([appNameLower hasPrefix:@"com.apple"]) {
        avoid = YES;
        internal = YES;
        goto done;
    }
    
    if ([noPromptApps containsObject:appNameLower]) {
        avoid = YES;
        goto done;
    }
    
    if ([noPromptApps containsObject:appPathLower.lastPathComponent]) {
        avoid = YES;
        goto done;
    }
    
    if ([appBundleID isEqualToString:@"com.apple.dt.Xcode"]) {
        avoid = YES;
        internal = YES;
        if (newNamePt) {
            *newNamePt = @"This Xcode project";
        }
        if (newReasonPt) {
            NSString *newReason = nil;
            if (appPromptInternalReason) {
                newReason = appPromptInternalReason;
            } else {
                newReason = @"Reason: It's using Python 2.7, which has reached sunset.";
            }
            *newReasonPt = [newReason stringByAppendingString:@" Confirm that scripts being run in each target's Build Phase do not call Python 2.7."];
        }
        goto done;
    }
    
done:
    if (internalAlertPt) {
        *internalAlertPt = internal;
    }
    return avoid;
}

static void displayAlert(NSString *appName, NSString *appPath, NSString *appBundleID, NSString *appPromptInternalReason)
{
    BOOL internalAlert = NO;
    BOOL addInternalTitle = NO;
    BOOL createdLock = NO;
    BOOL appleInternal = isAppleInternal();
    NSString *newName = nil;
    NSString *newReason = nil;
    
    if (!appName || !appPath) {
        return;
    }
    
    if (avoidKnownApp(appName, appPath, appBundleID, appPromptInternalReason, &internalAlert, &newName, &newReason)) {
        if (appleInternal && internalAlert) {
            addInternalTitle = YES;
        } else {
            return;
        }
    }
    
    // 1. Only prompt if we can acquire the file lock LOCK_PATH
    int fd = lockPlistLock(&createdLock);
    if (fd == -1) {
        return;
    }
    
    NSProcessInfo *info = [NSProcessInfo processInfo];
    NSString *osVersion = info.operatingSystemVersionString;
    NSURL *appsPlist = [NSURL fileURLWithPath:[@PROMPTED_APPS_PLIST stringByExpandingTildeInPath]];
    NSError *error = nil;
    NSMutableDictionary *appsDict = nil;
    
    NSString *localizedAlertTitle = NSLocalizedString(@"%@ needs to be updated", @"This is the title of an alert presented to users when an application they're using calls Python 2.7, which is deprecated. The application name will be formatted into the string before displaying to the user.");
    NSString *localizedAlertMsg = NSLocalizedString(@"This app will not work with future versions of macOS and needs to be updated to improve compatibility. Contact the developer for more information.", @"This is the body of an alert presented to users when an application they're using calls Python 2.7, which is deprecated.");
    
    NSFileManager *fm = [NSFileManager defaultManager];
    NSDictionary *appsDictTmp = [NSMutableDictionary dictionaryWithContentsOfURL:appsPlist error:&error];
    if (appsDictTmp) {
        if ([appsDictTmp[appPath] isEqualToString:osVersion]) {
            // We have prompted for this before. Don't prompt again.
            return;
        }
        appsDict = [appsDictTmp mutableCopy];
    } else {
        NSError *underlyingError = error.userInfo[NSUnderlyingErrorKey];
        if ( !(underlyingError && underlyingError.domain == NSPOSIXErrorDomain && underlyingError.code == ENOENT) ) {
            NSLog(@"Error reading preference from %@: %@", appsPlist, error);
        }
        // Don't break if the plist is corrupted
        appsDict = [NSMutableDictionary new];
    }
    appsDict[appPath] = osVersion;
    
    error = nil;
    NSString *dir = [@PROMPTED_APPS_PLIST_DIR stringByExpandingTildeInPath];
    if (![fm fileExistsAtPath:dir] && ![fm createDirectoryAtURL:[NSURL fileURLWithPath:dir] withIntermediateDirectories:NO attributes:nil error:&error]) {
        NSLog(@"Unable to create directory %@: %@", dir, error);
    }
    
    // 2. Only prompt if it's been more than PROMPT_LIMIT_SECONDS since the last prompt
    if (!createdLock && avoidMultiplePrompts()) {
        close(fd);
        return;
    }
    
    error = nil;
    if (![appsDict writeToURL:appsPlist error:&error]) {
        NSLog(@"Unable to write preference %@: %@", appsPlist, error);
    }

    // 3. Refresh the modified date, so avoidMultiplePrompts can pick it up next time.
    NSDictionary *attrs = [NSDictionary dictionaryWithObjectsAndKeys:[NSDate date], NSFileModificationDate, NULL];
    [[NSFileManager defaultManager] setAttributes:attrs ofItemAtPath:@LOCK_PATH error:nil];
    
    // 4. Close the file lock
    close(fd);
    
    if (newReason) {
        appPromptInternalReason = newReason;
    }
    
    NSString *displayedName = nil;
    NSString *title = nil;
    if (newName) {
        displayedName = newName;
    } else {
        displayedName = [NSString stringWithFormat:@"\"%@\"", appName];
    }
    
    title = [NSString stringWithFormat:localizedAlertTitle, displayedName];
    
    NSString *logReason = nil;
    NSString *logMsg = nil;
    
    if (appPromptInternalReason) {
        logReason = [NSString stringWithFormat:@"%@\n\n%@", appPromptInternalReason, localizedAlertMsg];
        if (appleInternal) {
            localizedAlertMsg = logReason;
        }
    }
    
    logMsg = [@"Showing alert to user because " stringByAppendingFormat:@"%@. %@", title, logReason];
    NSLog(@"%@", logMsg);
    
    if (appleInternal && addInternalTitle) {
        title = [@INTERNAL_ONLY_STRING"\n" stringByAppendingString:title];
    } else if (appleInternal && appPromptInternalReason) {
        localizedAlertMsg = [@INTERNAL_ONLY_STRING stringByAppendingString:localizedAlertMsg];
    }
    CFOptionFlags responseFlags = 0;
    // Note: This alert is a blocking call until the user presses a button
    CFUserNotificationDisplayAlert(ALERT_DURATION_SECONDS, kCFUserNotificationCautionAlertLevel, NULL, NULL, NULL,
                                   (__bridge CFStringRef)title,
                                   (__bridge CFStringRef)localizedAlertMsg,
                                   (__bridge CFStringRef)NSLocalizedString(@"OK", @"Alert ok button"),
                                   (__bridge CFStringRef)NSLocalizedString(@"Learn More…", @"Alert learn more button"),
                                   NULL, &responseFlags);
    if (responseFlags == kCFUserNotificationAlternateResponse) {
        learnMore();
    }
    
}

int prompt(int pid, NSString *newBinPath, NSString *oldBinPath)
{
    @autoreleasepool {
        NSString *appName = nil;
        NSString *appPath = nil;
        NSString *appBundleID = nil;
        NSString *appPromptInternalReason = nil;
        NSFileManager *fm = [NSFileManager defaultManager];
        NSMutableDictionary *analyticsdict = getInfo(pid, &appName, &appPath, &appBundleID, &appPromptInternalReason);

        if (isAppleInternal()) {
            analyticsdict[@"pythonOldBinPath"] = filterPath(oldBinPath);
            analyticsdict[@"pythonNewBinPath"] = filterPath(newBinPath);
            sendAnalytics(analyticsdict);
        }

        BOOL alertEnabled = ![fm fileExistsAtPath:@DISABLE_PYTHON_ALERT_FILE] && !CFPreferencesGetAppBooleanValue(CFSTR(DISABLE_PYTHON_ALERT_PREFERENCES_KEY), CFSTR(PYTHON_PREFERENCES_APP_ID), NULL);
        if (alertEnabled) {
            displayAlert(appName, appPath, appBundleID, appPromptInternalReason);
        }
    }
    return 0;
}

@implementation pythonprompt

- (void)prompt:(int)pid withNewBinPath:(NSString *)newBinPath withOldBinPath:(NSString *)oldBinPath withReply:(void (^)(NSString *))reply {
    prompt(pid, newBinPath, oldBinPath);
    NSString *response = @"OK";
    reply(response);
}

@end
