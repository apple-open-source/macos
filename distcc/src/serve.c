/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003 by Martin Pool <mbp@samba.org>
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

                /* He who waits until circumstances completely favour *
                 * his undertaking will never accomplish anything.    *
                 *              -- Martin Luther                      */
   

/**
 * @file
 *
 * Actually serve remote requests.  Called from daemon.c.
 *
 * @todo Make sure wait statuses are packed in a consistent format
 * (exit<<8 | signal).  Is there any platform that doesn't do this?
 *
 * @todo The server should catch signals, and terminate the compiler process
 * group before handling them.
 *
 * @todo It might be nice to detect that the client has dropped the
 * connection, and then kill the compiler immediately.  However, we probably
 * won't notice that until we try to do IO.  SIGPIPE won't help because it's
 * not triggered until we try to do IO.  I don't think it matters a lot,
 * though, because the client's not very likely to do that.  The main case is
 * probably somebody getting bored and interrupting compilation.
 *
 * What might help is to select() on the network socket while we're waiting
 * for the child to complete, allowing SIGCHLD to interrupt the select() when
 * the child completes.  However I'm not sure if it's really worth the trouble
 * of doing that just to handle a fairly marginal case.
 **/



#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_SIGNAL_H
#  include <sys/signal.h>
#endif /* HAVE_SYS_SIGNAL_H */

#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "util.h"
#include "rpc.h"
#include "exitcode.h"
#include "dopt.h"
#include "tempfile.h"
#include "bulk.h"
#include "exec.h"
#include "srvnet.h"
#include "filename.h"


static int dcc_r_request_header(int ifd)
{
    int vers;
    
    if (dcc_r_token_int(ifd, "DIST", &vers) == -1) {
        rs_log_error("client did not provide distcc magic fairy dust");
        return -1;
    }

    if (vers != PROTO_VER) {
        rs_log_error("client version is %d, i am %d", vers, 1);
        return -1;
    }
    
    return 0;
}


/**
 * Read an argv[] vector from the network.
 **/
static int dcc_r_argv(int ifd, /*@out@*/ char ***argv)
{
    int i;
    int argc;
    char **a;

    *argv = NULL;
     
    if (dcc_r_token_int(ifd, "ARGC", &argc))
        return -1;

    rs_trace("reading %d arguments from job submission", argc);
    
    /* Have to make the argv one element too long, so that it can be
     * terminated by a null element. */
    *argv = a = (char **) calloc((size_t) argc+1, sizeof a[0]);
    if (a == NULL) {
        rs_log_error("alloc failed");
        return -1;
    }
    a[argc] = NULL;

    for (i = 0; i < argc; i++) {
        int a_len;
        
        if (dcc_r_token_int(ifd, "ARGV", &a_len))
            return -1;

        if (dcc_r_str_alloc(ifd, a_len, &a[i]))
            return -1;
        
        rs_trace("argv[%d] = \"%s\"", i, a[i]);
    }

    dcc_trace_argv("got arguments", a);
    
    return 0;
}



#if 0
/* Not used at the moment because the server never kills off
   the compiler, but it might be in the future */

static int dcc_kill_compiler(pid_t pid, int signum)
{
    rs_log_info("killing compiler pid %d with signal %d",
                (int) pid, signum);

    if (kill(pid, signum)) {
        rs_log_warning("kill(%d, %d) failed: %s", (int) pid, signum,
                       strerror(errno));
        return -1;
    }
    
    return 0;
}
#endif


/**
 * Copy all server messages to the error file, so that they can be
 * echoed back to the client if necessary.
 **/
static int dcc_add_log_to_file(const char *err_fname)
{
    int log_fd;

    log_fd = open(err_fname, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (log_fd == -1) {
        rs_log_error("failed to open %s: %s", err_fname, strerror(errno));
        return EXIT_IO_ERROR;
    }

    /* Only send fairly serious errors back */
    rs_add_logger(rs_logger_file, RS_LOG_WARNING, NULL, log_fd);

    return 0;
}



/**
 * On receipt of a fatal signal, kill the compiler as well.
 **/
static void dcc_terminate_group(int whichsig)
{
    dcc_reset_signal(whichsig);
    kill(0, whichsig);
}



#if 0
/**
 * Actually run the compiler on the server.
 *
 * This whole function is run with a signal handler set such that if a
 * child terminates, it will immediately interrupt system calls and
 * return.
 *
 * This is called with SIGCHLD on, so any of the calls here may fail
 * with EINTR if the child exits.  I think that's OK, though, because
 * if that happens then the compiler has unexpectedly exited.
 *
 * @param argv     Compiler command line, already modified for temporary files.
 *
 * @param netfd    fd of client connection.
 **/
static int dcc_server_compile(char **argv, int netfd,
                              const char *temp_i,
                              const char *temp_out, const char *temp_err)
{
    int i_size;
    int ret;
    pid_t pid;

    pid = 0;

    if ((ret = dcc_r_token_int(netfd, "DOTI", &i_size)))
        return ret;

    rs_log_info("input file is %d bytes", i_size);

    /* read input before spawning */
    if ((ret = dcc_r_file(netfd, temp_i, i_size)) != 0)
        return ret;

    return dcc_spawn_child(argv, &pid, "/dev/null", temp_out, temp_err);
}
#endif /* 0 */


/**
 * Signals are caught only in the parent at the moment, because we
 * want to be able to kill off all children and log a message when
 * exiting.  This is perhaps not the best thing to do -- it might be
 * better to simply let ourselves be terminated, and for children to
 * go away when they're finished their current work.  If the admin is
 * really impatient, they can kill all the children anyhow.
 **/
static void dcc_hook_terminate_group(void)
{
    struct sigaction act_term;

    memset(&act_term, 0, sizeof act_term);
    act_term.sa_handler = dcc_terminate_group;

    if (sigaction(SIGTERM, &act_term, NULL)
        || sigaction(SIGHUP, &act_term, NULL)
        || sigaction(SIGINT, &act_term, NULL)
        || sigaction(SIGQUIT, &act_term, NULL)) {
        rs_log_error("failed to install signal handlers: %s",
                     strerror(errno));
    }
}


              
/**
 * Read and execute a job to/from socket @p ifd
 *
 * @todo Split this beast up.
 *
 * @return standard exit code
 **/
int dcc_accept_job(int netfd)
{
    char **argv;
    int status;
    char *temp_i, *temp_o, *err_fname, *out_fname;
    size_t o_size = 0;
    int ret, compile_ret;
    char *orig_input, *orig_output;
    const char *input_exten;
    long u_us, s_us;
    int i_size;
    pid_t cc_pid;

    err_fname = dcc_make_tmpnam("cc", ".err");
    out_fname = dcc_make_tmpnam("cc", ".out");
    dcc_remove_if_exists(err_fname);
    dcc_remove_if_exists(out_fname);

    /* Capture any messages relating to this compilation to the same file as
     * compiler errors so that they can all be sent back to the client.
     *
     * TODO: Make sure this file is closed if this function fails in --no-fork
     * mode. */
    dcc_add_log_to_file(err_fname);

    /* Log client name and check access if appropriate. */
    if ((ret = dcc_check_client(netfd)) != 0)
        return ret;

    /* Ignore SIGPIPE; we consistently check error codes and will see the
     * EPIPE.  Note that it is set back to the default behaviour when spawning
     * a child, to handle cases like the assembler dying while its being fed
     * from the compiler */
    dcc_ignore_sigpipe(1);

    /* Enter a new process group, so that we can kill the compiler as well if
     * we're terminated. */
    dcc_setpgid(0, 0);
    dcc_hook_terminate_group();

    /* Allow output to accumulate into big packets. */
    tcp_cork_sock(netfd, 1);

    if ((ret = dcc_r_request_header(netfd))
        || (ret = dcc_r_argv(netfd, &argv))
        || (ret = dcc_scan_args(argv, &orig_input, &orig_output, &argv)))
        return ret;
    
    /* TODO: Make sure cleanup is called in case of error.
     *
     * TODO: Move out the guts of this function. */

    rs_log_info("input file %s, output file %s", orig_input, orig_output);

    input_exten = dcc_find_extension(orig_input);
    if (input_exten)
        input_exten = dcc_preproc_exten(input_exten);
    if (!input_exten)           /* previous line might return NULL */
        input_exten = ".tmp";
    temp_i = dcc_make_tmpnam("server", input_exten);
    temp_o = dcc_make_tmpnam("server", ".out");

    if ((ret = dcc_r_token_int(netfd, "DOTI", &i_size))
        || (ret = dcc_r_file_timed(netfd, temp_i, (size_t) i_size))
        || (ret = dcc_set_input(argv, temp_i))
        || (ret = dcc_set_output(argv, temp_o)))
        return ret;

    if ((compile_ret = dcc_spawn_child(argv, &cc_pid, "/dev/null", out_fname, err_fname))
        || (compile_ret = dcc_collect_child(cc_pid, &status, &u_us, &s_us))) {
        /* We didn't get around to finding a wait status from the actual compiler */
        status = W_EXITCODE(compile_ret, 0);
    }
    
    if ((ret = dcc_x_result_header(netfd))
        || (ret = dcc_x_cc_status(netfd, status))
        || (ret = dcc_x_file(netfd, err_fname, "SERR", NULL))
        || (ret = dcc_x_file(netfd, out_fname, "SOUT", NULL))
        || WIFSIGNALED(status)
        || WEXITSTATUS(status)) {
        dcc_x_token_int(netfd, "DOTO", 0);
    } else {
        ret = dcc_x_file_timed(netfd, temp_o, "DOTO", &o_size);
    }

    dcc_report_rusage(argv[0], u_us, s_us);
    dcc_critique_status(status, argv[0], dcc_gethostname());
    tcp_cork_sock(netfd, 0);

    rs_log_info("complete; output file: %ld bytes", (long) o_size);

    dcc_cleanup_tempfiles();

    return ret;
}
