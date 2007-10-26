/* common.c --- Common routines, constants, etc.  Used by all the wrappers.
 *
 * Copyright (C) 1998-2005 by the Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#include "common.h"

/* Passed in by configure. */
#define SCRIPTDIR PREFIX "/scripts/"         /* trailing slash */
#define MODULEDIR PREFIX                     /* no trailing slash */

const char* scriptdir = SCRIPTDIR;
const char* moduledir = MODULEDIR;
char* python = PYTHON;

/* Global variable used as a flag */
int running_as_cgi = 0;



/* Some older systems don't define strerror().  Provide a replacement that is
 * good enough for our purposes.
 */
#ifndef HAVE_STRERROR

extern char *sys_errlist[];
extern int sys_nerr;

char* strerror(int errno)
{
        if (errno < 0 || errno >= sys_nerr) {
                return "unknown error";
        }
        else {
                return sys_errlist[errno];
        }
}

#endif /* ! HAVE_STRERROR */



/* Report on errors and exit
 */
#define BUFSIZE 1024

void
fatal(const char* ident, int exitcode, char* format, ...)
{
#ifndef HAVE_VSNPRINTF
        /* A replacement is provided in vsnprintf.c for ancient systems still
         * lacking one in their C library.
         */
        int vsnprintf(char*, size_t, const char*, va_list);
#endif /* !HAVE_VSNPRINTF */

        char log_entry[BUFSIZE];

        va_list arg_ptr;
        va_start(arg_ptr, format);

        vsnprintf(log_entry, BUFSIZE, format, arg_ptr);
        va_end(arg_ptr);

#ifdef HAVE_SYSLOG
        /* Write to the console, maillog is often mostly ignored, and root
         * should definitely know about any problems.
         */
        openlog(ident, LOG_CONS, LOG_MAIL);
        syslog(LOG_ERR, "%s\n", log_entry);
        closelog();
#endif /* HAVE_SYSLOG */

#ifdef HELPFUL
        /* If we're running as a CGI script, we also want to write the log
         * file out as HTML, so the admin who is probably trying to debug his
         * installation will have a better clue as to what's going on.
         *
         * Otherwise, print to stderr a short message, hopefully returned to
         * the sender by the MTA.
         */
        if (running_as_cgi) {
                printf("Content-type: text/html\n\n");
                printf("<head>\n");
                printf("<title>Mailman CGI error!!!</title>\n");
                printf("</head><body>\n");
                printf("<h1>Mailman CGI error!!!</h1>\n");
                printf("The Mailman CGI wrapper encountered a fatal error. ");
                printf("This entry is being stored in your syslog:");
                printf("\n<pre>\n");
                printf("%s", log_entry);
                printf("</pre>\n");
        }
        else
                fprintf(stderr, "%s\n", log_entry);
#endif /* HELPFUL */
        exit(exitcode);
}



/* Is the parent process allowed to call us?
 */
void
check_caller(const char* ident, const char* parentgroup)
{
        GID_T mygid = getgid();
        struct group *mygroup = getgrgid(mygid);
        char* option;
        char* server;
        char* wrapper;

        if (running_as_cgi) {
                option = "--with-cgi-gid";
                server = "web";
                wrapper = "CGI";
        }
        else {
                option = "--with-mail-gid";
                server = "mail";
                wrapper = "mail";
        }

        if (!mygroup)
                fatal(ident, GROUP_NAME_NOT_FOUND,
                      "Failure to find group name for GID %d.  Mailman\n"
                      "expected the %s wrapper to be executed as group\n"
                      "\"%s\", but the system's %s server executed the\n"
                      "wrapper as GID %d for which the name could not be\n"
                      "found.  Try adding GID %d to your system as \"%s\",\n"
                      "or tweak your %s server to run the wrapper as group\n"
                      "\"%s\".",
                      mygid, wrapper, parentgroup, server, mygid, mygid,
                      parentgroup, server, parentgroup);

        if (strcmp(parentgroup, mygroup->gr_name))
                fatal(ident, GROUP_MISMATCH,
                      "Group mismatch error.  Mailman expected the %s\n"
                      "wrapper script to be executed as group \"%s\", but\n"
                      "the system's %s server executed the %s script as\n"
                      "group \"%s\".  Try tweaking the %s server to run the\n"
                      "script as group \"%s\", or re-run configure, \n"
                      "providing the command line option `%s=%s'.",
                      wrapper, parentgroup, server, wrapper, mygroup->gr_name,
                      server, parentgroup, option, mygroup->gr_name);
}



/* list of environment variables which are removed from the given
 * environment.  Some may or may not be hand crafted and passed into
 * the execv'd environment.
 *
 * TBD: The logic of this should be inverted.  IOW, we should audit the
 * Mailman CGI code for those environment variables that are used, and
 * specifically white list them, removing all other variables.  John Viega
 * also suggests imposing a maximum size just in case Python doesn't handle
 * them right (which it should because Python strings have no hard limits).
 */
static char* killenvars[] = {
        "PYTHONPATH=",
        "PYTHONHOME=",
        "PATH=",
        NULL
};



/* Run a Python script out of the script directory
 *
 * args[0] should be the abs path to the Python script to execute
 * argv[1:] are other args for the script
 * env may or may not contain PYTHONPATH, we'll substitute our own
 *
 * TBD: third argument env may not be universally portable
 */
int
run_script(const char* script, int argc, char** argv, char** env)
{
        const char envstr[] = "PYTHONPATH=";
        const int envlen = strlen(envstr);

        int envcnt = 0;
        int i, j, status;
        char** newenv;
        char** newargv;

        /* We need to set the real gid to the effective gid because there are
         * some Linux systems which do not preserve the effective gid across
         * popen() calls.  This breaks mail delivery unless the ~mailman/data
         * directory is chown'd to the uid that runs mail programs, and that
         * isn't a viable alternative.
         */
#ifdef HAVE_SETREGID
        status = setregid(getegid(), -1);
        if (status)
                fatal(logident, SETREGID_FAILURE, "%s", strerror(errno));
#endif /* HAVE_SETREGID */

        /* We want to tightly control how the CGI scripts get executed.
         * For portability and security, the path to the Python executable
         * is hard-coded into this C wrapper, rather than encoded in the #!
         * line of the script that gets executed.  So we invoke those
         * scripts by passing the script name on the command line to the
         * Python executable.
         *
         * We also need to hack on the PYTHONPATH environment variable so
         * that the path to the installed Mailman modules will show up
         * first on sys.path.
         *
         */
        for (envcnt = 0; env[envcnt]; envcnt++)
                ;

        /* okay to be a little too big */
        newenv = (char**)malloc(sizeof(char*) * (envcnt + 2));

        /* filter out any troublesome environment variables */
        for (i = 0, j = 0; i < envcnt; i++) {
                char** k = &killenvars[0];
                int keep = 1;
                while (*k) {
                        if (!strncmp(*k, env[i], strlen(*k))) {
                                keep = 0;
                                break;
                        }
                        *k++;
                }
                if (keep)
                        newenv[j++] = env[i];
        }

        /* Tack on our own version of PYTHONPATH, which should contain only
         * the path to the Mailman package modules.
         *
         * $(PREFIX)/modules
         */
        newenv[j] = (char*)malloc(sizeof(char) * (
                strlen(envstr) +
                strlen(moduledir) +
                1));
        strcpy(newenv[j], envstr);
        strcat(newenv[j], moduledir);
        j++;

        newenv[j] = NULL;

        /* Now put together argv.  This will contain first the absolute path
         * to the Python executable, then the -S option (to speed executable
         * start times), then the absolute path to the script, then any
      * additional args passed in argv above.
         */
        newargv = (char**)malloc(sizeof(char*) * (argc + 3));
        j = 0;
        newargv[j++] = python;
        newargv[j++] = "-S";
        newargv[j] = (char*)malloc(sizeof(char) * (
                strlen(scriptdir) +
                strlen(script) +
                1));
        strcpy(newargv[j], scriptdir);
        strcat(newargv[j], script);

        /* now tack on all the rest of the arguments.  we can skip argv's
         * first two arguments because, for cgi-wrapper there is only argv[0].
         * For mail-wrapper, argv[1] is the mail command (e.g. post,
         * mailowner, or mailcmd) and argv[2] is the listname.  The mail
         * command to execute gets passed in as this function's `script'
         * parameter and becomes the argument to the python interpreter.  The
         * list name therefore should become argv[2] to this process.
         *
         * TBD: have to make sure this works with alias-wrapper.
         */
        for (i=2, j++; i < argc; i++)
                newargv[j++] = argv[i];

        newargv[j] = NULL;

        /* return always means failure */
        (void)execve(python, &newargv[0], &newenv[0]);
        return EXECVE_FAILURE;
}



/*
 * Local Variables:
 * c-file-style: "python"
 * indent-tabs-mode: nil
 * End:
 */
