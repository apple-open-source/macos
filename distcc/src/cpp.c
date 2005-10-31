/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 * $Header: /cvs/karma/distcc/src/cpp.c,v 1.1.1.1 2005/05/06 05:09:42 deatley Exp $ 
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

/**
 * @file
 *
 * Run the preprocessor.  Client-side only.
 **/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "distcc.h"
#include "trace.h"
#include "exitcode.h"
#include "util.h"
#include "strip.h"
#include "implicit.h"
#include "exec.h"
#include "tempfile.h"
#include "cpp.h"


/**
 * If the input filename is a plain source file rather than a
 * preprocessed source file, then preprocess it to a temporary file
 * and return the name in @p cpp_fname.
 *
 * The preprocessor may still be running when we return; you have to
 * wait for @p cpp_fid to exit before the output is complete.  This
 * allows us to overlap opening the TCP socket, which probably doesn't
 * use many cycles, with running the preprocessor.
 *
 * Rather than setting -o, we could capture stdout from cpp, remove the -o
 * option, add -E, and look at the results of that.  That might avoid problems
 * with compilers that have trouble with -o and -E.  This may include some
 * versions of gcc?
 **/
int dcc_cpp_maybe(char **argv, char *input_fname, char **cpp_fname,
		  pid_t *cpp_pid)
{
    char **cpp_argv;
    int ret;

    *cpp_pid = 0;
    
    if (dcc_is_preprocessed(input_fname)) {
        /* already preprocessed, great. */
        if (!(*cpp_fname = strdup(input_fname))) {
            rs_fatal("couldn't duplicate string");
        }
        return 0;
    }

    *cpp_fname = dcc_make_tmpnam("cppout", ".i");

    /* FIXME: Stripping -o fixes some, but not all problems with -MD.
     *
     * It does fix compilers that can't handle -E -o, such as Sun.
     *
     * It also avoids gcc-3.2 sending dependencies to the -o file with -MD -E.
     *
     * gcc-3.2 with -MD without -E determines the output file based on whether
     * a -o option is given.
     *
     * I think at the moment distcc may cause dependencies to be written to
     * the source directory, when plain gcc may write them to the output
     * directory.
     *
     * We could fix that by synthesizing a -MF option if one is needed and not
     * present, by reimplementing gcc's rules for deciding where to put it.
     *
     * However that has several problems: gcc-2.95 doesn't have a -MF option,
     * and in any case it puts the default .d file in the working directory
     * rather than the source or output directory.
     *
     * So this change allows you to get dependencies; but if you're not
     * building everything in a single directory you need to use -MF to set
     * the output location.
     *
     * As a workaround people might just specify a -MF option.
     */

    if ((ret = dcc_strip_dasho(argv, &cpp_argv))
        || (ret = dcc_set_action_opt(cpp_argv, "-E")))
        return ret;

    return dcc_spawn_child(cpp_argv, cpp_pid,
                           "/dev/null", *cpp_fname, NULL);
}


