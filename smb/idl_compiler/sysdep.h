/*
 * 
 * (c) Copyright 1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1993 DIGITAL EQUIPMENT CORPORATION
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 *                 permission to use, copy, modify, and distribute this
 * file for any purpose is hereby granted without fee, provided that
 * the above copyright notices and this notice appears in all source
 * code copies, and that none of the names of Open Software
 * Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 * Corporation be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Neither Open Software Foundation, Inc., Hewlett-
 * Packard Company, nor Digital Equipment Corporation makes any
 * representations about the suitability of this software for any
 * purpose.
 * 
 */

/*
 *  OSF DCE Version 1.0
 */

#ifndef sysdep_incl
#define sysdep_incl

/*
**
**  NAME
**
**      SYSDEP.H
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Operating system and compiler dependencies.
**
**  VERSION: DCE 1.0
**
*/

#if defined(__VMS) && !defined(VMS)
#define VMS
#define vms
#endif

/*
 *  exit status codes
 */
#ifdef VMS
#   define pgm_warning 0x10000000  /* 0 % 8 == warning */
#   define pgm_ok      0x00000001  /* 1 % 8 == success */
#   define pgm_error   0x10000002  /* 2 % 8 == error   */
#else
#   define pgm_ok      0
#   define pgm_warning 2
#   define pgm_error   3
#endif

/*
** Macro to test a system-specific status code for failure status.
*/
#ifdef VMS
#define ERROR_STATUS(s) (((s) & 1) != 1)
#else
#define ERROR_STATUS(s) ((s) != 0)
#endif

/*
** define HASDIRTREE if OS has foo/widget/bar file system.
** if HASDIRTREE, define BRANCHCHAR and BRANCHSTRING appropriately
** define HASPOPEN if system can do popen()
** define HASINODES if system has real inodes returned by stat()
*/

/* MSDOS */
#ifdef _MSDOS
#ifndef __STDC__
#define __STDC__ 1
#endif
#define BRANCHCHAR '\\'
#define BRANCHSTRING "\\"
#define HASPOPEN
#define HASDIRTREE
#define DEFAULT_IDIR     "\\mstools\\h"
#define DEFAULT_H_IDIR   "\\mstools\\h"
#define INCLUDE_TEMPLATE "#include <dce\\%s>\n"
#define USER_INCLUDE_TEMPLATE "#include \"%s\"\n"
#define USER_INCLUDE_H_TEMPLATE "#include \"%s.h\"\n"
#define MESSAGE_CATALOG_DIR "\\bin"
#define CD_IDIR "."
#define unlink _unlink
#define getcwd _getcwd
#define chdir _chdir
#define stat _stat
#define popen _popen
#define pclose _pclose
#define S_IFMT _S_IFMT
#define S_IFDIR _S_IFDIR
#define S_IFREG _S_IFREG
#define NO_TRY_CATCH_FINALLY
#ifdef TURBOC
#define stat(a,b) turboc_stat(a,b)
#endif
#endif


/* VAX VMS  */
#ifdef VMS
#define HASDIRTREE
#define BRANCHCHAR ']'
#define BRANCHSTRING "]"
/* VMS model is that system .IDL and .H files are in the same directory. */
#define DEFAULT_IDIR    "DCE:"
#define DEFAULT_H_IDIR  "DCE:"
#define INCLUDE_TEMPLATE "#include <dce/%s>\n"
#define USER_INCLUDE_TEMPLATE "#include <%s>\n"
#define USER_INCLUDE_H_TEMPLATE "#include <%s.h>\n"
#define CD_IDIR "[]"
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(__APPLE__)
#ifndef BSD
#define BSD
#endif
#endif

#if defined(__OSF__) || defined(__OSF1__) || defined(__osf__) ||	\
    defined(BSD) || defined(SYS5) || defined(ultrix) || defined(_AIX) || \
    defined(__ultrix) || defined(_BSD) || defined (linux) || \
    defined (__linux__)
#define UNIX
#define HASDIRTREE
#define HASPOPEN
#define HASINODES
#define BRANCHCHAR '/'
#define BRANCHSTRING "/"
#define CD_IDIR "."
#endif

#ifndef CD_IDIR
Porting Message:  You must provide definitions for the symbols
    describing the directory structure available on your platform.  
#endif

/*
 * Default DCE include directory
 */
#ifndef DEFAULT_IDIR
# define DEFAULT_IDIR "/usr/include"
# define DEFAULT_H_IDIR "/usr/include"
# define INCLUDE_TEMPLATE "#include <dce/%s>\n"
# define USER_INCLUDE_TEMPLATE "#include <%s>\n"
# define USER_INCLUDE_H_TEMPLATE "#include <%s.h>\n"
#endif

/*
 * Default DCE auto import path
 */
#ifndef AUTO_IMPORT_FILE
# define AUTO_IMPORT_FILE "dce/nbase.idl"
#endif

/*
** Default filetype names.
*/
#if defined(VMS) || defined(_MSDOS)
#define OBJ_FILETYPE ".OBJ"
#else
#define OBJ_FILETYPE ".o"
#endif

/*
** Default command to invoke C preprocessor.
*/
#ifdef UNIX
# ifdef apollo
#  define CPP "/usr/lib/cpp "
# elif defined(__APPLE__)
#  define CPP "/usr/bin/cpp -E "
# else
#  define CPP "/lib/cpp "
# endif
#endif

#ifdef VMS
#ifdef __alpha
# define CPP "CC/STANDARD=VAX"
#else
# define CPP "CC "
#endif
#endif

/*
** Default suffixes for IDL-generated files.
*/
#ifdef UNIX
# define CSTUB_SUFFIX   "_cstub.c"
# define SSTUB_SUFFIX   "_sstub.c"
# define HEADER_SUFFIX  ".h"
# define CAUX_SUFFIX    "_caux.c"
# define SAUX_SUFFIX    "_saux.c"
#endif

#ifdef _MSDOS
# define CSTUB_SUFFIX   "_c.c"
# define SSTUB_SUFFIX   "_s.c"
# define HEADER_SUFFIX  ".h"
# define CAUX_SUFFIX    "_x.c"
# define SAUX_SUFFIX    "_y.c"
#endif

#ifdef VMS
# define CSTUB_SUFFIX   "_cstub.c"
# define SSTUB_SUFFIX   "_sstub.c"
# define HEADER_SUFFIX  ".h"
# define CAUX_SUFFIX    "_caux.c"
# define SAUX_SUFFIX    "_saux.c"
#endif

#ifndef CSTUB_SUFFIX
Porting Message:  You must provide definitions for the files suffixes to
    be used on your platform.
#endif

/*
 * Template for IDL version text emitted as comment into generated files.
 */
#ifndef IDL_VERSION_TEXT 
#define IDL_VERSION_TEXT "OSF DCE T1.1.0-03"
#endif
#define IDL_VERSION_TEMPLATE "/* Generated by IDL compiler version %s */\n"

/*
** Default C compiler command and options.  CC_OPT_OBJECT is not defined for
** some unix platforms since "cc -c -o file.o file.c" does not work as expected.
*/
#ifdef UNIX
# if defined(vax) && defined(ultrix)
#  define CC_DEF_CMD "cc -c -Mg"
# else
#  define CC_DEF_CMD "cc -c"
# endif
# if !(defined(vax) && defined(ultrix)) && !defined(apollo) && !defined(_AIX)
#  define CC_OPT_OBJECT "-o "
# endif
#endif

#ifdef VMS
# define CC_DEF_CMD "CC/G_FLOAT"
# define CC_OPT_OBJECT "/OBJECT="
#endif

#ifdef _MSDOS
# define CC_DEF_CMD "cc /c"
# define CC_OPT_OBJECT "/Fo"
#endif

/*
** PASS_I_DIRS_TO_CC determines whether the list of import directories, with
** the system IDL directory replaced by the system H directory if present,
** gets passed as command option(s) to the C compiler when compiling stubs.
*/
#ifndef apollo
# define PASS_I_DIRS_TO_CC
#endif

/*
** Environment variables for IDL system include directories
** on supported platforms.
*/
#ifdef DUMPERS
# define NIDL_LIBRARY_EV "NIDL_LIBRARY"
#endif

/*
** Maximum length of IDL identifiers.  Architecturally specified as 31, but
** on platforms whose C (or other) compilers have more stringent lengths,
** this value might have to be less.
*/
#define MAX_ID 31

/*
** Estimation of available stack size in a server stub.  Under DCE threads
** stack overflow by large amounts can result in indeterminant behavior.  If
** the estimated stack requirements for stack surrogates exceeds the value
** below, objects are allocated via malloc instead of on the stack.
*/
#define AUTO_HEAP_STACK_THRESHOLD 7000

/*
** Symbol for 'audible bell' character.  A workaround for the problem that
** some non-stdc compilers incorrectly map '\a' to 'a'.  Might need work
** on a non-stdc EBCDIC platform.
*/
#if defined(__STDC__)
#define AUDIBLE_BELL '\a'
#define AUDIBLE_BELL_CSTR "\\a"
#else
#define AUDIBLE_BELL '\007'
#define AUDIBLE_BELL_CSTR "\\007"
#endif

/*
** Data type of memory returned by malloc.  In ANSI standard compilers, this
** is a void *, but default to char * for others.
*/
#if defined(__STDC__) || defined(vaxc)
#define heap_mem void
#else
#define heap_mem char
#endif

/*
**  Maximum number of characters in a directory path name for a file.  Used
**  to allocate buffer space for manipulating the path name string.
*/
#ifndef PATH_MAX
# ifdef VMS
# define PATH_MAX  256
# else
# define PATH_MAX 1024
# endif
#endif

/*
** Define macros for NLS entry points used only in message.c
*/
#if defined(_AIX)
#       define NL_SPRINTF NLsprintf
#       define NL_VFPRINTF NLvfprintf
#else
#       define NL_SPRINTF sprintf
#       define NL_VFPRINTF vfprintf
#endif

/*
** Define symbols to reference variables of various YACC implementations
*/
#if defined(ultrix) || defined(__ultrix) || defined(VMS) || defined(_MSDOS)
#define ULTRIX_LEX_YACC
#endif

#if defined(__hp9000s300) || defined(__hp9000s800)
#define HPUX_LEX_YACC
#endif

#if defined(apollo)
#define APOLLO_LEX_YACC
#endif

#if (defined(__OSF__) || defined(__OSF1__)) && !defined(APOLLO_LEX_YACC)
#define OSF_LEX_YACC
#endif

#if defined(_AIX) || defined(__osf__)
#define AIX_LEX_YACC
#define AIX_YACC_VAR extern
#endif

#if defined(linux) || defined(BSD)
#define GNU_LEX_YACC
#endif



/*
 * On SunOS 4.1 platforms, we define an additional constant to
 * distingquish changes between SunOS 4.0 and SunOS 4.1.
 */
#if defined(sun) && defined(sparc)
# define SUN_LEX_YACC
# if defined(__sunos_41__)      /* to distinguish 4.0 from 4.1 */
#  define SUN_41_LEX_YACC
# endif
#endif

/*
 * These tricks let us share lex/yacc macros across certain implementations,
 * even though they might be different.  YACC_VAR is used to handle the
 * situation where a lex/yacc variable is local or nonexistent data in one
 * implementation, but global in others.  YACC_INT is used to handle the
 * situation where a lex/yacc variable has data type short in one
 * implementation, but data type int in others.  AIX_YACC_VAR is to handle
 * yacc variables that only exist in AIX.
 */
#ifndef YACC_INT
#if defined (OSF_LEX_YACC)
#define YACC_VAR
#define YACC_INT int
#elif defined(ULTRIX_LEX_YACC) || (defined(SUN_LEX_YACC) && !defined(SUN_41_LEX_YACC))
#define YACC_VAR
#define YACC_INT short
#else /* SUN_41_LEX_YACC || AIX_LEX_YACC || APOLLO_LEX_YACC || GNU_LEX_YACC */
#define YACC_VAR extern
#define YACC_INT int
#endif
#endif


#if defined(OSF_LEX_YACC) || defined(ULTRIX_LEX_YACC) || defined(APOLLO_LEX_YACC) || defined(AIX_LEX_YACC) || defined(SUN_LEX_YACC) || defined(HPUX_LEX_YACC)\
 || defined(SVR4_LEX_YACC) || defined(UMIPS_LEX_YACC) || defined(GNU_LEX_YACC)

/*
 * The constants below are defined by the output of LEX.  They are not
 * made available in an include file, and are therefore duplicated here.
 * Be very careful!! THESE DEFINITION MUST TRACK THOSE IN THE LEX GENERATED
 * MODULE!!!
 */

#ifndef YYLMAX
#if defined(SVR4_LEX_YACC)
#define YYLMAX 200
#else
#define YYLMAX 1024
#endif
#endif

#ifndef YYTYPE
#ifndef PROCESSING_LEX  /* Avoid doing if compiling lex-generated source */
#define YYTYPE int      /* Warning: if grammar shrinks, YYTYPE can be char! */
#endif
#endif

/*
 * The constants below are defined by the output of YACC.  They are not
 * made available in an include file, and are therefore duplicated here.
 * Be very careful!! THESE DEFINITION MUST TRACK THOSE IN THE YACC GENERATED
 * MODULE!!!
 */
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif

/*
 * Macro to declare all the external cells we are responsible for saving.
 * The non-extern struct declarations below are defined by the output of LEX
 * or YACC as indicated by the comment.  They are not made available in an
 * include file, and are therefore duplicated here.  Be very careful!!
 * THESE DEFINITION MUST TRACK THOSE IN THE LEX/YACC GENERATED MODULE!!!
 */
#define LEX_YACC_EXTERNS        \
extern int      yyprevious;     \
extern char *   yysptr;               \
extern char     yysbuf[];      \
extern int      yynerrs

/*
 * These are variables that are used by some lex or yacc's, but are
 * not current required.
 */
#ifdef notdef
struct yywork { YYTYPE verify, advance; };  /* lex */\
struct yysvf {                              /* lex */\
        struct yywork *yystoff;\
        struct yysvf *yyother;\
        int *yystops;};\
extern struct yysvf *yybgin;\
extern int          yychar;\
extern YACC_INT     yyerrflag;\
extern struct yysvf *yyestate;\
extern int          *yyfnd;\
extern FILE         *yyin;\
extern int          yyleng;\
extern int          yylineno;\
extern struct yysvf **yylsp;\
extern struct yysvf *yylstate;\
extern YYSTYPE      yylval;\
extern int          yymorfg;\
extern int          yynerrs;\
extern struct yysvf **yyolsp;\
extern int          yyprevious;\
YACC_VAR int        *yyps;\
YACC_VAR YYSTYPE    *yypv;\
AIX_YACC_VAR YYSTYPE    *yypvt;\
YACC_VAR YACC_INT   yys[ YYMAXDEPTH ];\
extern char         yysbuf[];\
extern char         *yysptr;\
YACC_VAR YACC_INT   yystate;\
extern int          yytchar;\
extern char         yytext[];\
YACC_VAR int        yytmp;\
extern YYSTYPE      yyv[];\
extern YYSTYPE      yyval
#endif


/*
** This is an opaque pointer to the real state, which is allocated by
** the NIDL_save_lex_yacc_state() routine.
*/
typedef char *lex_yacc_state_t;

extern lex_yacc_state_t NIDL_save_lex_yacc_state(
    void
);
extern void NIDL_restore_lex_yacc_state(
    lex_yacc_state_t state_ptr
);

#define LEX_YACC_STATE_BUFFER(name) lex_yacc_state_t name

#define SAVE_LEX_YACC_STATE(x)  ((x) = NIDL_save_lex_yacc_state())

#define RESTORE_LEX_YACC_STATE(x) NIDL_restore_lex_yacc_state(x)

#endif /* a known lex/yacc machine is defined */



#ifndef LEX_YACC_STATE_BUFFER
Porting Message:  Due to differences between implementations of the
      lex and yacc tools, different state variables must be saved
      in order to perform multiple parses within a single program
      exectuion.  Either enable one of the LEX_YACC sets above on this
      architecture or add an additional set of macros to save/restore
      the variables used by lex and yacc.  This is done via inspection
      of the generated lex/yacc output files for any non-automatic
      state variables.  You might also need to make additions to the
      file acf.h depending on your implementation of lex/yacc.  See
      the comments in acf.h for more information.
#endif
#endif

/*
** To avoid passing the VMS C compiler lines of more than 255 characters, we
** redirect fprintf through a special routine, use on other Digital platforms
** also for consistancy in testing of compiler output.
*/
#if defined(vms) || defined(ultrix) || defined(__osf__)
#define IDL_USE_OUTPUT_LINE
#ifdef fprintf
#undef fprintf
#endif
#define fprintf output_line

/*
** Functions exported by sysdep.c
*/
extern int output_line(
    FILE * fid,                 /* [in] File handle */
    char *format,               /* [in] Format string */
    ...                         /* [in] 0-N format arguments */
);

extern void flush_output_line(
    FILE * fid                  /* [in] File handle */
);
#endif
