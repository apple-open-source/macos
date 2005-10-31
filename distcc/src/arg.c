/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*- 
 *
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003 by Martin Pool <mbp@samba.org>
 * Copyright (C)2005 by Apple Computer, Inc.
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


                /* "I have a bone to pick, and a few to break." */

/**
 * @file
 *
 * Functions for understanding and manipulating argument vectors.
 *
 * @todo We don't need to run the full argument scanner on the
 * server, only something simple to recognize input and output files.
 * That would perhaps make the function simpler, and also mean that if
 * argument recognizer bugs are fixed in the future, they only need to
 * be fixed on the client, not on the server.
 *
 * @todo Detect arguments which ask the assembler to produce a
 * listing in addition to an object file.  These must be run locally.
 * This may be complex, because it will look like -Wa,-alh or
 * something.  We need to see -Wa and then recursive
 *
 * @todo Perhaps make the argument parser driven by a data table.
 * (Would that actually be clearer?)  Perhaps use regexps to recognize
 * strings.
 **/


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "util.h"
#include "exitcode.h"
#include "cpp_dialect.h"


static int dcc_argv_append(char *argv[], char *toadd)
{
    int l = dcc_argv_len(argv);
    argv[l] = toadd;
    argv[l+1] = NULL;           /* just make sure */
    return 0;
}


int dcc_trace_argv(const char *message, char *argv[])
{
    if (rs_trace_enabled()) {
        char *astr;
        astr = dcc_argv_tostr(argv);
        rs_trace("%s: %s", message, astr);
        free(astr);
    }
    return 0;
}


/**
 * Parse arguments, extract ones we care about, and also work out
 * whether it will be possible to distribute this invocation remotely.
 *
 * This is a little hard because the cc argument rules are pretty
 * complex, but the function still ought to be simpler than it already
 * is.
 *
 * The few options explicitly handled by the client are processed in its
 * main().  At the moment, this is just --help and --version, so this function
 * never has to worry about them.
 *
 * We recognize two basic forms "distcc gcc ..." and "distcc ...", with no
 * explicit compiler name.  This second one is used if you have a Makefile
 * that can't manage two-word values for $CC; eventually it might support
 * putting a link to distcc on your path as 'gcc'.  We call this second one an
 * implicit compiler.
 *
 * We need to distinguish the two by working out whether the first argument
 * "looks like" a compiler name or not.  I think the two cases in which we
 * should assume it's implicit are "distcc -c hello.c" (starts with a hypen),
 * and "distcc hello.c" (starts with a source filename.)
 *
 * In the case of implicit compilation "distcc --help" will always give you
 * distcc's help, not gcc's, and similarly for --version.  I don't see much
 * that we can do about that.
 *
 * XXX: It might be nice to split this up into several parsing runs.
 * However, that would possibly break handling of crazy command lines
 * like "gcc -o -o foo.c".
 *
 * @todo To fully support implicit compilers, we ought to call the child with
 * an environment variable set to protect against infinite (>1) recursion.
 *
 * This code is called on both the client and the server, though they use the
 * results differently.  So it does not directly cause any actions, but only
 * updates fields in @p jobinfo.
 *
 * @retval 0 if it's ok to distribute this compilation.
 *
 * @retval -1 if it would be unsafe to distribute this compilation.
 *
 * @todo Fix this to use standard return codes.
 **/
int dcc_scan_args(char *argv[], char **input_file, char **output_file,
                  char ***ret_newargv)
{
    int seen_opt_c = 0, seen_opt_s = 0;
    int i;
    char *a;
    int ret;

     /* allow for -o foo.o */
    if ((ret = dcc_shallowcopy_argv(argv, ret_newargv, 2)) != 0)
        return ret;
    argv = *ret_newargv;

    dcc_trace_argv("scanning arguments", argv);

    /* Things like "distcc -c hello.c" with an implied compiler are
     * handled earlier on by inserting a compiler name.  At this
     * point, argv[0] should always be a compiler name. */
    if (argv[0][0] == '-') {
        rs_log_error("unrecognized distcc option: %s", argv[0]);
        exit(EXIT_BAD_ARGUMENTS);
    }

    *input_file = *output_file = NULL;

    for (i = 0; (a = argv[i]); i++) {
        if (a[0] == '-') {
            if (!strcmp(a, "-E")) {
                rs_trace("-E call for cpp must be local");
                return -1;
            } else if (!strcmp(a, "-MD") || !strcmp(a, "-MMD") || 
                       !strcmp(a, "-MG") || !strcmp(a, "-MP")) {
                /* -MD does *not* imply -E.  It will locally produce
                    foo.d, and then we can remotely compile foo.o */
                ;
            } else if (!strcmp(a, "-MF") || !strcmp(a, "-MT") || 
                       !strcmp(a, "-MQ")) {
                /* as above but with extra argument */
                i++;
            } else if (a[1] == 'M') {
                /* -M(anything else) causes the preprocessor to
                    produce a list of make-style dependencies on
                    header files, either to stdout or to a local file.
                    It implies -E, so only the preprocessor is run,
                    not the compiler.  There would be no point trying
                    to distribute it even if we could. */
                rs_trace("%s implies -E (maybe) and must be local", a);
                return -1;
            } else if (!strcmp(a, "-S")) {
                seen_opt_s = 1;
            } else if (!strcmp(a, "-fprofile-arcs")
                       || !strcmp(a, "-ftest-coverage")) {
                rs_log_info("compiler will emit profile info; must be local");
                return -1;
            } else if (!strcmp(a, "-x")) {
                /* TODO: We could also detect options like "-x
                 * cpp-output" or "-x assembler-with-cpp", because they
                 * should override language detection based on
                 * extension.  I haven't seen anyone use them yet
                 * though. */

                a = argv[++i];      /* get argument for -x */
                char *ext;
                if (ext = ext_lookup(a)) {
                    opt_x_ext = ext;
                    seen_opt_x = 1; /* if it's something we understand, keep parsing */
                } else {
                    rs_log_info("gcc's -x handling is complex; running locally");
                    return -1;
                }
            } else if (!strcmp(a, "-c")) {
                seen_opt_c = 1;
            } else if (!strcmp(a, "-o")) {
                /* Whatever follows must be the output */
                a = argv[++i];
                goto GOT_OUTPUT;
            } else if (str_startswith("-o", a)) {
                a += 2;         /* skip "-o" */
                goto GOT_OUTPUT;
            }
        } else {
            if (dcc_is_source(a)) {
                rs_trace("found input file \"%s\"", a);
                if (*input_file) {
                    rs_log_info("do we have two inputs?  i give up");
                    return -1;
                }
                *input_file = a;
            } else if (str_endswith(".o", a)) {
              GOT_OUTPUT:
                rs_trace("found object/output file \"%s\"", a);
                if (*output_file) {
                    rs_log_info("called for link?  i give up");
                    return -1;
                }
                *output_file = a;
            }
        }
    }

    /* TODO: ccache has the heuristic of ignoring arguments that do
     * not exist when looking for the input file; that's possibly
     * worthwile.  Of course we can't do that on the server. */

    if (!seen_opt_c && !seen_opt_s) {
        rs_log_info("compiler apparently called not for compile");
        return -1;
    }

    if (!*input_file) {
        rs_log_info("no visible input file");
        return -1;
    }

    if (!*output_file) {
        /* This is a commandline like "gcc -c hello.c".  They want
         * hello.o, but they don't say so.  For example, the Ethereal
         * makefile does this. 
         *
         * Note: this doesn't handle a.out, the other implied
         * filename, but that doesn't matter because it would already
         * be excluded by not having -c or -S.
         */
        char *ofile;

        /* -S takes precedence over -c, because it means "stop after
         * preprocessing" rather than "stop after compilation." */
        if (seen_opt_s) {
            if (dcc_output_from_source(*input_file, ".s", &ofile))
                return -1; 
        } else if (seen_opt_c) {
            if (dcc_output_from_source(*input_file, ".o", &ofile))
                return -1;
        } else {
            rs_log_crit("this can't be happening(%d)!", __LINE__);
            return -1;
        }
        rs_log_info("no visible output file, going to add \"-o %s\" at end",
                      ofile);
        dcc_argv_append(argv, strdup("-o"));
        dcc_argv_append(argv, ofile);
        *output_file = ofile;
    }

    rs_log(RS_LOG_INFO|RS_LOG_NONAME, "compile from %s to %s", *input_file, *output_file);

    if (strcmp(*output_file, "-") == 0) {
        /* Different compilers may treat "-o -" as either "write to
         * stdout", or "write to a file called '-'".  We can't know,
         * so we just always run it locally.  Hopefully this is a
         * pretty rare case. */
        rs_log_info("output to stdout?  running locally");
        return -1;
    }

    return 0;
}


int
dcc_argv_len (char **a)
{
  int i;

  for (i = 0; a[i]; i++)
    ;
  return i;
}

/**
 * Adds @p delta extra NULL arguments, to allow for adding more
 * arguments later.
 **/
int dcc_shallowcopy_argv(char **from, /*@-nullstate@*/ char ***out, int delta)
{
    /*@null@*/ char **b;
    int l, i;

    assert(out != NULL);
    assert(from != NULL);

    l = dcc_argv_len(from);
    b = malloc((l+1+delta) * (sizeof from[0]));
    if (b == NULL) {
        rs_log_error("failed to allocate copy of argv");
        exit(EXIT_FAILURE);
    }
    for (i = 0; i < l; i++) {
        b[i] = from[i];
    }
    b[l] = NULL;
    
    *out = b;

    return 0;
}


/**
 * Adds @p delta extra NULL arguments, to allow for adding more
 * arguments later.
 **/
int dcc_deepcopy_argv(char **from, /*@-nullstate@*/ char ***out)
{
    char **b;
    int i, l;

    l = dcc_argv_len(from);
    
    assert(out != NULL);
    assert(from != NULL);

    l = dcc_argv_len(from);
    b = malloc((l+1) * (sizeof from[0]));
    if (b == NULL) {
        rs_log_error("failed to allocate copy of argv");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < l; i++) 
        b[i] = strdup(from[i]);
    b[l] = NULL;

    *out = b;

    return 0;
}


/**
 * Used to change "-c" or "-S" to "-E", so that we get preprocessed
 * source.
 **/
int dcc_set_action_opt(char **a, const char *new_c)
{
    int gotone = 0;
    
    for (; *a; a++) 
        if (!strcmp(*a, "-c") || !strcmp(*a, "-S")) {
            *a = strdup(new_c);
            if (*a == NULL) {
                rs_log_error("strdup failed");
                exit(EXIT_FAILURE);
            }
            gotone = 1;
            /* keep going; it's not impossible they wrote "gcc -c -c
             * -c hello.c" */
        }

    if (!gotone) {
        rs_log_error("failed to find -c or -S");
        return -1;
    } else {
        return 0;
    }
}



/**
 * Change object file or suffix of -o to @p ofname
 *
 * It's crucially important that in every case where an output file is
 * detected by dcc_scan_args(), it's also correctly identified here.
 * It might be better to make the code shared.
 *
 * XXX: The -MD and -MMD options look at the output filename to work
 * out where to put the dependencies.  If we move it into the
 * temporary directory, then the .d file will end up in the wrong
 * place.  (This behaviour is new in gcc-3.2; 2.95 bases the name on
 * the source file.)  However, some versions of gcc-3.2 cannot produce
 * dependencies when preprocessing to an output file, so that is no
 * good anyhow.
 **/
int dcc_set_output(char **a, char *ofname)
{
    int i;
    
    for (i = 0; a[i]; i++) 
        if (0==strcmp(a[i], "-o")  &&  a[i+1] != NULL) {
            rs_trace("changed output from \"%s\" to \"%s\"", a[i+1], ofname);
            a[i+1] = ofname;
            dcc_trace_argv("command after", a);
            return 0;
        }
    
    /* TODO: Handle -ofoo.o.  gcc supports it, though it seems to be
     * rarely used.  At the moment we probably just fail to distribute
     * these commands. */

    rs_log_error("failed to find \"-o\"");
    return -1;
}


/**
 * Change input file to @p ifname; called on compiler.
 *
 * @todo Unify this with dcc_scan_args
 *
 * @todo Test this by making sure that when the modified arguments are
 * run through scan_args, the new ifname is identified as the input.
 **/
int dcc_set_input(char **a, char *ifname)
{
    int i;

    for (i =0; a[i]; i++)
        if (dcc_is_source(a[i])) {
            rs_trace("changed input from \"%s\" to \"%s\"", a[i], ifname);
            a[i] = ifname;
            dcc_trace_argv("command after", a);
            return 0;
        }
    
    rs_log_error("failed to find input file");
    return -1;
}


/**
 * Convert an argv array to printable form for debugging output.
 *
 * @note The result is not necessarily properly quoted for passing to
 * shells.
 *
 * @return newly-allocated string containing representation of
 * arguments.
 **/
char *dcc_argv_tostr(char **a)
{
    int l, i;
    char *s, *ss;
    
    /* calculate total length */
    for (l = 0, i = 0; a[i]; i++) {
        l += strlen(a[i]) + 3;  /* two quotes and space */
    }

    ss = s = malloc((size_t) l + 1);
    if (!s) {
        rs_log_crit("failed to allocate %d bytes", l+1);
        exit(EXIT_OUT_OF_MEMORY);
    }
    
    for (i = 0; a[i]; i++) {
        /* kind of half-assed quoting; won't handle strings containing
         * quotes properly, but good enough for debug messages for the
         * moment. */
        int needs_quotes = (strpbrk(a[i], " \t\n\"\';") != NULL);
        if (i)
            *ss++ = ' ';
        if (needs_quotes)
            *ss++ = '"';
        strcpy(ss, a[i]);
        ss += strlen(a[i]);
        if (needs_quotes)
            *ss++ = '"';
    }
    *ss = '\0';

    return s;
}


static char **_dcc_allowed_compilers(void) {
    static char **allowedCompilers = NULL;
    static char *allowedCompilersStrings = NULL;
    if (allowedCompilersStrings == NULL) {
#define COMPILERS_FILE_PATH "/etc/compilers"
        rs_trace("parsing %s", COMPILERS_FILE_PATH);
        
        int allowedCompilersFile = open(COMPILERS_FILE_PATH, O_RDONLY, 0);
        if ( allowedCompilersFile < 0 ) {
            rs_log_crit("failed to open() '%s'", COMPILERS_FILE_PATH);
            exit(EXIT_DISTCC_FAILED);
        }
        
        struct stat allowedCompilersFileStatB;
        if ( fstat( allowedCompilersFile, &allowedCompilersFileStatB) == -1 ) {
            close( allowedCompilersFile );
            rs_log_crit("failed to fstat() '%s'", COMPILERS_FILE_PATH);
            exit(EXIT_DISTCC_FAILED);
        }
        
        allowedCompilersStrings = malloc( allowedCompilersFileStatB.st_size + 1 );
        if ( allowedCompilersStrings == NULL ) {
            rs_log_crit("failed to allocate buffer for '%s' content.", COMPILERS_FILE_PATH);
            exit(EXIT_OUT_OF_MEMORY);
        }
        allowedCompilersStrings[allowedCompilersFileStatB.st_size] = '\0';
        
        int maximumNumberOfCompilers = 10;
        int currentCompilerSlot = 0;
        allowedCompilers = calloc( maximumNumberOfCompilers + 1, sizeof(char *) );
        if ( allowedCompilers == NULL ) {
            close( allowedCompilersFile );
            free( allowedCompilersStrings );
            rs_log_crit("failed to allocate buffer for '%s' content [slots].", COMPILERS_FILE_PATH);
            exit(EXIT_OUT_OF_MEMORY);
        }
        
        int dataRead = read( allowedCompilersFile, allowedCompilersStrings, allowedCompilersFileStatB.st_size );
        if ( dataRead != allowedCompilersFileStatB.st_size ) {
            close( allowedCompilersFile );
            free( allowedCompilersStrings );
            free( allowedCompilers );
            rs_log_crit("failed to read '%s' content (read %d of %d bytes).", COMPILERS_FILE_PATH, (int)dataRead, (int)allowedCompilersFileStatB.st_size);
            exit(EXIT_DISTCC_FAILED);
        }
        
        char *currentCharacterPtr = allowedCompilersStrings;
        char *endOfBuffer = allowedCompilersStrings + allowedCompilersFileStatB.st_size;
        while ( currentCharacterPtr < endOfBuffer ) {
            // are we at the start of a compiler?
            if ( ( *currentCharacterPtr != '#' ) && ( *currentCharacterPtr != '\n' ) ) {
                // yes -- store it away.
                if ( currentCompilerSlot == maximumNumberOfCompilers ) {
#define SLOTS_TO_EXPAND_BY 10
                    maximumNumberOfCompilers = maximumNumberOfCompilers + SLOTS_TO_EXPAND_BY;
                    allowedCompilers = realloc( allowedCompilers,  sizeof( char * ) * (maximumNumberOfCompilers + 1) );
                    bzero(allowedCompilers + currentCompilerSlot, (SLOTS_TO_EXPAND_BY + 1) * sizeof( char * ));
                    if ( allowedCompilers == NULL ) {
                        close( allowedCompilersFile );
                        free( allowedCompilers );
                        free( allowedCompilersStrings );
                        rs_log_crit("failed to reallocate buffer for '%s' content.", COMPILERS_FILE_PATH);
                        exit(EXIT_OUT_OF_MEMORY);
                    }
                }
                
                allowedCompilers[currentCompilerSlot] = currentCharacterPtr;
                currentCompilerSlot++;
            }
            
            // Advance to EOL.
            while ( ( currentCharacterPtr < endOfBuffer ) && ( *currentCharacterPtr != '\n' ) )
                currentCharacterPtr++;
            
            // stop if at EOF.
            if ( currentCharacterPtr == endOfBuffer )
                break;
            
            // Terminate string at EOL.
            *currentCharacterPtr = '\0';

            // Go to next line (or EOF).
            currentCharacterPtr++;
        }
        close( allowedCompilersFile );
        
        if (rs_trace_enabled()) {
            char **compilers = allowedCompilers;
            int i = 0;
            while ( *compilers != NULL ) {
                rs_trace("allowed compiler: %s", *compilers);
                i++;
                compilers++;
            }
            rs_trace("allowed compilers count: %d", i);
        }
    }
    
    return allowedCompilers;    
}

/**
 * Ensure that aPath is one of the allowed compilers.
 *
 * @note The file /etc/compilers will be read once.  It should
 * contain a list of executables that are allowed to be launched.
 *
 * @retval 0 if it is OK to launch the specified compiler.
 *
 * @retval -1 if the specified compiler is not in /etc/compilers
 *
 */
int dcc_validate_compiler(char *aPath)
{
    rs_trace("validating compiler %s", aPath);
    
    char **allowedCompilers = _dcc_allowed_compilers();
    while ( *allowedCompilers != NULL ) {
        rs_trace("testing %s against %s", *allowedCompilers, aPath);
        if ( !strcmp(*allowedCompilers, aPath) ) {
            rs_trace("compiler '%s' is allowed", aPath);
            return 0;
        }
        allowedCompilers++;
    }
    
    rs_log_crit("compiler '%s' not allowed by '%s'.", aPath, COMPILERS_FILE_PATH);
    return -1;
}
