/* $Copyright:
 *
 * Copyright © 2000 by the Massachusetts Institute of Technology.
 * 
 * All rights reserved.
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Furthermore if you modify
 * this software you must label your software as modified software and not
 * distribute it in such a fashion that it might be confused with the
 * original MIT software. M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Individual source code files are copyright MIT, Cygnus Support,
 * OpenVision, Oracle, Sun Soft, FundsXpress, and others.
 * 
 * Project Athena, Athena, Athena MUSE, Discuss, Hesiod, Kerberos, Moira,
 * and Zephyr are trademarks of the Massachusetts Institute of Technology
 * (MIT).  No commercial use of these trademarks may be made without prior
 * written permission of MIT.
 * 
 * "Commercial use" means use of a name in a product or other for-profit
 * manner.  It does NOT prevent a commercial firm from referring to the MIT
 * trademarks in order to convey information (although in doing so,
 * recognition of their trademark status should be given).
 * $
 */


/* 
 *
 * PreferenceLib.c -- Allows Kerberos based clients to discover and use Kerberos Preference files.
 *
 */
 
#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Kerberos/FSpUtils.h>

#define KRB5_PRIVATE 1    /* So we can include k5-int.h */
#include <k5-int.h>  /* for os_get_default_config_files */
#include <profile.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

#include <Kerberos/KerberosPreferences.h>

/* Preferences file should be readable by everyone and writable by the creator */
#define PREFERENCES_UMASK (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

#define kerberosPreferences_FileNameCStr	"edu.mit.Kerberos"

#pragma mark -

/* ********************************************************* */
/* Creates a valid preference file at the location specified */
/* ********************************************************* */
OSErr KPInitializeWithDefaultKerberosLibraryPreferences (const FSSpec* inPrefsFile)
{
    OSStatus err = noErr;
    CFBundleRef kerberosBundle = NULL;
	CFURLRef defaultPrefsURL = NULL;
	CFStringRef defaultPrefsString = NULL;
    char initPrefs[PATH_MAX];
    char defaultPrefs[PATH_MAX];
    int defaultFd = -1;
    int initFd = -1;
    
    // Find the Kerberos framework
    if (err == noErr) {
        kerberosBundle = CFBundleGetBundleWithIdentifier (CFSTR ("edu.mit.Kerberos"));
        if (kerberosBundle == NULL) { err = ENOMEM; }
    }

    // Get a URL to the default preferences
    if (err == noErr) {
        defaultPrefsURL = CFBundleCopyResourceURL (kerberosBundle, CFSTR (kerberosPreferences_FileNameCStr), NULL, NULL);
        if (defaultPrefsURL == NULL) { err = ENOMEM; }
	}

    // Get a path to the default preferences
    if (err == noErr) {
        defaultPrefsString = CFURLCopyFileSystemPath (defaultPrefsURL, kCFURLPOSIXPathStyle);
        if (defaultPrefsString == NULL) { err = ENOMEM; }
    }

    // Get a c string path to the default preferences
    if (err == noErr) {	
        Boolean converted = CFStringGetCString (defaultPrefsString, defaultPrefs, PATH_MAX, 
                                                CFStringGetSystemEncoding ());
        if (!converted) { err = ENOENT; }
    }
    
    // Convert the prefs to a path
    if (err == noErr) {
        err = FSSpecToPOSIXPath (inPrefsFile, initPrefs, PATH_MAX);
    }
    
    // Open the default preferences
    if (err == noErr) {
        defaultFd = open (defaultPrefs, O_RDONLY, 0);
        if (defaultFd < 0) { err = errno; }
    }
    
    // Open the new preferences
    if (err == noErr) {
        initFd = open (initPrefs, O_RDWR|O_CREAT|O_TRUNC, 0);
        if (initFd < 0) { err = errno; }
    }
    
    // Seek to the beginning of the defaults file:
    if (err == noErr) {
        err = lseek (defaultFd, 0, SEEK_SET);
    }

    // Seek to the beginning of the new preferences file:
    if (err == noErr) {
        err = lseek (initFd, 0, SEEK_SET);
    }

    // Copy the preferences from one file to the other
    if (err == noErr) {
        char buffer[BUFSIZ];
        
        while (true) {
            int bytesRead = read (initFd, buffer, BUFSIZ);
            if (bytesRead < 0) {
                if (errno == EINTR) {
                    continue;    // interrupted, retry
                } else {
                    err = errno; // real error
                    break;
                }
            }
            if (bytesRead == 0) {
                break; // EOF - we're done here
            }
            
            // Write out the data we just read
            int bytesWritten = 0;
            int bytesToWrite = bytesRead;
            
            while (bytesToWrite > 0) {
                int wrote = write (defaultFd, buffer + bytesWritten, bytesToWrite);
                if (wrote <= 0) {
                    if (errno == EINTR) {
                        continue;    // interrupted, retry
                    } else {
                        err = errno; // real error
                        break;
                    }
                } 
                
                bytesWritten += wrote;
                bytesToWrite -= wrote;
            }            
        }
        
    }
    
    // Close the files
    if (defaultFd >= 0) { close (defaultFd); }
    if (initFd >= 0)    { close (initFd); }
    
    // Clean up memory
    if (kerberosBundle != NULL)     { CFRelease (kerberosBundle); }
    if (defaultPrefsURL != NULL)    { CFRelease (defaultPrefsURL); }
    if (defaultPrefsString != NULL) { CFRelease (defaultPrefsString); }
    
    return err;
}

/* ****************************************************************************** */
/* File the array with list of preferences that match the options provided        */
/* ****************************************************************************** */
OSErr KPGetListOfPreferencesFiles (UInt32 inUserSystemFlags, FSSpecPtr* thePrefFiles, UInt32* outNumberOfFiles)
{
    OSStatus err = noErr;
    int secure = ((inUserSystemFlags & kpUserPreferences) == 0);;
    profile_filespec_t *pfiles = NULL;
    UInt32 tempNumberOfFiles = 0;
    FSSpecPtr tempPrefsFiles = NULL;
    UInt32 i;
    
    // Get the preference file paths
    if (err == noErr) {
        err = os_get_default_config_files (&pfiles, secure);
    }
    
    // Count the number of files
    if (err == noErr) {
        for (i = 0; pfiles[i] != NULL; i++) {
            tempNumberOfFiles++;
        }
    }
    
    // Allocate the list of preference files:
    if (err == noErr) {
        tempPrefsFiles = (FSSpecPtr) malloc (tempNumberOfFiles * sizeof (FSSpec));
        if (tempPrefsFiles == NULL) { err = ENOMEM; }
    }
    
    // Iterate over the preference files, translating them to FSSpecs:
    if (err == noErr) {
        for (i = 0; i < tempNumberOfFiles; i++) {
            err = POSIXPathToFSSpec (pfiles[i], &tempPrefsFiles[i]);
            if (err != noErr) { break; }
        }
    }

    // We're done!  Either pass the values back to the user or clean up memory
    if (err == noErr) {
        *thePrefFiles = tempPrefsFiles;
        *outNumberOfFiles = tempNumberOfFiles;
    } else {
        // Clean up on error
        if (tempPrefsFiles != NULL) { free (tempPrefsFiles); }
    }
    
    return err;
}

/* ********************************************************* */
/* Free the array containing the list of preference files    */
/* ********************************************************* */
void KPFreeListOfPreferencesFiles (FSSpecPtr thePrefFiles)
{
    free (thePrefFiles);
}

/* ********************************************************* */
/* Check if file exists and is readable                      */
/* ********************************************************* */
OSErr KPPreferencesFileIsReadable (const FSSpec* inPrefsFile)
{
    OSStatus err = noErr;
    char prefs[PATH_MAX];
    
    // Use path-based APIs for better remote filesystem support
    if (err == noErr) {
        err = FSSpecToPOSIXPath (inPrefsFile, prefs, PATH_MAX);
    }
    
    // Check to see if we can open the file for reading
    if (err == noErr) {
        int fd = open (prefs, O_RDONLY, PREFERENCES_UMASK);
        if (fd < 0) { 
            err = errno; 
        } else {
            close (fd);
        }
    }
    
    return err;
}

/* ********************************************************* */
/* Check if file is writable                                 */
/* ********************************************************* */
OSErr KPPreferencesFileIsWritable (const FSSpec* inPrefsFile)
{
    OSStatus err = noErr;
    char prefs[PATH_MAX];
    
    // Use path-based APIs for better remote filesystem support
    if (err == noErr) {
        err = FSSpecToPOSIXPath (inPrefsFile, prefs, PATH_MAX);
    }
    
    // Check to see if we can open the file for reading
    if (err == noErr) {
        int fd = open (prefs, O_RDWR, PREFERENCES_UMASK);
        if (fd < 0) { 
            err = errno; 
        } else {
            close (fd);
        }
    }
    
    // If the file doesn't exist, try to create it
    if (err == ENOENT) {
        int fd = open (prefs, O_RDWR | O_CREAT, PREFERENCES_UMASK);
        if (fd < 0) {
            err = errno;
        } else {
            close (fd);
            err = unlink (prefs);
            if (err != noErr) { err = errno; }
        }
    }
    
    return err;
}

/* ********************************************************* */
/* Create an empty file                                      */
/* ********************************************************* */
OSErr KPCreatePreferencesFile (const FSSpec* inPrefsFile)
{
    OSStatus err = noErr;
    char path[PATH_MAX];
    
    // Use path-based APIs for better remote filesystem support
    if (err == noErr) {
        err = FSSpecToPOSIXPath (inPrefsFile, path, PATH_MAX);
    }
    
    // Create the file or truncate it to zero length
    if (err == noErr) {
        int fd = open (path, O_RDWR|O_CREAT|O_TRUNC, PREFERENCES_UMASK);
        if (fd < 0) {
            err = errno;
        } else {
            close (fd);
        }
    }
    
    return err;
}
