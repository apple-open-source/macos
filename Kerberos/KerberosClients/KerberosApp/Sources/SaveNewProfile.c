/*
 * SaveNewProfile.c
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
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "UNIXReadWrite.h"

int main (int argc, const char **argv)
{
    int err = 0;
    char *newFileName = NULL;
    char *oldFileName = NULL;
    pid_t pid = getpid ();
    
    dprintf ("starting up...");
    
    // Write my pid first so my parent can wait on me
    if (!err) {
        err = WriteBuffer (STDOUT_FILENO, (char *) &pid, sizeof (pid));
        dprintf ("Wrote pid %d (err = %d)", pid, err);
    }
    
    if (!err) {
        if (argc != 2) { err = EINVAL; }
    }
    
    if (!err) {
        newFileName = (char *) malloc (strlen (argv[1]) + 5 /* .new */);
        if (newFileName != NULL) { 
            sprintf (newFileName, "%s.new", argv[1]);
            dprintf ("newFileName is %s", newFileName);
        } else { 
            err = ENOMEM; 
        }
    }
    
    if (!err) {
        oldFileName = (char *) malloc (strlen (argv[1]) + 5 /* .old */);
        if (oldFileName != NULL) { 
            sprintf (oldFileName, "%s.old", argv[1]);
            dprintf ("oldFileName is %s", oldFileName);
        } else {
            err = ENOMEM; 
        }
    }
    
    if (!err) {
        int    newFD = -1;
        char  *fileData = NULL;
        size_t fileLength = 0;
        
        // Read the length of the file so the tool knows if it got the whole thing
        if (!err) {
            dprintf ("opened new file");
            err = ReadDynamicLengthBuffer (STDIN_FILENO, &fileData, &fileLength);
            dprintf ("read new file of length %ld (err = %d)", fileLength, err);
        }
                
        if (!err) {
            newFD = open (newFileName, (O_WRONLY | O_CREAT | O_TRUNC), 
                          (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
            if (newFD < 0) { err = errno; }
        }
        
        if (!err) {
            err = WriteBuffer (newFD, fileData, fileLength);
            dprintf ("wrote data to file (err = %d)", err);
        }
        
        if (newFD    >= 0   ) { close (newFD); }    
        if (fileData != NULL) { free (fileData); }
    }
    
    if (!err) {
        unlink (oldFileName);  // make sure this doesn't exist 
        
        dprintf ("copying new file into place");
        err = link (argv[1], oldFileName);
        if (!err || errno == ENOENT) { 
            // we are creating the file or have a hard link.  Safe to just rename.
            err = rename (newFileName, argv[1]);
            if (err) { err = errno; }
        } else {
            // filesystem doesn't support hard links.  Make a copy.
            err = rename (argv[1], oldFileName);
            if (err) { err = errno; }
            
            if (!err) {
                err = rename (newFileName, argv[1]);                
                if (err) { 
                    err = errno; 
                    rename (oldFileName, argv[1]);  // Try to restore
                }
            }
        }
    }
    
    if (newFileName != NULL) { unlink (newFileName); free (newFileName); }
    if (oldFileName != NULL) { free (oldFileName); }
    
    dprintf ("done (err = %d)", err);
    return err;
}
