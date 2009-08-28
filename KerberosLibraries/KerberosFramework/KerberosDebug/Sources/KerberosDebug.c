/*
 * KerberosDebug.c
 *
 * $Header$
 *
 * Copyright 2004 Massachusetts Institute of Technology.
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

#include <Kerberos/KerberosDebug.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/AuthSession.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <servers/bootstrap.h>
#include <mach/mach_error.h>
#include <pthread.h>
#include <asl.h>

static int dappendformat (char **io_string, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
static int dlog (aslclient client, const char *format, ...)          __attribute__ ((format (printf, 2, 3)));

#pragma mark -

// ---------------------------------------------------------------------------

static int dnewstring (char **out_string, const char *in_string)
{
    int err = 0;
    
    if (!in_string ) { err = EINVAL; }
    if (!out_string) { err = EINVAL; }
    
    if (!err) {
        *out_string = (char *) calloc (strlen (in_string) + 1, sizeof (char));
        if (!*out_string) { err = ENOMEM; }
    }
    
    if (!err) {
        strcpy (*out_string, in_string);
    }
    
    return err;    
}

// ---------------------------------------------------------------------------

static int dfreestring (char *io_string)
{
    int err = 0;
    
    if (!io_string) { err = EINVAL; }
    
    if (!err) {
        free (io_string);
    }
    
    return err;
}

// ---------------------------------------------------------------------------

static int dappendstring (char **io_string, const char *in_append_string)
{
    int err = 0;
    char *string = NULL;
    int new_length = 0;
    int old_length = 0;
    
    if (!in_append_string        ) { err = EINVAL; }
    if (!io_string || !*io_string) { err = EINVAL; }
    
    if (!err) {
        old_length = strlen (*io_string);
        new_length = old_length + strlen (in_append_string) + 1;
        
        string = (char *) calloc (new_length, sizeof (char));
        if (!string) { err = ENOMEM; }
    }
    
    if (!err) {
        memcpy (string, *io_string, old_length);
        strcpy (string + old_length, in_append_string);
        
        free (*io_string);
        *io_string = string;
    }
    
    return err;
}

// ---------------------------------------------------------------------------

static int dappendformat (char **io_string, const char *format, ...)
{
    int err = 0;
    char *string = NULL;
    
    if (!format                  ) { err = EINVAL; }
    if (!io_string || !*io_string) { err = EINVAL; }
    
    if (!err) {
        int count;
        va_list args;
        
        va_start (args, format);
        count = vasprintf (&string, format, args);
        if (count < 0) { err = ENOMEM; }
        va_end (args);        
    }
    
    if (!err)  {
        err = dappendstring (io_string, string);
    }

    if (string) { free (string); }
    
    return err;
}

#pragma mark -

// ---------------------------------------------------------------------------

static CFPropertyListRef dprefsvalue (CFStringRef in_key)
{
    CFPropertyListRef value = NULL;
    
    if (in_key) {
        // Only allow debug keys in kCFPreferencesAnyUser/kCFPreferencesCurrentHost 
        // so we don't let normal users expose debugging data
        
        if (!value) {
            value = CFPreferencesCopyValue (in_key, kCFPreferencesCurrentApplication,
                                            kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        }
        
        if (!value) {
            value = CFPreferencesCopyValue (in_key, CFSTR ("edu.mit.Kerberos"),
                                            kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        }
    }
    
    return value;    
}

// ---------------------------------------------------------------------------

static int dprefsstring (char **out_string, CFStringRef in_key)
{
    int               err = 0;
    CFStringRef       cfstring = NULL;
    CFStringEncoding  encoding = CFStringGetSystemEncoding ();
    CFIndex           length = 0;
    char             *string = NULL;

    if (!in_key    ) { err = EINVAL; }
    if (!out_string) { err = EINVAL; }

    if (!err) {
        cfstring = dprefsvalue (in_key);
        if (!cfstring || (CFGetTypeID (cfstring) != CFStringGetTypeID ())) { err = ENOENT; }
    }
    
    if (!err) {
        length = CFStringGetMaximumSizeForEncoding (CFStringGetLength (cfstring), encoding) + 1;
    }
    
    if (!err) {
        string = (char *) calloc (length, sizeof (char));
        if (!string) { err = ENOMEM; }
    }
    
    if (!err) {
        if (!CFStringGetCString (cfstring, string, length, encoding)) { err = ENOMEM; }
    }
    
    if (!err) {
        *out_string = string;
        string = NULL;
    }
    
    if (cfstring) { CFRelease (cfstring); }
    if (string  ) { free (string); }
    
    return err;
}

#pragma mark -

#define kLogFilePermMask (S_IXUSR | S_IRWXG | S_IRWXO)

// ---------------------------------------------------------------------------
// Note: for efficiency purposes, once s_initialized is TRUE, we don't use
// the mutex anymore.  We can only do this because s_initialized is never
// set to FALSE again.

static int dlevel (aslclient *client)
{
    static pthread_mutex_t s_debug_mutex = PTHREAD_MUTEX_INITIALIZER;
    static aslclient       s_aslclient = NULL;
    static int             s_debug_level = 0;      // Default to debugging is off
    static int             s_initialized = FALSE;  // s_debug not yet set
    
    if (!s_initialized && (pthread_mutex_lock (&s_debug_mutex) == 0)) {
        // Once we are locked, check s_initialized again because
        //  it may have changed while we were waiting for the lock
        if (!s_initialized) {
            s_aslclient = asl_open (NULL, NULL, 0L);  // currently required <rdar://3931658>

            if (s_aslclient) {
                CFStringRef cf_debug_level = NULL;
                
                cf_debug_level = dprefsvalue (CFSTR ("KerberosDebugLogLevel"));  ;
                if ((cf_debug_level) && (CFGetTypeID (cf_debug_level) == CFStringGetTypeID ())) {
                    s_debug_level = CFStringGetIntValue (cf_debug_level);  // returns 0 on failure
                }
                
                if (s_debug_level > 0) {
                    char *log_file = NULL;
                    
                    if (dprefsstring (&log_file, CFSTR ("KerberosDebugLogFile")) == 0) {
                        int fd = -1;
                        
                        mode_t saveMask = umask (kLogFilePermMask);
                        fd = open (log_file, O_CREAT | O_WRONLY | O_APPEND, (S_IRUSR | S_IWUSR));
                        umask (saveMask);
                        
                        if (fd >= 0) {
                            asl_add_log_file (s_aslclient, fd);  // Leave the file open
                        }            
                    }
                    
                    if (log_file) { free (log_file); }
                }
                
                if (s_debug_level > 0) {
                    asl_set_filter (s_aslclient, ASL_FILTER_MASK_UPTO (ASL_LEVEL_DEBUG));
                }

                if (cf_debug_level) { CFRelease (cf_debug_level); }
            }
            
            s_initialized = TRUE;  // If asl_open() fails, don't keep trying.          
        }
        
        pthread_mutex_unlock (&s_debug_mutex);
    }
    
    if ((client) && (s_debug_level > 0)) {
        *client = s_aslclient;
    }
    
    return s_debug_level;
}

#pragma mark -

// ---------------------------------------------------------------------------

static int dvlog (aslclient client, const char *format, va_list args)
{
    return asl_vlog (client, NULL, ASL_LEVEL_DEBUG, format, args);
}

// ---------------------------------------------------------------------------

static int dlog (aslclient client, const char *format, ...)
{
    int err = 0;
    va_list args;
    
    va_start (args, format);
    err = asl_vlog (client, NULL, ASL_LEVEL_DEBUG, format, args);
    va_end (args);
    
    return err;
}

#pragma mark -
#pragma mark -- API --

// ---------------------------------------------------------------------------

int ddebuglevel (void)
{
    return dlevel (NULL);
}

// ---------------------------------------------------------------------------

void dprintf (const char *format, ...)
{
    va_list args;
        
    va_start (args, format);
    dvprintf (format, args);
    va_end (args);        
}

// ---------------------------------------------------------------------------

void dvprintf (const char *format, va_list args)
{
    aslclient client = NULL;
    
    if (dlevel (&client) > 0) {
        dvlog (client, format, args);       
    }
}

// ---------------------------------------------------------------------------

void dprintmem (const void *inBuffer, size_t inLength)
{
    aslclient client = NULL;
    
    if (dlevel (&client) > 0) {
        int err = 0;
        
        if (!err) {
            err = dlog (client, "Buffer:");
        }
        
        if (!err) {
            const u_int8_t *buffer = inBuffer;
            size_t i;  
            
            // print 16 bytes per line
            for (i = 0; !err && (i < inLength); i += 16) {
                char *string = NULL;
                size_t l;
                
                err = dnewstring (&string, "");
                
                // print line in hex
                for (l = i; !err && (l < (i + 16)); l++) {
                    if (l >= inLength) {
                        err = dappendstring (&string, "  ");
                    } else {
                        err = dappendformat (&string, "%2.2x", *((u_int8_t *) buffer + l));
                    }
                    
                    if (!err && ((l % 4) == 3)) { 
                        err = dappendstring (&string, " ");
                    }
                }
                
                // space between hex and ascii
                if (!err) {
                    err = dappendstring (&string, "   ");
                }
                
                // print line in ascii
                for (l = i; !err && (l < (i + 16)); l++) {
                    if (l >= inLength) {
                        err = dappendstring (&string, " ");
                    } else {
                        u_int8_t c = ((buffer[l] > 0x1f) && (buffer[l] < 0x7f)) ? buffer[l] : '.';
                        err = dappendformat (&string, "%c", c);
                    }
                }
                
                if (!err) {
                    err = dlog (client, "%s", string);
                }

                if (string) { dfreestring (string); }
            }
        }
    }
}

// ---------------------------------------------------------------------------

void dprintsession (void)
{
    aslclient client = NULL;
    
    if (dlevel (&client) > 0) {
	OSStatus              err = noErr;
	SecuritySessionId     sessionID;
	SessionAttributeBits  attributes;
	
	if (!err) {
	    err = SessionGetInfo (callerSecuritySession, &sessionID, &attributes);
	}
	
	if (!err) {
	    dlog (client, "Security session is %ld (%s%s%s%s%s)", sessionID,
		  (attributes & sessionWasInitialized) ? "inited," : "",
		  (attributes & sessionIsRoot) ? "root," : "",
		  (attributes & sessionHasGraphicAccess) ? "gui," : "",
		  (attributes & sessionHasTTY) ? "tty," : "",
		  (attributes & sessionIsRemote) ? "remote" : "local");
	} else {
	    dlog (client, "SessionGetInfo() failed: %ld", err);
	}    
    }
}

