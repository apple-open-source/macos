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

#define PROTO_VER 1

#ifdef NORETURN
/* nothing */
#elif defined(__GNUC__)
#  define NORETURN __attribute__((noreturn))
#elif defined(__LCLINT__)
#  define NORETURN /*@noreturn@*/ x
#else                           /* !__GNUC__ && !__LCLINT__ */
#  define NORETURN
#endif                          /* !__GNUC__ && !__LCLINT__ */

#ifdef UNUSED
/* nothing */
#elif defined(__GNUC__)
#  define UNUSED(x) x __attribute__((unused))
#elif defined(__LCLINT__)
#  define UNUSED(x) /*@unused@*/ x
#else				/* !__GNUC__ && !__LCLINT__ */
#  define UNUSED(x) x
#endif				/* !__GNUC__ && !__LCLINT__ */

#ifndef HAVE_SA_FAMILY_T
typedef int sa_family_t;
#endif

#ifndef TRUE
#  define TRUE 1
#  define FALSE 0
#endif


struct dcc_hostdef;


int str_endswith(const char *tail, const char *tiger);



/* filename.c */
int dcc_is_source(const char *sfile);
int dcc_is_preprocessed(const char *sfile);
char * dcc_find_extension(char *sfile);
int dcc_output_from_source(const char *sfile, const char *out_extn,
                           char **ofile);

/* arg.c */
int dcc_deepcopy_argv(char **, /*@relnull@*/ char ***);
int dcc_shallowcopy_argv(char **, /*@relnull@*/ char ***, int);
int dcc_set_action_opt(char **, const char *);
int dcc_set_output(char **, char *);
int dcc_set_input(char **, char *);
int dcc_scan_args(char *argv[], /*@out@*/ /*@relnull@*/ char **orig_o,
                  char **orig_i, char ***ret_newargv);
char *dcc_argv_tostr(char **a);
int dcc_trace_argv(const char *message, char *argv[]);

int dcc_argv_len(char **a);

/* dopt.c */
extern struct dcc_allow_list *opt_allowed;
int distccd_parse_options(int argc, const char *argv[]);

/* serve.c */
int dcc_accept_job(int fd);

/* help.c */
int dcc_show_copyright(void);
int dcc_show_version(const char *prog);

/* daemon.c */
void dcc_server_child(int) NORETURN;
int dcc_refuse_root(void);
int dcc_set_lifetime(void);

/* dsignal.c */
void dcc_catch_signals(void);
void dcc_ignore_sighup(void);

/* dparent.c */
int dcc_standalone_server(void);
void dcc_remove_pid(void);


/* bulk.c */
int dcc_open_read(const char *fname, int *ifd, off_t *fsize);


/* hosts.c */
int dcc_parse_hosts_env(struct dcc_hostdef **ret_list,
                        int *ret_nhosts);
int dcc_parse_hosts(const char *where,
                    struct dcc_hostdef **ret_list,
                    int *ret_nhosts);

/* ncpu.c */
int dcc_ncpus(int *);

#define DISTCC_DEFAULT_PORT 3632

