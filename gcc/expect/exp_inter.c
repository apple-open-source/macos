/* interact (using select) - give user keyboard control

Written by: Don Libes, NIST, 2/6/90

Design and implementation of this program was paid for by U.S. tax
dollars.  Therefore it is public domain.  However, the author and NIST
would appreciate credit if this program or parts of it are used.

*/

#include "expect_cf.h"
#include <stdio.h>
#ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
#endif
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
  
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <ctype.h>

#include "tcl.h"
#include "string.h"

#include "exp_tty_in.h"
#include "exp_rename.h"
#include "exp_prog.h"
#include "exp_command.h"
#include "exp_log.h"
#include "exp_tstamp.h"	/* remove when timestamp stuff is gone */

#include "tclRegexp.h"
#include "exp_regexp.h"

extern char *TclGetRegError();
extern void TclRegError();

#define INTER_OUT "interact_out"

/*
 * tests if we are running this using a real tty
 *
 * these tests are currently only used to control what gets written to the
 * logfile.  Note that removal of the test of "..._is_tty" means that stdin
 * or stdout could be redirected and yet stdout would still be logged.
 * However, it's not clear why anyone would use log_file when these are
 * redirected in the first place.  On the other hand, it is reasonable to
 * run expect as a daemon in which case, stdin/out do not appear to be
 * ttys, yet it makes sense for them to be logged with log_file as if they
 * were.
 */
#if 0
#define real_tty_output(x) (exp_stdout_is_tty && (((x)==1) || ((x)==exp_dev_tty)))
#define real_tty_input(x) (exp_stdin_is_tty && (((x)==0) || ((x)==exp_dev_tty)))
#endif

#define real_tty_output(x) (((x)==1) || ((x)==exp_dev_tty))
#define real_tty_input(x) (exp_stdin_is_tty && (((x)==0) || ((x)==exp_dev_tty)))

#define new(x)	(x *)ckalloc(sizeof(x))

struct action {
	char *statement;
	int tty_reset;		/* if true, reset tty mode upon action */
	int iread;		/* if true, reread indirects */
	int iwrite;		/* if true, write spawn_id element */
	int timestamp;		/* if true, generate timestamp */
	struct action *next;	/* chain only for later for freeing */
};

struct keymap {
	char *keys;	/* original pattern provided by user */
	regexp *re;
	int null;	/* true if looking to match 0 byte */
	int case_sensitive;
	int echo;	/* if keystrokes should be echoed */
	int writethru;	/* if keystrokes should go through to process */
	int indices;	/* true if should write indices */
	struct action action;
	struct keymap *next;
};

struct output {
	struct exp_i *i_list;
	struct action *action_eof;
	struct output *next;
};

struct input {
	struct exp_i *i_list;
	struct output *output;
	struct action *action_eof;
	struct action *action_timeout;
	struct keymap *keymap;
	int timeout_nominal;		/* timeout nominal */
	int timeout_remaining;		/* timeout remaining */
	struct input *next;
};

static void free_input();
static void free_keymap();
static void free_output();
static void free_action();
static struct action *new_action();
static int inter_eval();

/* in_keymap() accepts user keystrokes and returns one of MATCH,
CANMATCH, or CANTMATCH.  These describe whether the keystrokes match a
key sequence, and could or can't if more characters arrive.  The
function assigns a matching keymap if there is a match or can-match.
A matching keymap is assigned on can-match so we know whether to echo
or not.

in_keymap is optimized (if you can call it that) towards a small
number of key mappings, but still works well for large maps, since no
function calls are made, and we stop as soon as there is a single-char
mismatch, and go on to the next one.  A hash table or compiled DFA
probably would not buy very much here for most maps.

The basic idea of how this works is it does a smart sequential search.
At each position of the input string, we attempt to match each of the
keymaps.  If at least one matches, the first match is returned.

If there is a CANMATCH and there are more keymaps to try, we continue
trying.  If there are no more keymaps to try, we stop trying and
return with an indication of the first keymap that can match.

Note that I've hacked up the regexp pattern matcher in two ways.  One
is to force the pattern to always be anchored at the front.  That way,
it doesn't waste time attempting to match later in the string (before
we're ready).  The other is to return can-match.

*/

static int
in_keymap(string,stringlen,keymap,km_match,match_length,skip,rm_nulls)
char *string;
int stringlen;
struct keymap *keymap;		/* linked list of keymaps */
struct keymap **km_match;	/* keymap that matches or can match */
int *match_length;		/* # of chars that matched */
int *skip;			/* # of chars to skip */
int rm_nulls;			/* skip nulls if true */
{
	struct keymap *km;
	char *ks;		/* string from a keymap */
	char *start_search;	/* where in the string to start searching */
	char *string_end;

	/* assert (*km == 0) */

	/* a shortcut that should help master output which typically */
	/* is lengthy and has no key maps.  Otherwise it would mindlessly */
	/* iterate on each character anyway. */
	if (!keymap) {
		*skip = stringlen;
		return(EXP_CANTMATCH);
	}

	string_end = string + stringlen;

	/* Mark beginning of line for ^ . */
	regbol = string;

/* skip over nulls - Pascal Meheut, pascal@cnam.cnam.fr 18-May-1993 */
/*    for (start_search = string;*start_search;start_search++) {*/
    for (start_search = string;start_search<string_end;start_search++) {
	if (*km_match) break; /* if we've already found a CANMATCH */
			/* don't bother starting search from positions */
			/* further along the string */

	for (km=keymap;km;km=km->next) {
	    char *s;	/* current character being examined */

	    if (km->null) {
		if (*start_search == 0) {
		    *skip = start_search-string;
		    *match_length = 1;	/* s - start_search == 1 */
		    *km_match = km;
		    return(EXP_MATCH);
	        }
	    } else if (!km->re) {
		/* fixed string */
		for (s = start_search,ks = km->keys ;;s++,ks++) {
			/* if we hit the end of this map, must've matched! */
			if (*ks == 0) {
				*skip = start_search-string;
				*match_length = s-start_search;
				*km_match = km;
				return(EXP_MATCH);
			}

			/* if we ran out of user-supplied characters, and */
			/* still haven't matched, it might match if the user */
			/* supplies more characters next time */

			if (s == string_end) {
				/* skip to next key entry, but remember */
				/* possibility that this entry might match */
				if (!*km_match) *km_match = km;
				break;
			}

			/* if this is a problem for you, use exp_parity command */
/*			if ((*s & 0x7f) == *ks) continue;*/
			if (*s == *ks) continue;
			if ((*s == '\0') && rm_nulls) {
				ks--;
				continue;
			}
			break;
		}
	    } else {
		/* regexp */
		int r;	/* regtry status */
		regexp *prog = km->re;

		/* if anchored, but we're not at beginning, skip pattern */
		if (prog->reganch) {
			if (string != start_search) continue;
		}

		/* known starting char - quick test 'fore lotta work */
		if (prog->regstart) {
			/* if this is a problem for you, use exp_parity command */
/*			/* if ((*start_search & 0x7f) != prog->regstart) continue; */
			if (*start_search != prog->regstart) continue;
		}
		r = exp_regtry(prog,start_search,match_length);
		if (r == EXP_MATCH) {
			*km_match = km;
			*skip = start_search-string;
			return(EXP_MATCH);
		}
		if (r == EXP_CANMATCH) {
			if (!*km_match) *km_match = km;
		}
	    }
	}
    }

	if (*km_match) {
		/* report a can-match */

		char *p;

		*skip = (start_search-string)-1;
#if 0
		*match_length = stringlen - *skip;
#else
		/*
		 * there may be nulls in the string in which case
		 * the pattern matchers can report CANMATCH when
		 * the null is hit.  So find the null and compute
		 * the length of the possible match.
		 *
		 * Later, after we squeeze out the nulls, we will
		 * retry the match, but for now, go along with
		 * calling it a CANMATCH
		 */
		p = start_search;
		while (*p) {
			p++;
		}
		*match_length = (p - start_search) + 1;
		/*printf(" match_length = %d\n",*match_length);*/
#endif
		return(EXP_CANMATCH);
	}

	*skip = start_search-string;
	return(EXP_CANTMATCH);
}

#ifdef SIMPLE_EVENT

/*

The way that the "simple" interact works is that the original Expect
process reads from the tty and writes to the spawned process.  A child
process is forked to read from the spawned process and write to the
tty.  It looks like this:

                        user
                    --> tty >--
                   /           \
                  ^             v
                child        original
               process        Expect
                  ^          process
                  |             v
                   \           /
                    < spawned <
                      process

*/



#ifndef WEXITSTATUS
#define WEXITSTATUS(stat) (((*((int *) &(stat))) >> 8) & 0xff)
#endif

#include <setjmp.h>

static jmp_buf env;		/* for interruptable read() */
static int reading;		/* while we are reading */
				/* really, while "env" is valid */
static int deferred_interrupt = FALSE;	/* if signal is received, but not */
				/* in i_read record this here, so it will */
				/* be handled next time through i_read */

void sigchld_handler()
{
	if (reading) longjmp(env,1);

	deferred_interrupt = TRUE;
}

#define EXP_CHILD_EOF -100

/* interruptable read */
static int
i_read(fd,buffer,length)
int fd;
char *buffer;
int length;
{
	int cc = EXP_CHILD_EOF;

	if (deferred_interrupt) return(cc);

	if (0 == setjmp(env)) {
		reading = TRUE;
		cc = read(fd,buffer,length);
	}
	reading = FALSE;
	return(cc);
}

/* exit status for the child process created by cmdInteract */
#define CHILD_DIED		-2
#define SPAWNED_PROCESS_DIED	-3

static void
clean_up_after_child(interp,master)
Tcl_Interp *interp;
int master;
{
/* should really be recoded using the common wait code in command.c */
	int status;
	int pid;
	int i;

	pid = wait(&status);	/* for slave */
	for (i=0;i<=exp_fd_max;i++) {
		if (exp_fs[i].pid == pid) {
			exp_fs[i].sys_waited = TRUE;
			exp_fs[i].wait = status;
		}
	}
	pid = wait(&status);	/* for child */
	for (i=0;i<=exp_fd_max;i++) {
		if (exp_fs[i].pid == pid) {
			exp_fs[i].sys_waited = TRUE;
			exp_fs[i].wait = status;
		}
	}

	deferred_interrupt = FALSE;
	exp_close(interp,master);
	master = -1;
}
#endif /*SIMPLE_EVENT*/

static int
update_interact_fds(interp,fd_count,fd_to_input,fd_list,input_base,
			do_indirect,config_count,real_tty_caller)
Tcl_Interp *interp;
int *fd_count;
struct input ***fd_to_input;	/* map from fd's to "struct input"s */
int **fd_list;
struct input *input_base;
int do_indirect;		/* if true do indirects */
int *config_count;
int *real_tty_caller;
{
	struct input *inp;
	struct output *outp;
	struct exp_fd_list *fdp;
	int count;

	int real_tty = FALSE;

	*config_count = exp_configure_count;

	count = 0;
	for (inp = input_base;inp;inp=inp->next) {

		if (do_indirect) {
			/* do not update "direct" entries (again) */
			/* they were updated upon creation */
			if (inp->i_list->direct == EXP_INDIRECT) {
				exp_i_update(interp,inp->i_list);
			}
			for (outp = inp->output;outp;outp=outp->next) {
				if (outp->i_list->direct == EXP_INDIRECT) {
					exp_i_update(interp,outp->i_list);
				}
			}
		}

		/* revalidate all input descriptors */
		for (fdp = inp->i_list->fd_list;fdp;fdp=fdp->next) {
			count++;
			/* have to "adjust" just in case spawn id hasn't had */
			/* a buffer sized yet */
			if (!exp_fd2f(interp,fdp->fd,1,1,"interact"))
				return(TCL_ERROR);
		}

		/* revalidate all output descriptors */
		for (outp = inp->output;outp;outp=outp->next) {
			for (fdp = outp->i_list->fd_list;fdp;fdp=fdp->next) {
				/* make user_spawn_id point to stdout */
				if (fdp->fd == 0) {
					fdp->fd = 1;
				} else if (fdp->fd == 1) {
					/* do nothing */
				} else if (!exp_fd2f(interp,fdp->fd,1,0,"interact"))
					return(TCL_ERROR);
			}
		}
	}
	if (!do_indirect) return TCL_OK;

	if (*fd_to_input == 0) {
		*fd_to_input = (struct input **)ckalloc(
				(exp_fd_max+1) * sizeof(struct input *));
		*fd_list = (int *)ckalloc(count * sizeof(int));
	} else {
		*fd_to_input = (struct input **)ckrealloc((char *)*fd_to_input,
				(exp_fd_max+1) * sizeof(struct input *));
		*fd_list = (int *)ckrealloc((char *)*fd_list,count * sizeof(int));
	}

	count = 0;
	for (inp = input_base;inp;inp=inp->next) {
		for (fdp = inp->i_list->fd_list;fdp;fdp=fdp->next) {
			/* build map to translate from spawn_id to struct input */
			(*fd_to_input)[fdp->fd] = inp;

			/* build input to ready() */
			(*fd_list)[count] = fdp->fd;

			if (real_tty_input(fdp->fd)) real_tty = TRUE;

			count++;
		}
	}
	*fd_count = count;

	*real_tty_caller = real_tty; /* tell caller if we have found that */
					/* we are using real tty */

	return TCL_OK;
}

/*ARGSUSED*/
static char *
inter_updateproc(clientData, interp, name1, name2, flags)
ClientData clientData;
Tcl_Interp *interp;	/* Interpreter containing variable. */
char *name1;		/* Name of variable. */
char *name2;		/* Second part of variable name. */
int flags;		/* Information about what happened. */
{
	exp_configure_count++;
	return 0;
}
			
#define finish(x)	{ status = x; goto done; }

static char return_cmd[] = "return";
static char interpreter_cmd[] = "interpreter";

/*ARGSUSED*/
int
Exp_InteractCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char *arg;	/* shorthand for current argv */
#ifdef SIMPLE_EVENT
	int pid;
#endif /*SIMPLE_EVENT*/

	/*declarations*/
	int input_count;	/* count of struct input descriptors */
	struct input **fd_to_input;	/* map from fd's to "struct input"s */
	int *fd_list;
	struct keymap *km;	/* ptr for above while parsing */
/* 	extern char *tclRegexpError;	/* declared in tclInt.h */
	int master = EXP_SPAWN_ID_BAD;
	char *master_string = 0;/* string representation of master */
	int need_to_close_master = FALSE;	/* if an eof is received */
				/* we use this to defer close until later */

	int next_tty_reset = FALSE;	/* if we've seen a single -reset */
	int next_iread = FALSE;/* if we've seen a single -iread */
	int next_iwrite = FALSE;/* if we've seen a single -iread */
	int next_re = FALSE;	/* if we've seen a single -re */
	int next_null = FALSE;	/* if we've seen the null keyword */
	int next_writethru = FALSE;/*if macros should also go to proc output */
	int next_indices = FALSE;/* if we should write indices */
	int next_echo = FALSE;	/* if macros should be echoed */
	int next_timestamp = FALSE; /* if we should generate a timestamp */
/*	int next_case_sensitive = TRUE;*/
	char **oldargv = 0;	/* save original argv here if we split it */
	int status = TCL_OK;	/* final return value */
	int i;			/* trusty temp */

	int timeout_simple = TRUE;	/* if no or global timeout */

	int real_tty;		/* TRUE if we are interacting with real tty */
	int tty_changed = FALSE;/* true if we had to change tty modes for */
				/* interact to work (i.e., to raw, noecho) */
	int was_raw;
	int was_echo;
	exp_tty tty_old;

	char *replace_user_by_process = 0; /* for -u flag */

	struct input *input_base;
#define input_user input_base
	struct input *input_default;
	struct input *inp;	/* overused ptr to struct input */
	struct output *outp;	/* overused ptr to struct output */

	int dash_input_count = 0; /* # of "-input"s seen */
	int arbitrary_timeout;
	int default_timeout;
	struct action action_timeout;	/* common to all */
	struct action action_eof;	/* common to all */
	struct action **action_eof_ptr;	/* allow -input/ouput to */
		/* leave their eof-action assignable by a later */
		/* -eof */
	struct action *action_base = 0;
	struct keymap **end_km;

	int key;
	int configure_count;	/* monitor reconfigure events */

	if ((argc == 2) && exp_one_arg_braced(argv[1])) {
		return(exp_eval_with_one_arg(clientData,interp,argv));
	} else if ((argc == 3) && streq(argv[1],"-brace")) {
		char *new_argv[2];
		new_argv[0] = argv[0];
		new_argv[1] = argv[2];
		return(exp_eval_with_one_arg(clientData,interp,new_argv));
	}

	argv++;
	argc--;

	default_timeout = EXP_TIME_INFINITY;
	arbitrary_timeout = EXP_TIME_INFINITY;	/* if user specifies */
		/* a bunch of timeouts with EXP_TIME_INFINITY, this will be */
		/* left around for us to find. */

	input_user = new(struct input);
	input_user->i_list = exp_new_i_simple(0,EXP_TEMPORARY); /* stdin by default */
	input_user->output = 0;
	input_user->action_eof = &action_eof;
	input_user->timeout_nominal = EXP_TIME_INFINITY;
	input_user->action_timeout = 0;
	input_user->keymap = 0;

	end_km = &input_user->keymap;
	inp = input_user;
	action_eof_ptr = &input_user->action_eof;

	input_default = new(struct input);
	input_default->i_list = exp_new_i_simple(EXP_SPAWN_ID_BAD,EXP_TEMPORARY); /* fix up later */
	input_default->output = 0;
	input_default->action_eof = &action_eof;
	input_default->timeout_nominal = EXP_TIME_INFINITY;
	input_default->action_timeout = 0;
	input_default->keymap = 0;
	input_default->next = 0;		/* no one else */
	input_user->next = input_default;

	/* default and common -eof action */
	action_eof.statement = return_cmd;
	action_eof.tty_reset = FALSE;
	action_eof.iread = FALSE;
	action_eof.iwrite = FALSE;
	action_eof.timestamp = FALSE;

	for (;argc>0;argc--,argv++) {
		arg = *argv;
		if (exp_flageq("eof",arg,3)) {
			struct action *action;

			argc--;argv++;
			*action_eof_ptr = action = new_action(&action_base);

			action->statement = *argv;

			action->tty_reset = next_tty_reset;
			next_tty_reset = FALSE;
			action->iwrite = next_iwrite;
			next_iwrite = FALSE;
			action->iread = next_iread;
			next_iread = FALSE;
			action->timestamp = next_timestamp;
			next_timestamp = FALSE;
			continue;
		} else if (exp_flageq("timeout",arg,7)) {
			int t;
			struct action *action;

			argc--;argv++;
			if (argc < 1) {
				exp_error(interp,"timeout needs time");
				return(TCL_ERROR);
			}
			t = atoi(*argv);
			argc--;argv++;

			/* we need an arbitrary timeout to start */
			/* search for lowest one later */
			if (t != -1) arbitrary_timeout = t;

			timeout_simple = FALSE;
			action = inp->action_timeout = new_action(&action_base);
			inp->timeout_nominal = t;

			action->statement = *argv;

			action->tty_reset = next_tty_reset;
			next_tty_reset = FALSE;
			action->iwrite = next_iwrite;
			next_iwrite = FALSE;
			action->iread = next_iread;
			next_iread = FALSE;
			action->timestamp = next_timestamp;
			next_timestamp = FALSE;
			continue;
		} else if (exp_flageq("null",arg,4)) {
			next_null = TRUE;			
		} else if (arg[0] == '-') {
			arg++;
			if (exp_flageq1('-',arg)		/* "--" */
			 || (exp_flageq("exact",arg,3))) {
				argc--;argv++;
			} else if (exp_flageq("regexp",arg,2)) {
				if (argc < 1) {
					exp_error(interp,"-re needs pattern");
					return(TCL_ERROR);
				}
				next_re = TRUE;
				argc--;
				argv++;
			} else if (exp_flageq("input",arg,2)) {
				dash_input_count++;
				if (dash_input_count == 2) {
					inp = input_default;
					input_user->next = input_default;
				} else if (dash_input_count > 2) {
					struct input *previous_input = inp;
					inp = new(struct input);
					previous_input->next = inp;
				}
				inp->output = 0;
				inp->action_eof = &action_eof;
				action_eof_ptr = &inp->action_eof;
				inp->timeout_nominal = default_timeout;
				inp->action_timeout = &action_timeout;
				inp->keymap = 0;
				end_km = &inp->keymap;
				inp->next = 0;
				argc--;argv++;
				if (argc < 1) {
					exp_error(interp,"-input needs argument");
					return(TCL_ERROR);
				}
/*				inp->spawn_id = atoi(*argv);*/
				inp->i_list = exp_new_i_complex(interp,*argv,
						EXP_TEMPORARY,inter_updateproc);
				continue;
			} else if (exp_flageq("output",arg,3)) {
				struct output *tmp;

				/* imply a "-input" */
				if (dash_input_count == 0) dash_input_count = 1;

				outp = new(struct output);

				/* link new output in front of others */
				tmp = inp->output;
				inp->output = outp;
				outp->next = tmp;

				argc--;argv++;
				if (argc < 1) {
					exp_error(interp,"-output needs argument");
					return(TCL_ERROR);
				}
				outp->i_list = exp_new_i_complex(interp,*argv,
					EXP_TEMPORARY,inter_updateproc);

				outp->action_eof = &action_eof;
				action_eof_ptr = &outp->action_eof;
				continue;
			} else if (exp_flageq1('u',arg)) {	/* treat process as user */
				argc--;argv++;
				if (argc < 1) {
					exp_error(interp,"-u needs argument");
					return(TCL_ERROR);
				}
				replace_user_by_process = *argv;

				/* imply a "-input" */
				if (dash_input_count == 0) dash_input_count = 1;

				continue;
			} else if (exp_flageq1('o',arg)) {
				/* apply following patterns to opposite side */
				/* of interaction */

				end_km = &input_default->keymap;

				/* imply two "-input" */
				if (dash_input_count < 2) {
					dash_input_count = 2;
					inp = input_default;
					action_eof_ptr = &inp->action_eof;
				}
				continue;
			} else if (exp_flageq1('i',arg)) {
				/* substitute master */

				argc--;argv++;
/*				master = atoi(*argv);*/
				master_string = *argv;
				/* will be used later on */

				end_km = &input_default->keymap;

				/* imply two "-input" */
				if (dash_input_count < 2) {
					dash_input_count = 2;
					inp = input_default;
					action_eof_ptr = &inp->action_eof;
				}
				continue;
/*			} else if (exp_flageq("nocase",arg,3)) {*/
/*				next_case_sensitive = FALSE;*/
/*				continue;*/
			} else if (exp_flageq("echo",arg,4)) {
				next_echo = TRUE;
				continue;
			} else if (exp_flageq("nobuffer",arg,3)) {
				next_writethru = TRUE;
				continue;
			} else if (exp_flageq("indices",arg,3)) {
				next_indices = TRUE;
				continue;
			} else if (exp_flageq1('f',arg)) {
				/* leftover from "fast" days */
				continue;
			} else if (exp_flageq("reset",arg,5)) {
				next_tty_reset = TRUE;
				continue;
			} else if (exp_flageq1('F',arg)) {
				/* leftover from "fast" days */
				continue;
			} else if (exp_flageq("iread",arg,2)) {
				next_iread = TRUE;
				continue;
			} else if (exp_flageq("iwrite",arg,2)) {
				next_iwrite = TRUE;
				continue;
			} else if (exp_flageq("eof",arg,3)) {
				struct action *action;

				argc--;argv++;
				debuglog("-eof is deprecated, use eof\r\n");
				*action_eof_ptr = action = new_action(&action_base);
				action->statement = *argv;
				action->tty_reset = next_tty_reset;
				next_tty_reset = FALSE;
				action->iwrite = next_iwrite;
				next_iwrite = FALSE;
				action->iread = next_iread;
				next_iread = FALSE;
				action->timestamp = next_timestamp;
				next_timestamp = FALSE;

				continue;
			} else if (exp_flageq("timeout",arg,7)) {
				int t;
				struct action *action;
				debuglog("-timeout is deprecated, use timeout\r\n");

				argc--;argv++;
				if (argc < 1) {
					exp_error(interp,"-timeout needs time");
					return(TCL_ERROR);
				}

				t = atoi(*argv);
				argc--;argv++;
				if (t != -1)
					arbitrary_timeout = t;
				/* we need an arbitrary timeout to start */
				/* search for lowest one later */

#if 0
				/* if -timeout comes before "-input", then applies */
				/* to all descriptors, else just the current one */
				if (dash_input_count > 0) {
					timeout_simple = FALSE;
					action = inp->action_timeout = 
						new_action(&action_base);
					inp->timeout_nominal = t;
				} else {
					action = &action_timeout;
					default_timeout = t;
				}
#endif
				timeout_simple = FALSE;
				action = inp->action_timeout = new_action(&action_base);
				inp->timeout_nominal = t;

				action->statement = *argv;
				action->tty_reset = next_tty_reset;
				next_tty_reset = FALSE;
				action->iwrite = next_iwrite;
				next_iwrite = FALSE;
				action->iread = next_iread;
				next_iread = FALSE;
				action->timestamp = next_timestamp;
				next_timestamp = FALSE;
				continue;
			} else if (exp_flageq("timestamp",arg,2)) {
				debuglog("-timestamp is deprecated, use exp_timestamp command\r\n");
				next_timestamp = TRUE;
				continue;
			} else if (exp_flageq("nobrace",arg,7)) {
				/* nobrace does nothing but take up space */
				/* on the command line which prevents */
				/* us from re-expanding any command lines */
				/* of one argument that looks like it should */
				/* be expanded to multiple arguments. */
				continue;
			}
		}

		/*
		 * pick up the pattern
		 */

		km = new(struct keymap);

		/* so that we can match in order user specified */
		/* link to end of keymap list */
		*end_km = km;
		km->next = 0;
		end_km = &km->next;

		km->echo = next_echo;
		km->writethru = next_writethru;
		km->indices = next_indices;
		km->action.tty_reset = next_tty_reset;
		km->action.iwrite = next_iwrite;
		km->action.iread = next_iread;
		km->action.timestamp = next_timestamp;
/*		km->case_sensitive = next_case_sensitive;*/

		next_indices = next_echo = next_writethru = FALSE;
		next_tty_reset = FALSE;
		next_iwrite = next_iread = FALSE;
/*		next_case_sensitive = TRUE;*/

		km->keys = *argv;

		km->null = FALSE;
		km->re = 0;
		if (next_re) {
			TclRegError((char *)0);
			if (0 == (km->re = TclRegComp(*argv))) {
				exp_error(interp,"bad regular expression: %s",
								TclGetRegError());
				return(TCL_ERROR);
			}
			next_re = FALSE;
		} if (next_null) {
			km->null = TRUE;
			next_null = FALSE;
		}

		argc--;argv++;

		km->action.statement = *argv;
		debuglog("defining key %s, action %s\r\n",
		 km->keys,
		 km->action.statement?(dprintify(km->action.statement))
				   :interpreter_cmd);

		/* imply a "-input" */
		if (dash_input_count == 0) dash_input_count = 1;
	}

	/* if the user has not supplied either "-output" for the */
	/* default two "-input"s, fix them up here */

	if (!input_user->output) {
		struct output *o = new(struct output);
		if (master_string == 0) {
			if (0 == exp_update_master(interp,&master,1,1)) {
				return(TCL_ERROR);
			}
			o->i_list = exp_new_i_simple(master,EXP_TEMPORARY);
		} else {
			o->i_list = exp_new_i_complex(interp,master_string,
					EXP_TEMPORARY,inter_updateproc);
		}
#if 0
		if (master == EXP_SPAWN_ID_BAD) {
			if (0 == exp_update_master(interp,&master,1,1)) {
				return(TCL_ERROR);
			}
		}
		o->i_list = exp_new_i_simple(master,EXP_TEMPORARY);
#endif
		o->next = 0;	/* no one else */
		o->action_eof = &action_eof;
		input_user->output = o;
	}

	if (!input_default->output) {
		struct output *o = new(struct output);
		o->i_list = exp_new_i_simple(1,EXP_TEMPORARY);/* stdout by default */
		o->next = 0;	/* no one else */
		o->action_eof = &action_eof;
		input_default->output = o;
	}

	/* if user has given "-u" flag, substitute process for user */
	/* in first two -inputs */
	if (replace_user_by_process) {
		/* through away old ones */
		exp_free_i(interp,input_user->i_list,   inter_updateproc);
		exp_free_i(interp,input_default->output->i_list,inter_updateproc);

		/* replace with arg to -u */
		input_user->i_list = exp_new_i_complex(interp,
				replace_user_by_process,
				EXP_TEMPORARY,inter_updateproc);
		input_default->output->i_list = exp_new_i_complex(interp,
				replace_user_by_process,
				EXP_TEMPORARY,inter_updateproc);
	}

	/*
	 * now fix up for default spawn id
	 */

	/* user could have replaced it with an indirect, so force update */
	if (input_default->i_list->direct == EXP_INDIRECT) {
		exp_i_update(interp,input_default->i_list);
	}

	if    (input_default->i_list->fd_list
	   && (input_default->i_list->fd_list->fd == EXP_SPAWN_ID_BAD)) {
		if (master_string == 0) {
			if (0 == exp_update_master(interp,&master,1,1)) {
				return(TCL_ERROR);
			}
			input_default->i_list->fd_list->fd = master;
		} else {
			/* discard old one and install new one */
			exp_free_i(interp,input_default->i_list,inter_updateproc);
			input_default->i_list = exp_new_i_complex(interp,master_string,
				EXP_TEMPORARY,inter_updateproc);
		}
#if 0
		if (master == EXP_SPAWN_ID_BAD) {
			if (0 == exp_update_master(interp,&master,1,1)) {
				return(TCL_ERROR);
			}
		}
		input_default->i_list->fd_list->fd = master;
#endif
	}

	/*
	 * check for user attempting to interact with self
	 * they're almost certainly just fooling around
	 */

	/* user could have replaced it with an indirect, so force update */
	if (input_user->i_list->direct == EXP_INDIRECT) {
		exp_i_update(interp,input_user->i_list);
	}

	if (input_user->i_list->fd_list && input_default->i_list->fd_list
	    && (input_user->i_list->fd_list->fd == input_default->i_list->fd_list->fd)) {
		exp_error(interp,"cannot interact with self - set spawn_id to a spawned process");
		return(TCL_ERROR);
	}

	fd_list = 0;
	fd_to_input = 0;

	/***************************************************************/
	/* all data structures are sufficiently set up that we can now */
	/* "finish()" to terminate this procedure                      */
	/***************************************************************/

	status = update_interact_fds(interp,&input_count,&fd_to_input,&fd_list,input_base,1,&configure_count,&real_tty);
	if (status == TCL_ERROR) finish(TCL_ERROR);

	if (real_tty) {
		tty_changed = exp_tty_raw_noecho(interp,&tty_old,&was_raw,&was_echo);
	}

	for (inp = input_base,i=0;inp;inp=inp->next,i++) {
		/* start timers */
		inp->timeout_remaining = inp->timeout_nominal;
	    }

	key = expect_key++;

	/* declare ourselves "in sync" with external view of close/indirect */
	configure_count = exp_configure_count;

#ifndef SIMPLE_EVENT
	/* loop waiting (in event handler) for input */
	for (;;) {
		int te;	/* result of Tcl_Eval */
		struct exp_f *u;
		int rc;	/* return code from ready.  This is further */
			/* refined by matcher. */
		int cc;	/* chars count from read() */
		int m;	/* master */
		int m_out; /* where master echoes to */
		struct action *action = 0;
		time_t previous_time;
		time_t current_time;
		int match_length, skip;
		int change;	/* if action requires cooked mode */
		int attempt_match = TRUE;
		struct input *soonest_input;
		int print;		/* # of chars to print */
		int oldprinted;		/* old version of u->printed */

		int timeout;	/* current as opposed to default_timeout */

		/* calculate how long to wait */
		/* by finding shortest remaining timeout */
		if (timeout_simple) {
			timeout = default_timeout;
		} else {
			timeout = arbitrary_timeout;

			for (inp=input_base;inp;inp=inp->next) {
				if ((inp->timeout_remaining != EXP_TIME_INFINITY) &&
				    (inp->timeout_remaining <= timeout)) {
					soonest_input = inp;
					timeout = inp->timeout_remaining;
				}
			}

			time(&previous_time);
			/* timestamp here rather than simply saving old */
			/* current time (after ready()) to account for */
			/* possibility of slow actions */

			/* timeout can actually be EXP_TIME_INFINITY here if user */
			/* explicitly supplied it in a few cases (or */
			/* the count-down code is broken) */
		}

		/* update the world, if necessary */
		if (configure_count != exp_configure_count) {
			status = update_interact_fds(interp,&input_count,
					&fd_to_input,&fd_list,input_base,1,
					&configure_count,&real_tty);
			if (status) finish(status);
		}

		rc = exp_get_next_event(interp,fd_list,input_count,&m,timeout,key);
		if (rc == EXP_TCLERROR) return(TCL_ERROR);

		if (rc == EXP_RECONFIGURE) continue;

		if (rc == EXP_TIMEOUT) {
			if (timeout_simple) {
				action = &action_timeout;
				goto got_action;
			} else {
				action = soonest_input->action_timeout;
				/* arbitrarily pick first fd out of list */
				m = soonest_input->i_list->fd_list->fd;
			}
		}
		if (!timeout_simple) {
			int time_diff;

			time(&current_time);
			time_diff = current_time - previous_time;

			/* update all timers */
			for (inp=input_base;inp;inp=inp->next) {
				if (inp->timeout_remaining != EXP_TIME_INFINITY) {
					inp->timeout_remaining -= time_diff;
					if (inp->timeout_remaining < 0)
						inp->timeout_remaining = 0;
				}
			}
		}

		/* at this point, we have some kind of event which can be */
		/* immediately processed - i.e. something that doesn't block */

		/* figure out who we are */
		inp = fd_to_input[m];
/*		u = inp->f;*/
		u = exp_fs+m;

		/* reset timer */
		inp->timeout_remaining = inp->timeout_nominal;

		switch (rc) {
		case EXP_DATA_NEW:
			if (u->size == u->msize) {
			    /* In theory, interact could be invoked when this situation */
			    /* already exists, hence the "probably" in the warning below */

			    debuglog("WARNING: interact buffer is full, probably because your\r\n");
			    debuglog("patterns have matched all of it but require more chars\r\n");
			    debuglog("in order to complete the match.\r\n");
			    debuglog("Dumping first half of buffer in order to continue\r\n");
			    debuglog("Recommend you enlarge the buffer or fix your patterns.\r\n");
			    exp_buffer_shuffle(interp,u,0,INTER_OUT,"interact");
		        }
			cc = read(m,	u->buffer + u->size,
					u->msize - u->size);
			if (cc > 0) {
				u->key = key;
				u->size += cc;
				u->buffer[u->size] = '\0';

				/* strip parity if requested */
				if (u->parity == 0) {
					/* do it from end backwards */
					char *p = u->buffer + u->size - 1;
					int count = cc;
					while (count--) {
						*p-- &= 0x7f;
					}
				}

				/* avoid another function call if possible */
				if (debugfile || is_debugging) {
					debuglog("spawn id %d sent <%s>\r\n",m,
						exp_printify(u->buffer + u->size - cc));
				}
				break;
			}

			rc = EXP_EOF;
			/* Most systems have read() return 0, allowing */
			/* control to fall thru and into this code.  On some */
			/* systems (currently HP and new SGI), read() does */
			/* see eof, and it must be detected earlier.  Then */
			/* control jumps directly to this EXP_EOF label. */

			/*FALLTHRU*/
		case EXP_EOF:
			action = inp->action_eof;
			attempt_match = FALSE;
			skip = u->size;
			debuglog("interact: received eof from spawn_id %d\r\n",m);
			/* actual close is done later so that we have a */
			/* chance to flush out any remaining characters */
			need_to_close_master = TRUE;

#if EOF_SO
			/* should really check for remaining chars and */
			/* flush them but this will only happen in the */
			/* unlikely scenario that there are partially */
			/* matched buffered chars. */
			/* So for now, indicate no chars to skip. */
			skip = 0;
			exp_close(interp,m);
#endif
			break;
		case EXP_DATA_OLD:
			cc = 0;
			break;
		case EXP_TIMEOUT:
			action = inp->action_timeout;
			attempt_match = FALSE;
			skip = u->size;
			break;
		}

		km = 0;

		if (attempt_match) {
			rc = in_keymap(u->buffer,u->size,inp->keymap,
				&km,&match_length,&skip,u->rm_nulls);
		} else {
			attempt_match = TRUE;
		}

		/* put regexp result in variables */
		if (km && km->re) {
#define out(var,val)  debuglog("expect: set %s(%s) \"%s\"\r\n",INTER_OUT,var, \
						dprintify(val)); \
		    Tcl_SetVar2(interp,INTER_OUT,var,val,0);

			char name[20], value[20];
			regexp *re = km->re;
			char match_char;/* place to hold char temporarily */
					/* uprooted by a NULL */

			for (i=0;i<NSUBEXP;i++) {
				int offset;

				if (re->startp[i] == 0) continue;

				if (km->indices) {
				  /* start index */
				  sprintf(name,"%d,start",i);
				  offset = re->startp[i]-u->buffer;
				  sprintf(value,"%d",offset);
				  out(name,value);

				  /* end index */
				  sprintf(name,"%d,end",i);
				  sprintf(value,"%d",re->endp[i]-u->buffer-1);
				  out(name,value);
				}

				/* string itself */
				sprintf(name,"%d,string",i);
				/* temporarily null-terminate in */
				/* middle */
				match_char = *re->endp[i];
				*re->endp[i] = 0;
				out(name,re->startp[i]);
				*re->endp[i] = match_char;
			}
		}

		/*
		 * dispose of chars that should be skipped
		 * i.e., chars that cannot possibly be part of a match.
		 */
		
		/* "skip" is count of chars not involved in match */
		/* "print" is count with chars involved in match */

		if (km && km->writethru) {
			print = skip + match_length;
		} else print = skip;

		/*
		 * echo chars if appropriate
		 */
		if (km && km->echo) {
			int seen;	/* either printed or echoed */

			/* echo to stdout rather than stdin */
			m_out = (m == 0)?1:m;

			/* write is unlikely to fail, since we just read */
			/* from same descriptor */
			seen = u->printed + u->echoed;
			if (skip >= seen) {
				write(m_out,u->buffer+skip,match_length);
			} else if ((match_length + skip - seen) > 0) {
				write(m_out,u->buffer+seen,match_length+skip-seen);
			}
			u->echoed = match_length + skip - u->printed;
		}

		oldprinted = u->printed;

		/* If expect has left characters in buffer, it has */
		/* already echoed them to the screen, thus we must */
		/* prevent them being rewritten.  Unfortunately this */
		/* gives the possibility of matching chars that have */
		/* already been output, but we do so since the user */
		/* could have avoided it by flushing the output */
		/* buffers directly. */
		if (print > u->printed) {	/* usual case */
			int wc;	/* return code from write() */
			for (outp = inp->output;outp;outp=outp->next) {
			    struct exp_fd_list *fdp;
			    for (fdp = outp->i_list->fd_list;fdp;fdp=fdp->next) {
				int od;	/* output descriptor */

				/* send to logfile if open */
				/* and user is seeing it */
				if (logfile && real_tty_output(fdp->fd)) {
					fwrite(u->buffer+u->printed,1,
					       print - u->printed,logfile);
				}

				/* send to each output descriptor */
				od = fdp->fd;
				/* if opened by Tcl, it may use a different */
				/* output descriptor */
				od = (exp_fs[od].tcl_handle?exp_fs[od].tcl_output:od);

				wc = write(od,u->buffer+u->printed,
					print - u->printed);
				if (wc <= 0) {
					debuglog("interact: write on spawn id %d failed (%s)\r\n",fdp->fd,Tcl_PosixError(interp));
					action = outp->action_eof;
					change = (action && action->tty_reset);

					if (change && tty_changed)
						exp_tty_set(interp,&tty_old,was_raw,was_echo);
					te = inter_eval(interp,action,m);

					if (change && real_tty) tty_changed =
					   exp_tty_raw_noecho(interp,&tty_old,&was_raw,&was_echo);
					switch (te) {
					case TCL_BREAK:
					case TCL_CONTINUE:
						finish(te);
					case EXP_TCL_RETURN:
						finish(TCL_RETURN);
					case TCL_RETURN:
						finish(TCL_OK);
					case TCL_OK:
						/* god knows what the user might */
						/* have done to us in the way of */
						/* closed fds, so .... */
						action = 0;	/* reset action */
						continue;
					default:
						finish(te);
					}
				}
			    }
			}
			u->printed = print;
		}

		/* u->printed is now accurate with respect to the buffer */
		/* However, we're about to shift the old data out of the */
		/* buffer.  Thus, u->size, printed, and echoed must be */
		/* updated */

		/* first update size based on skip information */
		/* then set skip to the total amount skipped */

		if (rc == EXP_MATCH) {
			action = &km->action;

			skip += match_length;
			u->size -= skip;

			if (u->size) {
				memcpy(u->buffer, u->buffer + skip, u->size);
				exp_lowmemcpy(u->lower,u->buffer+ skip, u->size);
			}
		} else {
			if (skip) {
				u->size -= skip;
				memcpy(u->buffer, u->buffer + skip, u->size);
				exp_lowmemcpy(u->lower,u->buffer+ skip, u->size);
			}
		}

#if EOF_SO
		/* as long as buffer is still around, null terminate it */
		if (rc != EXP_EOF) {
			u->buffer[u->size] = '\0';
			u->lower [u->size] = '\0';
		}
#else
		u->buffer[u->size] = '\0';
		u->lower [u->size] = '\0';
#endif

		/* now update printed based on total amount skipped */

		u->printed -= skip;
		/* if more skipped than printed (i.e., keymap encountered) */
		/* for printed positive */
		if (u->printed < 0) u->printed = 0;

		/* if we are in the middle of a match, force the next event */
		/* to wait for more data to arrive */
		u->force_read = (rc == EXP_CANMATCH);

		/* finally reset echoed if necessary */
		if (rc != EXP_CANMATCH) {
			if (skip >= oldprinted + u->echoed) u->echoed = 0;
		}

		if (rc == EXP_EOF) {
			exp_close(interp,m);
			need_to_close_master = FALSE;
		}

		if (action) {
got_action:
			change = (action && action->tty_reset);
			if (change && tty_changed)
				exp_tty_set(interp,&tty_old,was_raw,was_echo);

			te = inter_eval(interp,action,m);

			if (change && real_tty) tty_changed =
			   exp_tty_raw_noecho(interp,&tty_old,&was_raw,&was_echo);
			switch (te) {
			case TCL_BREAK:
			case TCL_CONTINUE:
				finish(te);
			case EXP_TCL_RETURN:
				finish(TCL_RETURN);
			case TCL_RETURN:
				finish(TCL_OK);
			case TCL_OK:
				/* god knows what the user might */
				/* have done to us in the way of */
				/* closed fds, so .... */
				action = 0;	/* reset action */
				continue;
			default:
				finish(te);
			}
		}
	}

#else /* SIMPLE_EVENT */
/*	deferred_interrupt = FALSE;*/
{
		int te;	/* result of Tcl_Eval */
		struct exp_f *u;
		int rc;	/* return code from ready.  This is further */
			/* refined by matcher. */
		int cc;	/* chars count from read() */
		int m;	/* master */
		struct action *action = 0;
		time_t previous_time;
		time_t current_time;
		int match_length, skip;
		int change;	/* if action requires cooked mode */
		int attempt_match = TRUE;
		struct input *soonest_input;
		int print;		/* # of chars to print */
		int oldprinted;		/* old version of u->printed */

		int timeout;	/* current as opposed to default_timeout */

	if (-1 == (pid = fork())) {
		exp_error(interp,"fork: %s",Tcl_PosixError(interp));
		finish(TCL_ERROR);
	}
	if (pid == 0) { /* child - send process output to user */
	    exp_close(interp,0);

	    m = fd_list[1];	/* get 2nd fd */
	    input_count = 1;

	    while (1) {

		/* calculate how long to wait */
		/* by finding shortest remaining timeout */
		if (timeout_simple) {
			timeout = default_timeout;
		} else {
			timeout = arbitrary_timeout;

			for (inp=input_base;inp;inp=inp->next) {
				if ((inp->timeout_remaining != EXP_TIME_INFINITY) &&
				    (inp->timeout_remaining < timeout))
					soonest_input = inp;
					timeout = inp->timeout_remaining;
			}

			time(&previous_time);
			/* timestamp here rather than simply saving old */
			/* current time (after ready()) to account for */
			/* possibility of slow actions */

			/* timeout can actually be EXP_TIME_INFINITY here if user */
			/* explicitly supplied it in a few cases (or */
			/* the count-down code is broken) */
		}

		/* +1 so we can look at the "other" file descriptor */
		rc = exp_get_next_event(interp,fd_list+1,input_count,&m,timeout,key);
		if (!timeout_simple) {
			int time_diff;

			time(&current_time);
			time_diff = current_time - previous_time;

			/* update all timers */
			for (inp=input_base;inp;inp=inp->next) {
				if (inp->timeout_remaining != EXP_TIME_INFINITY) {
					inp->timeout_remaining -= time_diff;
					if (inp->timeout_remaining < 0)
						inp->timeout_remaining = 0;
				}
			}
		}

		/* at this point, we have some kind of event which can be */
		/* immediately processed - i.e. something that doesn't block */

		/* figure out who we are */
		inp = fd_to_input[m];
/*		u = inp->f;*/
		u = exp_fs+m;

		switch (rc) {
		case EXP_DATA_NEW:
			cc = read(m,	u->buffer + u->size,
					u->msize - u->size);
			if (cc > 0) {
				u->key = key;
				u->size += cc;
				u->buffer[u->size] = '\0';

				/* strip parity if requested */
				if (u->parity == 0) {
					/* do it from end backwards */
					char *p = u->buffer + u->size - 1;
					int count = cc;
					while (count--) {
						*p-- &= 0x7f;
					}
				}

				/* avoid another function call if possible */
				if (debugfile || is_debugging) {
					debuglog("spawn id %d sent <%s>\r\n",m,
						exp_printify(u->buffer + u->size - cc));
				}
				break;
			}
			/*FALLTHRU*/

			/* Most systems have read() return 0, allowing */
			/* control to fall thru and into this code.  On some */
			/* systems (currently HP and new SGI), read() does */
			/* see eof, and it must be detected earlier.  Then */
			/* control jumps directly to this EXP_EOF label. */
		case EXP_EOF:
			action = inp->action_eof;
			attempt_match = FALSE;
			skip = u->size;
			rc = EXP_EOF;
			debuglog("interact: child received eof from spawn_id %d\r\n",m);
			exp_close(interp,m);
			break;
		case EXP_DATA_OLD:
			cc = 0;
			break;
		}

		km = 0;

		if (attempt_match) {
			rc = in_keymap(u->buffer,u->size,inp->keymap,
				&km,&match_length,&skip);
		} else {
			attempt_match = TRUE;
		}

		/* put regexp result in variables */
		if (km && km->re) {
#define INTER_OUT "interact_out"
#define out(i,val)  debuglog("expect: set %s(%s) \"%s\"\r\n",INTER_OUT,i, \
						dprintify(val)); \
		    Tcl_SetVar2(interp,INTER_OUT,i,val,0);

			char name[20], value[20];
			regexp *re = km->re;
			char match_char;/* place to hold char temporarily */
					/* uprooted by a NULL */

			for (i=0;i<NSUBEXP;i++) {
				int offset;

				if (re->startp[i] == 0) continue;

				if (km->indices) {
				  /* start index */
				  sprintf(name,"%d,start",i);
				  offset = re->startp[i]-u->buffer;
				  sprintf(value,"%d",offset);
				  out(name,value);

				  /* end index */
				  sprintf(name,"%d,end",i);
				  sprintf(value,"%d",re->endp[i]-u->buffer-1);
				  out(name,value);
				}

				/* string itself */
				sprintf(name,"%d,string",i);
				/* temporarily null-terminate in */
				/* middle */
				match_char = *re->endp[i];
				*re->endp[i] = 0;
				out(name,re->startp[i]);
				*re->endp[i] = match_char;
			}
		}

		/* dispose of chars that should be skipped */
		
		/* skip is chars not involved in match */
		/* print is with chars involved in match */

		if (km && km->writethru) {
			print = skip + match_length;
		} else print = skip;

		/* figure out if we should echo any chars */
		if (km && km->echo) {
			int seen;	/* either printed or echoed */

			/* echo to stdout rather than stdin */
			if (m == 0) m = 1;

			/* write is unlikely to fail, since we just read */
			/* from same descriptor */
			seen = u->printed + u->echoed;
			if (skip >= seen) {
				write(m,u->buffer+skip,match_length);
			} else if ((match_length + skip - seen) > 0) {
				write(m,u->buffer+seen,match_length+skip-seen);
			}
			u->echoed = match_length + skip - u->printed;
		}

		oldprinted = u->printed;

		/* If expect has left characters in buffer, it has */
		/* already echoed them to the screen, thus we must */
		/* prevent them being rewritten.  Unfortunately this */
		/* gives the possibility of matching chars that have */
		/* already been output, but we do so since the user */
		/* could have avoided it by flushing the output */
		/* buffers directly. */
		if (print > u->printed) {	/* usual case */
			int wc;	/* return code from write() */
			for (outp = inp->output;outp;outp=outp->next) {
			    struct exp_fd_list *fdp;
			    for (fdp = outp->i_list->fd_list;fdp;fdp=fdp->next) {
				int od;	/* output descriptor */

				/* send to logfile if open */
				/* and user is seeing it */
				if (logfile && real_tty_output(fdp->fd)) {
					fwrite(u->buffer+u->printed,1,
					       print - u->printed,logfile);
				}

				/* send to each output descriptor */
				od = fdp->fd;
				/* if opened by Tcl, it may use a different */
				/* output descriptor */
				od = (exp_fs[od].tcl_handle?exp_fs[od].tcl_output:od);

				wc = write(od,u->buffer+u->printed,
					print - u->printed);
				if (wc <= 0) {
					debuglog("interact: write on spawn id %d failed (%s)\r\n",fdp->fd,Tcl_PosixError(interp));
					action = outp->action_eof;

					te = inter_eval(interp,action,m);

					switch (te) {
					case TCL_BREAK:
					case TCL_CONTINUE:
						finish(te);
					case EXP_TCL_RETURN:
						finish(TCL_RETURN);
					case TCL_RETURN:
						finish(TCL_OK);
					case TCL_OK:
						/* god knows what the user might */
						/* have done to us in the way of */
						/* closed fds, so .... */
						action = 0;	/* reset action */
						continue;
					default:
						finish(te);
					}
				}
			    }
			}
			u->printed = print;
		}

		/* u->printed is now accurate with respect to the buffer */
		/* However, we're about to shift the old data out of the */
		/* buffer.  Thus, u->size, printed, and echoed must be */
		/* updated */

		/* first update size based on skip information */
		/* then set skip to the total amount skipped */

		if (rc == EXP_MATCH) {
			action = &km->action;

			skip += match_length;
			u->size -= skip;

			if (u->size)
				memcpy(u->buffer, u->buffer + skip, u->size);
				exp_lowmemcpy(u->lower,u->buffer+ skip, u->size);
		} else {
			if (skip) {
				u->size -= skip;
				memcpy(u->buffer, u->buffer + skip, u->size);
				exp_lowmemcpy(u->lower,u->buffer+ skip, u->size);
			}
		}

		/* as long as buffer is still around, null terminate it */
		if (rc != EXP_EOF) {
			u->buffer[u->size] = '\0';
			u->lower [u->size] = '\0';
		}
		/* now update printed based on total amount skipped */

		u->printed -= skip;
		/* if more skipped than printed (i.e., keymap encountered) */
		/* for printed positive */
		if (u->printed < 0) u->printed = 0;

		/* if we are in the middle of a match, force the next event */
		/* to wait for more data to arrive */
		u->force_read = (rc == EXP_CANMATCH);

		/* finally reset echoed if necessary */
		if (rc != EXP_CANMATCH) {
			if (skip >= oldprinted + u->echoed) u->echoed = 0;
		}

		if (action) {
			te = inter_eval(interp,action,m);
			switch (te) {
			case TCL_BREAK:
			case TCL_CONTINUE:
				finish(te);
			case EXP_TCL_RETURN:
				finish(TCL_RETURN);
			case TCL_RETURN:
				finish(TCL_OK);
			case TCL_OK:
				/* god knows what the user might */
				/* have done to us in the way of */
				/* closed fds, so .... */
				action = 0;	/* reset action */
				continue;
			default:
				finish(te);
			}
		}
	    }
	} else { /* parent - send user keystrokes to process */
#include <signal.h>

#if defined(SIGCLD) && !defined(SIGCHLD)
#define SIGCHLD SIGCLD
#endif
		debuglog("fork = %d\r\n",pid);
		signal(SIGCHLD,sigchld_handler);
/*	restart:*/
/*		tty_changed = exp_tty_raw_noecho(interp,&tty_old,&was_raw,&was_echo);*/

	    m = fd_list[0];	/* get 1st fd */
	    input_count = 1;

	    while (1) {
		/* calculate how long to wait */
		/* by finding shortest remaining timeout */
		if (timeout_simple) {
			timeout = default_timeout;
		} else {
			timeout = arbitrary_timeout;

			for (inp=input_base;inp;inp=inp->next) {
				if ((inp->timeout_remaining != EXP_TIME_INFINITY) &&
				    (inp->timeout_remaining < timeout))
					soonest_input = inp;
					timeout = inp->timeout_remaining;
			}

			time(&previous_time);
			/* timestamp here rather than simply saving old */
			/* current time (after ready()) to account for */
			/* possibility of slow actions */

			/* timeout can actually be EXP_TIME_INFINITY here if user */
			/* explicitly supplied it in a few cases (or */
			/* the count-down code is broken) */
		}

		rc = exp_get_next_event(interp,fd_list,input_count,&m,timeout,key);
		if (!timeout_simple) {
			int time_diff;

			time(&current_time);
			time_diff = current_time - previous_time;

			/* update all timers */
			for (inp=input_base;inp;inp=inp->next) {
				if (inp->timeout_remaining != EXP_TIME_INFINITY) {
					inp->timeout_remaining -= time_diff;
					if (inp->timeout_remaining < 0)
						inp->timeout_remaining = 0;
				}
			}
		}

		/* at this point, we have some kind of event which can be */
		/* immediately processed - i.e. something that doesn't block */

		/* figure out who we are */
		inp = fd_to_input[m];
/*		u = inp->f;*/
		u = exp_fs+m;

		switch (rc) {
		case EXP_DATA_NEW:
			cc = i_read(m,	u->buffer + u->size,
					u->msize - u->size);
			if (cc > 0) {
				u->key = key;
				u->size += cc;
				u->buffer[u->size] = '\0';

				/* strip parity if requested */
				if (u->parity == 0) {
					/* do it from end backwards */
					char *p = u->buffer + u->size - 1;
					int count = cc;
					while (count--) {
						*p-- &= 0x7f;
					}
				}

				/* avoid another function call if possible */
				if (debugfile || is_debugging) {
					debuglog("spawn id %d sent <%s>\r\n",m,
						exp_printify(u->buffer + u->size - cc));
				}
				break;
			} else if (cc == EXP_CHILD_EOF) {
				/* user could potentially have two outputs in which */
				/* case we might be looking at the wrong one, but */
				/* the likelihood of this is nil */
				action = inp->output->action_eof;
				attempt_match = FALSE;
				skip = u->size;
				rc = EXP_EOF;
				debuglog("interact: process died/eof\r\n");
				clean_up_after_child(interp,fd_list[1]);
				break;
			}
			/*FALLTHRU*/

			/* Most systems have read() return 0, allowing */
			/* control to fall thru and into this code.  On some */
			/* systems (currently HP and new SGI), read() does */
			/* see eof, and it must be detected earlier.  Then */
			/* control jumps directly to this EXP_EOF label. */
		case EXP_EOF:
			action = inp->action_eof;
			attempt_match = FALSE;
			skip = u->size;
			rc = EXP_EOF;
			debuglog("user sent EOF or disappeared\n\n");
			break;
		case EXP_DATA_OLD:
			cc = 0;
			break;
		}

		km = 0;

		if (attempt_match) {
			rc = in_keymap(u->buffer,u->size,inp->keymap,
				&km,&match_length,&skip);
		} else {
			attempt_match = TRUE;
		}

		/* put regexp result in variables */
		if (km && km->re) {
			char name[20], value[20];
			regexp *re = km->re;
			char match_char;/* place to hold char temporarily */
					/* uprooted by a NULL */

			for (i=0;i<NSUBEXP;i++) {
				int offset;

				if (re->startp[i] == 0) continue;

				if (km->indices) {
				  /* start index */
				  sprintf(name,"%d,start",i);
				  offset = re->startp[i]-u->buffer;
				  sprintf(value,"%d",offset);
				  out(name,value);

				  /* end index */
				  sprintf(name,"%d,end",i);
				  sprintf(value,"%d",re->endp[i]-u->buffer-1);
				  out(name,value);
				}

				/* string itself */
				sprintf(name,"%d,string",i);
				/* temporarily null-terminate in */
				/* middle */
				match_char = *re->endp[i];
				*re->endp[i] = 0;
				out(name,re->startp[i]);
				*re->endp[i] = match_char;
			}
		}

		/* dispose of chars that should be skipped */
		
		/* skip is chars not involved in match */
		/* print is with chars involved in match */

		if (km && km->writethru) {
			print = skip + match_length;
		} else print = skip;

		/* figure out if we should echo any chars */
		if (km && km->echo) {
			int seen;	/* either printed or echoed */

			/* echo to stdout rather than stdin */
			if (m == 0) m = 1;

			/* write is unlikely to fail, since we just read */
			/* from same descriptor */
			seen = u->printed + u->echoed;
			if (skip >= seen) {
				write(m,u->buffer+skip,match_length);
			} else if ((match_length + skip - seen) > 0) {
				write(m,u->buffer+seen,match_length+skip-seen);
			}
			u->echoed = match_length + skip - u->printed;
		}

		oldprinted = u->printed;

		/* If expect has left characters in buffer, it has */
		/* already echoed them to the screen, thus we must */
		/* prevent them being rewritten.  Unfortunately this */
		/* gives the possibility of matching chars that have */
		/* already been output, but we do so since the user */
		/* could have avoided it by flushing the output */
		/* buffers directly. */
		if (print > u->printed) {	/* usual case */
			int wc;	/* return code from write() */
			for (outp = inp->output;outp;outp=outp->next) {
			    struct exp_fd_list *fdp;
			    for (fdp = outp->i_list->fd_list;fdp;fdp=fdp->next) {
				int od;	/* output descriptor */

				/* send to logfile if open */
				/* and user is seeing it */
				if (logfile && real_tty_output(fdp->fd)) {
					fwrite(u->buffer+u->printed,1,
					       print - u->printed,logfile);
				}

				/* send to each output descriptor */
				od = fdp->fd;
				/* if opened by Tcl, it may use a different */
				/* output descriptor */
				od = (exp_fs[od].tcl_handle?exp_fs[od].tcl_output:od);

				wc = write(od,u->buffer+u->printed,
					print - u->printed);
				if (wc <= 0) {
					debuglog("interact: write on spawn id %d failed (%s)\r\n",fdp->fd,Tcl_PosixError(interp));
					clean_up_after_child(interp,fdp->fd);
					action = outp->action_eof;
					change = (action && action->tty_reset);
					if (change && tty_changed)
						exp_tty_set(interp,&tty_old,was_raw,was_echo);
					te = inter_eval(interp,action,m);

					if (change && real_tty) tty_changed =
					   exp_tty_raw_noecho(interp,&tty_old,&was_raw,&was_echo);
					switch (te) {
					case TCL_BREAK:
					case TCL_CONTINUE:
						finish(te);
					case EXP_TCL_RETURN:
						finish(TCL_RETURN);
					case TCL_RETURN:
						finish(TCL_OK);
					case TCL_OK:
						/* god knows what the user might */
						/* have done to us in the way of */
						/* closed fds, so .... */
						action = 0;	/* reset action */
						continue;
					default:
						finish(te);
					}
				}
			    }
			}
			u->printed = print;
		}

		/* u->printed is now accurate with respect to the buffer */
		/* However, we're about to shift the old data out of the */
		/* buffer.  Thus, u->size, printed, and echoed must be */
		/* updated */

		/* first update size based on skip information */
		/* then set skip to the total amount skipped */

		if (rc == EXP_MATCH) {
			action = &km->action;

			skip += match_length;
			u->size -= skip;

			if (u->size)
				memcpy(u->buffer, u->buffer + skip, u->size);
				exp_lowmemcpy(u->lower,u->buffer+ skip, u->size);
		} else {
			if (skip) {
				u->size -= skip;
				memcpy(u->buffer, u->buffer + skip, u->size);
				exp_lowmemcpy(u->lower,u->buffer+ skip, u->size);
			}
		}

		/* as long as buffer is still around, null terminate it */
		if (rc != EXP_EOF) {
			u->buffer[u->size] = '\0';
			u->lower [u->size] = '\0';
		}
		/* now update printed based on total amount skipped */

		u->printed -= skip;
		/* if more skipped than printed (i.e., keymap encountered) */
		/* for printed positive */
		if (u->printed < 0) u->printed = 0;

		/* if we are in the middle of a match, force the next event */
		/* to wait for more data to arrive */
		u->force_read = (rc == EXP_CANMATCH);

		/* finally reset echoed if necessary */
		if (rc != EXP_CANMATCH) {
			if (skip >= oldprinted + u->echoed) u->echoed = 0;
		}

		if (action) {
			change = (action && action->tty_reset);
			if (change && tty_changed)
				exp_tty_set(interp,&tty_old,was_raw,was_echo);

			te = inter_eval(interp,action,m);

			if (change && real_tty) tty_changed =
			   exp_tty_raw_noecho(interp,&tty_old,&was_raw,&was_echo);
			switch (te) {
			case TCL_BREAK:
			case TCL_CONTINUE:
				finish(te);
			case EXP_TCL_RETURN:
				finish(TCL_RETURN);
			case TCL_RETURN:
				finish(TCL_OK);
			case TCL_OK:
				/* god knows what the user might */
				/* have done to us in the way of */
				/* closed fds, so .... */
				action = 0;	/* reset action */
				continue;
			default:
				finish(te);
			}
		}
	    }
	}
}
#endif /* SIMPLE_EVENT */

 done:
#ifdef SIMPLE_EVENT
	/* force child to exit upon eof from master */
	if (pid == 0) {
		exit(SPAWNED_PROCESS_DIED);
	}
#endif /* SIMPLE_EVENT */

	if (need_to_close_master) exp_close(interp,master);

	if (tty_changed) exp_tty_set(interp,&tty_old,was_raw,was_echo);
	if (oldargv) ckfree((char *)argv);
	if (fd_list) ckfree((char *)fd_list);
	if (fd_to_input) ckfree((char *)fd_to_input);
	free_input(interp,input_base);
	free_action(action_base);

	return(status);
}

/* version of Tcl_Eval for interact */ 
static int
inter_eval(interp,action,spawn_id)
Tcl_Interp *interp;
struct action *action;
int spawn_id;
{
	int status;
	char value[20];

	/* deprecated */
	if (action->timestamp) {
		time_t current_time;
		time(&current_time);
		exp_timestamp(interp,&current_time,INTER_OUT);
	}
	/* deprecated */

	if (action->iwrite) {
		sprintf(value,"%d",spawn_id);
		out("spawn_id",value);
	}

	if (action->statement) {
		status = Tcl_Eval(interp,action->statement);
	} else {
		exp_nflog("\r\n",1);
		status = exp_interpreter(interp);
	}

	return status;
}

static void
free_keymap(km)
struct keymap *km;
{
	if (km == 0) return;
	free_keymap(km->next);

	ckfree((char *)km);
}

static void
free_action(a)
struct action *a;
{
	struct action *next;

	while (a) {
		next = a->next;
		ckfree((char *)a);
		a = next;
	}
}

static void
free_input(interp,i)
Tcl_Interp *interp;
struct input *i;
{
	if (i == 0) return;
	free_input(interp,i->next);

	exp_free_i(interp,i->i_list,inter_updateproc);
	free_output(interp,i->output);
	free_keymap(i->keymap);
	ckfree((char *)i);
}

static struct action *
new_action(base)
struct action **base;
{
	struct action *o = new(struct action);

	/* stick new action into beginning of list of all actions */
	o->next = *base;
	*base = o;

	return o;
}

static void
free_output(interp,o)
Tcl_Interp *interp;
struct output *o;
{
	if (o == 0) return;
	free_output(interp,o->next);
	exp_free_i(interp,o->i_list,inter_updateproc);

	ckfree((char *)o);
}

static struct exp_cmd_data cmd_data[]  = {
{"interact",	exp_proc(Exp_InteractCmd),	0,	0},
{0}};

void
exp_init_interact_cmds(interp)
Tcl_Interp *interp;
{
	exp_create_commands(interp,cmd_data);
}
