/*
 *  KerberosDebug.cp
 *
 *  DCon replacement for Mac OS X, sends to /tmp/Kerberos.log
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <Kerberos/KerberosDebug.h>
#include <CoreFoundation/CoreFoundation.h>

// Disallow execute:
#define kLogFilePermMask (S_IXUSR | S_IXGRP | S_IXOTH)

#if MACDEV_DEBUG
static FILE* KerberosDebugConsoleOutputFile (void)
{
    static FILE* sOutput = NULL;
    
    if (sOutput == NULL) {
        mode_t saveMask = umask (kLogFilePermMask);
        
        sOutput = fopen ("/tmp/Kerberos.log", "a+");

        umask (saveMask);
    }
    
    return sOutput;
}

static const char *KerberosDebugProgramName (void)
{
    static char* sLabel = NULL;
    
    if (sLabel == NULL) {
        // Get the main bundle (should not be released)
        CFBundleRef mainBundle = CFBundleGetMainBundle ();
        if (mainBundle != NULL) {
            CFURLRef mainURL = CFBundleCopyBundleURL (mainBundle);
            if (mainURL != NULL) {
                CFStringRef mainName = CFURLCopyLastPathComponent (mainURL);
                if (mainName != NULL) {
                    CFStringEncoding encoding = CFStringGetSystemEncoding ();
                    CFIndex mainNameLength = CFStringGetMaximumSizeForEncoding (CFStringGetLength (mainName),
                                                                                encoding) + 1;
                    
                    sLabel = (char *) calloc (mainNameLength, sizeof (char));
                    if (sLabel != NULL) {
                        CFStringGetCString (mainName, sLabel, mainNameLength, encoding);
                    }
                    CFRelease (mainName);
                }
                CFRelease (mainURL);
            }
        }
    }
     
    return (sLabel == NULL) ? "????" : sLabel;
}
#endif

//void dopen(const char *log);
void dprintf(const char *format, ...)
{
#if MACDEV_DEBUG
    FILE* output = KerberosDebugConsoleOutputFile ();
    if (output != NULL) {
        const time_t now = time(0);
        va_list	args;
        
        va_start (args, format);
        fprintf (output, "%.24s %s: ", ctime (&now), KerberosDebugProgramName ());
        vfprintf (output, format, args);
        va_end (args);

        fflush (output);
    }
#endif
}

void dprintmem(const void *data, size_t len)
{
#if MACDEV_DEBUG
    FILE* output = KerberosDebugConsoleOutputFile ();
    
    if (output != NULL) {
        vm_offset_t start = ((vm_offset_t) data) % 16;
        vm_offset_t end = ((vm_offset_t) data + len + 15) % 16;
        vm_offset_t i;
        
        for (i = start; i < end; i++) {
            if (i % 16 == 0) { fprintf (output, "%8X", (vm_offset_t) data + i); }
            if (i % 8  == 0) { fprintf (output, " ");                           }
            if (i % 2  == 0) { fprintf (output, " ");                           }
            
            fprintf (output, "%2X", * (((char*) data) + i));
            
            if (i % 16 == 15) { fprintf (output, "\n"); }
        }

        fflush (output);
    }
#endif
}    
