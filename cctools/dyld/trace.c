/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef NO_DYLD_TRACING
#include <stdio.h>
#include <strings.h>
#include "trace.h"

/*
 * Limit to size of pathname to be encoded.  This number is more or less
 * arbitrary and meant to fit as much of the path as possible without line
 * wrapping.  We can change this (probably a good idea), but beware that
 * performance tools rely on that behavior for VFS_USAGE.
 */
#define NUMPARMS 23

/*
 * trace_with_string() is passed the trace_type and a string.  It does the
 * syscall(180,...) for the trace and then a syscall(180,...) for the string.
 */
void
trace_with_string(
int trace_type,
char *name)
{
    int i, j, n;
    int namelen;
    int save_namelen;
    long parms[NUMPARMS];
    char buf[4];
    static char *filler;
    int string_identifier;

        filler = ">>>>";
	string_identifier = (SYSCALL_CODE(DYLD_CLASS,
			   		  DYLD_TRACE_STRING_SUBCLASS,
					  DYLD_TRACE_string_type) |
			     DBG_FUNC_NONE);

        /*
	 * If the name is invalid we can't do much, so print a generic trace
	 * message.
	 */
        if(name == NULL){
            syscall(180, trace_type, 0, 0, 0, 0, 0); 
            return;
        }

        /* Collect the pathname for tracing */
        namelen = strlen(name);
	/* fprintf(stderr, "name length: %d name %s: \n", namelen, name); */
        
        if(namelen > sizeof(parms))
            namelen = sizeof(parms);

        /* Remember the length of name. */
        save_namelen = namelen;

	/*
         * Store characters in longs.  Longs are the parameters we need to pass
         * to kernel debug functions, so take each 4 character chunk in name, 
         * treat it as a long and copy it into on of the parms which we will
	 * pass to the kernel.  Decrement namelen as we go until all character
	 * are stored. 
	 */
        i = 0;
        while(namelen > 0){
            if(namelen >= 4){
                parms[i++] = *(long *)name;
                name += sizeof(long);
                namelen -= sizeof(long);
            } 
	    /*
             * buf is here to pick up the last few characters when there are no
             * more 4 byte chunks.
	     */
            else{
                for(n = 0; n < namelen; n++)
                    buf[n] = *name++;
		/*
                 * All characters have been collected.  Now fill the rest of
		 * buff with '>'s if there are more left in the path, or with			 * nulls if the path has been entirely copied.
		 */
                while(n <= 3){
                    if(*name != '\0')
                        buf[n++] = '>';
                    else
                        buf[n++] = 0;
                }
                
                /* Finally, place buf in a parm. */
                parms[i++] = *(long *)&buf[0];

                break;
            }
        }

	/* 
         * Done reading 23 ints of the path into parms.  If we have reached the
	 * end of the path, put zeros in the rest of the parms.  I dunno how we
	 * could reach the first case, where we didn't fit the whole path, but
	 * somehow want to put filler in at the end anyway???
	 */
        while(i < NUMPARMS){
            if(*name != '\0')
                parms[i++] = *(long *)filler;
            else
                parms[i++] = 0;
        }

	/* 
         * String class identifier in arg4 indicates to the trace tool that its 
	 * arguments should be printed as a string.
	 */
        i = 0;
        syscall(180, trace_type, parms[i], parms[i+1], parms[i+2],
		string_identifier, 0);
	i = i + 3;
        /* Finally, loop writing the parms to the trace buffer. */
        for(namelen = save_namelen - 3 * (sizeof(long));
            namelen > 0;
            namelen -=(3 * sizeof(long))){
            /* fprintf(stderr, "trace type: |%d|.\n", trace_type); */
	    j = i;
            syscall(180, string_identifier, parms[j], parms[j+1], parms[j+2],
		    0, 0);
	    i = i + 3;
	}
}
#endif /* defined(NO_DYLD_TRACING) */
