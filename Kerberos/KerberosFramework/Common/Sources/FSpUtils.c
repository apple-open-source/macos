#include <CoreFoundation/CoreFoundation.h>
#include <libgen.h>
#include <limits.h>

#include <Kerberos/KerberosDebug.h>
#include <Kerberos/com_err.h>

#include "FSpUtils.h"

/*
 * Why this sucks:
 * 
 * FSSpecs have the property that they can point to a non-existent file.
 * The parent directory of the file needs to exist, but not the file itself.
 *
 * FSRefs must point to a file or directory that exists.
 *
 * No component of a path need exist.
 *
 * In order to get from an FSSpec to a path or vice versa, you need to
 * pass through an FSRef.  If the file doesn't exist, you need to break it
 * into a parent directory and a file name and get an FSRef for the parent.
 * Woot.
 */
     
OSStatus FSSpecToPOSIXPath (const FSSpec *inSpec, char *ioPath, unsigned long inPathLength)
{
    OSStatus err = noErr;
    FSRef ref;
    FSSpec spec;
    CFStringRef nameString = NULL;
    CFStringRef pathString = NULL;
    CFURLRef pathURL = NULL;
    CFURLRef parentURL = NULL;
    int i;
    
    dprintf ("FSSpecToPOSIXPath called on volID %d, parID %d and name '", inSpec->vRefNum, inSpec->parID);
    for (i = 0; i < inSpec->name[0]; i++) { dprintf ("%c", inSpec->name[i+1]); }
    dprintf ("'\n");

    // First, try to create an FSRef for the FSSpec
    if (err == noErr) {
        err = FSpMakeFSRef (inSpec, &ref);
    }
    
    if (err == noErr) {
        // It's a directory or a file that exists; convert directly into a path
        err = FSRefMakePath (&ref, (UInt8 *)ioPath, inPathLength);
    } else {
        // The suck case.  It's a file that doesn't exist.
        err = noErr;
    	
        // Build an FSSpec for the parent directory, which must exist
        if (err == noErr) {
            Str31 name;
            name[0] = 0;
            
            err = FSMakeFSSpec (inSpec->vRefNum, inSpec->parID, name, &spec);
        }
    
        // Build an FSRef for the parent directory
        if (err == noErr) {
            err = FSpMakeFSRef (&spec, &ref);
        }
    
        // Now make a CFURL for the parent
        if (err == noErr) {
            parentURL = CFURLCreateFromFSRef(CFAllocatorGetDefault (), &ref);
            if (parentURL == NULL) { err = memFullErr; }
        }
    
        if (err == noErr) {
            nameString = CFStringCreateWithPascalString (CFAllocatorGetDefault (), inSpec->name, 
                                                        kCFStringEncodingMacRoman);
            if (nameString == NULL) { err = memFullErr; }
        }
    
        // Now we just add the filename back onto the path
        if (err == noErr) {
            pathURL = CFURLCreateCopyAppendingPathComponent (CFAllocatorGetDefault (), 
                                                            parentURL, nameString, 
                                                            false /* Not a directory */);
            if (pathURL == NULL) { err = memFullErr; }
        }
    
        if (err == noErr) {
            pathString = CFURLCopyFileSystemPath (pathURL, kCFURLPOSIXPathStyle);
            if (pathString == NULL) { err = memFullErr; }
        }
    
        if (err == noErr) {	
            Boolean converted = CFStringGetCString (pathString, ioPath, inPathLength, CFStringGetSystemEncoding ());
            if (!converted) { err = fnfErr; }
        }
    }    
    
    // Free allocated memory
    if (parentURL != NULL)  { CFRelease (parentURL);  }
    if (nameString != NULL) { CFRelease (nameString); }
    if (pathURL != NULL)    { CFRelease (pathURL);    }
    if (pathString != NULL) { CFRelease (pathString); }
    
    if (err == noErr) {
        dprintf ("FSSpecToPOSIXPath returned path '%s'\n", ioPath);
    } else {
        dprintf ("FSSpecToPOSIXPath returned error %d (%s)\n", err, error_message (err));
    }
    
    return err;
}

OSStatus POSIXPathToFSSpec (const char *inPath, FSSpec *outSpec)
{
    OSStatus err = noErr;
    FSRef ref;
    Boolean isDirectory;
    FSCatalogInfo info;
    CFStringRef pathString = NULL;
    CFURLRef pathURL = NULL;
    CFURLRef parentURL = NULL;
    CFStringRef nameString = NULL;
    
    dprintf ("FSSpecToPOSIXPath called on '%s'\n", inPath);

    // First, try to create an FSRef for the full path 
    if (err == noErr) {
        err = FSPathMakeRef ((UInt8 *) inPath, &ref, &isDirectory);
    }
    
    if (err == noErr) {
        // It's a directory or a file that exists; convert directly into an FSSpec:
        err = FSGetCatalogInfo (&ref, kFSCatInfoNone, NULL, NULL, outSpec, NULL);
    } else {
        // The suck case.  The file doesn't exist.
        err = noErr;
    
        // Get a CFString for the path
        if (err == noErr) {
            pathString = CFStringCreateWithCString (CFAllocatorGetDefault (), inPath, CFStringGetSystemEncoding ());
            if (pathString == NULL) { err = memFullErr; }
        }
    
        // Get a CFURL for the path
        if (err == noErr) {
            pathURL = CFURLCreateWithFileSystemPath (CFAllocatorGetDefault (), 
                                                    pathString, kCFURLPOSIXPathStyle, 
                                                    false /* Not a directory */);
            if (pathURL == NULL) { err = memFullErr; }
        }
        
        // Get a CFURL for the parent
        if (err == noErr) {
            parentURL = CFURLCreateCopyDeletingLastPathComponent (CFAllocatorGetDefault (), pathURL);
            if (parentURL == NULL) { err = memFullErr; }
        }
        
        // Build an FSRef for the parent directory, which must be valid to make an FSSpec
        if (err == noErr) {
            Boolean converted = CFURLGetFSRef (parentURL, &ref);
            if (!converted) { err = fnfErr; } 
        }
        
        // Get the node ID of the parent directory
        if (err == noErr) {
            err = FSGetCatalogInfo(&ref, kFSCatInfoNodeFlags|kFSCatInfoNodeID, &info, NULL, outSpec, NULL);
        }
        
        // Get a CFString for the file name
        if (err == noErr) {
            nameString = CFURLCopyLastPathComponent (pathURL);
            if (nameString == NULL) { err = memFullErr; }
        }
        
        // Copy the string into the FSSpec
        if (err == noErr) {	
            Boolean converted = CFStringGetPascalString (pathString, outSpec->name, sizeof (outSpec->name), 
                                                        CFStringGetSystemEncoding ());
            if (!converted) { err = fnfErr; }
        }
    
        // Set the node ID in the FSSpec
        if (err == noErr) {
            outSpec->parID = info.nodeID;
        }
    }
    
    if (err == noErr) {
        int i;
        
        dprintf ("POSIXPathToFSSpec returned volID %d, parID %d and name '", outSpec->vRefNum, outSpec->parID);
        for (i = 0; i < outSpec->name[0]; i++) { dprintf ("%c", outSpec->name[i+1]); }
        dprintf ("'\n");
    } else {
        dprintf ("POSIXPathToFSSpec returned error %d (%s)\n", err, error_message (err));
    }
    
    // Free allocated memory
    if (pathURL != NULL)    { CFRelease (pathURL);    }
    if (pathString != NULL) { CFRelease (pathString); }
    if (parentURL != NULL)  { CFRelease (parentURL);  }
    if (nameString != NULL) { CFRelease (nameString); }
    
    return err;
}
