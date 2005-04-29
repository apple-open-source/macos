
/*
 * needed for auto configuration
 * $Id: acconfig.h,v 1.1 2004/04/19 17:50:27 dasenbro Exp $
 */

/*
 * If your system is kinda special
 */
#undef SYSTEM_DOS
#undef SYSTEM_QUICKWIN
#undef SYSTEM_WINDLL
#undef SYSTEM_OS2

/*
 * If your system has stdin/stdout/stderr
 */
#undef HAVE_STDIO

/*
 * how to declare functions that are exported from the UU library
 */
#undef UUEXPORT

/*
 * how to declare functions that are exported from the fptools library
 */
#undef TOOLEXPORT

/*
 * define if your compiler supports function prototypes
 */
#undef PROTOTYPES

/*
 * define if your system has chmod(2)
 */
#undef HAVE_CHMOD

/*
 * define if your system has umask(2)
 */
#undef HAVE_UMASK

/*
 * define if your system has mkstemp
 */
#undef HAVE_MKSTEMP

/*
 * Replacement functions.
 * #define strerror _FP_strerror
 * #define tempnam  _FP_tempnam
 * if you don't have these functions
 */
#undef strerror
#undef tempnam

/* 
 * your mailing program. full path and the necessary parameters.
 * the recepient address is added to the command line (with a leading
 * space) without any further options
 */
#undef PROG_MAILER

/* 
 * define if the mailer needs to have the subject set on the command
 * line with -s "Subject". Preferredly, we send the subject as a header.
 */
#undef MAILER_NEEDS_SUBJECT

/* 
 * define if posting is enabled. Do not edit.
 */
#undef HAVE_NEWS

/*
 * your local news posting program. full path and parameters, so that
 * the article and all its headers are read from stdin
 */
#undef PROG_INEWS

