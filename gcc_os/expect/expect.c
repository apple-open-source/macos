/* expect.c - expect commands

Written by: Don Libes, NIST, 2/6/90

Design and implementation of this program was paid for by U.S. tax
dollars.  Therefore it is public domain.  However, the author and NIST
would appreciate credit if this program or parts of it are used.

*/

#include <sys/types.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>	/* for isspace */
#include <time.h>	/* for time(3) */
#if 0
#include <setjmp.h>
#endif

#include "expect_cf.h"

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "tcl.h"

#include "string.h"

#include "tclRegexp.h"
#include "exp_rename.h"
#include "exp_prog.h"
#include "exp_command.h"
#include "exp_log.h"
#include "exp_event.h"
#include "exp_tty.h"
#include "exp_tstamp.h"	/* this should disappear when interact */
			/* loses ref's to it */
#ifdef TCL_DEBUGGER
#include "Dbg.h"
#endif

/* CYGNUS LOCAL: dj/expect */
#if defined(__CYGWIN__) || defined(__CYGWIN32__)
/* Hack.  expect is a mongrel win32/cygwin app, so we can't rely on tcl to
   properly poll our descriptors.  This define lets timeouts work. */
#define SIMPLE_EVENT
#endif
/* END CYGNUS LOCAL: dj/expect */

/* initial length of strings that we can guarantee patterns can match */
int exp_default_match_max =	2000;
#define INIT_EXPECT_TIMEOUT_LIT	"10"	/* seconds */
#define INIT_EXPECT_TIMEOUT	10	/* seconds */
int exp_default_parity =	TRUE;
int exp_default_rm_nulls =	TRUE;

/* user variable names */
#define EXPECT_TIMEOUT		"timeout"
#define EXPECT_OUT		"expect_out"

/* 1 ecase struct is reserved for each case in the expect command.  Note that
eof/timeout don't use any of theirs, but the algorithm is simpler this way. */

struct ecase {	/* case for expect command */
	struct exp_i	*i_list;
	char *pat;	/* original pattern spec */
	char *body;	/* ptr to body to be executed upon match */
#define PAT_EOF		1
#define PAT_TIMEOUT	2
#define PAT_DEFAULT	3
#define PAT_FULLBUFFER	4
#define PAT_GLOB	5 /* glob-style pattern list */
#define PAT_RE		6 /* regular expression */
#define PAT_EXACT	7 /* exact string */
#define PAT_NULL	8 /* ASCII 0 */
#define PAT_TYPES	9 /* used to size array of pattern type descriptions */
	int use;	/* PAT_XXX */
	int simple_start;/* offset from start of buffer denoting where a */
			/* glob or exact match begins */
	int transfer;	/* if false, leave matched chars in input stream */
	int indices;	/* if true, write indices */
/*	int iwrite;*/	/* if true write spawn_id */
	int iread;	/* if true, reread indirects */
	int timestamp;	/* if true, write timestamps */
#define CASE_UNKNOWN	0
#define CASE_NORM	1
#define CASE_LOWER	2
	int Case;	/* convert case before doing match? */
	regexp *re;	/* if this is 0, then pattern match via glob */
};

/* descriptions of the pattern types, used for debugging */
char *pattern_style[PAT_TYPES];

struct exp_cases_descriptor {
	int count;
	struct ecase **cases;
};

/* This describes an Expect command */
static
struct exp_cmd_descriptor {
	int cmdtype;			/* bg, before, after */
	int duration;			/* permanent or temporary */
	int timeout_specified_by_flag;	/* if -timeout flag used */
	int timeout;			/* timeout period if flag used */
	struct exp_cases_descriptor ecd;
	struct exp_i *i_list;
} exp_cmds[4];
/* note that exp_cmds[FG] is just a fake, the real contents is stored
   in some dynamically-allocated variable.  We use exp_cmds[FG] mostly
   as a well-known address and also as a convenience and so we allocate
   just a few of its fields that we need. */

static void
exp_cmd_init(cmd,cmdtype,duration)
struct exp_cmd_descriptor *cmd;
int duration;
int cmdtype;
{
	cmd->duration = duration;
	cmd->cmdtype = cmdtype;
	cmd->ecd.cases = 0;
	cmd->ecd.count = 0;
	cmd->i_list = 0;
}

static int i_read_errno;/* place to save errno, if i_read() == -1, so it
			   doesn't get overwritten before we get to read it */
#if 0
static jmp_buf env;	/* for interruptable read() */
			/* longjmp(env,1) times out the read */
			/* longjmp(env,2) restarts the read */
static int env_valid = FALSE;	/* whether we can longjmp or not */
#endif

#ifdef SIMPLE_EVENT
static int alarm_fired;	/* if alarm occurs */
#endif

void exp_background_filehandlers_run_all();

/* exp_indirect_updateX is called by Tcl when an indirect variable is set */
static char *exp_indirect_update1();	/* 1-part Tcl variable names */
static char *exp_indirect_update2();	/* 2-part Tcl variable names */

static int	exp_i_read _ANSI_ARGS_((Tcl_Interp *,int,int,int));

#ifdef SIMPLE_EVENT
/*ARGSUSED*/
static RETSIGTYPE
sigalarm_handler(n)
int n;		       	/* unused, for compatibility with STDC */
{
	alarm_fired = TRUE;
#if 0
	/* check env_valid first to protect us from the alarm occurring */
	/* in the window between i_read and alarm(0) */
	if (env_valid) longjmp(env,1);
#endif /*0*/
}
#endif /*SIMPLE_EVENT*/

#if 0
/*ARGSUSED*/
static RETSIGTYPE
sigalarm_handler(n)
int n;		       	/* unused, for compatibility with STDC */
{
#ifdef REARM_SIG
	signal(SIGALRM,sigalarm_handler);
#endif

	/* check env_valid first to protect us from the alarm occurring */
	/* in the window between i_read and alarm(0) */
	if (env_valid) longjmp(env,1);
}
#endif /*0*/

#if 0

/* upon interrupt, act like timeout */
/*ARGSUSED*/
static RETSIGTYPE
sigint_handler(n)
int n;			/* unused, for compatibility with STDC */
{
#ifdef REARM_SIG
	signal(SIGINT,sigint_handler);/* not nec. for BSD, but doesn't hurt */
#endif

#ifdef TCL_DEBUGGER
	if (exp_tcl_debugger_available) {
		/* if the debugger is active and we're reading something, */
		/* force the debugger to go interactive now and when done, */
		/* restart the read.  */

		Dbg_On(exp_interp,env_valid);

		/* restart the read */
		if (env_valid) longjmp(env,2);

		/* if no read is in progess, just let debugger start at */
		/* the next command. */
		return;
	}
#endif

#if 0
/* the ability to timeout a read via ^C is hereby removed 8-Mar-1993 - DEL */

	/* longjmp if we are executing a read inside of expect command */
	if (env_valid) longjmp(env,1);
#endif

	/* if anywhere else in code, prepare to exit */
	exp_exit(exp_interp,0);
}
#endif /*0*/

/* remove nulls from s.  Initially, the number of chars in s is c, */
/* not strlen(s).  This count does not include the trailing null. */
/* returns number of nulls removed. */
static int
rm_nulls(s,c)
char *s;
int c;
{
	char *s2 = s;	/* points to place in original string to put */
			/* next non-null character */
	int count = 0;
	int i;

	for (i=0;i<c;i++,s++) {
		if (0 == *s) {
			count++;
			continue;
		}
		if (count) *s2 = *s;
		s2++;
	}
	return(count);
}

/* free up everything in ecase */
static void
free_ecase(interp,ec,free_ilist)
Tcl_Interp *interp;
struct ecase *ec;
int free_ilist;		/* if we should free ilist */
{
	if (ec->re) ckfree((char *)ec->re);

	if (ec->i_list->duration == EXP_PERMANENT) {
		if (ec->pat) ckfree(ec->pat);
		if (ec->body) ckfree(ec->body);
	}

	if (free_ilist) {
		ec->i_list->ecount--;
		if (ec->i_list->ecount == 0)
			exp_free_i(interp,ec->i_list,exp_indirect_update2);
	}

	ckfree((char *)ec);	/* NEW */
}

/* free up any argv structures in the ecases */
static void
free_ecases(interp,eg,free_ilist)
Tcl_Interp *interp;
struct exp_cmd_descriptor *eg;
int free_ilist;		/* if true, free ilists */
{
	int i;

	if (!eg->ecd.cases) return;

	for (i=0;i<eg->ecd.count;i++) {
		free_ecase(interp,eg->ecd.cases[i],free_ilist);
	}
	ckfree((char *)eg->ecd.cases);

	eg->ecd.cases = 0;
	eg->ecd.count = 0;
}


#if 0
/* no standard defn for this, and some systems don't even have it, so avoid */
/* the whole quagmire by calling it something else */
static char *exp_strdup(s)
char *s;
{
	char *news = ckalloc(strlen(s) + 1);
	strcpy(news,s);
	return(news);
}
#endif

/* In many places, there is no need to malloc a copy of a string, since it */
/* will be freed before we return to Tcl */
static void
save_str(lhs,rhs,nosave)
char **lhs;	/* left hand side */
char *rhs;	/* right hand side */
int nosave;
{
	if (nosave || (rhs == 0)) {
		*lhs = rhs;
	} else {
		*lhs = ckalloc(strlen(rhs) + 1);
		strcpy(*lhs,rhs);
	}
}

/* return TRUE if string appears to be a set of arguments
   The intent of this test is to support the ability of commands to have
   all their args braced as one.  This conflicts with the possibility of
   actually intending to have a single argument.
   The bad case is in expect which can have a single argument with embedded
   \n's although it's rare.  Examples that this code should handle:
   \n		FALSE (pattern)
   \n\n		FALSE
   \n  \n \n	FALSE
   foo		FALSE
   foo\n	FALSE
   \nfoo\n	TRUE  (set of args)
   \nfoo\nbar	TRUE

   Current test is very cheap and almost always right :-)
*/
int 
exp_one_arg_braced(p)
char *p;
{
	int seen_nl = FALSE;

	for (;*p;p++) {
		if (*p == '\n') {
			seen_nl = TRUE;
			continue;
		}

		if (!isspace(*p)) {
			return(seen_nl);
		}
	}
	return FALSE;
}

/* called to execute a command of only one argument - a hack to commands */
/* to be called with all args surrounded by an outer set of braces */
/* returns TCL_whatever */
/*ARGSUSED*/
int
exp_eval_with_one_arg(clientData,interp,argv)
ClientData clientData;
Tcl_Interp *interp;
char **argv;
{
	char *buf;
	int rc;
	char *a;

	/* + 11 is for " -nobrace " and null at end */
	buf = ckalloc(strlen(argv[0]) + strlen(argv[1]) + 11);
	/* recreate statement (with -nobrace to prevent recursion) */
	sprintf(buf,"%s -nobrace %s",argv[0],argv[1]);

	/*
	 * replace top-level newlines with blanks
	 */

	/* Should only be necessary to run over argv[1] and then sprintf */
	/* that into the buffer, but the ICEM guys insist that writing */
	/* back over the original arguments makes their Tcl compiler very */
	/* unhappy. */
	for (a=buf;*a;) {
		extern char *TclWordEnd();

		for (;isspace(*a);a++) {
			if (*a == '\n') *a = ' ';
		}
#if TCL_MAJOR_VERSION < 8
		a = TclWordEnd(a,0,(int *)0)+1;
#else
		a = TclWordEnd(a,&a[strlen(a)],0,(int *)0)+1;
#endif
	}

	rc = Tcl_Eval(interp,buf);

	ckfree(buf);
	return(rc);
}

static void
ecase_clear(ec)
struct ecase *ec;
{
	ec->i_list = 0;
	ec->pat = 0;
	ec->body = 0;
	ec->transfer = TRUE;
	ec->indices = FALSE;
/*	ec->iwrite = FALSE;*/
	ec->iread = FALSE;
	ec->timestamp = FALSE;
	ec->re = 0;
	ec->Case = CASE_NORM;
	ec->use = PAT_GLOB;
}

static struct ecase *
ecase_new()
{
	struct ecase *ec = (struct ecase *)ckalloc(sizeof(struct ecase));

	ecase_clear(ec);
	return ec;
}

/*

parse_expect_args parses the arguments to expect or its variants. 
It normally returns TCL_OK, and returns TCL_ERROR for failure.
(It can't return i_list directly because there is no way to differentiate
between clearing, say, expect_before and signalling an error.)

eg (expect_global) is initialized to reflect the arguments parsed
eg->ecd.cases is an array of ecases
eg->ecd.count is the # of ecases
eg->i_list is a linked list of exp_i's which represent the -i info

Each exp_i is chained to the next so that they can be easily free'd if
necessary.  Each exp_i has a reference count.  If the -i is not used
(e.g., has no following patterns), the ref count will be 0.

Each ecase points to an exp_i.  Several ecases may point to the same exp_i.
Variables named by indirect exp_i's are read for the direct values.

If called from a foreground expect and no patterns or -i are given, a
default exp_i is forced so that the command "expect" works right.

The exp_i chain can be broken by the caller if desired.

*/

static int
parse_expect_args(interp,eg,default_spawn_id,argc,argv)
Tcl_Interp *interp;
struct exp_cmd_descriptor *eg;
int default_spawn_id;	/* suggested master if called as expect_user or _tty */
int argc;
char **argv;
{
	int i;
	char *arg;
	struct ecase ec;	/* temporary to collect args */

	argv++;
	argc--;

	eg->timeout_specified_by_flag = FALSE;

	ecase_clear(&ec);

	/* Allocate an array to store the ecases.  Force array even if 0 */
	/* cases.  This will often be too large (i.e., if there are flags) */
	/* but won't affect anything. */

	eg->ecd.cases = (struct ecase **)ckalloc(
		sizeof(struct ecase *) * (1+(argc/2)));

	eg->ecd.count = 0;

	for (i = 0;i<argc;i++) {
		arg = argv[i];
	
		if (exp_flageq("timeout",arg,7)) {
			ec.use = PAT_TIMEOUT;
		} else if (exp_flageq("eof",arg,3)) {
			ec.use = PAT_EOF;
		} else if (exp_flageq("full_buffer",arg,11)) {
			ec.use = PAT_FULLBUFFER;
		} else if (exp_flageq("default",arg,7)) {
			ec.use = PAT_DEFAULT;
		} else if (exp_flageq("null",arg,4)) {
			ec.use = PAT_NULL;
		} else if (arg[0] == '-') {
			arg++;
			if (exp_flageq1('-',arg)		/* "--" is deprecated */
			  || exp_flageq("glob",arg,2)) {
				i++;
				/* assignment here is not actually necessary */
				/* since cases are initialized this way above */
				/* ec.use = PAT_GLOB; */
			} else if (exp_flageq("regexp",arg,2)) {
				i++;
				ec.use = PAT_RE;
				TclRegError((char *)0);
				if (!(ec.re = TclRegComp(argv[i]))) {
					exp_error(interp,"bad regular expression: %s",
								TclGetRegError());
					goto error;
				}
			} else if (exp_flageq("exact",arg,2)) {
				i++;
				ec.use = PAT_EXACT;
			} else if (exp_flageq("notransfer",arg,1)) {
				ec.transfer = 0;
				continue;
			} else if (exp_flageq("nocase",arg,3)) {
				ec.Case = CASE_LOWER;
				continue;
			} else if (exp_flageq1('i',arg)) {
				i++;
				if (i>=argc) {
					exp_error(interp,"-i requires following spawn_id");
					goto error;
				}

				ec.i_list = exp_new_i_complex(interp,argv[i],
					eg->duration,exp_indirect_update2);

				ec.i_list->cmdtype = eg->cmdtype;

				/* link new i_list to head of list */
				ec.i_list->next = eg->i_list;
				eg->i_list = ec.i_list;

				continue;
			} else if (exp_flageq("indices",arg,2)) {
				ec.indices = TRUE;
				continue;
			} else if (exp_flageq("iwrite",arg,2)) {
/*				ec.iwrite = TRUE;*/
				continue;
			} else if (exp_flageq("iread",arg,2)) {
				ec.iread = TRUE;
				continue;
			} else if (exp_flageq("timestamp",arg,2)) {
				ec.timestamp = TRUE;
				continue;
			} else if (exp_flageq("timeout",arg,2)) {
				i++;
				if (i>=argc) {
					exp_error(interp,"-timeout requires following # of seconds");
					goto error;
				}

				eg->timeout = atoi(argv[i]);
				eg->timeout_specified_by_flag = TRUE;
				continue;
			} else if (exp_flageq("nobrace",arg,7)) {
				/* nobrace does nothing but take up space */
				/* on the command line which prevents */
				/* us from re-expanding any command lines */
				/* of one argument that looks like it should */
				/* be expanded to multiple arguments. */
				continue;
			} else {
				exp_error(interp,"usage: unrecognized flag <%s>",arg);
				goto error;
			}
		}

		/* if no -i, use previous one */
		if (!ec.i_list) {
			/* if no -i flag has occurred yet, use default */
			if (!eg->i_list) {
				if (default_spawn_id != EXP_SPAWN_ID_BAD) {
					eg->i_list = exp_new_i_simple(default_spawn_id,eg->duration);
				} else {
					/* it'll be checked later, if used */
					(void) exp_update_master(interp,&default_spawn_id,0,0);
					eg->i_list = exp_new_i_simple(default_spawn_id,eg->duration);
				}
			}
			ec.i_list = eg->i_list;
		}
		ec.i_list->ecount++;

		/* save original pattern spec */
		/* keywords such as "-timeout" are saved as patterns here */
		/* useful for debugging but not otherwise used */
		save_str(&ec.pat,argv[i],eg->duration == EXP_TEMPORARY);
		save_str(&ec.body,argv[i+1],eg->duration == EXP_TEMPORARY);
			
		i++;

		*(eg->ecd.cases[eg->ecd.count] = ecase_new()) = ec;

		/* clear out for next set */
		ecase_clear(&ec);

		eg->ecd.count++;
	}

	/* if no patterns at all have appeared force the current */
	/* spawn id to be added to list anyway */

	if (eg->i_list == 0) {
		if (default_spawn_id != EXP_SPAWN_ID_BAD) {
			eg->i_list = exp_new_i_simple(default_spawn_id,eg->duration);
		} else {
			/* it'll be checked later, if used */
			(void) exp_update_master(interp,&default_spawn_id,0,0);
			eg->i_list = exp_new_i_simple(default_spawn_id,eg->duration);
		}
	}

	return(TCL_OK);

 error:
	/* very hard to free case_master_list here if it hasn't already */
	/* been attached to a case, ugh */

	/* note that i_list must be avail to free ecases! */
	free_ecases(interp,eg,0);

	/* undo temporary ecase */
	/* free_ecase doesn't quite handle this right, so do it by hand */
	if (ec.re) ckfree((char *)ec.re);
	if (eg->duration == EXP_PERMANENT) {
		if (ec.pat) ckfree(ec.pat);
		if (ec.body) ckfree(ec.body);
	}

	if (eg->i_list)
		exp_free_i(interp,eg->i_list,exp_indirect_update2);
	return(TCL_ERROR);
}

#define EXP_IS_DEFAULT(x)	((x) == EXP_TIMEOUT || (x) == EXP_EOF)

static char yes[] = "yes\r\n";
static char no[] = "no\r\n";

/* this describes status of a successful match */
struct eval_out {
	struct ecase *e;		/* ecase that matched */
	struct exp_f *f;			/* struct exp_f that matched */
	char *buffer;			/* buffer that matched */
	int match;			/* # of chars in buffer that matched */
					/* or # of chars in buffer at EOF */
};


/* like eval_cases, but handles only a single cases that needs a real */
/* string match */
/* returns EXP_X where X is MATCH, NOMATCH, FULLBUFFER, TCLERRROR */
static int
eval_case_string(interp,e,m,o,last_f,last_case,suffix)
Tcl_Interp *interp;
struct ecase *e;
int m;
struct eval_out *o;		/* 'output' - i.e., final case of interest */
/* next two args are for debugging, when they change, reprint buffer */
struct exp_f **last_f;
int *last_case;
char *suffix;
{
	struct exp_f *f = exp_fs + m;
	char *buffer;

	/* if -nocase, use the lowerized buffer */
	buffer = ((e->Case == CASE_NORM)?f->buffer:f->lower);

	/* if master or case changed, redisplay debug-buffer */
	if ((f != *last_f) || e->Case != *last_case) {
		debuglog("\r\nexpect%s: does \"%s\" (spawn_id %d) match %s ",
			 	suffix,
				dprintify(buffer),f-exp_fs,
				pattern_style[e->use]);
		*last_f = f;
		*last_case = e->Case;
	}

	if (e->use == PAT_RE) {
		debuglog("\"%s\"? ",dprintify(e->pat));
		TclRegError((char *)0);
		if (buffer && TclRegExec(e->re,buffer,buffer)) {
			o->e = e;
			o->match = e->re->endp[0]-buffer;
			o->buffer = buffer;
			o->f = f;
			debuglog(yes);
			return(EXP_MATCH);
		} else {
			debuglog(no);
			if (TclGetRegError()) {
			    exp_error(interp,"-re failed: %s",TclGetRegError());
			    return(EXP_TCLERROR);
		        }
		    }
	} else if (e->use == PAT_GLOB) {
		int match; /* # of chars that matched */

	        debuglog("\"%s\"? ",dprintify(e->pat));
		if (buffer && (-1 != (match = Exp_StringMatch(
				buffer,e->pat,&e->simple_start)))) {
			o->e = e;
			o->match = match;
			o->buffer = buffer;
			o->f = f;
			debuglog(yes);
			return(EXP_MATCH);
		} else debuglog(no);
	} else if (e->use == PAT_EXACT) {
		char *p = strstr(buffer,e->pat);
	        debuglog("\"%s\"? ",dprintify(e->pat));
		if (p) {
			e->simple_start = p - buffer;
			o->e = e;
			o->match = strlen(e->pat);
			o->buffer = buffer;
			o->f = f;
			debuglog(yes);
			return(EXP_MATCH);
		} else debuglog(no);
	} else if (e->use == PAT_NULL) {
		int i = 0;
		debuglog("null? ");
		for (;i<f->size;i++) {
			if (buffer[i] == 0) {
				o->e = e;
				o->match = i+1;	/* in this case, match is */
						/* just the # of chars + 1 */
						/* before the null */
				o->buffer = buffer;
				o->f = f;
				debuglog(yes);
				return EXP_MATCH;
			}
		}
		debuglog(no);
	} else if ((f->size == f->msize) && (f->size > 0)) {
		debuglog("%s? ",e->pat);
		o->e = e;
		o->match = f->umsize;
		o->buffer = f->buffer;
		o->f = f;
		debuglog(yes);
		return(EXP_FULLBUFFER);
	}
	return(EXP_NOMATCH);
}

/* sets o.e if successfully finds a matching pattern, eof, timeout or deflt */
/* returns original status arg or EXP_TCLERROR */
static int
eval_cases(interp,eg,m,o,last_f,last_case,status,masters,mcount,suffix)
Tcl_Interp *interp;
struct exp_cmd_descriptor *eg;
int m;
struct eval_out *o;		/* 'output' - i.e., final case of interest */
/* next two args are for debugging, when they change, reprint buffer */
struct exp_f **last_f;
int *last_case;
int status;
int *masters;
int mcount;
char *suffix;
{
	int i;
	int em;	/* master of ecase */
	struct ecase *e;

	if (o->e || status == EXP_TCLERROR || eg->ecd.count == 0) return(status);

	if (status == EXP_TIMEOUT) {
		for (i=0;i<eg->ecd.count;i++) {
			e = eg->ecd.cases[i];
			if (e->use == PAT_TIMEOUT || e->use == PAT_DEFAULT) {
				o->e = e;
				break;
			}
		}
		return(status);
	} else if (status == EXP_EOF) {
		for (i=0;i<eg->ecd.count;i++) {
			e = eg->ecd.cases[i];
			if (e->use == PAT_EOF || e->use == PAT_DEFAULT) {
				struct exp_fd_list *fdl;

				for (fdl=e->i_list->fd_list; fdl ;fdl=fdl->next) {
					em = fdl->fd;
					if (em == EXP_SPAWN_ID_ANY || em == m) {
						o->e = e;
						return(status);
					}
				}
			}
		}
		return(status);
	}

	/* the top loops are split from the bottom loop only because I can't */
	/* split'em further. */

	/* The bufferful condition does not prevent a pattern match from */
	/* occurring and vice versa, so it is scanned with patterns */
	for (i=0;i<eg->ecd.count;i++) {
		struct exp_fd_list *fdl;
		int j;

		e = eg->ecd.cases[i];
		if (e->use == PAT_TIMEOUT ||
		    e->use == PAT_DEFAULT ||
		    e->use == PAT_EOF) continue;

		for (fdl = e->i_list->fd_list; fdl; fdl = fdl->next) {
			em = fdl->fd;
			/* if em == EXP_SPAWN_ID_ANY, then user is explicitly asking */
			/* every case to be checked against every master */
			if (em == EXP_SPAWN_ID_ANY) {
				/* test against each spawn_id */
				for (j=0;j<mcount;j++) {
					status = eval_case_string(interp,e,masters[j],o,last_f,last_case,suffix);
					if (status != EXP_NOMATCH) return(status);
				}
			} else {
				/* reject things immediately from wrong spawn_id */
				if (em != m) continue;

				status = eval_case_string(interp,e,m,o,last_f,last_case,suffix);
				if (status != EXP_NOMATCH) return(status);
			}
		}
	}
	return(EXP_NOMATCH);
}

static void
ecases_remove_by_expi(interp,ecmd,exp_i)
Tcl_Interp *interp;
struct exp_cmd_descriptor *ecmd;
struct exp_i *exp_i;
{
	int i;

	/* delete every ecase dependent on it */
	for (i=0;i<ecmd->ecd.count;) {
		struct ecase *e = ecmd->ecd.cases[i];
		if (e->i_list == exp_i) {
			free_ecase(interp,e,0);

			/* shift remaining elements down */
			/* but only if there are any left */
			if (i+1 != ecmd->ecd.count) {
				memcpy(&ecmd->ecd.cases[i],
				       &ecmd->ecd.cases[i+1],
					((ecmd->ecd.count - i) - 1) * 
					sizeof(struct exp_cmd_descriptor *));
			}
			ecmd->ecd.count--;
			if (0 == ecmd->ecd.count) {
				ckfree((char *)ecmd->ecd.cases);
				ecmd->ecd.cases = 0;
			}
		} else {
			i++;
		}
	}
}

/* remove exp_i from list */
static void
exp_i_remove(interp,ei,exp_i)
Tcl_Interp *interp;
struct exp_i **ei;	/* list to remove from */
struct exp_i *exp_i;	/* element to remove */
{
	/* since it's in middle of list, free exp_i by hand */
	for (;*ei; ei = &(*ei)->next) {
		if (*ei == exp_i) {
			*ei = exp_i->next;
			exp_i->next = 0;
			exp_free_i(interp,exp_i,exp_indirect_update2);
			break;
		}
	}
}

/* remove exp_i from list and remove any dependent ecases */
static void
exp_i_remove_with_ecases(interp,ecmd,exp_i)
Tcl_Interp *interp;
struct exp_cmd_descriptor *ecmd;
struct exp_i *exp_i;
{
	ecases_remove_by_expi(interp,ecmd,exp_i);
	exp_i_remove(interp,&ecmd->i_list,exp_i);
}

/* remove ecases tied to a single direct spawn id */
static void
ecmd_remove_fd(interp,ecmd,m,direct)
Tcl_Interp *interp;
struct exp_cmd_descriptor *ecmd;
int m;
int direct;
{
	struct exp_i *exp_i, *next;
	struct exp_fd_list **fdl;

	for (exp_i=ecmd->i_list;exp_i;exp_i=next) {
		next = exp_i->next;

		if (!(direct & exp_i->direct)) continue;

		for (fdl = &exp_i->fd_list;*fdl;) {
			if (m == ((*fdl)->fd)) {
				struct exp_fd_list *tmp = *fdl;
				*fdl = (*fdl)->next;
				exp_free_fd_single(tmp);

				/* if last bg ecase, disarm spawn id */
				if ((ecmd->cmdtype == EXP_CMD_BG) && (m != EXP_SPAWN_ID_ANY)) {
					exp_fs[m].bg_ecount--;
					if (exp_fs[m].bg_ecount == 0) {
						exp_disarm_background_filehandler(m);
						exp_fs[m].bg_interp = 0;
					}
				}

				continue;
			}
			fdl = &(*fdl)->next;
		}

		/* if left with no fds (and is direct), get rid of it */
		/* and any dependent ecases */
		if (exp_i->direct == EXP_DIRECT && !exp_i->fd_list) {
			exp_i_remove_with_ecases(interp,ecmd,exp_i);
		}
	}
}

/* this is called from exp_close to clean up the fd */
void
exp_ecmd_remove_fd_direct_and_indirect(interp,m)
Tcl_Interp *interp;
int m;
{
	ecmd_remove_fd(interp,&exp_cmds[EXP_CMD_BEFORE],m,EXP_DIRECT|EXP_INDIRECT);
	ecmd_remove_fd(interp,&exp_cmds[EXP_CMD_AFTER],m,EXP_DIRECT|EXP_INDIRECT);
	ecmd_remove_fd(interp,&exp_cmds[EXP_CMD_BG],m,EXP_DIRECT|EXP_INDIRECT);

	/* force it - explanation in exp_tk.c where this func is defined */
	exp_disarm_background_filehandler_force(m);
}

/* arm a list of background fd's */
static void
fd_list_arm(interp,fdl)
Tcl_Interp *interp;
struct exp_fd_list *fdl;
{
	/* for each spawn id in list, arm if necessary */
	for (;fdl;fdl=fdl->next) {
		int m = fdl->fd;
		if (m == EXP_SPAWN_ID_ANY) continue;

		if (exp_fs[m].bg_ecount == 0) {
			exp_arm_background_filehandler(m);
			exp_fs[m].bg_interp = interp;
		}
		exp_fs[m].bg_ecount++;
	}
}

/* return TRUE if this ecase is used by this fd */
static int
exp_i_uses_fd(exp_i,fd)
struct exp_i *exp_i;
int fd;
{
	struct exp_fd_list *fdp;

	for (fdp = exp_i->fd_list;fdp;fdp=fdp->next) {
		if (fdp->fd == fd) return 1;
	}
	return 0;
}

static void
ecase_append(interp,ec)
Tcl_Interp *interp;
struct ecase *ec;
{
	if (!ec->transfer) Tcl_AppendElement(interp,"-notransfer");
	if (ec->indices) Tcl_AppendElement(interp,"-indices");
/*	if (ec->iwrite) Tcl_AppendElement(interp,"-iwrite");*/
	if (!ec->Case) Tcl_AppendElement(interp,"-nocase");

	if (ec->re) Tcl_AppendElement(interp,"-re");
	else if (ec->use == PAT_GLOB) Tcl_AppendElement(interp,"-gl");
	else if (ec->use == PAT_EXACT) Tcl_AppendElement(interp,"-ex");
	Tcl_AppendElement(interp,ec->pat);
	Tcl_AppendElement(interp,ec->body?ec->body:"");
}

/* append all ecases that match this exp_i */
static void
ecase_by_exp_i_append(interp,ecmd,exp_i)
Tcl_Interp *interp;
struct exp_cmd_descriptor *ecmd;
struct exp_i *exp_i;
{
	int i;
	for (i=0;i<ecmd->ecd.count;i++) {
		if (ecmd->ecd.cases[i]->i_list == exp_i) {
			ecase_append(interp,ecmd->ecd.cases[i]);
		}
	}
}

static void
exp_i_append(interp,exp_i)
Tcl_Interp *interp;
struct exp_i *exp_i;
{
	Tcl_AppendElement(interp,"-i");
	if (exp_i->direct == EXP_INDIRECT) {
		Tcl_AppendElement(interp,exp_i->variable);
	} else {
		struct exp_fd_list *fdp;

		/* if more than one element, add braces */
		if (exp_i->fd_list->next)
			Tcl_AppendResult(interp," {",(char *)0);

		for (fdp = exp_i->fd_list;fdp;fdp=fdp->next) {
			char buf[10];	/* big enough for a small int */
			sprintf(buf,"%d",fdp->fd);
			Tcl_AppendElement(interp,buf);
		}

		if (exp_i->fd_list->next)
			Tcl_AppendResult(interp,"} ",(char *)0);
	}
}

#if 0
/* delete ecases based on named -i descriptors */
int
expect_delete(interp,ecmd,argc,argv)
Tcl_Interp *interp;
struct exp_cmd_descriptor *ecmd;
int argc;
char **argv;
{
	while (*argv) {
		if (streq(argv[0],"-i") && argv[1]) {
			iflag = argv[1];
			argc-=2; argv+=2;
		} else if (streq(argv[0],"-all")) {
			all = TRUE;
			argc--; argv++;
		} else if (streq(argv[0],"-noindirect")) {
			direct &= ~EXP_INDIRECT;
			argc--; argv++;
		} else {
			exp_error(interp,"usage: -delete [-all | -i spawn_id]\n");
			return TCL_ERROR;
		}
	}

	if (all) {
		/* same logic as at end of regular expect cmd */
		free_ecases(interp,ecmd,0);
		exp_free_i(interp,ecmd->i_list,exp_indirect_update2);
		return TCL_OK;
	}

	if (!iflag) {
		if (0 == exp_update_master(interp,&m,0,0)) {
			return TCL_ERROR;
		}
	} else if (Tcl_GetInt(interp,iflag,&m) != TCL_OK) {
		/* handle as in indirect */

		struct exp_i **old_i;

		for (old_i=&ecmd->i_list;*old_i;) {
			struct exp_i *tmp;

			if ((*old_i)->direct == EXP_DIRECT) continue;
			if (!streq((*old_i)->variable,iflag)) continue;

			ecases_remove_by_expi(interp,ecmd,*old_i);

			/* unlink from middle of list */
			tmp = *old_i;
			*old_i = tmp->next;
			tmp->next = 0;
			exp_free_i(interp,tmp_i,exp_indirect_update2);
		} else {
			old_i = &(*old_i)->next;
		}
		return TCL_OK;
	}

	/* delete ecases of this direct_fd */
	/* unfinish after this ... */
	for (exp_i=ecmd->i_list;exp_i;exp_i=exp_i->next) {
		if (!(direct & exp_i->direct)) continue;
		if (!exp_i_uses_fd(exp_i,m)) continue;

		/* delete each ecase that uses this exp_i */


		ecase_by_exp_i_append(interp,ecmd,exp_i);
	}

	return TCL_OK;
}
#endif

/* return current setting of the permanent expect_before/after/bg */
int
expect_info(interp,ecmd,argc,argv)
Tcl_Interp *interp;
struct exp_cmd_descriptor *ecmd;
int argc;
char **argv;
{
	struct exp_i *exp_i;
	int i;
	int direct = EXP_DIRECT|EXP_INDIRECT;
	char *iflag = 0;
	int all = FALSE;	/* report on all fds */
	int m;

	while (*argv) {
		if (streq(argv[0],"-i") && argv[1]) {
			iflag = argv[1];
			argc-=2; argv+=2;
		} else if (streq(argv[0],"-all")) {
			all = TRUE;
			argc--; argv++;
		} else if (streq(argv[0],"-noindirect")) {
			direct &= ~EXP_INDIRECT;
			argc--; argv++;
		} else {
			exp_error(interp,"usage: -info [-all | -i spawn_id]\n");
			return TCL_ERROR;
		}
	}

	if (all) {
		/* avoid printing out -i when redundant */
		struct exp_i *previous = 0;

		for (i=0;i<ecmd->ecd.count;i++) {
			if (previous != ecmd->ecd.cases[i]->i_list) {
				exp_i_append(interp,ecmd->ecd.cases[i]->i_list);
				previous = ecmd->ecd.cases[i]->i_list;
			}
			ecase_append(interp,ecmd->ecd.cases[i]);
		}
		return TCL_OK;
	}

	if (!iflag) {
		if (0 == exp_update_master(interp,&m,0,0)) {
			return TCL_ERROR;
		}
	} else if (Tcl_GetInt(interp,iflag,&m) != TCL_OK) {
		/* handle as in indirect */
		Tcl_ResetResult(interp);
		for (i=0;i<ecmd->ecd.count;i++) {
			if (ecmd->ecd.cases[i]->i_list->direct == EXP_INDIRECT &&
			    streq(ecmd->ecd.cases[i]->i_list->variable,iflag)) {
				ecase_append(interp,ecmd->ecd.cases[i]);
			}
		}
		return TCL_OK;
	}

	/* print ecases of this direct_fd */
	for (exp_i=ecmd->i_list;exp_i;exp_i=exp_i->next) {
		if (!(direct & exp_i->direct)) continue;
		if (!exp_i_uses_fd(exp_i,m)) continue;
		ecase_by_exp_i_append(interp,ecmd,exp_i);
	}

	return TCL_OK;
}

/* Exp_ExpectGlobalCmd is invoked to process expect_before/after */
/*ARGSUSED*/
int
Exp_ExpectGlobalCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int result = TCL_OK;
	struct exp_i *exp_i, **eip;
	struct exp_fd_list *fdl;	/* temp for interating over fd_list */
	struct exp_cmd_descriptor eg;
	int count;

	struct exp_cmd_descriptor *ecmd = (struct exp_cmd_descriptor *) clientData;

	if ((argc == 2) && exp_one_arg_braced(argv[1])) {
		return(exp_eval_with_one_arg(clientData,interp,argv));
	} else if ((argc == 3) && streq(argv[1],"-brace")) {
		char *new_argv[2];
		new_argv[0] = argv[0];
		new_argv[1] = argv[2];
		return(exp_eval_with_one_arg(clientData,interp,new_argv));
	}

	if (argc > 1 && (argv[1][0] == '-')) {
		if (exp_flageq("info",&argv[1][1],4)) {
			return(expect_info(interp,ecmd,argc-2,argv+2));
		} 
	}

	exp_cmd_init(&eg,ecmd->cmdtype,EXP_PERMANENT);

	if (TCL_ERROR == parse_expect_args(interp,&eg,EXP_SPAWN_ID_BAD,
					argc,argv)) {
		return TCL_ERROR;
	}

	/*
	 * visit each NEW direct exp_i looking for spawn ids.
	 * When found, remove them from any OLD exp_i's.
	 */

	/* visit each exp_i */
	for (exp_i=eg.i_list;exp_i;exp_i=exp_i->next) {
		if (exp_i->direct == EXP_INDIRECT) continue;

		/* for each spawn id, remove it from ecases */
		for (fdl=exp_i->fd_list;fdl;fdl=fdl->next) {
			int m = fdl->fd;

			/* validate all input descriptors */
			if (m != EXP_SPAWN_ID_ANY) {
				if (!exp_fd2f(interp,m,1,1,"expect")) {
					result = TCL_ERROR;
					goto cleanup;
				}
			}

			/* remove spawn id from exp_i */
			ecmd_remove_fd(interp,ecmd,m,EXP_DIRECT);
		}
	}
	
	/*
	 * For each indirect variable, release its old ecases and 
	 * clean up the matching spawn ids.
	 * Same logic as in "expect_X delete" command.
	 */

	for (exp_i=eg.i_list;exp_i;exp_i=exp_i->next) {
		struct exp_i **old_i;

		if (exp_i->direct == EXP_DIRECT) continue;

		for (old_i = &ecmd->i_list;*old_i;) {
			struct exp_i *tmp;

			if (((*old_i)->direct == EXP_DIRECT) ||
			    (!streq((*old_i)->variable,exp_i->variable))) {
				old_i = &(*old_i)->next;
				continue;
			}

			ecases_remove_by_expi(interp,ecmd,*old_i);

			/* unlink from middle of list */
			tmp = *old_i;
			*old_i = tmp->next;
			tmp->next = 0;
			exp_free_i(interp,tmp,exp_indirect_update2);
		}

		/* if new one has ecases, update it */
		if (exp_i->ecount) {
			char *msg = exp_indirect_update1(interp,ecmd,exp_i);
			if (msg) {
				/* unusual way of handling error return */
				/* because of Tcl's variable tracing */
				strcpy(interp->result,msg);
				result = TCL_ERROR;
				goto indirect_update_abort;
			}
		}
	}
	/* empty i_lists have to be removed from global eg.i_list */
	/* before returning, even if during error */
 indirect_update_abort:

	/*
	 * New exp_i's that have 0 ecases indicate fd/vars to be deleted.
	 * Now that the deletions have been done, discard the new exp_i's.
	 */

	for (exp_i=eg.i_list;exp_i;) {
		struct exp_i *next = exp_i->next;

		if (exp_i->ecount == 0) {
			exp_i_remove(interp,&eg.i_list,exp_i);
		}
		exp_i = next;
	}
	if (result == TCL_ERROR) goto cleanup;

	/*
	 * arm all new bg direct fds
	 */

	if (ecmd->cmdtype == EXP_CMD_BG) {
		for (exp_i=eg.i_list;exp_i;exp_i=exp_i->next) {
			if (exp_i->direct == EXP_DIRECT) {
				fd_list_arm(interp,exp_i->fd_list);
			}
		}
	}

	/*
	 * now that old ecases are gone, add new ecases and exp_i's (both
	 * direct and indirect).
	 */

	/* append ecases */

	count = ecmd->ecd.count + eg.ecd.count;
	if (eg.ecd.count) {
		int start_index; /* where to add new ecases in old list */

		if (ecmd->ecd.count) {
			/* append to end */
			ecmd->ecd.cases = (struct ecase **)ckrealloc((char *)ecmd->ecd.cases, count * sizeof(struct ecase *));
			start_index = ecmd->ecd.count;
		} else {
			/* append to beginning */
			ecmd->ecd.cases = (struct ecase **)ckalloc(eg.ecd.count * sizeof(struct ecase *));
			start_index = 0;
		}
		memcpy(&ecmd->ecd.cases[start_index],eg.ecd.cases,
					eg.ecd.count*sizeof(struct ecase *));
		ecmd->ecd.count = count;
	}

	/* append exp_i's */
	for (eip = &ecmd->i_list;*eip;eip = &(*eip)->next) {
		/* empty loop to get to end of list */
	}
	/* *exp_i now points to end of list */

	*eip = eg.i_list;	/* connect new list to end of current list */

 cleanup:
	if (result == TCL_ERROR) {
		/* in event of error, free any unreferenced ecases */
		/* but first, split up i_list so that exp_i's aren't */
		/* freed twice */

		for (exp_i=eg.i_list;exp_i;) {
			struct exp_i *next = exp_i->next;
			exp_i->next = 0;
			exp_i = next;
		}
		free_ecases(interp,&eg,1);
	} else {
		if (eg.ecd.cases) ckfree((char *)eg.ecd.cases);
	}

	if (ecmd->cmdtype == EXP_CMD_BG) {
		exp_background_filehandlers_run_all();
	}

	return(result);
}

/* adjusts file according to user's size request */
void
exp_adjust(f)
struct exp_f *f;
{
	int new_msize;

	/* get the latest buffer size.  Double the user input for */
	/* two reasons.  1) Need twice the space in case the match */
	/* straddles two bufferfuls, 2) easier to hack the division */
	/* by two when shifting the buffers later on.  The extra  */
	/* byte in the malloc's is just space for a null we can slam on the */
	/* end.  It makes the logic easier later.  The -1 here is so that */
	/* requests actually come out to even/word boundaries (if user */
	/* gives "reasonable" requests) */
	new_msize = f->umsize*2 - 1;
	if (new_msize != f->msize) {
		if (!f->buffer) {
			/* allocate buffer space for 1st time */
			f->buffer = ckalloc((unsigned)new_msize+1);
			f->lower = ckalloc((unsigned)new_msize+1);
			f->size = 0;
		} else {
			/* buffer already exists - resize */

			/* if truncated, forget about some data */
			if (f->size > new_msize) {
				/* copy end of buffer down */
				memmove(f->buffer,f->buffer+(f->size - new_msize),new_msize);
				memmove(f->lower, f->lower +(f->size - new_msize),new_msize);
				f->size = new_msize;

				f->key = expect_key++;
			}

			f->buffer = ckrealloc(f->buffer,new_msize+1);
			f->lower = ckrealloc(f->lower,new_msize+1);
		}
		f->msize = new_msize;
		f->buffer[f->size] = '\0';
		f->lower[f->size] = '\0';
	}
}


/*

 expect_read() does the logical equivalent of a read() for the
expect command.  This includes figuring out which descriptor should
be read from.

The result of the read() is left in a spawn_id's buffer rather than
explicitly passing it back.  Note that if someone else has modified a
buffer either before or while this expect is running (i.e., if we or
some event has called Tcl_Eval which did another expect/interact),
expect_read will also call this a successful read (for the purposes if
needing to pattern match against it).

*/
/* if it returns a negative number, it corresponds to a EXP_XXX result */
/* if it returns a non-negative number, it means there is data */
/* (0 means nothing new was actually read, but it should be looked at again) */
int
expect_read(interp,masters,masters_max,m,timeout,key)
Tcl_Interp *interp;
int *masters;			/* If 0, then m is already known and set. */
int masters_max;		/* If *masters is not-zero, then masters_max */
				/* is the number of masters. */
				/* If *masters is zero, then masters_max */
				/* is used as the mask (ready vs except). */
				/* Crude but simplifies the interface. */
int *m;				/* Out variable to leave new master. */
int timeout;
int key;
{
	struct exp_f *f;
	int cc;
	int write_count;
	int tcl_set_flags;	/* if we have to discard chars, this tells */
				/* whether to show user locally or globally */

	if (masters == 0) {
		/* we already know the master, just find out what happened */
		cc = exp_get_next_event_info(interp,*m,masters_max);
		tcl_set_flags = TCL_GLOBAL_ONLY;
	} else {
		cc = exp_get_next_event(interp,masters,masters_max,m,timeout,key);
		tcl_set_flags = 0;
	}

	if (cc == EXP_DATA_NEW) {
		/* try to read it */

		cc = exp_i_read(interp,*m,timeout,tcl_set_flags);

		/* the meaning of 0 from i_read means eof.  Muck with it a */
		/* little, so that from now on it means "no new data arrived */
		/* but it should be looked at again anyway". */
		if (cc == 0) {
			cc = EXP_EOF;
		} else if (cc > 0) {
			f = exp_fs + *m;
			f->buffer[f->size += cc] = '\0';

			/* strip parity if requested */
			if (f->parity == 0) {
				/* do it from end backwards */
				char *p = f->buffer + f->size - 1;
				int count = cc;
				while (count--) {
					*p-- &= 0x7f;
				}
			}
		} /* else {
			assert(cc < 0) in which case some sort of error was
			encountered such as an interrupt with that forced an
			error return
		} */
	} else if (cc == EXP_DATA_OLD) {
		f = exp_fs + *m;
		cc = 0;
	} else if (cc == EXP_RECONFIGURE) {
		return EXP_RECONFIGURE;
	}

	if (cc == EXP_ABEOF) {	/* abnormal EOF */
		/* On many systems, ptys produce EIO upon EOF - sigh */
		if (i_read_errno == EIO) {
			/* Sun, Cray, BSD, and others */
			cc = EXP_EOF;
		} else if (i_read_errno == EINVAL) {
			/* Solaris 2.4 occasionally returns this */
			cc = EXP_EOF;
		} else {
			if (i_read_errno == EBADF) {
				exp_error(interp,"bad spawn_id (process died earlier?)");
			} else {
				exp_error(interp,"i_read(spawn_id=%d): %s",*m,
					Tcl_PosixError(interp));
				exp_close(interp,*m);
			}
			return(EXP_TCLERROR);
			/* was goto error; */
		}
	}

	/* EOF, TIMEOUT, and ERROR return here */
	/* In such cases, there is no need to update screen since, if there */
	/* was prior data read, it would have been sent to the screen when */
	/* it was read. */
	if (cc < 0) return (cc);

	/* update display */

	if (f->size) write_count = f->size - f->printed;
	else write_count = 0;

	if (write_count) {
		if (logfile_all || (loguser && logfile)) {
			fwrite(f->buffer + f->printed,1,write_count,logfile);
		}
		/* don't write to user if they're seeing it already, */
		/* that is, typing it! */
		if (loguser && !exp_is_stdinfd(*m) && !exp_is_devttyfd(*m))
			fwrite(f->buffer + f->printed,
					1,write_count,stdout);
		if (debugfile) fwrite(f->buffer + f->printed,
					1,write_count,debugfile);

		/* remove nulls from input, since there is no way */
		/* for Tcl to deal with such strings.  Doing it here */
		/* lets them be sent to the screen, just in case */
		/* they are involved in formatting operations */
		if (f->rm_nulls) {
			f->size -= rm_nulls(f->buffer + f->printed,write_count);
		}
		f->buffer[f->size] = '\0';

		/* copy to lowercase buffer */
		exp_lowmemcpy(f->lower+f->printed,
			      f->buffer+f->printed,
					1 + f->size - f->printed);

		f->printed = f->size; /* count'm even if not logging */
	}
	return(cc);
}

/* when buffer fills, copy second half over first and */
/* continue, so we can do matches over multiple buffers */
void
exp_buffer_shuffle(interp,f,save_flags,array_name,caller_name)
Tcl_Interp *interp;
struct exp_f *f;
int save_flags;
char *array_name;
char *caller_name;
{
	char spawn_id[10];	/* enough for a %d */
	char match_char;	/* place to hold char temporarily */
				/* uprooted by a NULL */

	int first_half = f->size/2;
	int second_half = f->size - first_half;

	/*
	 * allow user to see data we are discarding
	 */

	sprintf(spawn_id,"%d",f-exp_fs);
	debuglog("%s: set %s(spawn_id) \"%s\"\r\n",
		 caller_name,array_name,dprintify(spawn_id));
	Tcl_SetVar2(interp,array_name,"spawn_id",spawn_id,save_flags);

	/* temporarily null-terminate buffer in middle */
	match_char = f->buffer[first_half];
	f->buffer[first_half] = 0;

	debuglog("%s: set %s(buffer) \"%s\"\r\n",
		 caller_name,array_name,dprintify(f->buffer));
	Tcl_SetVar2(interp,array_name,"buffer",f->buffer,save_flags);

	/* remove middle-null-terminator */
	f->buffer[first_half] = match_char;

	memcpy(f->buffer,f->buffer+first_half,second_half);
	memcpy(f->lower, f->lower +first_half,second_half);
	f->size = second_half;
	f->printed -= first_half;
	if (f->printed < 0) f->printed = 0;
}

/* map EXP_ style return value to TCL_ style return value */
/* not defined to work on TCL_OK */
int
exp_tcl2_returnvalue(x)
int x;
{
	switch (x) {
	case TCL_ERROR:			return EXP_TCLERROR;
	case TCL_RETURN:		return EXP_TCLRET;
	case TCL_BREAK:			return EXP_TCLBRK;
	case TCL_CONTINUE:		return EXP_TCLCNT;
	case EXP_CONTINUE:		return EXP_TCLCNTEXP;
	case EXP_CONTINUE_TIMER:	return EXP_TCLCNTTIMER;
	case EXP_TCL_RETURN:		return EXP_TCLRETTCL;
	}
}

/* map from EXP_ style return value to TCL_ style return values */
int
exp_2tcl_returnvalue(x)
int x;
{
	switch (x) {
	case EXP_TCLERROR:		return TCL_ERROR;
	case EXP_TCLRET:		return TCL_RETURN;
	case EXP_TCLBRK:		return TCL_BREAK;
	case EXP_TCLCNT:		return TCL_CONTINUE;
	case EXP_TCLCNTEXP:		return EXP_CONTINUE;
	case EXP_TCLCNTTIMER:		return EXP_CONTINUE_TIMER;
	case EXP_TCLRETTCL:		return EXP_TCL_RETURN;
	}
}

/* returns # of chars read or (non-positive) error of form EXP_XXX */
/* returns 0 for end of file */
/* If timeout is non-zero, set an alarm before doing the read, else assume */
/* the read will complete immediately. */
/*ARGSUSED*/
static int
exp_i_read(interp,m,timeout,save_flags)
Tcl_Interp *interp;
int m;
int timeout;
int save_flags;
{
	struct exp_f *f;
	int cc = EXP_TIMEOUT;

	f = exp_fs + m;
	if (f->size == f->msize) 
		exp_buffer_shuffle(interp,f,save_flags,EXPECT_OUT,"expect");

#ifdef SIMPLE_EVENT
 restart:

	alarm_fired = FALSE;

	if (timeout > -1) {
		signal(SIGALRM,sigalarm_handler);
		alarm((timeout > 0)?timeout:1);
	}
#endif

	cc = read(m,f->buffer+f->size, f->msize-f->size);
	i_read_errno = errno;

#ifdef SIMPLE_EVENT
	alarm(0);

	if (cc == -1) {
		/* check if alarm went off */
		if (i_read_errno == EINTR) {
			if (alarm_fired) {
				return EXP_TIMEOUT;
			} else {
				if (Tcl_AsyncReady()) {
					int rc = Tcl_AsyncInvoke(interp,TCL_OK);
					if (rc != TCL_OK) return(exp_tcl2_returnvalue(rc));
				}
				if (!f->valid) {
					exp_error(interp,"spawn_id %d no longer valid",f-exp_fs);
					return EXP_TCLERROR;
				}
				goto restart;
			}
		}
	}
#endif
	return(cc);
}

/* variables predefined by expect are retrieved using this routine
which looks in the global space if they are not in the local space.
This allows the user to localize them if desired, and also to
avoid having to put "global" in procedure definitions.
*/
char *
exp_get_var(interp,var)
Tcl_Interp *interp;
char *var;
{
	char *val;

	if (NULL != (val = Tcl_GetVar(interp,var,0 /* local */)))
		return(val);
	return(Tcl_GetVar(interp,var,TCL_GLOBAL_ONLY));
}

static int
get_timeout(interp)
Tcl_Interp *interp;
{
	static int timeout = INIT_EXPECT_TIMEOUT;
 	char *t;

	if (NULL != (t = exp_get_var(interp,EXPECT_TIMEOUT))) {
		timeout = atoi(t);
	}
	return(timeout);
}

/* make a copy of a linked list (1st arg) and attach to end of another (2nd
arg) */
static int
update_expect_fds(i_list,fd_union)
struct exp_i *i_list;
struct exp_fd_list **fd_union;
{
	struct exp_i *p;

	/* for each i_list in an expect statement ... */
	for (p=i_list;p;p=p->next) {
		struct exp_fd_list *fdl;

		/* for each fd in the i_list */
		for (fdl=p->fd_list;fdl;fdl=fdl->next) {
			struct exp_fd_list *tmpfdl;
			struct exp_fd_list *u;

			if (fdl->fd == EXP_SPAWN_ID_ANY) continue;

			/* check this one against all so far */
			for (u = *fd_union;u;u=u->next) {
				if (fdl->fd == u->fd) goto found;
			}
			/* if not found, link in as head of list */
			tmpfdl = exp_new_fd(fdl->fd);
			tmpfdl->next = *fd_union;
			*fd_union = tmpfdl;
		found:;
		}
	}
	return TCL_OK;
}

char *
exp_cmdtype_printable(cmdtype)
int cmdtype;
{
	switch (cmdtype) {
	case EXP_CMD_FG: return("expect");
	case EXP_CMD_BG: return("expect_background");
	case EXP_CMD_BEFORE: return("expect_before");
	case EXP_CMD_AFTER: return("expect_after");
	}
#ifdef LINT
	return("unknown expect command");
#endif
}

/* exp_indirect_update2 is called back via Tcl's trace handler whenever */
/* an indirect spawn id list is changed */
/*ARGSUSED*/
static char *
exp_indirect_update2(clientData, interp, name1, name2, flags)
ClientData clientData;
Tcl_Interp *interp;	/* Interpreter containing variable. */
char *name1;		/* Name of variable. */
char *name2;		/* Second part of variable name. */
int flags;		/* Information about what happened. */
{
	char *msg;

	struct exp_i *exp_i = (struct exp_i *)clientData;
	exp_configure_count++;
	msg = exp_indirect_update1(interp,&exp_cmds[exp_i->cmdtype],exp_i);

	exp_background_filehandlers_run_all();

	return msg;
}

static char *
exp_indirect_update1(interp,ecmd,exp_i)
Tcl_Interp *interp;
struct exp_cmd_descriptor *ecmd;
struct exp_i *exp_i;
{
	struct exp_fd_list *fdl;	/* temp for interating over fd_list */

	/*
	 * disarm any fd's that lose all their ecases
	 */

	if (ecmd->cmdtype == EXP_CMD_BG) {
		/* clean up each spawn id used by this exp_i */
		for (fdl=exp_i->fd_list;fdl;fdl=fdl->next) {
			int m = fdl->fd;

			if (m == EXP_SPAWN_ID_ANY) continue;

			/* silently skip closed or preposterous fds */
			/* since we're just disabling them anyway */
			/* preposterous fds will have been reported */
			/* by code in next section already */
			if (!exp_fd2f(interp,fdl->fd,1,0,"")) continue;

			exp_fs[m].bg_ecount--;
			if (exp_fs[m].bg_ecount == 0) {
				exp_disarm_background_filehandler(m);
				exp_fs[m].bg_interp = 0;
			}
		}
	}

	/*
	 * reread indirect variable
	 */

	exp_i_update(interp,exp_i);

	/*
	 * check validity of all fd's in variable
	 */

	for (fdl=exp_i->fd_list;fdl;fdl=fdl->next) {
		/* validate all input descriptors */
		if (fdl->fd == EXP_SPAWN_ID_ANY) continue;

		if (!exp_fd2f(interp,fdl->fd,1,1,
				exp_cmdtype_printable(ecmd->cmdtype))) {
			static char msg[200];
			sprintf(msg,"%s from indirect variable (%s)",
				interp->result,exp_i->variable);
			return msg;
		}
	}

	/* for each spawn id in list, arm if necessary */
	if (ecmd->cmdtype == EXP_CMD_BG) {
		fd_list_arm(interp,exp_i->fd_list);
	}

	return (char *)0;
}

void
exp_background_filehandlers_run_all()
{
	int m;
	struct exp_f *f;

	/* kick off any that already have input waiting */
	for (m=0;m<=exp_fd_max;m++) {
		f = exp_fs + m;
		if (!f->valid) continue;

		/* is bg_interp the best way to check if armed? */
		if (f->bg_interp && (f->size > 0)) {
			exp_background_filehandler((ClientData)f->fd_ptr,0/*ignored*/);
		}
	}
}

/* this function is called from the background when input arrives */
/*ARGSUSED*/
void
exp_background_filehandler(clientData,mask)
ClientData clientData;
int mask;
{
	int m;

	Tcl_Interp *interp;
	int cc;			/* number of chars returned in a single read */
				/* or negative EXP_whatever */
	struct exp_f *f;		/* file associated with master */

	int i;			/* trusty temporary */

	struct eval_out eo;	/* final case of interest */
	struct exp_f *last_f;	/* for differentiating when multiple f's */
				/* to print out better debugging messages */
	int last_case;		/* as above but for case */

	/* restore our environment */
	m = *(int *)clientData;
	f = exp_fs + m;
	interp = f->bg_interp;

	/* temporarily prevent this handler from being invoked again */
	exp_block_background_filehandler(m);

	/* if mask == 0, then we've been called because the patterns changed */
	/* not because the waiting data has changed, so don't actually do */
	/* any I/O */

	if (mask == 0) {
		cc = 0;
	} else {
		cc = expect_read(interp,(int *)0,mask,&m,EXP_TIME_INFINITY,0);
	}

do_more_data:
	eo.e = 0;		/* no final case yet */
	eo.f = 0;		/* no final file selected yet */
	eo.match = 0;		/* nothing matched yet */

	/* force redisplay of buffer when debugging */
	last_f = 0;

	if (cc == EXP_EOF) {
		/* do nothing */
	} else if (cc < 0) { /* EXP_TCLERROR or any other weird value*/
		goto finish;
		/* if we were going to do this right, we should */
		/* differentiate between things like HP ioctl-open-traps */
		/* that fall out here and should rightfully be ignored */
		/* and real errors that should be reported.  Come to */
		/* think of it, the only errors will come from HP */
		/* ioctl handshake botches anyway. */
	} else {
		/* normal case, got data */
		/* new data if cc > 0, same old data if cc == 0 */

		/* below here, cc as general status */
		cc = EXP_NOMATCH;
	}

	cc = eval_cases(interp,&exp_cmds[EXP_CMD_BEFORE],
			m,&eo,&last_f,&last_case,cc,&m,1,"_background");
	cc = eval_cases(interp,&exp_cmds[EXP_CMD_BG],
			m,&eo,&last_f,&last_case,cc,&m,1,"_background");
	cc = eval_cases(interp,&exp_cmds[EXP_CMD_AFTER],
			m,&eo,&last_f,&last_case,cc,&m,1,"_background");
	if (cc == EXP_TCLERROR) {
		/* only likely problem here is some internal regexp botch */
		Tcl_BackgroundError(interp);
		goto finish;
	}
	/* special eof code that cannot be done in eval_cases */
	/* or above, because it would then be executed several times */
	if (cc == EXP_EOF) {
		eo.f = exp_fs + m;
		eo.match = eo.f->size;
		eo.buffer = eo.f->buffer;
		debuglog("expect_background: read eof\r\n");
		goto matched;
	}
	if (!eo.e) {
		/* if we get here, there must not have been a match */
		goto finish;
	}

 matched:
#define out(i,val)  debuglog("expect_background: set %s(%s) \"%s\"\r\n",EXPECT_OUT,i, \
						dprintify(val)); \
		    Tcl_SetVar2(interp,EXPECT_OUT,i,val,TCL_GLOBAL_ONLY);
	{
/*		int iwrite = FALSE;*/	/* write spawn_id? */
		char *body = 0;
		char *buffer;	/* pointer to normal or lowercased data */
		struct ecase *e = 0;	/* points to current ecase */
		int match = -1;		/* characters matched */
		char match_char;	/* place to hold char temporarily */
					/* uprooted by a NULL */
		char *eof_body = 0;

		if (eo.e) {
			e = eo.e;
			body = e->body;
/*			iwrite = e->iwrite;*/
			if (cc != EXP_TIMEOUT) {
				f = eo.f;
				match = eo.match;
				buffer = eo.buffer;
			}
#if 0
			if (e->timestamp) {
				char value[20];

				time(&current_time);
				elapsed_time = current_time - start_time;
				elapsed_time_total = current_time - start_time_total;
				sprintf(value,"%d",elapsed_time);
				out("seconds",value);
				sprintf(value,"%d",elapsed_time_total);
				out("seconds_total",value);
				/* deprecated */
				exp_timestamp(interp,&current_time,EXPECT_OUT);
			}
#endif
		} else if (cc == EXP_EOF) {
			/* read an eof but no user-supplied case */
			f = eo.f;
			match = eo.match;
			buffer = eo.buffer;
		}			

		if (match >= 0) {
			char name[20], value[20];

			if (e && e->use == PAT_RE) {
				regexp *re = e->re;

				for (i=0;i<NSUBEXP;i++) {
					int offset;

					if (re->startp[i] == 0) continue;

					if (e->indices) {
					  /* start index */
					  sprintf(name,"%d,start",i);
					  offset = re->startp[i]-buffer;
					  sprintf(value,"%d",offset);
					  out(name,value);

					  /* end index */
					  sprintf(name,"%d,end",i);
					  sprintf(value,"%d",
						re->endp[i]-buffer-1);
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
				/* redefine length of string that */
				/* matched for later extraction */
				match = re->endp[0]-buffer;
			} else if (e && (e->use == PAT_GLOB || e->use == PAT_EXACT)) {
				char *str;

				if (e->indices) {
				  /* start index */
				  sprintf(value,"%d",e->simple_start);
				  out("0,start",value);

				  /* end index */
				  sprintf(value,"%d",e->simple_start + match - 1);
				  out("0,end",value);
				}

				/* string itself */
				str = f->buffer + e->simple_start;
				/* temporarily null-terminate in middle */
				match_char = str[match];
				str[match] = 0;
				out("0,string",str);
				str[match] = match_char;

				/* redefine length of string that */
				/* matched for later extraction */
				match += e->simple_start;
			} else if (e && e->use == PAT_NULL && e->indices) {
				/* start index */
				sprintf(value,"%d",match-1);
				out("0,start",value);
				/* end index */
				sprintf(value,"%d",match-1);
				out("0,end",value);
			} else if (e && e->use == PAT_FULLBUFFER) {
				debuglog("expect_background: full buffer\r\n");
			}
		}

		/* this is broken out of (match > 0) (above) since it can */
		/* that an EOF occurred with match == 0 */
		if (eo.f) {
			char spawn_id[10];	/* enough for a %d */

/*			if (iwrite) {*/
				sprintf(spawn_id,"%d",f-exp_fs);
				out("spawn_id",spawn_id);
/*			}*/

			/* save buf[0..match] */
			/* temporarily null-terminate string in middle */
			match_char = f->buffer[match];
			f->buffer[match] = 0;
			out("buffer",f->buffer);
			/* remove middle-null-terminator */
			f->buffer[match] = match_char;

			/* "!e" means no case matched - transfer by default */
			if (!e || e->transfer) {
				/* delete matched chars from input buffer */
				f->size -= match;
				f->printed -= match;
				if (f->size != 0) {
				   memmove(f->buffer,f->buffer+match,f->size);
				   memmove(f->lower,f->lower+match,f->size);
				}
				f->buffer[f->size] = '\0';
				f->lower[f->size] = '\0';
			}

			if (cc == EXP_EOF) {
				/* exp_close() deletes all background bodies */
				/* so save eof body temporarily */
				if (body) {
					eof_body = ckalloc(strlen(body)+1);
					strcpy(eof_body,body);
					body = eof_body;
				}

				exp_close(interp,f - exp_fs);
			}

		}

		if (body) {
			int result = Tcl_GlobalEval(interp,body);
			if (result != TCL_OK) Tcl_BackgroundError(interp);

			if (eof_body) ckfree(eof_body);
		}


		/*
		 * Event handler will not call us back if there is more input
		 * pending but it has already arrived.  bg_status will be
		 * "blocked" only if armed.
		 */
		if (exp_fs[m].valid && (exp_fs[m].bg_status == blocked)
		 && (f->size > 0)) {
			cc = f->size;
			goto do_more_data;
		}
	}
 finish:
	/* fd could have gone away, so check before using */
	if (exp_fs[m].valid)
		exp_unblock_background_filehandler(m);
}
#undef out

/*ARGSUSED*/
int
Exp_ExpectCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int cc;			/* number of chars returned in a single read */
				/* or negative EXP_whatever */
	int m;			/* before doing an actual read, attempt */
				/* to match upon any spawn_id */
	struct exp_f *f;		/* file associated with master */

	int i;			/* trusty temporary */
	struct exp_cmd_descriptor eg;
	struct exp_fd_list *fd_list;	/* list of masters to watch */
	struct exp_fd_list *fdl;	/* temp for interating over fd_list */
	int *masters;		/* array of masters to watch */
	int mcount;		/* number of masters to watch */

	struct eval_out eo;	/* final case of interest */

	int result;		/* Tcl result */

	time_t start_time_total;/* time at beginning of this procedure */
	time_t start_time = 0;	/* time when restart label hit */
	time_t current_time = 0;/* current time (when we last looked)*/
	time_t end_time;	/* future time at which to give up */
	time_t elapsed_time_total;/* time from now to match/fail/timeout */
	time_t elapsed_time;	/* time from restart to (ditto) */

	struct exp_f *last_f;	/* for differentiating when multiple f's */
				/* to print out better debugging messages */
	int last_case;		/* as above but for case */
	int first_time = 1;	/* if not "restarted" */

	int key;		/* identify this expect command instance */
	int configure_count;	/* monitor exp_configure_count */

	int timeout;		/* seconds */
	int remtime;		/* remaining time in timeout */
	int reset_timer;	/* should timer be reset after continue? */

	if ((argc == 2) && exp_one_arg_braced(argv[1])) {
		return(exp_eval_with_one_arg(clientData,interp,argv));
	} else if ((argc == 3) && streq(argv[1],"-brace")) {
		char *new_argv[2];
		new_argv[0] = argv[0];
		new_argv[1] = argv[2];
		return(exp_eval_with_one_arg(clientData,interp,new_argv));
	}

	time(&start_time_total);
	start_time = start_time_total;
	reset_timer = TRUE;

	/* make arg list for processing cases */
	/* do it dynamically, since expect can be called recursively */

	exp_cmd_init(&eg,EXP_CMD_FG,EXP_TEMPORARY);
	fd_list = 0;
	masters = 0;
	if (TCL_ERROR == parse_expect_args(interp,&eg,
					*(int *)clientData,argc,argv))
		return TCL_ERROR;

 restart_with_update:
	/* validate all descriptors */
	/* and flatten fds into array */

	if ((TCL_ERROR == update_expect_fds(exp_cmds[EXP_CMD_BEFORE].i_list,&fd_list))
	 || (TCL_ERROR == update_expect_fds(exp_cmds[EXP_CMD_AFTER].i_list, &fd_list))
	 || (TCL_ERROR == update_expect_fds(eg.i_list,&fd_list))) {
		result = TCL_ERROR;
		goto cleanup;
	}

	/* declare ourselves "in sync" with external view of close/indirect */
	configure_count = exp_configure_count;

	/* count and validate fd_list */
	mcount = 0;
	for (fdl=fd_list;fdl;fdl=fdl->next) {
		mcount++;
		/* validate all input descriptors */
		if (!exp_fd2f(interp,fdl->fd,1,1,"expect")) {
			result = TCL_ERROR;
			goto cleanup;
		}
	}

	/* make into an array */
	masters = (int *)ckalloc(mcount * sizeof(int));
	for (fdl=fd_list,i=0;fdl;fdl=fdl->next,i++) {
		masters[i] = fdl->fd;
	}

     restart:
	if (first_time) first_time = 0;
	else time(&start_time);

	if (eg.timeout_specified_by_flag) {
		timeout = eg.timeout;
	} else {
		/* get the latest timeout */
		timeout = get_timeout(interp);
	}

	key = expect_key++;

	result = TCL_OK;
	last_f = 0;

	/* end of restart code */

	eo.e = 0;		/* no final case yet */
	eo.f = 0;		/* no final file selected yet */
	eo.match = 0;		/* nothing matched yet */

	/* timeout code is a little tricky, be very careful changing it */
	if (timeout != EXP_TIME_INFINITY) {
		/* if exp_continue -continue_timer, do not update end_time */
		if (reset_timer) {
			time(&current_time);
			end_time = current_time + timeout;
		} else {
			reset_timer = TRUE;
		}
	}

	/* remtime and current_time updated at bottom of loop */
	remtime = timeout;

	for (;;) {
		if ((timeout != EXP_TIME_INFINITY) && (remtime < 0)) {
			cc = EXP_TIMEOUT;
		} else {
			cc = expect_read(interp,masters,mcount,&m,remtime,key);
		}

		/*SUPPRESS 530*/
		if (cc == EXP_EOF) {
			/* do nothing */
		} else if (cc == EXP_TIMEOUT) {
			debuglog("expect: timed out\r\n");
		} else if (cc == EXP_RECONFIGURE) {
			reset_timer = FALSE;
			goto restart_with_update;
		} else if (cc < 0) { /* EXP_TCLERROR or any other weird value*/
			goto error;
		} else {
			/* new data if cc > 0, same old data if cc == 0 */

			f = exp_fs + m;

			/* below here, cc as general status */
			cc = EXP_NOMATCH;

			/* force redisplay of buffer when debugging */
			last_f = 0;
		}

		cc = eval_cases(interp,&exp_cmds[EXP_CMD_BEFORE],
			m,&eo,&last_f,&last_case,cc,masters,mcount,"");
		cc = eval_cases(interp,&eg,
			m,&eo,&last_f,&last_case,cc,masters,mcount,"");
		cc = eval_cases(interp,&exp_cmds[EXP_CMD_AFTER],
			m,&eo,&last_f,&last_case,cc,masters,mcount,"");
		if (cc == EXP_TCLERROR) goto error;
		/* special eof code that cannot be done in eval_cases */
		/* or above, because it would then be executed several times */
		if (cc == EXP_EOF) {
			eo.f = exp_fs + m;
			eo.match = eo.f->size;
			eo.buffer = eo.f->buffer;
			debuglog("expect: read eof\r\n");
			break;
		} else if (cc == EXP_TIMEOUT) break;
		/* break if timeout or eof and failed to find a case for it */

		if (eo.e) break;

		/* no match was made with current data, force a read */
		f->force_read = TRUE;

		if (timeout != EXP_TIME_INFINITY) {
			time(&current_time);
			remtime = end_time - current_time;
		}
	}

	goto done;

error:
	result = exp_2tcl_returnvalue(cc);
 done:
#define out(i,val)  debuglog("expect: set %s(%s) \"%s\"\r\n",EXPECT_OUT,i, \
						dprintify(val)); \
		    Tcl_SetVar2(interp,EXPECT_OUT,i,val,0);

	if (result != TCL_ERROR) {
/*		int iwrite = FALSE;*/	/* write spawn_id? */
		char *body = 0;
		char *buffer;	/* pointer to normal or lowercased data */
		struct ecase *e = 0;	/* points to current ecase */
		int match = -1;		/* characters matched */
		char match_char;	/* place to hold char temporarily */
					/* uprooted by a NULL */
		char *eof_body = 0;

		if (eo.e) {
			e = eo.e;
			body = e->body;
/*			iwrite = e->iwrite;*/
			if (cc != EXP_TIMEOUT) {
				f = eo.f;
				match = eo.match;
				buffer = eo.buffer;
			}
			if (e->timestamp) {
				char value[20];

				time(&current_time);
				elapsed_time = current_time - start_time;
				elapsed_time_total = current_time - start_time_total;
				sprintf(value,"%d",elapsed_time);
				out("seconds",value);
				sprintf(value,"%d",elapsed_time_total);
				out("seconds_total",value);

				/* deprecated */
				exp_timestamp(interp,&current_time,EXPECT_OUT);
			}
		} else if (cc == EXP_EOF) {
			/* read an eof but no user-supplied case */
			f = eo.f;
			match = eo.match;
			buffer = eo.buffer;
		}			

		if (match >= 0) {
			char name[20], value[20];

			if (e && e->use == PAT_RE) {
				regexp *re = e->re;

				for (i=0;i<NSUBEXP;i++) {
					int offset;

					if (re->startp[i] == 0) continue;

					if (e->indices) {
					  /* start index */
					  sprintf(name,"%d,start",i);
					  offset = re->startp[i]-buffer;
					  sprintf(value,"%d",offset);
					  out(name,value);

					  /* end index */
					  sprintf(name,"%d,end",i);
					  sprintf(value,"%d",
						re->endp[i]-buffer-1);
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
				/* redefine length of string that */
				/* matched for later extraction */
				match = re->endp[0]-buffer;
			} else if (e && (e->use == PAT_GLOB || e->use == PAT_EXACT)) {
				char *str;

				if (e->indices) {
				  /* start index */
				  sprintf(value,"%d",e->simple_start);
				  out("0,start",value);

				  /* end index */
				  sprintf(value,"%d",e->simple_start + match - 1);
				  out("0,end",value);
				}

				/* string itself */
				str = f->buffer + e->simple_start;
				/* temporarily null-terminate in middle */
				match_char = str[match];
				str[match] = 0;
				out("0,string",str);
				str[match] = match_char;

				/* redefine length of string that */
				/* matched for later extraction */
				match += e->simple_start;
			} else if (e && e->use == PAT_NULL && e->indices) {
				/* start index */
				sprintf(value,"%d",match-1);
				out("0,start",value);
				/* end index */
				sprintf(value,"%d",match-1);
				out("0,end",value);
			} else if (e && e->use == PAT_FULLBUFFER) {
				debuglog("expect: full buffer\r\n");
			}
		}

		/* this is broken out of (match > 0) (above) since it can */
		/* that an EOF occurred with match == 0 */
		if (eo.f) {
			char spawn_id[10];	/* enough for a %d */

/*			if (iwrite) {*/
				sprintf(spawn_id,"%d",f-exp_fs);
				out("spawn_id",spawn_id);
/*			}*/

			/* save buf[0..match] */
			/* temporarily null-terminate string in middle */
			match_char = f->buffer[match];
			f->buffer[match] = 0;
			out("buffer",f->buffer);
			/* remove middle-null-terminator */
			f->buffer[match] = match_char;

			/* "!e" means no case matched - transfer by default */
			if (!e || e->transfer) {
				/* delete matched chars from input buffer */
				f->size -= match;
				f->printed -= match;
				if (f->size != 0) {
				   memmove(f->buffer,f->buffer+match,f->size);
				   memmove(f->lower,f->lower+match,f->size);
				}
				f->buffer[f->size] = '\0';
				f->lower[f->size] = '\0';
			}

			if (cc == EXP_EOF) {
				/* exp_close() deletes all background bodies */
				/* so save eof body temporarily */
				if (body) {
					eof_body = ckalloc(strlen(body)+1);
					strcpy(eof_body,body);
					body = eof_body;
				}

				exp_close(interp,f - exp_fs);
			}

		}

		if (body) {
			result = Tcl_Eval(interp,body);

			if (eof_body) ckfree(eof_body);
		}
	}

 cleanup:
	if (result == EXP_CONTINUE_TIMER) {
		reset_timer = FALSE;
		result = EXP_CONTINUE;
	}

	if ((result == EXP_CONTINUE)
	     && (configure_count == exp_configure_count)) {
		debuglog("expect: continuing expect\r\n");
		goto restart;
	}

	if (fd_list) {
		exp_free_fd(fd_list);
		fd_list = 0;
	}
	if (masters) {
		ckfree((char *)masters);
		masters = 0;
	}

	if (result == EXP_CONTINUE) {
		debuglog("expect: continuing expect after update\r\n");
		goto restart_with_update;
	}

	free_ecases(interp,&eg,0);	/* requires i_lists to be avail */
	exp_free_i(interp,eg.i_list,exp_indirect_update2);

	return(result);
}
#undef out

/* beginning of deprecated code */

#define out(elt)		Tcl_SetVar2(interp,array,elt,ascii,0);
void
exp_timestamp(interp,timeval,array)
Tcl_Interp *interp;
time_t *timeval;
char *array;
{
	struct tm *tm;
	char *ascii;

	tm = localtime(timeval);	/* split */
	ascii = asctime(tm);		/* print */
	ascii[24] = '\0';		/* zap trailing \n */

	out("timestamp");

	sprintf(ascii,"%ld",*timeval);
	out("epoch");

	sprintf(ascii,"%d",tm->tm_sec);
	out("sec");
	sprintf(ascii,"%d",tm->tm_min);
	out("min");
	sprintf(ascii,"%d",tm->tm_hour);
	out("hour");
	sprintf(ascii,"%d",tm->tm_mday);
	out("mday");
	sprintf(ascii,"%d",tm->tm_mon);
	out("mon");
	sprintf(ascii,"%d",tm->tm_year);
	out("year");
	sprintf(ascii,"%d",tm->tm_wday);
	out("wday");
	sprintf(ascii,"%d",tm->tm_yday);
	out("yday");
	sprintf(ascii,"%d",tm->tm_isdst);
	out("isdst");
}
/* end of deprecated code */

/*ARGSUSED*/
static int
Exp_TimestampCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char *format = 0;
	time_t seconds = -1;
	int gmt = FALSE;	/* local time by default */
	struct tm *tm;
	Tcl_DString dstring;

	argc--; argv++;

	while (*argv) {
		if (streq(*argv,"-format")) {
			argc--; argv++;
			if (!*argv) goto usage_error;
			format = *argv;
			argc--; argv++;
		} else if (streq(*argv,"-seconds")) {
			argc--; argv++;
			if (!*argv) goto usage_error;
			seconds = atoi(*argv);
			argc--; argv++;
		} else if (streq(*argv,"-gmt")) {
			gmt = TRUE;
			argc--; argv++;
		} else break;
	}

	if (argc) goto usage_error;

	if (seconds == -1) {
		time(&seconds);
	}

	Tcl_DStringInit(&dstring);

	if (format) {
		if (gmt) {
			tm = gmtime(&seconds);
		} else {
			tm = localtime(&seconds);
		}
/*		exp_strftime(interp->result,TCL_RESULT_SIZE,format,tm);*/
		exp_strftime(format,tm,&dstring);
		Tcl_DStringResult(interp,&dstring);
	} else {
		sprintf(interp->result,"%ld",seconds);
	}
	
	return TCL_OK;
 usage_error:
	exp_error(interp,"args: [-seconds #] [-format format]");
	return TCL_ERROR;

}

/* lowmemcpy - like memcpy but it lowercases result */
void
exp_lowmemcpy(dest,src,n)
char *dest;
char *src;
int n;
{
	for (;n>0;n--) {
		*dest = ((isascii(*src) && isupper(*src))?tolower(*src):*src);
		src++;	dest++;
	}
}

/*ARGSUSED*/
int
Exp_MatchMaxCmd(clientData,interp,argc,argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int size = -1;
	int m = -1;
	struct exp_f *f;
	int Default = FALSE;

	argc--; argv++;

	for (;argc>0;argc--,argv++) {
		if (streq(*argv,"-d")) {
			Default = TRUE;
		} else if (streq(*argv,"-i")) {
			argc--;argv++;
			if (argc < 1) {
				exp_error(interp,"-i needs argument");
				return(TCL_ERROR);
			}
			m = atoi(*argv);
		} else break;
	}

	if (!Default) {
		if (m == -1) {
			if (!(f = exp_update_master(interp,&m,0,0)))
				return(TCL_ERROR);
		} else {
			if (!(f = exp_fd2f(interp,m,0,0,"match_max")))
				return(TCL_ERROR);
		}
	} else if (m != -1) {
		exp_error(interp,"cannot do -d and -i at the same time");
		return(TCL_ERROR);
	}

	if (argc == 0) {
		if (Default) {
			size = exp_default_match_max;
		} else {
			size = f->umsize;
		}
		sprintf(interp->result,"%d",size);
		return(TCL_OK);
	}

	if (argc > 1) {
		exp_error(interp,"too many arguments");
		return(TCL_OK);
	}

	/* all that's left is to set the size */
	size = atoi(argv[0]);
	if (size <= 0) {
		exp_error(interp,"must be positive");
		return(TCL_ERROR);
	}

	if (Default) exp_default_match_max = size;
	else f->umsize = size;

	return(TCL_OK);
}

/*ARGSUSED*/
int
Exp_RemoveNullsCmd(clientData,interp,argc,argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int value = -1;
	int m = -1;
	struct exp_f *f;
	int Default = FALSE;

	argc--; argv++;

	for (;argc>0;argc--,argv++) {
		if (streq(*argv,"-d")) {
			Default = TRUE;
		} else if (streq(*argv,"-i")) {
			argc--;argv++;
			if (argc < 1) {
				exp_error(interp,"-i needs argument");
				return(TCL_ERROR);
			}
			m = atoi(*argv);
		} else break;
	}

	if (!Default) {
		if (m == -1) {
			if (!(f = exp_update_master(interp,&m,0,0)))
				return(TCL_ERROR);
		} else {
			if (!(f = exp_fd2f(interp,m,0,0,"remove_nulls")))
				return(TCL_ERROR);
		}
	} else if (m != -1) {
		exp_error(interp,"cannot do -d and -i at the same time");
		return(TCL_ERROR);
	}

	if (argc == 0) {
		if (Default) {
			value = exp_default_match_max;
		} else {
			value = f->rm_nulls;
		}
		sprintf(interp->result,"%d",value);
		return(TCL_OK);
	}

	if (argc > 1) {
		exp_error(interp,"too many arguments");
		return(TCL_OK);
	}

	/* all that's left is to set the value */
	value = atoi(argv[0]);
	if (value != 0 && value != 1) {
		exp_error(interp,"must be 0 or 1");
		return(TCL_ERROR);
	}

	if (Default) exp_default_rm_nulls = value;
	else f->rm_nulls = value;

	return(TCL_OK);
}

/*ARGSUSED*/
int
Exp_ParityCmd(clientData,interp,argc,argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int parity;
	int m = -1;
	struct exp_f *f;
	int Default = FALSE;

	argc--; argv++;

	for (;argc>0;argc--,argv++) {
		if (streq(*argv,"-d")) {
			Default = TRUE;
		} else if (streq(*argv,"-i")) {
			argc--;argv++;
			if (argc < 1) {
				exp_error(interp,"-i needs argument");
				return(TCL_ERROR);
			}
			m = atoi(*argv);
		} else break;
	}

	if (!Default) {
		if (m == -1) {
			if (!(f = exp_update_master(interp,&m,0,0)))
				return(TCL_ERROR);
		} else {
			if (!(f = exp_fd2f(interp,m,0,0,"parity")))
				return(TCL_ERROR);
		}
	} else if (m != -1) {
		exp_error(interp,"cannot do -d and -i at the same time");
		return(TCL_ERROR);
	}

	if (argc == 0) {
		if (Default) {
			parity = exp_default_parity;
		} else {
			parity = f->parity;
		}
		sprintf(interp->result,"%d",parity);
		return(TCL_OK);
	}

	if (argc > 1) {
		exp_error(interp,"too many arguments");
		return(TCL_OK);
	}

	/* all that's left is to set the parity */
	parity = atoi(argv[0]);

	if (Default) exp_default_parity = parity;
	else f->parity = parity;

	return(TCL_OK);
}

#if DEBUG_PERM_ECASES
/* This big chunk of code is just for debugging the permanent */
/* expect cases */
void
exp_fd_print(fdl)
struct exp_fd_list *fdl;
{
	if (!fdl) return;
	printf("%d ",fdl->fd);
	exp_fd_print(fdl->next);
}

void
exp_i_print(exp_i)
struct exp_i *exp_i;
{
	if (!exp_i) return;
	printf("exp_i %x",exp_i);
	printf((exp_i->direct == EXP_DIRECT)?" direct":" indirect");
	printf((exp_i->duration == EXP_PERMANENT)?" perm":" tmp");
	printf("  ecount = %d\n",exp_i->ecount);
	printf("variable %s, value %s\n",
		((exp_i->variable)?exp_i->variable:"--"),
		((exp_i->value)?exp_i->value:"--"));
	printf("fds: ");
	exp_fd_print(exp_i->fd_list); printf("\n");
	exp_i_print(exp_i->next);
}

void
exp_ecase_print(ecase)
struct ecase *ecase;
{
	printf("pat <%s>\n",ecase->pat);
	printf("exp_i = %x\n",ecase->i_list);
}

void
exp_ecases_print(ecd)
struct exp_cases_descriptor *ecd;
{
	int i;

	printf("%d cases\n",ecd->count);
	for (i=0;i<ecd->count;i++) exp_ecase_print(ecd->cases[i]);
}

void
exp_cmd_print(ecmd)
struct exp_cmd_descriptor *ecmd;
{
	printf("expect cmd type: %17s",exp_cmdtype_printable(ecmd->cmdtype));
	printf((ecmd->duration==EXP_PERMANENT)?" perm ": "tmp ");
	/* printdict */
	exp_ecases_print(&ecmd->ecd);
	exp_i_print(ecmd->i_list);
}

void
exp_cmds_print()
{
	exp_cmd_print(&exp_cmds[EXP_CMD_BEFORE]);
	exp_cmd_print(&exp_cmds[EXP_CMD_AFTER]);
	exp_cmd_print(&exp_cmds[EXP_CMD_BG]);
}

/*ARGSUSED*/
int
cmdX(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	exp_cmds_print();
	return TCL_OK;
}
#endif /*DEBUG_PERM_ECASES*/

/* need address for passing into cmdExpect */
static int spawn_id_bad = EXP_SPAWN_ID_BAD;
static int spawn_id_user = EXP_SPAWN_ID_USER;

static struct exp_cmd_data
cmd_data[]  = {
{"expect",	exp_proc(Exp_ExpectCmd),	(ClientData)&spawn_id_bad,	0},
{"expect_after",exp_proc(Exp_ExpectGlobalCmd),(ClientData)&exp_cmds[EXP_CMD_AFTER],0},
{"expect_before",exp_proc(Exp_ExpectGlobalCmd),(ClientData)&exp_cmds[EXP_CMD_BEFORE],0},
{"expect_user",	exp_proc(Exp_ExpectCmd),	(ClientData)&spawn_id_user,	0},
{"expect_tty",	exp_proc(Exp_ExpectCmd),	(ClientData)&exp_dev_tty,	0},
{"expect_background",exp_proc(Exp_ExpectGlobalCmd),(ClientData)&exp_cmds[EXP_CMD_BG],0},
{"match_max",	exp_proc(Exp_MatchMaxCmd),	0,	0},
{"remove_nulls",exp_proc(Exp_RemoveNullsCmd),	0,	0},
{"parity",	exp_proc(Exp_ParityCmd),		0,	0},
{"timestamp",	exp_proc(Exp_TimestampCmd),	0,	0},
{0}};

void
exp_init_expect_cmds(interp)
Tcl_Interp *interp;
{
	exp_create_commands(interp,cmd_data);

	Tcl_SetVar(interp,EXPECT_TIMEOUT,INIT_EXPECT_TIMEOUT_LIT,0);
	Tcl_SetVar(interp,EXP_SPAWN_ID_ANY_VARNAME,EXP_SPAWN_ID_ANY_LIT,0);

	exp_cmd_init(&exp_cmds[EXP_CMD_BEFORE],EXP_CMD_BEFORE,EXP_PERMANENT);
	exp_cmd_init(&exp_cmds[EXP_CMD_AFTER ],EXP_CMD_AFTER, EXP_PERMANENT);
	exp_cmd_init(&exp_cmds[EXP_CMD_BG    ],EXP_CMD_BG,    EXP_PERMANENT);
	exp_cmd_init(&exp_cmds[EXP_CMD_FG    ],EXP_CMD_FG,    EXP_TEMPORARY);

	/* preallocate to one element, so future realloc's work */
	exp_cmds[EXP_CMD_BEFORE].ecd.cases = 0;
	exp_cmds[EXP_CMD_AFTER ].ecd.cases = 0;
	exp_cmds[EXP_CMD_BG    ].ecd.cases = 0;

	pattern_style[PAT_EOF] = "eof";
	pattern_style[PAT_TIMEOUT] = "timeout";
	pattern_style[PAT_DEFAULT] = "default";
	pattern_style[PAT_FULLBUFFER] = "full buffer";
	pattern_style[PAT_GLOB] = "glob pattern";
	pattern_style[PAT_RE] = "regular expression";
	pattern_style[PAT_EXACT] = "exact string";
	pattern_style[PAT_NULL] = "null";

#if 0
	Tcl_CreateCommand(interp,"x",
		cmdX,(ClientData)0,exp_deleteProc);
#endif
}

void
exp_init_sig() {
#if 0
	signal(SIGALRM,sigalarm_handler);
	signal(SIGINT,sigint_handler);
#endif
}
