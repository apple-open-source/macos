/*
 * zsh.h - standard header file
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

/* We use <config.h> instead of "config.h" so that a compilation        *
 * using -I. -I$srcdir will use ./config.h rather than $srcdir/config.h *
 * (which it would do because it found this file in $srcdir).           */
#include <config.h>

/*
 * Our longest integer type:  will be a 64 bit either if long already is,
 * or if we found some alternative such as long long.
 * Currently we only define this to be longer than a long if --enable-lfs
 * was given.  That enables internal use of 64-bit types even if
 * no actual large file support is present.
 *
 * This comes before system.h, where we use SIZEOF_ZLONG for
 * defining the digit buffer size.
 */
#ifdef ZSH_64_BIT_TYPE
# define SIZEOF_ZLONG 8
typedef ZSH_64_BIT_TYPE zlong;
#ifdef ZSH_64_BIT_UTYPE
typedef ZSH_64_BIT_UTYPE zulong;
#else
typedef unsigned zlong zulong;
#endif
#else
# define SIZEOF_ZLONG SIZEOF_LONG
typedef long zlong;
typedef unsigned long zulong;
#endif

#include <system.h>

/* A few typical macros */
#define minimum(a,b)  ((a) < (b) ? (a) : (b))

/* math.c */
typedef int LV;

#include "zle.h"

/* Character tokens are sometimes casted to (unsigned char)'s.         * 
 * Unfortunately, some compilers don't correctly cast signed to        * 
 * unsigned promotions; i.e. (int)(unsigned char)((char) -1) evaluates * 
 * to -1, instead of 255 like it should.  We circumvent the troubles   * 
 * of such shameful delinquency by casting to a larger unsigned type   * 
 * then back down to unsigned char.                                    */

#ifdef BROKEN_SIGNED_TO_UNSIGNED_CASTING
# define STOUC(X)	((unsigned char)(unsigned short)(X))
#else
# define STOUC(X)	((unsigned char)(X))
#endif

/* Meta together with the character following Meta denotes the character *
 * which is the exclusive or of 32 and the character following Meta.     *
 * This is used to represent characters which otherwise has special      *
 * meaning for zsh.  These are the characters for which the imeta() test *
 * is true: the null character, and the characters from Meta to Marker.  */

#define Meta		((char) 0x83)

/* Note that the fourth character in DEFAULT_IFS is Meta *
 * followed by a space which denotes the null character. */

#define DEFAULT_IFS	" \t\n\203 "

/* Character tokens */
#define Pound		((char) 0x84)
#define String		((char) 0x85)
#define Hat		((char) 0x86)
#define Star		((char) 0x87)
#define Inpar		((char) 0x88)
#define Outpar		((char) 0x89)
#define Qstring	        ((char) 0x8a)
#define Equals		((char) 0x8b)
#define Bar	      	((char) 0x8c)
#define Inbrace	        ((char) 0x8d)
#define Outbrace	((char) 0x8e)
#define Inbrack	        ((char) 0x8f)
#define Outbrack	((char) 0x90)
#define Tick		((char) 0x91)
#define Inang		((char) 0x92)
#define Outang		((char) 0x93)
#define Quest		((char) 0x94)
#define Tilde		((char) 0x95)
#define Qtick		((char) 0x96)
#define Comma		((char) 0x97)
#define Snull		((char) 0x98)
#define Dnull		((char) 0x99)
#define Bnull		((char) 0x9a)
#define Nularg		((char) 0x9b)

#define INULL(x)	(((x) & 0xfc) == 0x98)

/* Marker used in paramsubst for rc_expand_param */
#define Marker		((char) 0x9c)

/* chars that need to be quoted if meant literally */

#define SPECCHARS "#$^*()=|{}[]`<>?~;&\n\t \\\'\""

enum {
    NULLTOK,		/* 0  */
    SEPER,
    NEWLIN,
    SEMI,
    DSEMI,
    AMPER,		/* 5  */
    INPAR,
    OUTPAR,
    DBAR,
    DAMPER,
    OUTANG,		/* 10 */
    OUTANGBANG,
    DOUTANG,
    DOUTANGBANG,
    INANG,
    INOUTANG,		/* 15 */
    DINANG,
    DINANGDASH,
    INANGAMP,
    OUTANGAMP,
    AMPOUTANG,		/* 20 */
    OUTANGAMPBANG,
    DOUTANGAMP,
    DOUTANGAMPBANG,
    TRINANG,
    BAR,		/* 25 */
    BARAMP,
    INOUTPAR,
    DINPAR,
    DOUTPAR,
    AMPERBANG,		/* 30 */
    DOUTBRACK,
    STRING,
    ENVSTRING,
    ENVARRAY,
    ENDINPUT,		/* 35 */
    LEXERR,

    /* Tokens for reserved words */
    BANG,	/* !         */
    DINBRACK,	/* [[        */
    INBRACE,    /* {         */
    OUTBRACE,   /* }         */	/* 40 */
    CASE,	/* case      */
    COPROC,	/* coproc    */
    DO,		/* do        */
    DONE,	/* done      */
    ELIF,	/* elif      */ /* 45 */
    ELSE,	/* else      */
    ZEND,	/* end       */
    ESAC,	/* esac      */
    FI,		/* fi        */
    FOR,	/* for       */ /* 50 */
    FOREACH,	/* foreach   */
    FUNC,	/* function  */
    IF,		/* if        */
    NOCORRECT,	/* nocorrect */
    REPEAT,	/* repeat    */ /* 55 */
    SELECT,	/* select    */
    THEN,	/* then      */
    TIME,	/* time      */
    UNTIL,	/* until     */
    WHILE	/* while     */ /* 60 */
};

/* Redirection types.  If you modify this, you may also have to modify *
 * redirtab in globals.h and getredirs() in text.c and the IS_* macros *
 * below.                                                              */

enum {
    WRITE,		/* > */
    WRITENOW,		/* >| */
    APP,		/* >> */
    APPNOW,		/* >>| */
    ERRWRITE,		/* &>, >& */
    ERRWRITENOW,	/* >&| */
    ERRAPP,		/* >>& */
    ERRAPPNOW,		/* >>&| */
    READ,		/* < */
    READWRITE,		/* <> */
    HEREDOC,		/* << */
    HEREDOCDASH,	/* <<- */
    HERESTR,		/* <<< */
    MERGEIN,		/* <&n */
    MERGEOUT,		/* >&n */
    CLOSE,		/* >&-, <&- */
    INPIPE,		/* < <(...) */
    OUTPIPE		/* > >(...) */
};

#define IS_APPEND_REDIR(X)    ((X)>=WRITE && (X)<=ERRAPPNOW && ((X) & 2))
#define IS_CLOBBER_REDIR(X)   ((X)>=WRITE && (X)<=ERRAPPNOW && ((X) & 1))
#define IS_ERROR_REDIR(X)     ((X)>=ERRWRITE && (X)<=ERRAPPNOW)
#define IS_READFD(X)          (((X)>=READ && (X)<=MERGEIN) || (X)==INPIPE)
#define IS_REDIROP(X)         ((X)>=OUTANG && (X)<=TRINANG)

/* Flags for input stack */
#define INP_FREE      (1<<0)	/* current buffer can be free'd            */
#define INP_ALIAS     (1<<1)	/* expanding alias or history              */
#define INP_HIST      (1<<2)	/* expanding history                       */
#define INP_CONT      (1<<3)	/* continue onto previously stacked input  */
#define INP_ALCONT    (1<<4)	/* stack is continued from alias expn.     */
#define INP_LINENO    (1<<5)    /* update line number                      */

/* Flags for metafy */
#define META_REALLOC	0
#define META_USEHEAP	1
#define META_STATIC	2
#define META_DUP	3
#define META_ALLOC	4
#define META_NOALLOC	5
#define META_HEAPDUP	6


/**************************/
/* Abstract types for zsh */
/**************************/

typedef struct linknode  *LinkNode;
typedef struct linklist  *LinkList;
typedef struct hashnode  *HashNode;
typedef struct hashtable *HashTable;

typedef struct reswd     *Reswd;
typedef struct alias     *Alias;
typedef struct param     *Param;
typedef struct cmdnam    *Cmdnam;
typedef struct shfunc    *Shfunc;
typedef struct builtin   *Builtin;
typedef struct nameddir  *Nameddir;

typedef struct schedcmd  *Schedcmd;
typedef struct process   *Process;
typedef struct job       *Job;
typedef struct value     *Value;
typedef struct varasg    *Varasg;
typedef struct cond      *Cond;
typedef struct cmd       *Cmd;
typedef struct pline     *Pline;
typedef struct sublist   *Sublist;
typedef struct list      *List;
typedef struct comp      *Comp;
typedef struct redir     *Redir;
typedef struct complist  *Complist;
typedef struct heap      *Heap;
typedef struct heapstack *Heapstack;
typedef struct histent   *Histent;
typedef struct compctlp  *Compctlp;
typedef struct compctl   *Compctl;
typedef struct compcond  *Compcond;
typedef struct forcmd    *Forcmd;

typedef struct asgment  *Asgment;


/********************************/
/* Definitions for linked lists */
/********************************/

/* linked list abstract data type */

struct linknode {
    LinkNode next;
    LinkNode last;
    void *dat;
};

struct linklist {
    LinkNode first;
    LinkNode last;
};

/* Macros for manipulating link lists */

#define addlinknode(X,Y) insertlinknode(X,(X)->last,Y)
#define empty(X)     ((X)->first == NULL)
#define nonempty(X)  ((X)->first != NULL)
#define firstnode(X) ((X)->first)
#define getaddrdata(X) (&((X)->dat))
#define getdata(X)   ((X)->dat)
#define setdata(X,Y) ((X)->dat = (Y))
#define lastnode(X)  ((X)->last)
#define nextnode(X)  ((X)->next)
#define prevnode(X)  ((X)->last)
#define peekfirst(X) ((X)->first->dat)
#define pushnode(X,Y) insertlinknode(X,(LinkNode) X,Y)
#define incnode(X) (X = nextnode(X))
#define gethistent(X) (histentarr+((X)%histentct))


/********************************/
/* Definitions for syntax trees */
/********************************/

/* struct list, struct sublist, struct pline, etc.  all fit the form *
 * of this structure and are used interchangably. The ptrs may hold  *
 * integers or pointers, depending on the type of the node.          */

/* Generic node structure for syntax trees */
struct node {
    int ntype;			/* node type */
};

#define N_LIST    0
#define N_SUBLIST 1
#define N_PLINE   2
#define N_CMD     3
#define N_REDIR   4
#define N_COND    5
#define N_FOR     6
#define N_CASE    7
#define N_IF      8
#define N_WHILE   9
#define N_VARASG 10
#define N_COUNT  11

/* values for types[4] */

#define NT_EMPTY 0
#define NT_NODE  1
#define NT_STR   2
#define NT_LIST  4
#define NT_ARR   8

#define NT_TYPE(T) ((T) & 0xff)
#define NT_N(T, N) (((T) >> (8 + (N) * 4)) & 0xf)
#define NT_SET(T0, N, T1, T2, T3, T4) \
    ((T0) | ((N) << 24) | \
     ((T1) << 8) | ((T2) << 12) | ((T3) << 16) | ((T4) << 20))
#define NT_NUM(T) (((T) >> 24) & 7)
#define NT_HEAP   (1 << 30)

/* tree element for lists */

struct list {
    int ntype;			/* node type */
    int type;
    Sublist left;
    List right;
};

/* These are control flags that are passed *
 * down the execution pipeline.            */
#define Z_TIMED	(1<<0)	/* pipeline is being timed                   */
#define Z_SYNC	(1<<1)	/* run this sublist synchronously       (;)  */
#define Z_ASYNC	(1<<2)	/* run this sublist asynchronously      (&)  */
#define Z_DISOWN (1<<3)	/* run this sublist without job control (&|) */

/* tree element for sublists */

struct sublist {
    int ntype;			/* node type */
    int type;
    int flags;			/* see PFLAGs below */
    Pline left;
    Sublist right;
};

#define ORNEXT  10		/* || */
#define ANDNEXT 11		/* && */

#define PFLAG_NOT     1		/* ! ... */
#define PFLAG_COPROC 32		/* coproc ... */

/* tree element for pipes */

struct pline {
    int ntype;			/* node type */
    int type;
    Cmd left;
    Pline right;
};

#define END	0		/* pnode *right is null                     */
#define PIPE	1		/* pnode *right is the rest of the pipeline */

/* tree element for commands */

struct cmd {
    int ntype;			/* node type                    */
    int type;
    int flags;			/* see CFLAGs below             */
    int lineno;			/* lineno of script for command */
    union {
	List list;		/* for SUBSH/CURSH/SHFUNC       */
	Forcmd forcmd;
	struct casecmd *casecmd;
	struct ifcmd *ifcmd;
	struct whilecmd *whilecmd;
	Sublist pline;
	Cond cond;
	void *generic;
    } u;
    LinkList args;		/* command & argmument List (char *'s)   */
    LinkList redir;		/* i/o redirections (struct redir *'s)   */
    LinkList vars;		/* param assignments (struct varasg *'s) */
};

/* cmd types */
#define SIMPLE   0
#define SUBSH    1
#define CURSH    2
#define ZCTIME   3
#define FUNCDEF  4
#define CFOR     5
#define CWHILE   6
#define CREPEAT  7
#define CIF      8
#define CCASE    9
#define CSELECT 10
#define COND    11
#define CARITH  12

/* flags for command modifiers */
#define CFLAG_EXEC	(1<<0)	/* exec ...    */

/* tree element for redirection lists */

struct redir {
    int ntype;			/* node type */
    int type;
    int fd1, fd2;
    char *name;
};

/* tree element for conditionals */

struct cond {
    int ntype;		/* node type                     */
    int type;		/* can be cond_type, or a single */
			/* letter (-a, -b, ...)          */
    void *left, *right;
};

#define COND_NOT    0
#define COND_AND    1
#define COND_OR     2
#define COND_STREQ  3
#define COND_STRNEQ 4
#define COND_STRLT  5
#define COND_STRGTR 6
#define COND_NT     7
#define COND_OT     8
#define COND_EF     9
#define COND_EQ    10
#define COND_NE    11
#define COND_LT    12
#define COND_GT    13
#define COND_LE    14
#define COND_GE    15

struct forcmd {			/* for/select */
/* Cmd->args contains list of words to loop thru */
    int ntype;			/* node type                          */
    int inflag;			/* if there is an in ... clause       */
    char *name;			/* parameter to assign values to      */
    List list;			/* list to look through for each name */
};

struct casecmd {
/* Cmd->args contains word to test */
    int ntype;			/* node type       */
    char **pats;
    List *lists;		/* list to execute */
};


/*  A command like "if foo then bar elif baz then fubar else fooble"  */
/*  generates a tree like:                                            */
/*                                                                    */
/*  struct ifcmd a = { next =  &b,  ifl = "foo", thenl = "bar" }      */
/*  struct ifcmd b = { next =  &c,  ifl = "baz", thenl = "fubar" }    */
/*  struct ifcmd c = { next = NULL, ifl = NULL, thenl = "fooble" }    */

struct ifcmd {
    int ntype;			/* node type */
    List *ifls;
    List *thenls;
};

struct whilecmd {
    int ntype;			/* node type                           */
    int cond;			/* 0 for while, 1 for until            */
    List cont;			/* condition                           */
    List loop;			/* list to execute until condition met */
};

/* The number of fds space is allocated for  *
 * each time a multio must increase in size. */
#define MULTIOUNIT 8

/* A multio is a list of fds associated with a certain fd.       *
 * Thus if you do "foo >bar >ble", the multio for fd 1 will have *
 * two fds, the result of open("bar",...), and the result of     *
 * open("ble",....).                                             */

/* structure used for multiple i/o redirection */
/* one for each fd open                        */

struct multio {
    int ct;			/* # of redirections on this fd                 */
    int rflag;			/* 0 if open for reading, 1 if open for writing */
    int pipe;			/* fd of pipe if ct > 1                         */
    int fds[MULTIOUNIT];	/* list of src/dests redirected to/from this fd */
};

/* variable assignment tree element */

struct varasg {
    int ntype;			/* node type                             */
    int type;			/* nonzero means array                   */
    char *name;
    char *str;			/* should've been a union here.  oh well */
    LinkList arr;
};

/* lvalue for variable assignment/expansion */

struct value {
    int isarr;
    Param pm;		/* parameter node                      */
    int inv;		/* should we return the index ?        */
    int a;		/* first element of array slice, or -1 */
    int b;		/* last element of array slice, or -1  */
};

/* structure for foo=bar assignments */

struct asgment {
    struct asgment *next;
    char *name;
    char *value;
};

#define MAX_ARRLEN    262144


/********************************************/
/* Defintions for job table and job control */
/********************************************/

/* size of job table */
#define MAXJOB 50

/* entry in the job table */

struct job {
    pid_t gleader;		/* process group leader of this job  */
    pid_t other;		/* subjob id or subshell pid         */
    int stat;                   /* see STATs below                   */
    char *pwd;			/* current working dir of shell when *
				 * this job was spawned              */
    struct process *procs;	/* list of processes                 */
    LinkList filelist;		/* list of files to delete when done */
    int stty_in_env;		/* if STTY=... is present            */
    struct ttyinfo *ty;		/* the modes specified by STTY       */
};

#define STAT_CHANGED	(1<<0)	/* status changed and not reported      */
#define STAT_STOPPED	(1<<1)	/* all procs stopped or exited          */
#define STAT_TIMED	(1<<2)	/* job is being timed                   */
#define STAT_DONE	(1<<3)	/* job is done                          */
#define STAT_LOCKED	(1<<4)	/* shell is finished creating this job, */
                                /*   may be deleted from job table      */
#define STAT_NOPRINT	(1<<5)	/* job was killed internally,           */
                                /*   we don't want to show that         */
#define STAT_INUSE	(1<<6)	/* this job entry is in use             */
#define STAT_SUPERJOB	(1<<7)	/* job has a subjob                     */
#define STAT_SUBJOB	(1<<8)	/* job is a subjob                      */
#define STAT_WASSUPER   (1<<9)  /* was a super-job, sub-job needs to be */
				/* deleted */
#define STAT_CURSH	(1<<10)	/* last command is in current shell     */
#define STAT_NOSTTY	(1<<11)	/* the tty settings are not inherited   */
				/* from this job when it exits.         */
#define STAT_ATTACH	(1<<12)	/* delay reattaching shell to tty       */
#define STAT_SUBLEADER  (1<<13) /* is super-job, but leader is sub-shell */

#define SP_RUNNING -1		/* fake status for jobs currently running */

struct timeinfo {
    long ut;                    /* user space time   */
    long st;                    /* system space time */
};

#define JOBTEXTSIZE 80

/* node in job process lists */

struct process {
    struct process *next;
    pid_t pid;                  /* process id                       */
    char text[JOBTEXTSIZE];	/* text to print when 'jobs' is run */
    int status;			/* return code from waitpid/wait3() */
    struct timeinfo ti;
    struct timeval bgtime;	/* time job was spawned             */
    struct timeval endtime;	/* time job exited                  */
};

struct execstack {
    struct execstack *next;

    LinkList args;
    pid_t list_pipe_pid;
    int nowait;
    int pline_level;
    int list_pipe_child;
    int list_pipe_job;
    char list_pipe_text[JOBTEXTSIZE];
    int lastval;
    int noeval;
    int badcshglob;
    pid_t cmdoutpid;
    int cmdoutval;
    int trapreturn;
    int noerrs;
    int subsh_close;
    char *underscore;
};

struct heredocs {
    struct heredocs *next;
    Redir rd;
};

/*******************************/
/* Definitions for Hash Tables */
/*******************************/

typedef void *(*VFunc) _((void *));
typedef void (*FreeFunc) _((void *));

typedef unsigned (*HashFunc)       _((char *));
typedef void     (*TableFunc)      _((HashTable));
typedef void     (*AddNodeFunc)    _((HashTable, char *, void *));
typedef HashNode (*GetNodeFunc)    _((HashTable, char *));
typedef HashNode (*RemoveNodeFunc) _((HashTable, char *));
typedef void     (*FreeNodeFunc)   _((HashNode));

/* type of function that is passed to *
 * scanhashtable or scanmatchtable    */
typedef void     (*ScanFunc)       _((HashNode, int));

#ifdef ZSH_HASH_DEBUG
typedef void (*PrintTableStats) _((HashTable));
#endif

/* hash table for standard open hashing */

struct hashtable {
    /* HASHTABLE DATA */
    int hsize;			/* size of nodes[]  (number of hash values)   */
    int ct;			/* number of elements                         */
    HashNode *nodes;		/* array of size hsize                        */

#ifdef ZSH_HASH_DEBUG
    char *tablename;		/* string containing name of the hash table */
#endif

    /* HASHTABLE METHODS */
    HashFunc hash;		/* pointer to hash function for this table    */
    TableFunc emptytable;	/* pointer to function to empty table         */
    TableFunc filltable;	/* pointer to function to fill table          */
    AddNodeFunc addnode;	/* pointer to function to add new node        */
    GetNodeFunc getnode;	/* pointer to function to get an enabled node */
    GetNodeFunc getnode2;	/* pointer to function to get node            */
				/* (getnode2 will ignore DISABLED flag)       */
    RemoveNodeFunc removenode;	/* pointer to function to delete a node       */
    ScanFunc disablenode;	/* pointer to function to disable a node      */
    ScanFunc enablenode;	/* pointer to function to enable a node       */
    FreeNodeFunc freenode;	/* pointer to function to free a node         */
    ScanFunc printnode;		/* pointer to function to print a node        */

#ifdef ZSH_HASH_DEBUG
    PrintTableStats printinfo;	/* pointer to function to print table stats */
#endif
};

/* generic hash table node */

struct hashnode {
    HashNode next;		/* next in hash chain */
    char *nam;			/* hash key           */
    int flags;			/* various flags      */
};

/* The flag to disable nodes in a hash table.  Currently  *
 * you can disable builtins, shell functions, aliases and *
 * reserved words.                                        */
#define DISABLED	(1<<0)

/* node in shell reserved word hash table (reswdtab) */

struct reswd {
    HashNode next;		/* next in hash chain        */
    char *nam;			/* name of reserved word     */
    int flags;			/* flags                     */
    int token;			/* corresponding lexer token */
};

/* node in alias hash table (aliastab) */

struct alias {
    HashNode next;		/* next in hash chain       */
    char *nam;			/* hash data                */
    int flags;			/* flags for alias types    */
    char *text;			/* expansion of alias       */
    int inuse;			/* alias is being expanded  */
};

/* is this alias global */
#define ALIAS_GLOBAL	(1<<1)

/* node in command path hash table (cmdnamtab) */

struct cmdnam {
    HashNode next;		/* next in hash chain */
    char *nam;			/* hash data          */
    int flags;
    union {
	char **name;		/* full pathname for external commands */
	char *cmd;		/* file name for hashed commands       */
    }
    u;
};

/* flag for nodes explicitly added to *
 * cmdnamtab with hash builtin        */
#define HASHED		(1<<1)

/* node in shell function hash table (shfunctab) */

struct shfunc {
    HashNode next;		/* next in hash chain     */
    char *nam;			/* name of shell function */
    int flags;			/* various flags          */
    List funcdef;		/* function definition    */
};

/* node in builtin command hash table (builtintab) */

typedef int (*HandlerFunc) _((char *, char **, char *, int));

struct builtin {
    HashNode next;		/* next in hash chain                                 */
    char *nam;			/* name of builtin                                    */
    int flags;			/* various flags                                      */
    HandlerFunc handlerfunc;	/* pointer to function that executes this builtin     */
    int minargs;		/* minimum number of arguments                        */
    int maxargs;		/* maximum number of arguments, or -1 for no limit    */
    int funcid;			/* xbins (see above) for overloaded handlerfuncs      */
    char *optstr;		/* string of legal options                            */
    char *defopts;		/* options set by default for overloaded handlerfuncs */
};

/* builtin flags */
/* DISABLE IS DEFINED AS (1<<0) */
#define BINF_PLUSOPTS		(1<<1)	/* +xyz legal */
#define BINF_R			(1<<2)	/* this is the builtin `r' (fc -e -) */
#define BINF_PRINTOPTS		(1<<3)
#define BINF_FCOPTS		(1<<5)
#define BINF_TYPEOPT		(1<<6)
#define BINF_ECHOPTS		(1<<7)
#define BINF_MAGICEQUALS	(1<<8)  /* needs automatic MAGIC_EQUAL_SUBST substitution */
#define BINF_PREFIX		(1<<9)
#define BINF_DASH		(1<<10)
#define BINF_BUILTIN		(1<<11)
#define BINF_COMMAND		(1<<12)
#define BINF_EXEC		(1<<13)
#define BINF_NOGLOB		(1<<14)
#define BINF_PSPECIAL		(1<<15)

#define BINF_TYPEOPTS   (BINF_TYPEOPT|BINF_PLUSOPTS)

/* node used in parameter hash table (paramtab) */

struct param {
    HashNode next;		/* next in hash chain */
    char *nam;			/* hash data          */
    int flags;			/* PM_* flags         */

    /* the value of this parameter */
    union {
	char **arr;		/* value if declared array   (PM_ARRAY)   */
	char *str;		/* value if declared string  (PM_SCALAR)  */
	zlong val;		/* value if declared integer (PM_INTEGER) */
    } u;

    /* pointer to function to set value of this parameter */
    union {
	void (*cfn) _((Param, char *));
	void (*ifn) _((Param, zlong));
	void (*afn) _((Param, char **));
    } sets;

    /* pointer to function to get value of this parameter */
    union {
	char *(*cfn) _((Param));
	zlong (*ifn) _((Param));
	char **(*afn) _((Param));
    } gets;

    int ct;			/* output base or field width            */
    void *data;			/* used by getfns                        */
    char *env;			/* location in environment, if exported  */
    char *ename;		/* name of corresponding environment var */
    Param old;			/* old struct for use with local         */
    int level;			/* if (old != NULL), level of localness  */
};

/* flags for parameters */

/* parameter types */
#define PM_SCALAR	0	/* scalar                                     */
#define PM_ARRAY	(1<<0)	/* array                                      */
#define PM_INTEGER	(1<<1)	/* integer                                    */

#define PM_TYPE(X) (X & (PM_SCALAR|PM_INTEGER|PM_ARRAY))

#define PM_LEFT		(1<<2)	/* left justify and remove leading blanks     */
#define PM_RIGHT_B	(1<<3)	/* right justify and fill with leading blanks */
#define PM_RIGHT_Z	(1<<4)	/* right justify and fill with leading zeros  */
#define PM_LOWER	(1<<5)	/* all lower case                             */

/* The following are the same since they *
 * both represent -u option to typeset   */
#define PM_UPPER	(1<<6)	/* all upper case                             */
#define PM_UNDEFINED	(1<<6)	/* undefined (autoloaded) shell function      */

#define PM_READONLY	(1<<7)	/* readonly                                   */
#define PM_TAGGED	(1<<8)	/* tagged                                     */
#define PM_EXPORTED	(1<<9)	/* exported                                   */
#define PM_UNIQUE	(1<<10)	/* remove duplicates                          */
#define PM_SPECIAL	(1<<11) /* special builtin parameter                  */
#define PM_DONTIMPORT	(1<<12)	/* do not import this variable                */
#define PM_UNSET	(1<<13)

/* node for compctl hash table (compctltab) */

struct compctlp {
    HashNode next;		/* next in hash chain               */
    char *nam;			/* command name                     */
    int flags;			/* CURRENTLY UNUSED                 */
    Compctl cc;			/* pointer to the compctl desc.     */
};

/* node for named directory hash table (nameddirtab) */

struct nameddir {
    HashNode next;		/* next in hash chain               */
    char *nam;			/* directory name                   */
    int flags;			/* see below                        */
    char *dir;			/* the directory in full            */
    int diff;			/* strlen(.dir) - strlen(.nam)      */
};

/* flags for named directories */
/* DISABLED is defined (1<<0) */
#define ND_USERNAME	(1<<1)	/* nam is actually a username       */


/* flags for controlling printing of hash table nodes */
#define PRINT_NAMEONLY		(1<<0)
#define PRINT_TYPE		(1<<1)
#define PRINT_LIST		(1<<2)

/* flags for printing for the whence builtin */
#define PRINT_WHENCE_CSH	(1<<3)
#define PRINT_WHENCE_VERBOSE	(1<<4)
#define PRINT_WHENCE_SIMPLE	(1<<5)
#define PRINT_WHENCE_FUNCDEF	(1<<6)


/******************************/
/* Definitions for sched list */
/******************************/
 
/* node in sched list */

struct schedcmd {
    struct schedcmd *next;
    char *cmd;			/* command to run */
    time_t time;		/* when to run it */
};


/***********************************/
/* Definitions for history control */
/***********************************/

/* history entry */

struct histent {
    char *text;			/* the history line itself          */
    char *zle_text;		/* the edited history line          */
    time_t stim;		/* command started time (datestamp) */
    time_t ftim;		/* command finished time            */
    short *words;		/* Position of words in history     */
				/*   line:  as pairs of start, end  */
    int nwords;			/* Number of words in history line  */
    int flags;			/* Misc flags                       */
};

#define HIST_OLD	0x00000002	/* Command is already written to disk*/
#define HIST_READ	0x00000004	/* Command was read back from disk*/

/* Parts of the code where history expansion is disabled *
 * should be within a pair of STOPHIST ... ALLOWHIST     */

#define STOPHIST (stophist += 4);
#define ALLOWHIST (stophist -= 4);

#define HISTFLAG_DONE   1
#define HISTFLAG_NOEXEC 2
#define HISTFLAG_RECALL 4
#define HISTFLAG_SETTY  8

/******************************************/
/* Definitions for programable completion */
/******************************************/

struct compcond {
    struct compcond *and, *or;	/* the next or'ed/and'ed conditions    */
    int type;			/* the type (CCT_*)                    */
    int n;			/* the array length                    */
    union {			/* these structs hold the data used to */
	struct {		/* test this condition                 */
	    int *a, *b;		/* CCT_POS, CCT_NUMWORDS               */
	}
	r;
	struct {		/* CCT_CURSTR, CCT_CURPAT,... */
	    int *p;
	    char **s;
	}
	s;
	struct {		/* CCT_RANGESTR,... */
	    char **a, **b;
	}
	l;
    }
    u;
};

#define CCT_UNUSED     0
#define CCT_POS        1
#define CCT_CURSTR     2
#define CCT_CURPAT     3
#define CCT_WORDSTR    4
#define CCT_WORDPAT    5
#define CCT_CURSUF     6
#define CCT_CURPRE     7
#define CCT_CURSUB     8
#define CCT_CURSUBC    9
#define CCT_NUMWORDS  10
#define CCT_RANGESTR  11
#define CCT_RANGEPAT  12

/* Contains the real description for compctls */

struct compctl {
    int refc;			/* reference count                         */
    struct compctl *next;	/* next compctl for -x                     */
    unsigned long mask;		/* mask of things to complete (CC_*)       */
    char *keyvar;		/* for -k (variable)                       */
    char *glob;			/* for -g (globbing)                       */
    char *str;			/* for -s (expansion)                      */
    char *func;			/* for -K (function)                       */
    char *explain;		/* for -X (explanation)                    */
    char *prefix, *suffix;	/* for -P and -S (prefix, suffix)          */
    char *subcmd;		/* for -l (command name to use)            */
    char *hpat;			/* for -H (history pattern)                */
    int hnum;			/* for -H (number of events to search)     */
    struct compctl *ext;	/* for -x (first of the compctls after -x) */
    struct compcond *cond;	/* for -x (condition for this compctl)     */
    struct compctl *xor;	/* for + (next of the xor'ed compctls)     */
};

/* objects to complete */
#define CC_FILES	(1<<0)
#define CC_COMMPATH	(1<<1)
#define CC_REMOVE	(1<<2)
#define CC_OPTIONS	(1<<3)
#define CC_VARS		(1<<4)
#define CC_BINDINGS	(1<<5)
#define CC_ARRAYS	(1<<6)
#define CC_INTVARS	(1<<7)
#define CC_SHFUNCS	(1<<8)
#define CC_PARAMS	(1<<9)
#define CC_ENVVARS	(1<<10)
#define CC_JOBS		(1<<11)
#define CC_RUNNING	(1<<12)
#define CC_STOPPED	(1<<13)
#define CC_BUILTINS	(1<<14)
#define CC_ALREG	(1<<15)
#define CC_ALGLOB	(1<<16)
#define CC_USERS	(1<<17)
#define CC_DISCMDS	(1<<18)
#define CC_EXCMDS	(1<<19)
#define CC_SCALARS	(1<<20)
#define CC_READONLYS	(1<<21)
#define CC_SPECIALS	(1<<22)
#define CC_DELETE	(1<<23)
#define CC_NAMED	(1<<24)
#define CC_QUOTEFLAG	(1<<25)
#define CC_EXTCMDS	(1<<26)
#define CC_RESWDS	(1<<27)

#define CC_RESERVED	(1<<31)


/******************************/
/* Definition for zsh options */
/******************************/

/* Possible values of emulation: the order must match the OPT_*SH values *
 * below.                                                                */

enum {
  EMULATE_CSH, /* C shell */
  EMULATE_KSH, /* Korn shell */
  EMULATE_SH,  /* Bourne shell */
  EMULATE_ZSH  /* `native' mode */
};

/* the option name table */

struct option {
    char *name;			/* full name */
    char id_zsh;		/* single letter name in zsh/csh mode */
    char id_ksh;		/* single letter name in ksh/sh mode */
    unsigned char flags;	/* see below */
};

#define optid(X) ( isset(SHOPTIONLETTERS) ? (X).id_ksh : (X).id_zsh )

/* option flags: the order of the shells here must match the order of the *
 * EMULATE_ values above                                                  */

#define OPT_CSH		0x01	/* option is set by default for csh */
#define OPT_KSH		0x02	/* option is set by default for ksh */
#define OPT_SH		0x04	/* option is set by default for sh */
#define OPT_ZSH		0x08	/* option is set by default for zsh */

#define OPT_ALL		(OPT_CSH|OPT_KSH|OPT_SH|OPT_ZSH)
#define OPT_BOURNE	(OPT_KSH|OPT_SH)
#define OPT_BSHELL	(OPT_KSH|OPT_SH|OPT_ZSH)
#define OPT_NONBOURNE	(OPT_ALL & ~OPT_BOURNE)
#define OPT_NONZSH	(OPT_ALL & ~OPT_ZSH)

#define OPT_EMULATE	0x40	/* option is relevant to emulation */
#define OPT_SPECIAL	0x80	/* option should never be set by emulate() */

/* this can be ored with an option letter to invert its sense */

#define OPT_REV		((char) 0x80)

/* option indices */

enum {
    OPT_INVALID,
    ALLEXPORT,
    ALWAYSLASTPROMPT,
    ALWAYSTOEND,
    APPENDHISTORY,
    AUTOCD,
    AUTOLIST,
    AUTOMENU,
    AUTONAMEDIRS,
    AUTOPARAMKEYS,
    AUTOPARAMSLASH,
    AUTOPUSHD,
    AUTOREMOVESLASH,
    AUTORESUME,
    BADPATTERN,
    BANGHIST,
    BEEP,
    BGNICE,
    BRACECCL,
    BSDECHO,
    CDABLEVARS,
    CHASELINKS,
    CLOBBER,
    COMPLETEALIASES,
    COMPLETEINWORD,
    CORRECT,
    CORRECTALL,
    CSHJUNKIEHISTORY,
    CSHJUNKIELOOPS,
    CSHJUNKIEQUOTES,
    CSHNULLGLOB,
    EQUALS,
    ERREXIT,
    EXECOPT,
    EXTENDEDGLOB,
    EXTENDEDHISTORY,
    FLOWCONTROL,
    FUNCTIONARGZERO,
    GLOBOPT,
    GLOBASSIGN,
    GLOBCOMPLETE,
    GLOBDOTS,
    GLOBSUBST,
    HASHCMDS,
    HASHDIRS,
    HASHLISTALL,
    HISTALLOWCLOBBER,
    HISTBEEP,
    HISTIGNOREDUPS,
    HISTIGNORESPACE,
    HISTNOSTORE,
    HISTREDUCEBLANKS,
    HISTVERIFY,
    HUP,
    IGNOREBRACES,
    IGNOREEOF,
    INTERACTIVE,
    INTERACTIVECOMMENTS,
    KSHARRAYS,
    KSHOPTIONPRINT,
    LISTAMBIGUOUS,
    LISTBEEP,
    LISTTYPES,
    LOCALOPTIONS,
    LOGINSHELL,
    LONGLISTJOBS,
    MAGICEQUALSUBST,
    MAILWARNING,
    MARKDIRS,
    MENUCOMPLETE,
    MONITOR,
    MULTIOS,
    NOMATCH,
    NOTIFY,
    NULLGLOB,
    NUMERICGLOBSORT,
    OVERSTRIKE,
    PATHDIRS,
    POSIXBUILTINS,
    PRINTEIGHTBIT,
    PRINTEXITVALUE,
    PRIVILEGED,
    PROMPTCR,
    PROMPTSUBST,
    PUSHDIGNOREDUPS,
    PUSHDMINUS,
    PUSHDSILENT,
    PUSHDTOHOME,
    RCEXPANDPARAM,
    RCQUOTES,
    RCS,
    RECEXACT,
    RMSTARSILENT,
    SHFILEEXPANSION,
    SHGLOB,
    SHINSTDIN,
    SHOPTIONLETTERS,
    SHORTLOOPS,
    SHWORDSPLIT,
    SINGLECOMMAND,
    SINGLELINEZLE,
    SUNKEYBOARDHACK,
    UNSET,
    VERBOSE,
    XTRACE,
    USEZLE,
    OPT_SIZE
};

#undef isset
#define isset(X) (opts[X])
#define unset(X) (!opts[X])
#define defset(X) (!!(optns[X].flags & (1 << emulation)))

#define interact (isset(INTERACTIVE))
#define jobbing  (isset(MONITOR))
#define islogin  (isset(LOGINSHELL))

/***********************************************/
/* Defintions for terminal and display control */
/***********************************************/

/* tty state structure */

struct ttyinfo {
#ifdef HAVE_TERMIOS_H
    struct termios tio;
#else
# ifdef HAVE_TERMIO_H
    struct termio tio;
# else
    struct sgttyb sgttyb;
    int lmodes;
    struct tchars tchars;
    struct ltchars ltchars;
# endif
#endif
#ifdef TIOCGWINSZ
    struct winsize winsize;
#endif
};

/* defines for whether tabs expand to spaces */
#if defined(HAVE_TERMIOS_H) || defined(HAVE_TERMIO_H)
#define SGTTYFLAG       shttyinfo.tio.c_oflag
#else   /* we're using sgtty */
#define SGTTYFLAG       shttyinfo.sgttyb.sg_flags
#endif
# ifdef TAB3
#define SGTABTYPE       TAB3
# else
#  ifdef OXTABS
#define SGTABTYPE       OXTABS
#  else
#define SGTABTYPE       XTABS
#  endif
# endif

/* flags for termflags */

#define TERM_BAD	0x01	/* terminal has extremely basic capabilities */
#define TERM_UNKNOWN	0x02	/* unknown terminal type */
#define TERM_NOUP	0x04	/* terminal has no up capability */
#define TERM_SHORT	0x08	/* terminal is < 3 lines high */
#define TERM_NARROW	0x10	/* terminal is < 3 columns wide */

/* interesting termcap strings */

#define TCCLEARSCREEN   0
#define TCLEFT          1
#define TCMULTLEFT      2
#define TCRIGHT         3
#define TCMULTRIGHT     4
#define TCUP            5
#define TCMULTUP        6
#define TCDOWN          7
#define TCMULTDOWN      8
#define TCDEL           9
#define TCMULTDEL      10
#define TCINS          11
#define TCMULTINS      12
#define TCCLEAREOD     13
#define TCCLEAREOL     14
#define TCINSLINE      15
#define TCDELLINE      16
#define TCNEXTTAB      17
#define TCBOLDFACEBEG  18
#define TCSTANDOUTBEG  19
#define TCUNDERLINEBEG 20
#define TCALLATTRSOFF  21
#define TCSTANDOUTEND  22
#define TCUNDERLINEEND 23
#define TCHORIZPOS     24
#define TC_COUNT       25

#define tccan(X) (tclen[X])

#define TXTBOLDFACE   0x01
#define TXTSTANDOUT   0x02
#define TXTUNDERLINE  0x04
#define TXTDIRTY      0x80

#define txtisset(X)  (txtattrmask & (X))
#define txtset(X)    (txtattrmask |= (X))
#define txtunset(X)  (txtattrmask &= ~(X))

#define TXTNOBOLDFACE	0x10
#define TXTNOSTANDOUT	0x20
#define TXTNOUNDERLINE	0x40

#define txtchangeisset(X)	(txtchange & (X))
#define txtchangeset(X, Y)	(txtchange |= (X), txtchange &= ~(Y))

/****************************************/
/* Definitions for the %_ prompt escape */
/****************************************/

#include "ztype.h"

#define cmdpush(X) do { \
		       if (cmdsp >= 0 && cmdsp < 256) \
			   cmdstack[cmdsp++] = (X); \
		   } while (0)
#ifdef DEBUG
# define cmdpop()  do { \
		       if (cmdsp <= 0) { \
			   fputs("BUG: cmdstack empty\n", stderr); \
			   fflush(stderr); \
		       } else cmdsp--; \
		   } while (0)
#else
# define cmdpop()  do { if (cmdsp > 0) cmdsp--; } while (0)
#endif

#define CS_FOR          0
#define CS_WHILE        1
#define CS_REPEAT       2
#define CS_SELECT       3
#define CS_UNTIL        4
#define CS_IF           5
#define CS_IFTHEN       6
#define CS_ELSE         7
#define CS_ELIF         8
#define CS_MATH         9
#define CS_COND        10
#define CS_CMDOR       11
#define CS_CMDAND      12
#define CS_PIPE        13
#define CS_ERRPIPE     14
#define CS_FOREACH     15
#define CS_CASE        16
#define CS_FUNCDEF     17
#define CS_SUBSH       18
#define CS_CURSH       19
#define CS_ARRAY       20
#define CS_QUOTE       21
#define CS_DQUOTE      22
#define CS_BQUOTE      23
#define CS_CMDSUBST    24
#define CS_MATHSUBST   25
#define CS_ELIFTHEN    26
#define CS_HEREDOC     27
#define CS_HEREDOCD    28
#define CS_BRACE       29
#define CS_BRACEPAR    30

/*********************
 * Memory management *
 *********************/

#ifndef DEBUG
# define HEAPALLOC	do { int nonlocal_useheap = global_heapalloc(); do

# define PERMALLOC	do { int nonlocal_useheap = global_permalloc(); do

# define LASTALLOC	while (0); \
			if (nonlocal_useheap) global_heapalloc(); \
			else global_permalloc(); \
		} while(0)

# define LASTALLOC_RETURN \
			for (nonlocal_useheap ? global_heapalloc() : \
			     global_permalloc(); 1;) return
#else
# define HEAPALLOC	do { int nonlocal_useheap = global_heapalloc(); \
			alloc_stackp++; do

# define PERMALLOC	do { int nonlocal_useheap = global_permalloc(); \
			alloc_stackp++; do

# define LASTALLOC	while (0); alloc_stackp--; \
			if (nonlocal_useheap) global_heapalloc(); \
			else global_permalloc(); \
		} while(0)

# define LASTALLOC_RETURN \
			for (nonlocal_useheap ? global_heapalloc() : \
			     global_permalloc(); alloc_stackp--;) return
#endif

/****************/
/* Debug macros */
/****************/

#ifdef DEBUG
# define DPUTS(X,Y) do { if ((X)) dputs(Y); } while (0)
# define MUSTUSEHEAP(X) do { if (useheap == 0) { \
		    fprintf(stderr, "BUG: permanent allocation in %s\n", X); \
		    fflush(stderr); \
		} } while (0)
#else
# define DPUTS(X,Y)
# define MUSTUSEHEAP(X)
#endif

/**************************/
/* Signal handling macros */
/**************************/

/* These used in the sigtrapped[] array */

#define ZSIG_TRAPPED	(1<<0)
#define ZSIG_IGNORED	(1<<1)
#define ZSIG_FUNC	(1<<2)

/***********************/
/* Shared header files */
/***********************/

#include "version.h"
#include "signals.h"
#include "globals.h"
#include "prototypes.h"
#include "hashtable.h"
