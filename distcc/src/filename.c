/* -*- c-file-style: "java"; indent-tabs-mode: nil -*- 
 *
 * distcc -- A simple distributed compiler system
 * $Header: /cvs/karma/distcc/src/filename.c,v 1.1.1.1 2005/05/06 05:09:42 deatley Exp $ 
 *
 * Copyright (C) 2002 by Martin Pool <mbp@samba.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>

#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "exitcode.h"
#include "cpp_dialect.h"
#include "filename.h"


/*
 * NOTE: As of 0.10, .s and .S files are never distributed, because
 * they might contain '.include' pseudo-operations, which are resolved
 * by the assembler.
 */


/**
 * Return a pointer to the extension, including the dot, or NULL.
 **/
char * dcc_find_extension(char *sfile)
{
    char *dot;

    dot = strrchr(sfile, '.');
    if (dot == NULL || dot[1] == '\0') {
        /* make sure there's space for one more character after the
         * dot */
        return NULL;
    }
    return dot;
}



static int dcc_set_file_extension(const char *sfile,
                                  const char *new_ext,
                                  char **ofile)
{
    char *dot, *o;

    o = strdup(sfile);
    dot = dcc_find_extension((char *) o);
    if (!dot) {
        rs_log_error("couldn't find extension in \"%s\"", o);
        return -1;
    }
    if (strlen(dot) < strlen(new_ext)) {
        rs_log_error("not enough space for new extension");
        return -1;
    }
    strcpy(dot, new_ext);
    *ofile = o;

    return 0;
}


/**
 * If you preprocessed a file with extension @p e, what would you get?
 *
 * @param e original extension (e.g. ".c")
 *
 * @returns preprocessed extension, (e.g. ".i"), or NULL if
 * unrecognized.
 **/
const char * dcc_preproc_exten(const char *e)
{
    if (e[0] != '.')
        return NULL;
    e++;
    if (seen_opt_x) {
        return opt_x_ext;;
    }
    if (!strcmp(e, "i") || !strcmp(e, "c")) {
        return ".i";
    } else if (!strcmp(e, "c") || !strcmp(e, "cc")
               || !strcmp(e, "cpp") || !strcmp(e, "cxx")
               || !strcmp(e, "cp") || !strcmp(e, "c++")
               || !strcmp(e, "C") || !strcmp(e, "ii")) {
        return ".ii";
    } else if (!strcmp(e, "m") || !strcmp(e, "mi")) {
        return ".mi";
    } else if (!strcmp(e, "mm") || !strcmp(e, "M") || !strcmp(e, "mii")) {
        return ".mii";
    } else if (!strcasecmp(e, "s")) {
        return ".s";
    } else {
        return NULL;
    }        
}


int dcc_is_preprocessed(const char *sfile)
{
    const char *dot, *ext;
    dot = dcc_find_extension((char *) sfile);
    if (!dot)
        return 0;
    ext = dot+1;

    switch (ext[0]) {
#ifdef ENABLE_REMOTE_ASSEMBLE
    case 's':
        /* .S needs to be run through cpp; .s does not */
        return !strcmp(ext, "s");
#endif
    case 'i':
        return !strcmp(ext, "i")
            || !strcmp(ext, "ii");
    case 'm':
        return !strcmp(ext, "mi")
            || !strcmp(ext, "mii");
    default:
        return 0;
    }
}


/**
 * Work out whether @p sfile is source based on extension
 **/
int dcc_is_source(const char *sfile)
{
    const char *dot, *ext;
    dot = dcc_find_extension((char *) sfile);
    if (!dot)
        return 0;
    ext = dot+1;

    /* you could expand this out further into a RE-like set of case
     * statements, but i'm not sure it's that important. */

    switch (ext[0]) {
    case 'i':
        return !strcmp(ext, "i")
            || !strcmp(ext, "ii");
    case 'c':
        return !strcmp(ext, "c")
            || !strcmp(ext, "cc")
            || !strcmp(ext, "cpp")
            || !strcmp(ext, "cxx")
            || !strcmp(ext, "cp")
            || !strcmp(ext, "c++");
    case 'C':
        return !strcmp(ext, "C");
    case 'm':
        return !strcmp(ext, "m")
            || !strcmp(ext, "mi")
            || !strcmp(ext, "mii")
            || !strcmp(ext, "mm");
    case 'M':
        return !strcmp(ext, "M");
#ifdef ENABLE_REMOTE_ASSEMBLE
    case 's':
        return !strcmp(ext, "s");
    case 'S':
        return !strcmp(ext, "S");
#endif
    default:
        return 0;
    }

    /* XXX: Also .m is Objective-C, but I don't know if that will work
     * so for the moment we always do it locally. -- mbp */
}


/**
 * Work out the default object file name the compiler would use if -o
 * was not specified.  We don't need to worry about "a.out" because
 * we've already determined that -c or -S was specified.
 *
 * However, the compiler does put the output file in the current
 * directory even if the source file is elsewhere, so we need to strip
 * off all leading directories.
 *
 * @param sfile Source filename.  Assumed to match one of the
 * recognized patterns, otherwise bad things might happen.
 **/
int dcc_output_from_source(const char *sfile,
                           const char *out_extn,
                           char **ofile)
{
    char *slash;
    int l;
    
    if ((slash = strrchr(sfile, '/')))
        sfile = slash+1;
    if ((l = strlen(sfile)) < 3) {
        rs_log_error("source file %s is bogus", sfile);
        return -1;
    }

    return dcc_set_file_extension(sfile, out_extn, ofile);
}

