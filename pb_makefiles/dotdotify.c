/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <stdio.h>
#include <ctype.h>

#define YES 1
#define NO 0
#define BUFFERSIZE 20

int relative (const char* path)
{
    if (path[0] == '/') return NO;
    if (isalpha(path[0]) && path[1] == ':' && path[2] == '/') return NO;
    return YES;
}

void process (const char* path)
{
    if (path[0]=='-' && isupper (path[1]))
        {
        putc (path[0], stdout);
        putc (path[1], stdout);
        path += 2;
        }
    if (isalpha (path[0]) && path[1]==':')
        {
        putc (path[0], stdout);
        putc (path[1], stdout);
        path += 2;
        }
    if (path[0] == '.' && path[1] == '/') path += 2;
    if (relative (path)) fputs ("../", stdout);
    fputs (path, stdout);
}

int main (int argc, char* argv[])
{
    int i;

    if (argc == 2 && argv[0][0] == '-' && argv[0][1] == '\0')
        {
        char buffer[BUFFERSIZE];

        fgets (buffer, BUFFERSIZE-1, stdin);
        buffer[BUFFERSIZE] = 0;
        process (buffer);
        while (fgets (buffer, BUFFERSIZE-1, stdin))
            {
            buffer[BUFFERSIZE] = 0;
            fputs (buffer, stdout);
            }
        }
    else
        {
        for (i=1; i<argc; i++)
            {
            if (i > 1) putc (' ', stdout);
            process (argv[i]);
            }
        putc ('\n', stdout);
        }
        
    return 0;
}

