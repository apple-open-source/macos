/*
 * mach_client_utilities.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosIPC/Sources/mach_client_utilities.c,v 1.6 2003/08/22 16:21:05 lxs Exp $
 *
 * Copyright 2003 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Kerberos/KerberosDebug.h>
#include <mach/mach.h>
#include <mach/boolean.h>
#include <mach/mach_error.h>
#include <mach/notify.h>
#include <servers/bootstrap.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pwd.h>

#include <Kerberos/mach_client_utilities.h>

#define	kMachIPCServerPathMaxLength		MAXPATHLEN

static kern_return_t
mach_launch_server_from_path (const char *inServerPath);

static kern_return_t
mach_find_server_path_relative_to_bundle (const char *inBundleIdentifier,
                                          const char *inOffsetPath,
                                          const char *inServerBundleName,
                                          char *ioServerPath);

#pragma mark -

// ---------------------------------------------------------------------------

kern_return_t
mach_client_lookup_server (const char *inServiceNamePrefix, mach_port_t *outServicePort) 
{
    kern_return_t err = KERN_SUCCESS;
    mach_port_t   bootPort = MACH_PORT_NULL;
    const char *serviceName = LoginSessionGetServiceName (inServiceNamePrefix);

    if (err == KERN_SUCCESS) {
        // Get our bootstrap port
        err = task_get_bootstrap_port (mach_task_self (), &bootPort);
    }

    if (err == KERN_SUCCESS) {
        // Look up the port for this service
        err = bootstrap_look_up (bootPort, (char *) serviceName, outServicePort);
    }

    if (bootPort != MACH_PORT_NULL) { mach_port_deallocate (mach_task_self (), bootPort); }
    
    return err;
}

// ---------------------------------------------------------------------------

kern_return_t
mach_client_lookup_and_launch_server (const char *inServiceNamePrefix,
                                      const char *inBundleIdentifier,
                                      const char *inPath,
                                      const char *inServerBundleName,
                                      mach_port_t *outServicePort)
{
    kern_return_t err = BOOTSTRAP_SUCCESS;
    boolean_t     serverLaunched = false;
    time_t        serverLaunchTime = 0; // Remember when we tried to launch the server
    char          serverPath[kMachIPCServerPathMaxLength];

    while (mach_client_lookup_server (inServiceNamePrefix, outServicePort) == BOOTSTRAP_UNKNOWN_SERVICE) {
        time_t now = time (NULL);
        
        if (!serverLaunched) {
            if (err == BOOTSTRAP_SUCCESS) {
                err = mach_find_server_path_relative_to_bundle (inBundleIdentifier, inPath,
                                                                inServerBundleName, serverPath);
            }
            
            if (err == BOOTSTRAP_SUCCESS) {
                dprintf ("mach_lookup_and_launch_server: launching %s\n", serverPath);
                err = mach_launch_server_from_path (serverPath);
            }

            if (err == BOOTSTRAP_SUCCESS) {
                serverLaunched = true;
                serverLaunchTime = now;
            }
        } else {
            if ((serverLaunchTime + 20) < now) { break; }  // timeout after 20 seconds
        }
        
        // Allow the server to launch before we try to look it up again
        usleep (10000);  // wait 10 milliseconds
    }
    
    if (err != BOOTSTRAP_SUCCESS) {
        dprintf ("mach_lookup_and_launch_server (%s) failed with error %d (%s)\n",
                 LoginSessionGetServiceName (inServiceNamePrefix), err, mach_error_string (err));
    }
    
    return err;
}

// ---------------------------------------------------------------------------

boolean_t
mach_client_allow_server (security_token_t inToken)
{
    uid_t clientUID = LoginSessionGetSessionUID ();
    if (inToken.val[0] == clientUID) {
        return true;
    } else if (geteuid() == 0) {
        // If we are a su-ed client, allow if we are effective uid root since server will allow us
        // We check the effective uid since that's what security trailers store
        dprintf ("mach_client_allow_server: WARNING, server with uid %ld approved for seteuid root client\n",
                clientUID, inToken.val[0]);
        return true;
    } else {
        dprintf ("mach_client_allow_server: WARNING! Client with uid %ld refused server with uid %ld\n",
                clientUID, inToken.val[0]);
        return false;
    }
}
#pragma mark -


// ---------------------------------------------------------------------------

static kern_return_t
mach_launch_server_from_path (const char *inServerPath)
{
    // Since the server will call daemon and become a child of init, it won't
    // necessarily know what its bootstrap port is because it can't get the terminal name
    uid_t serverUID = LoginSessionGetSessionUID ();

#if MACDEV_DEBUG
    const char *securitySessionName = LoginSessionGetSecuritySessionName (); // Just for informational purposes
#endif
    
    kern_return_t err = KERN_SUCCESS;
    pid_t         newPID = fork ();
    pid_t         waitPID;
    struct rlimit rlp;
    rlim_t        i;

    if (inServerPath == NULL || inServerPath[0] == '\0') {
        return KERN_INVALID_ARGUMENT;
    }
    
    switch (newPID) {
        case -1:
            // error!  no child created
            err = errno;
            dprintf ("fork (): child creation failed (errno = '%s')\n", strerror (err));
            break;
	        
        case 0:
            // I'm the child.  Become the server...
            dprintf ("Child of fork() is becoming server...\n");
            
            // set our privileges (obtained above)
            dprintf ("Setting uid to '%ld'\n", serverUID);
            seteuid (serverUID);
            setuid (serverUID);

            dprintf ("Calling daemon(0,0)..\n");
            err = daemon (0, 0);
            if (err) {
                dprintf ("daemon() failed (errno = '%s').  Exiting...\n", strerror (errno));
                exit (err);
            }
            
            dprintf ("Closing all open file descriptors (except stdin, stdout and stderr).\n");
            err = getrlimit (RLIMIT_NOFILE, &rlp);
            if (err) {
                dprintf ("getrlimit failed (errno = '%s').  Exiting...\n", strerror (errno));
                exit (1);
            }
            dprintf ("rlp.rlim_max = %qx; rlp.rlim_cur = %qx\n", rlp.rlim_max, rlp.rlim_cur);
            for (i = STDERR_FILENO + 1; i < rlp.rlim_cur; i++) {
                close (i);
            }
            
            /* Don't dprintf because the file descriptor has been closed */
#if MACDEV_DEBUG
            err = execl (inServerPath, inServerPath, securitySessionName, NULL);
#else
            err = execl (inServerPath, inServerPath, NULL);
#endif
            _exit (err);
        
        default:
            // I'm the parent!
            // Wait for the child to exit (which will happen because of the call to daemon)
            // otherwise we will leave a zombie process turd until the parent exits.
            waitPID = waitpid (newPID, &err, 0);
            dprintf ("waitpid(%d) returned pid = %d, err = %d\n", (int) newPID, (int) waitPID, err);
            return KERN_SUCCESS;
    }
    
    return KERN_FAILURE;
}

// ---------------------------------------------------------------------------

static kern_return_t
mach_find_server_path_relative_to_bundle (const char *inBundleIdentifier,
                                          const char *inPath,
                                          const char *inServerBundleName,
                                          char *ioServerPath)
{
    // If only CoreFoundation returned errors we wouldn't have to make this up:
    kern_return_t err = KERN_FAILURE;
    
    CFStringRef      pathCFString = NULL;
    CFStringRef      serverBundleNameCFString = NULL;
    CFURLRef         pathURL = NULL;
    CFURLRef         bundleURL = NULL;
    CFStringRef      bundleIdentifierCFString = NULL;
    CFURLRef         serverURL = NULL;
    CFBundleRef      serverBundle = NULL;
    CFURLRef         serverMainExecutable = NULL;
    CFURLRef         serverMainExecutableAbsolute = NULL;
    CFStringRef      serverPathCFString = NULL;
    CFStringEncoding encoding = CFStringGetSystemEncoding ();
  
    dprintf ("mach_find_server_path_relative_to_bundle(%s, %s, %s)\n",
                inBundleIdentifier == NULL ? "(null)" : inBundleIdentifier, 
                inPath, inServerBundleName);

    pathCFString = CFStringCreateWithCString (kCFAllocatorDefault, inPath, encoding);

    serverBundleNameCFString = CFStringCreateWithCString (kCFAllocatorDefault, inServerBundleName, encoding);

    if (inBundleIdentifier == NULL) {
        // if inBundleIdentifier == NULL then inPath is a full path
        pathURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, pathCFString, 
                                                    kCFURLPOSIXPathStyle, true);
    } else {
        CFBundleRef bundle = nil;

        // inPath is a relative path from inBundleIdentifier
        bundleIdentifierCFString = CFStringCreateWithCString (kCFAllocatorDefault, inBundleIdentifier, encoding);
                                                                
        if (bundleIdentifierCFString != nil) {
            bundle = CFBundleGetBundleWithIdentifier (bundleIdentifierCFString);
            dprintf ("CFBundleGetBundleWithIdentifier() (bundle = %p)\n", bundle);
        }
        
        if (bundle != nil) {
            bundleURL = CFBundleCopyBundleURL (bundle);
            dprintf ("CFBundleCopyBundleURL() (bundleURL = %p)\n", bundleURL);
        }
        
        if (bundleURL != nil && pathCFString != nil) {
            pathURL = CFURLCreateCopyAppendingPathComponent (kCFAllocatorDefault, bundleURL, 
                                                                pathCFString, true);
            dprintf ("CFURLCreateCopyAppendingPathComponent() (pathURL = %p)\n", pathURL);
        }
    }
    
    if (pathURL != nil && serverBundleNameCFString != nil) {
        serverURL = CFURLCreateCopyAppendingPathComponent (kCFAllocatorDefault, pathURL, 
                                                            serverBundleNameCFString, true);
		dprintf ("CFURLCreateCopyAppendingPathComponent() (serverURL = %p)\n", serverURL);
     }
    
#if MACDEV_DEBUG
    {
        CFStringRef serverPathBeforeCreate = nil;
        char pathBeforeCreate[kMachIPCServerPathMaxLength];

        if (serverURL != nil) {
            serverPathBeforeCreate = CFURLCopyFileSystemPath (serverURL, kCFURLPOSIXPathStyle);
            dprintf ("CFURLCopyFileSystemPath() (serverPathBeforeCreate = %p)\n", serverPathBeforeCreate);
        }

        if (serverPathBeforeCreate != nil) {
            CFIndex length = CFStringGetMaximumSizeForEncoding (CFStringGetLength (serverPathBeforeCreate), encoding);
            if (length < kMachIPCServerPathMaxLength) {
                CFStringGetCString (serverPathBeforeCreate, pathBeforeCreate,
                                    kMachIPCServerPathMaxLength, encoding);
                dprintf ("Attempting to create a bundle for %s at '%s'\n",
                         inServerBundleName, pathBeforeCreate);
            } else {
                dprintf ("String serverPathBeforeCreate too long\n");
            }

            CFRelease (serverPathBeforeCreate);
        }
    }
#endif
	    
    if (serverURL != nil) {
        serverBundle = CFBundleCreate (kCFAllocatorDefault, serverURL);
//		dprintf ("CFBundleCreate() (serverBundle = %p)\n", serverBundle);
    }
    
    if (serverBundle != nil) {
        serverMainExecutable = CFBundleCopyExecutableURL (serverBundle);
//		dprintf ("CFBundleCopyExecutableURL() (serverMainExecutable = %p)\n", serverMainExecutable);
    }
    
    if (serverMainExecutable != nil) {
        serverMainExecutableAbsolute = CFURLCopyAbsoluteURL (serverMainExecutable);
//		dprintf ("CFURLCopyAbsoluteURL() (serverMainExecutableAbsolute = %p)\n", serverMainExecutableAbsolute);
    }
    
    if (serverMainExecutableAbsolute != nil) {
        serverPathCFString = CFURLCopyFileSystemPath (serverMainExecutableAbsolute, kCFURLPOSIXPathStyle);
//		dprintf ("CFURLCopyFileSystemPath() (serverPathCFString = %p)\n", serverPathCFString);
    }
    
    if (serverPathCFString != nil) {
        CFIndex	length = CFStringGetMaximumSizeForEncoding (CFStringGetLength (serverPathCFString), encoding);

        // This had better not be true anyway because we won't be able to pass it into exec:
        if (length < kMachIPCServerPathMaxLength) {
            CFStringGetCString (serverPathCFString, ioServerPath, kMachIPCServerPathMaxLength, encoding);
//            dprintf ("CFStringGetCString() (ioServerPath = %s)\n", ioServerPath);
            
            err = KERN_SUCCESS;
        } else {
            dprintf ("Illegal server path length.  Server path is longer than MAXPATHLEN.\n");
        }
    }
    
    if (bundleIdentifierCFString != nil) 
        CFRelease (bundleIdentifierCFString);
    if (pathCFString != nil)
        CFRelease (pathCFString);
    if (serverBundleNameCFString != nil)
        CFRelease (serverBundleNameCFString);
        
    if (bundleURL != nil) 
        CFRelease (bundleURL);
    if (pathURL != nil)
        CFRelease (pathURL);
    if (serverURL != nil)
        CFRelease (serverURL);
        
    if (serverBundle != nil)
        CFRelease (serverBundle);
    if (serverMainExecutable != nil)
        CFRelease (serverMainExecutable);
    if (serverMainExecutableAbsolute != nil)
        CFRelease (serverMainExecutableAbsolute);
        
    if (serverPathCFString != nil)
        CFRelease (serverPathCFString);
        
    return err;
}
