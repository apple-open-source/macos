/*
 * globals.h - global variables for zsh
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 1992-1996 Paul Falstad
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */


/* GLOBALS is defined is init.c, so the global variables *
 * are actually contained in init.c, and are externs in  *
 * the other source files.                               */

#ifdef GLOBALS
# define EXTERN
#else
# define EXTERN extern
#endif
 
#ifdef GLOBALS
int redirtab[TRINANG - OUTANG + 1] =
{
    WRITE,
    WRITENOW,
    APP,
    APPNOW,
    READ,
    READWRITE,
    HEREDOC,
    HEREDOCDASH,
    MERGEIN,
    MERGEOUT,
    ERRWRITE,
    ERRWRITENOW,
    ERRAPP,
    ERRAPPNOW,
    HERESTR,
};
#else
extern int redirtab[TRINANG - OUTANG + 1];
#endif

#ifdef GLOBALS
char nulstring[] = {Nularg, '\0'};
int  nulstrlen   = sizeof(nulstring) - 1;
#else
extern char nulstring[];
extern int  nulstrlen;
#endif

/* NULL-terminated arrays containing path, cdpath, etc. */
 
EXTERN char **path;		/* $path     */
EXTERN char **cdpath;		/* $cdpath   */
EXTERN char **fpath;		/* $fpath    */
EXTERN char **watch;		/* $watch    */
EXTERN char **mailpath;		/* $mailpath */
EXTERN char **manpath;		/* $manpath  */
EXTERN char **fignore;		/* $fignore  */
EXTERN char **psvar;		/* $psvar    */
 
EXTERN char *yytext;

/* used to suppress ERREXIT and  *
 * trapping of SIGZERR, SIGEXIT. */

EXTERN int noerrexit;

/* do not save history on exec and exit */

EXTERN int nohistsave;
 
/* error/break flag */
 
EXTERN int errflag;
 
/* Status of return from a trap */
 
EXTERN int trapreturn;
 
EXTERN char *tokstr;
EXTERN int tok, tokfd;
 
/* lexical analyzer error flag */
 
EXTERN int lexstop;

EXTERN struct heredocs *hdocs;
 
/* suppress error messages */
 
EXTERN int noerrs;
 
/* nonzero means we are not evaluating, just parsing (in math.c) */
 
EXTERN int noeval;
 
/* current history event number */
 
EXTERN int curhist;
 
/* if != 0, we are expanding the current line */

EXTERN int expanding;

/* these are used to modify the cursor position during expansion */

EXTERN int excs, exlast;

/* if != 0, this is the first line of the command */
 
EXTERN int isfirstln;
 
/* if != 0, this is the first char of the command (not including
        white space) */
 
EXTERN int isfirstch;

/* number of history entries */
 
EXTERN int histentct;
 
/* array of history entries */
 
EXTERN Histent histentarr;
 
/* capacity of history lists */
 
EXTERN int histsiz;
 
/* if = 1, we have performed history substitution on the current line
        if = 2, we have used the 'p' modifier */
 
EXTERN int histdone;
 
/* default event (usually curhist-1, that is, "!!") */
 
EXTERN int defev;
 
/* != 0 if we are about to read a command word */
 
EXTERN int incmdpos;
 
/* != 0 if we are in the middle of a [[ ... ]] */
 
EXTERN int incond;
 
/* != 0 if we are after a redirection (for ctxtlex only) */
 
EXTERN int inredir;
 
/* != 0 if we are about to read a case pattern */
 
EXTERN int incasepat;
 
/* != 0 if we just read FUNCTION */
 
EXTERN int infunc;
 
/* != 0 if we just read a newline */
 
EXTERN int isnewlin;

/* the lists of history events */
 
EXTERN LinkList histlist;
 
/* the directory stack */
 
EXTERN LinkList dirstack;
 
/* the zle buffer stack */
 
EXTERN LinkList bufstack;

/* total # of characters waiting to be read. */

EXTERN int inbufct;

/* the flags controlling the input routines in input.c: see INP_* in zsh.h */

EXTERN int inbufflags;

/* flag that an alias should be expanded after expansion ending in space */

EXTERN int inalmore;

/* != 0 if this is a subshell */
 
EXTERN int subsh;
 
/* # of break levels */
 
EXTERN int breaks;
 
/* != 0 if we have a return pending */
 
EXTERN int retflag;
 
/* how far we've hashed the PATH so far */
 
EXTERN char **pathchecked;
 
/* # of nested loops we are in */
 
EXTERN int loops;
 
/* # of continue levels */
 
EXTERN int contflag;
 
/* the job we are working on */
 
EXTERN int thisjob;

/* the current job (+) */
 
EXTERN int curjob;
 
/* the previous job (-) */
 
EXTERN int prevjob;
 
/* hash table containing the aliases */
 
EXTERN HashTable aliastab;
 
/* hash table containing the reserved words */

EXTERN HashTable reswdtab;

/* hash table containing the parameters */
 
EXTERN HashTable paramtab;
 
/* hash table containing the external/hashed commands */
 
EXTERN HashTable cmdnamtab;

/* hash table containing the shell functions */

EXTERN HashTable shfunctab;

/* hash table containing builtin commands */

EXTERN HashTable builtintab;
 
/* hash table for completion info for commands */
 
EXTERN HashTable compctltab;

/* hash table for multi-character bindings */

EXTERN HashTable keybindtab;

/* hash table for emacs multi-character bindings */

EXTERN HashTable emkeybindtab;

/* hash table for vi multi-character bindings */

EXTERN HashTable vikeybindtab;

/* hash table for named directories */

EXTERN HashTable nameddirtab;
 
/* default completion infos */
 
EXTERN struct compctl cc_compos, cc_default, cc_first, cc_dummy;
 
/* the job table */
 
EXTERN struct job jobtab[MAXJOB];
 
/* shell timings */
 
EXTERN struct tms shtms;
 
/* the list of sched jobs pending */
 
EXTERN struct schedcmd *schedcmds;
 
/* the last l for s/l/r/ history substitution */
 
EXTERN char *hsubl;

/* the last r for s/l/r/ history substitution */
 
EXTERN char *hsubr;
 
/* We cache `USERNAME' and use check cached_uid *
 * so we know when to recompute it.             */

EXTERN uid_t cached_uid;
EXTERN char *cached_username;   /* $USERNAME   */
EXTERN char *zsh_name;		/* ZSH_NAME    */

EXTERN char *underscore;	/* $_          */
EXTERN zlong lastval;		/* $?          */
EXTERN zlong mypid;		/* $$          */
EXTERN zlong lastpid;		/* $!          */
EXTERN zlong ppid;		/* $PPID       */
EXTERN char *ifs;		/* $IFS        */
EXTERN char *pwd;		/* $PWD        */
EXTERN char *oldpwd;		/* $OLDPWD     */

EXTERN zlong columns;		/* $COLUMNS    */
EXTERN zlong lines;		/* $LINES      */

EXTERN char *zoptarg;		/* $OPTARG     */
EXTERN zlong zoptind;		/* $OPTIND     */
EXTERN char *prompt;		/* $PROMPT     */
EXTERN char *prompt2;		/* etc.        */
EXTERN char *prompt3;
EXTERN char *prompt4;
EXTERN char *rprompt;		/* $RPROMPT    */
EXTERN char *sprompt;

EXTERN char *wordchars;
EXTERN char *rstring, *Rstring;
EXTERN char *postedit;

EXTERN char *hostnam;           /* from gethostname */
EXTERN char *home;              /* $HOME */
EXTERN char **pparams;          /* $argv */

EXTERN pid_t mypgrp;		/* the process group of the shell */
 
EXTERN char *argzero;           /* $0 */
 
EXTERN char *hackzero;
EXTERN char *scriptname;        /* name of script being sourced */

EXTERN zlong lineno;		/* $LINENO       */
EXTERN zlong shlvl;		/* $SHLVL        */
 
EXTERN long lastval2;

/* the last time we checked mail */
 
EXTERN time_t lastmailcheck;
 
/* the last time we checked the people in the WATCH variable */
 
EXTERN time_t lastwatch;
 
/* the last time we did the periodic() shell function */
 
EXTERN time_t lastperiodic;
 
/* $SECONDS = time(NULL) - shtimer.tv_sec */
 
EXTERN struct timeval shtimer;
 
/* the default command for null commands */
 
EXTERN char *nullcmd;
EXTERN char *readnullcmd;
 
/* the List of local variables we have to destroy */
 
EXTERN LinkList locallist;

/* what level of localness we are at */
 
EXTERN int locallevel;
 
/* what level of sourcing we are at */
 
EXTERN int sourcelevel;

/* The table of file descriptors.  A table element is zero if the  *
 * corresponding fd is not used by the shell.  It is greater than  *
 * 1 if the fd is used by a <(...) or >(...) substitution and 1 if *
 * it is an internal file descriptor which must be closed before   *
 * executing an external command.  The first ten elements of the   *
 * table is not used.  A table element is set by movefd and cleard *
 * by zclose.                                                      */

EXTERN char *fdtable;

/* The allocated size of fdtable */

EXTERN int fdtable_size;

/* The highest fd that marked with nonzero in fdtable */

EXTERN int max_zsh_fd;

/* input fd from the coprocess */

EXTERN int coprocin;

/* output fd from the coprocess */

EXTERN int coprocout;

/* the shell input fd */

EXTERN int SHIN;

/* the shell tty fd */

EXTERN int SHTTY;

/* the FILE attached to the shell tty */

EXTERN FILE *shout;

/* buffered shell input for non-interactive shells */

EXTERN FILE *bshin;

/* the FILE for xtrace output */

EXTERN FILE *xtrerr;

/* != 0 means we are reading input from a string */
 
EXTERN int strin;
 
/* != 0 means history substitution is turned off */
 
EXTERN int stophist;
 
/* this line began with a space, so junk it if HISTIGNORESPACE is on */
 
EXTERN int spaceflag;
 
/* don't do spelling correction */
 
EXTERN int nocorrect;

/* state of the history mechanism (see hist.c) */
 
EXTERN int histactive;

/* current emulation (used to decide which set of option letters is used) */

EXTERN int emulation;
 
/* the options; e.g. if opts[SHGLOB] != 0, SH_GLOB is turned on */
 
EXTERN char opts[OPT_SIZE];
 
EXTERN int lastbase;            /* last input base we used */
 
#ifdef HAVE_GETRLIMIT
/* the resource limits for the shell and its children */

EXTERN struct rlimit current_limits[RLIM_NLIMITS];
EXTERN struct rlimit limits[RLIM_NLIMITS];
#endif
 
/* pointer into the history line */
 
EXTERN char *hptr;
 
/* the current history line */
 
EXTERN char *chline;

/* true if the last character returned by hgetc was an escaped bangchar
 * if it is set and NOBANGHIST is unset hwaddc escapes bangchars */

EXTERN int qbang;
 
/* text attribute mask */
 
#ifdef GLOBALS
unsigned txtattrmask = 0;
#else
extern unsigned txtattrmask;
#endif

/* text change - attribute change made by prompts */

EXTERN unsigned txtchange;

EXTERN char *term;		/* $TERM */
 
/* 0 if this $TERM setup is usable, otherwise it contains TERM_* flags */

EXTERN int termflags;
 
/* flag for CSHNULLGLOB */
 
EXTERN int badcshglob;
 
/* max size of histline */
 
EXTERN int hlinesz;
 
/* we have printed a 'you have stopped (running) jobs.' message */
 
EXTERN int stopmsg;
 
/* the default tty state */
 
EXTERN struct ttyinfo shttyinfo;
 
EXTERN char *ttystrname;	/* $TTY */
 
/* 1 if ttyctl -f has been executed */
 
EXTERN int ttyfrozen;
 
/* != 0 if we are allocating in the heaplist */
 
EXTERN int useheap;
 
/* Words on the command line, for use in completion */
 
EXTERN int clwsize, clwnum, clwpos;
EXTERN char **clwords;

/* Non-zero if a completion list was displayed. */

EXTERN int listshown;

/* Non-zero if refresh() should clear the list below the prompt. */

EXTERN int clearlist;

/* pid of process undergoing 'process substitution' */
 
EXTERN pid_t cmdoutpid;
 
/* exit status of process undergoing 'process substitution' */
 
EXTERN int cmdoutval;
 
/* Stack to save some variables before executing a signal handler function */

EXTERN struct execstack *exstack;

/* Array describing the state of each signal: an element contains *
 * 0 for the default action or some ZSIG_* flags ored together.   */

EXTERN int sigtrapped[VSIGCOUNT];

/* trap functions for each signal */

EXTERN List sigfuncs[VSIGCOUNT];

#ifdef DEBUG
EXTERN int alloc_stackp;
#endif

/* Variables used by signal queueing */

EXTERN int queueing_enabled;
EXTERN sigset_t signal_mask_queue[MAX_QUEUE_SIZE];
EXTERN int signal_queue[MAX_QUEUE_SIZE];
EXTERN int queue_front;
EXTERN int queue_rear;

/* Previous values of errflag and breaks if the signal handler had to
 * change them. And a flag saying if it did that. */

EXTERN int prev_errflag, prev_breaks, errbrk_saved;

/* 1 if aliases should not be expanded */
 
EXTERN int noaliases;

#ifdef GLOBALS
/* tokens */
char *ztokens = "#$^*()$=|{}[]`<>?~`,'\"\\";
#else
extern char *ztokens;
#endif

/* $histchars */
 
EXTERN unsigned char bangchar, hatchar, hashchar;
 
EXTERN int eofseen;
 
/* we are parsing a line sent to use by the editor */
 
EXTERN int zleparse;
 
EXTERN int wordbeg;
 
EXTERN int parbegin;

EXTERN int parend;
 
/* used in arrays of lists instead of NULL pointers */
 
EXTERN struct list dummy_list;

/* lengths of each string */
 
EXTERN int tclen[TC_COUNT];
 
EXTERN char *tcstr[TC_COUNT];

/* Values of the li and co entries */

EXTERN int tclines, tccolumns;
 
/* names of the strings we want */
#ifdef GLOBALS
char *tccapnams[TC_COUNT] =
{
    "cl", "le", "LE", "nd", "RI", "up", "UP", "do",
    "DO", "dc", "DC", "ic", "IC", "cd", "ce", "al", "dl", "ta",
    "md", "so", "us", "me", "se", "ue", "ch"
};
#else
extern char *tccapnams[TC_COUNT];
#endif

/* the command stack for use with %_ in prompts */
 
EXTERN unsigned char *cmdstack;
EXTERN int cmdsp;

#ifdef GLOBALS
char *tokstrings[WHILE + 1] = {
    NULL,	/* NULLTOK	  0  */
    ";",	/* SEPER	     */
    "\\n",	/* NEWLIN	     */
    ";",	/* SEMI		     */
    ";;",	/* DSEMI	     */
    "&",	/* AMPER	  5  */
    "(",	/* INPAR	     */
    ")",	/* OUTPAR	     */
    "||",	/* DBAR		     */
    "&&",	/* DAMPER	     */
    ">",	/* OUTANG	  10 */
    ">|",	/* OUTANGBANG	     */
    ">>",	/* DOUTANG	     */
    ">>|",	/* DOUTANGBANG	     */
    "<",	/* INANG	     */
    "<>",	/* INOUTANG	  15 */
    "<<",	/* DINANG	     */
    "<<-",	/* DINANGDASH	     */
    "<&",	/* INANGAMP	     */
    ">&",	/* OUTANGAMP	     */
    "&>",	/* AMPOUTANG	  20 */
    "&>|",	/* OUTANGAMPBANG     */
    ">>&",	/* DOUTANGAMP	     */
    ">>&|",	/* DOUTANGAMPBANG    */
    "<<<",	/* TRINANG	     */
    "|",	/* BAR		  25 */
    "|&",	/* BARAMP	     */
    "()",	/* INOUTPAR	     */
    "((",	/* DINPAR	     */
    "))",	/* DOUTPAR	     */
    "&|",	/* AMPERBANG	  30 */
};
#else
extern char *tokstrings[];
#endif

#ifdef GLOBALS
char *cmdnames[] =
{
    "for",      "while",     "repeat",    "select",
    "until",    "if",        "then",      "else",
    "elif",     "math",      "cond",      "cmdor",
    "cmdand",   "pipe",      "errpipe",   "foreach",
    "case",     "function",  "subsh",     "cursh",
    "array",    "quote",     "dquote",    "bquote",
    "cmdsubst", "mathsubst", "elif-then", "heredoc",
    "heredocd", "brace",     "braceparam",
};
#else
extern char *cmdnames[];
#endif
 
#ifndef GLOBALS
extern struct option optns[OPT_SIZE];
#else
struct option optns[OPT_SIZE] = {
# define x OPT_REV|
    {NULL, 0, 0, 0},
    {"allexport", 		'a',  'a',  OPT_EMULATE},
    {"alwayslastprompt", 	0,    0,    0},
    {"alwaystoend", 		0,    0,    0},
    {"appendhistory", 		0,    0,    0},
    {"autocd", 			'J',  0,    OPT_EMULATE},
    {"autolist", 		'9',  0,    0},
    {"automenu", 		0,    0,    0},
    {"autonamedirs", 		0,    0,    0},
    {"autoparamkeys", 		0,    0,    0},
    {"autoparamslash", 		0,    0,    OPT_CSH},
    {"autopushd", 		'N',  0,    0},
    {"autoremoveslash", 	0,    0,    0},
    {"autoresume", 		'W',  0,    0},
    {"badpattern", 		x'2', 0,    OPT_EMULATE|OPT_NONBOURNE},
    {"banghist", 		x'K', 0,    OPT_NONBOURNE},
    {"beep", 			x'B', 0,    OPT_ALL},
    {"bgnice", 			'6',  0,    OPT_EMULATE|OPT_NONBOURNE},
    {"braceccl", 		0,    0,    OPT_EMULATE},
    {"bsdecho", 		0,    0,    OPT_EMULATE|OPT_SH},
    {"cdablevars", 		'T',  0,    OPT_EMULATE},
    {"chaselinks", 		'w',  0,    OPT_EMULATE},
    {"clobber", 		x'C', x'C', OPT_EMULATE|OPT_ALL},
    {"completealiases", 	0,    0,    0},
    {"completeinword", 		0,    0,    0},
    {"correct", 		'0',  0,    0},
    {"correctall", 		'O',  0,    0},
    {"cshjunkiehistory", 	0,    0,    OPT_EMULATE|OPT_CSH},
    {"cshjunkieloops", 		0,    0,    OPT_EMULATE|OPT_CSH},
    {"cshjunkiequotes", 	0,    0,    OPT_EMULATE|OPT_CSH},
    {"cshnullglob", 		0,    0,    OPT_EMULATE|OPT_CSH},
    {"equals", 			0,    0,    OPT_EMULATE|OPT_ZSH},
    {"errexit", 		'e',  'e',  OPT_EMULATE},
    {"exec", 			x'n', x'n', OPT_ALL},
    {"extendedglob", 		0,    0,    OPT_EMULATE},
    {"extendedhistory", 	0,    0,    OPT_CSH},
    {"flowcontrol", 		0,    0,    OPT_ALL},
    {"functionargzero",		0,    0,    OPT_EMULATE|OPT_NONBOURNE},
    {"glob", 			x'F', x'f', OPT_EMULATE|OPT_ALL},
    {"globassign", 		0,    0,    OPT_EMULATE|OPT_CSH},
    {"globcomplete", 		0,    0,    0},
    {"globdots", 		'4',  0,    OPT_EMULATE},
    {"globsubst", 		0,    0,    OPT_EMULATE|OPT_NONZSH},
    {"hashcmds", 		0,    0,    OPT_ALL},
    {"hashdirs", 		0,    0,    OPT_ALL},
    {"hashlistall", 		0,    0,    OPT_ALL},
    {"histallowclobber", 	0,    0,    0},
    {"histbeep", 		0,    0,    OPT_ALL},
    {"histignoredups", 		'h',  0,    0},
    {"histignorespace", 	'g',  0,    0},
    {"histnostore", 		0,    0,    0},
    {"histreduceblanks",	0,    0,    0},
    {"histverify", 		0,    0,    0},
    {"hup", 			0,    0,    OPT_EMULATE|OPT_ZSH},
    {"ignorebraces", 		'I',  0,    OPT_EMULATE|OPT_SH},
    {"ignoreeof", 		'7',  0,    0},
    {"interactive", 		'i',  'i',  OPT_SPECIAL},
    {"interactivecomments", 	'k',  0,    OPT_BOURNE},
    {"ksharrays", 		0,    0,    OPT_EMULATE|OPT_BOURNE},
    {"kshoptionprint", 		0,    0,    OPT_EMULATE|OPT_KSH},
    {"listambiguous", 		0,    0,    0},
    {"listbeep", 		0,    0,    OPT_ALL},
    {"listtypes", 		'X',  0,    OPT_CSH},
    {"localoptions", 		0,    0,    OPT_EMULATE|OPT_KSH},
    {"login", 			'l',  'l',  OPT_SPECIAL},
    {"longlistjobs", 		'R',  0,    0},
    {"magicequalsubst", 	0,    0,    OPT_EMULATE},
    {"mailwarning", 		'U',  0,    0},
    {"markdirs", 		'8',  'X',  0},
    {"menucomplete", 		'Y',  0,    0},
    {"monitor", 		'm',  'm',  OPT_SPECIAL},
    {"multios", 		0,    0,    OPT_EMULATE|OPT_ZSH},
    {"nomatch", 		x'3', 0,    OPT_EMULATE|OPT_NONBOURNE},
    {"notify", 			'5',  'b',  OPT_ZSH},
    {"nullglob", 		'G',  0,    OPT_EMULATE},
    {"numericglobsort", 	0,    0,    OPT_EMULATE},
    {"overstrike", 		0,    0,    0},
    {"pathdirs", 		'Q',  0,    OPT_EMULATE},
    {"posixbuiltins",		0,    0,    OPT_EMULATE|OPT_BOURNE},
    {"printeightbit", 		0,    0,    0},
    {"printexitvalue", 		'1',  0,    0},
    {"privileged", 		'p',  'p',  OPT_SPECIAL},
    {"promptcr", 		x'V', 0,    OPT_ALL},
    {"promptsubst", 		0,    0,    OPT_KSH},
    {"pushdignoredups", 	0,    0,    OPT_EMULATE},
    {"pushdminus", 		0,    0,    OPT_EMULATE},
    {"pushdsilent", 		'E',  0,    0},
    {"pushdtohome", 		'D',  0,    OPT_EMULATE},
    {"rcexpandparam", 		'P',  0,    OPT_EMULATE},
    {"rcquotes", 		0,    0,    OPT_EMULATE},
    {"rcs", 			x'f', 0,    OPT_ALL},
    {"recexact", 		'S',  0,    0},
    {"rmstarsilent", 		'H',  0,    OPT_BOURNE},
    {"shfileexpansion",		0,    0,    OPT_EMULATE|OPT_BOURNE},
    {"shglob", 			0,    0,    OPT_EMULATE|OPT_BOURNE},
    {"shinstdin", 		's',  's',  OPT_SPECIAL},
    {"shoptionletters",		0,    0,    OPT_EMULATE|OPT_BOURNE},
    {"shortloops", 		0,    0,    OPT_NONBOURNE},
    {"shwordsplit", 		'y',  0,    OPT_EMULATE|OPT_BOURNE},
    {"singlecommand",		't',  't',  OPT_SPECIAL},
    {"singlelinezle", 		'M',  0,    OPT_KSH},
    {"sunkeyboardhack", 	'L',  0,    0},
    {"unset", 			x'u', x'u', OPT_EMULATE|OPT_BSHELL},
    {"verbose", 		'v',  'v',  0},
    {"xtrace", 			'x',  'x',  0},
    {"zle", 			'Z',  0,    OPT_SPECIAL},
};
# undef x
#endif

EXTERN short int typtab[256];
