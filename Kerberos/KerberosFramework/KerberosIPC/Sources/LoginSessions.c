/*
 * LoginSessions.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosIPC/Sources/LoginSessions.c,v 1.17 2005/01/27 22:35:04 lxs Exp $
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

#include <Security/AuthSession.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Kerberos/KerberosDebug.h>
#include <Kerberos/LoginSessions.h>
#include <Kerberos/mach_client_utilities.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// ---------------------------------------------------------------------------

const char *LoginSessionGetSecuritySessionName (void)
{
    SecuritySessionId    sessionID;
    SessionAttributeBits attributes;
    int                  needed = 0;

    static char sessionName[kSecuritySessionStringMaxLength + 32];

    // From a session standpoint, group all the stuff under the login window together
    if (SessionGetInfo (callerSecuritySession, &sessionID, &attributes) == noErr) {
//        needed = snprintf (sessionName, sizeof (sessionName), "%ld", sessionID);

        needed = snprintf (sessionName, sizeof (sessionName), "%ld (%s%s%s%s%s)",
                           sessionID,
                           (attributes & sessionWasInitialized) ? "inited," : "",
                           (attributes & sessionIsRoot) ? "root," : "",
                           (attributes & sessionHasGraphicAccess) ? "gui," : "",
                           (attributes & sessionHasTTY) ? "tty," : "",
                           (attributes & sessionIsRemote) ? "remote" : "local");
    } else {
        needed = snprintf (sessionName, sizeof (sessionName), "No Session");
    }

    if (needed > (int) sizeof (sessionName)) {
        sessionName [sizeof (sessionName) - 1] = '\0';
        dprintf ("LoginSessionGetSecuritySessionName overflowed static buffer (%d > %d)\n",
                 needed, sizeof (sessionName));
    }

    return sessionName;
}

// ---------------------------------------------------------------------------

boolean_t LoginSessionIsRootSession (void)
{
    boolean_t            isRootSession = FALSE;
    SessionAttributeBits sattrs = 0L;    

    if ((SessionGetInfo (callerSecuritySession, NULL, &sattrs) == noErr) && (sattrs & sessionIsRoot)) {
        isRootSession = TRUE;
    }
    
    dprintf ("LoginSessionIsRootSession(): caller is%s running in the root session", isRootSession ? "" : " not");
    return isRootSession;
}

// ---------------------------------------------------------------------------

LoginSessionAttributes LoginSessionGetSessionAttributes (void)
{
    LoginSessionAttributes attributes = 0L;
    SessionAttributeBits   sattrs = 0L;    
    int                    fdIn = fileno (stdin);
    int                    fdOut = fileno (stdout);
    char                  *fdInName = ttyname (fdIn);
    
    if ((SessionGetInfo (callerSecuritySession, NULL, &sattrs) == noErr) && (sattrs & sessionHasGraphicAccess)) {
        dprintf ("LoginSessionGetSessionAttributes(): Session has graphic access.");
        attributes |= loginSessionHasGraphicsAccess;
        
        // Check for the HIToolbox (Carbon) or AppKit (Cocoa).  If either is loaded, we are a GUI app!
        CFBundleRef hiToolBoxBundle = CFBundleGetBundleWithIdentifier (CFSTR ("com.apple.HIToolbox"));
        if (hiToolBoxBundle != NULL && CFBundleIsExecutableLoaded (hiToolBoxBundle)) {
            dprintf ("LoginSessionGetSessionAttributes(): Carbon Toolbox is loaded.");
            attributes |= loginSessionCallerUsesGUI;
        }
        
        CFBundleRef appKitBundle = CFBundleGetBundleWithIdentifier (CFSTR ("com.apple.AppKit"));
        if (appKitBundle != NULL && CFBundleIsExecutableLoaded (appKitBundle)) {
            dprintf ("LoginSessionGetSessionAttributes(): AppKit is loaded.");
            attributes |= loginSessionCallerUsesGUI;
        }
    }
    
    // Session info isn't reliable for remote sessions.
    // Check manually for terminal access with file descriptors
    if (isatty (fdIn) && isatty (fdOut) && (fdInName != NULL)) {
        dprintf ("LoginSessionGetSessionAttributes(): Terminal '%s' of type '%s' exists.", fdInName, getenv ("TERM"));
        attributes |= loginSessionHasTerminalAccess;
    }

    dprintf ("LoginSessionGetSessionAttributes(): Attributes are %x", attributes);
    return attributes;
}

// ---------------------------------------------------------------------------

uid_t LoginSessionGetSessionUID (void)
{
    // Get the uid of the user that the server will be run and named for.
    uid_t uid = geteuid ();

    // Avoid root because the client can later go back to the real uid    
    if (uid == 0 /* root */) {
        dprintf ("LoginSessionGetSessionUID: geteuid returned UID %d, trying getuid...\n", uid);
        uid = getuid ();
    }

    return uid;
}
