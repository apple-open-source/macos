/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
 * 
 * distcc -- A simple distributed compiler system
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


                /* "I do not trouble myself to be understood. I see
                 * that the elementary laws never apologize."
                 *         -- Whitman, "Song of Myself".             */



#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#include "distcc.h"
#include "trace.h"
#include "exitcode.h"
#include "util.h"
#include "implicit.h"


/**
 * @file
 *
 * Handle invocations where the compiler name is implied rather than
 * specified.
 *
 * This is used on the client only.  The compiler name is always passed (as
 * argv[0]) to the server.
 *
 * The current implementation determines that no compiler name has been
 * specified by checking whether the first argument is either an option, or a
 * source file name.  If not, it is assumed to be the name of the compiler to
 * use.
 *
 * At the moment the compiler name is always "cc", but this could change to
 * come from an environment variable.
 *
 * This would also allow installing distcc under the name "cc", and then
 * pointing DISTCC_CC=gcc.real or some such.  When this is done, we perhaps
 * ought to set an environment variable to prevent inadvertent infinite
 * recursion.
 *
 * The main shortcoming of that approach is that it would means all
 * invocations of distcc from the same Makefile probably have to end
 * up calling the same compiler.  For example, if both $(CC) and
 * $(CXX) are set to "distcc", then they can't be different remotely.
 * On the other hand, since "gcc" is our main target and it can handle
 * both C and C++ this is not important.  Using several different
 * compilers would be a bit strange.
 *
 * An alternative approach would be to examine our argv[0], and if
 * it's not "distcc" then conclude we are being implicitly invoked.
 * This would allow argv[0] to be used as the real compiler name.
 * There are a few disadvantages, though:
 *
 * <ol><li>Examining argv[1] is apparently unambiguous and simpler.
 *
 * <li>argv[0] tricks can be confusing and are deprecated by the GNU
 * Standards.
 *
 * <li>Requiring distcc to appear under this name on the correct PATH
 * may make installation more complex: people will need to get their
 * PATH set correctly, etc.
 *
 * </ol>
 **/


/*
 * FIXME: Handle "distcc hello.o -o hello".  How are we meant to work
 * this one out?  By seeing that the first argument is a .o file?
 * Mozilla does this.
 *
 * FIXME: Some people depend on calling the compiler "c++" or "g++",
 * because they have c++ source called foo.c.  
 */

/**
 * If we're invoked with no compiler name, insert one.  Either use an
 * environment variable, or "cc" by default.  You can tell there's no
 * compiler name because argv[1] will be either a source filename or
 * an option.  I don't think anything else is possible.
 **/
int dcc_find_compiler(char **argv, char ***out_argv)
{
    if (argv[1][0] == '-'
        || dcc_is_source(argv[1])) {
        dcc_shallowcopy_argv(argv, out_argv, 0);
        /* FIXME: Allow the command name to be specified by a
         * client-side environment variable */

        /* change "distcc -c foo.c" -> "cc -c foo.c" */
        (*out_argv)[0] = strdup("cc");
        return 0;
    } else {
        /* skip "distcc", point to "gcc -c foo.c"  */
        *out_argv = argv+1;
        return 0;
    }
}
