/* exp_log.c - logging routines and other things common to both Expect
   program and library.  Note that this file must NOT have any
   references to Tcl except for including tclInt.h
*/

#include "expect_cf.h"
#include <stdio.h>
/*#include <varargs.h>		tclInt.h drags in varargs.h.  Since Pyramid */
/*				objects to including varargs.h twice, just */
/*				omit this one. */
#include "tclInt.h"
#include "expect_comm.h"
#include "exp_int.h"
#include "exp_rename.h"
#include "exp_log.h"

int loguser = TRUE;		/* if TRUE, expect/spawn may write to stdout */
int logfile_all = FALSE;	/* if TRUE, write log of all interactions */
				/* despite value of loguser. */
FILE *logfile = 0;
FILE *debugfile = 0;
int exp_is_debugging = FALSE;

/* Following this are several functions that log the conversation. */
/* Most of them have multiple calls to printf-style functions.  */
/* At first glance, it seems stupid to reformat the same arguments again */
/* but we have no way of telling how long the formatted output will be */
/* and hence cannot allocate a buffer to do so. */
/* Fortunately, in production code, most of the duplicate reformatting */
/* will be skipped, since it is due to handling errors and debugging. */

/* send to log if open */
/* send to stderr if debugging enabled */
/* use this for logging everything but the parent/child conversation */
/* (this turns out to be almost nothing) */
/* uppercase L differentiates if from math function of same name */
#define LOGUSER		(loguser || force_stdout)
/*VARARGS*/
void
exp_log TCL_VARARGS_DEF(int,arg1)
/*exp_log(va_alist)*/
/*va_dcl*/
{
	int force_stdout;
	char *fmt;
	va_list args;

	force_stdout = TCL_VARARGS_START(int,arg1,args);
	/*va_start(args);*/
	/*force_stdout = va_arg(args,int);*/
	fmt = va_arg(args,char *);
	if (debugfile) vfprintf(debugfile,fmt,args);
	if (logfile_all || (LOGUSER && logfile)) vfprintf(logfile,fmt,args);
	if (LOGUSER) vfprintf(stdout,fmt,args);
	va_end(args);
}

/* just like log but does no formatting */
/* send to log if open */
/* use this function for logging the parent/child conversation */
void
exp_nflog(buf,force_stdout)
char *buf;
int force_stdout;	/* override value of loguser */
{
	int length = strlen(buf);

	if (debugfile) fwrite(buf,1,length,debugfile);
	if (logfile_all || (LOGUSER && logfile)) fwrite(buf,1,length,logfile);
	if (LOGUSER) fwrite(buf,1,length,stdout);
#if 0
	if (logfile_all || (LOGUSER && logfile)) {
		int newlength = exp_copy_out(length);
		fwrite(exp_out_buffer,1,newlength,logfile);
	}
#endif
}
#undef LOGUSER

/* send to log if open and debugging enabled */
/* send to stderr if debugging enabled */
/* use this function for recording unusual things in the log */
/*VARARGS*/
void
debuglog TCL_VARARGS_DEF(char *,arg1)
/*debuglog(va_alist)*/
/*va_dcl*/
{
	char *fmt;
	va_list args;

	fmt = TCL_VARARGS_START(char *,arg1,args);
	/*va_start(args);*/
	/*fmt = va_arg(args,char *);*/
	if (debugfile) vfprintf(debugfile,fmt,args);
	if (is_debugging) {
		vfprintf(stderr,fmt,args);
		if (logfile) vfprintf(logfile,fmt,args);
	}

	va_end(args);
}

/* send to log if open */
/* send to stderr */
/* use this function for error conditions */
/*VARARGS*/
void
exp_errorlog TCL_VARARGS_DEF(char *,arg1)
/*exp_errorlog(va_alist)*/
/*va_dcl*/
{
	char *fmt;
	va_list args;

	fmt = TCL_VARARGS_START(char *,arg1,args);
	/*va_start(args);*/
	/*fmt = va_arg(args,char *);*/
	vfprintf(stderr,fmt,args);
	if (debugfile) vfprintf(debugfile,fmt,args);
	if (logfile) vfprintf(logfile,fmt,args);
	va_end(args);
}

/* just like errorlog but does no formatting */
/* send to log if open */
/* use this function for logging the parent/child conversation */
/*ARGSUSED*/
void
exp_nferrorlog(buf,force_stdout)
char *buf;
int force_stdout;	/* not used, only declared here for compat with */
			/* exp_nflog() */
{
	int length = strlen(buf);
	fwrite(buf,1,length,stderr);
	if (debugfile) fwrite(buf,1,length,debugfile);
	if (logfile) fwrite(buf,1,length,logfile);
}

#if 0
static int out_buffer_size;
static char *outp_last;
static char *out_buffer;
static char *outp;	/* pointer into out_buffer - static in order */
			/* to update whenever out_buffer is enlarged */


void
exp_init_log()
{
	out_buffer = ckalloc(BUFSIZ);
	out_buffer_size = BUFSIZ;
	outp_last = out_buffer + BUFSIZ - 1;
}

char *
enlarge_out_buffer()
{
	int offset = outp - out_buffer;

	int new_out_buffer_size = out_buffer_size = BUFSIZ;
	realloc(out_buffer,new_out_buffer_size);

	out_buffer_size = new_out_buffer_size;
	outp = out_buffer + offset;

	outp_last = out_buffer + out_buffer_size - 1;

	return(out_buffer);
}

/* like sprintf, but uses a static buffer enlarged as necessary */
/* currently supported are %s, %d, and %#d where # is a single-digit */
void
exp_sprintf TCL_VARARGS_DEF(char *,arg1)
/* exp_sprintf(va_alist)*/
/*va_dcl*/
{
	char *fmt;
	va_list args;
	char int_literal[20];	/* big enough for an int literal? */
	char *int_litp;		/* pointer into int_literal */
	char *width;
	char *string_arg;
	int int_arg;
	char *int_fmt;

	fmt = TCL_VARARGS_START(char *,arg1,args);
	/*va_start(args);*/
	/*fmt = va_arg(args,char *);*/

	while (*fmt != '\0') {
		if (*fmt != '%') {
			*outp++ = *fmt++;
			continue;
		}

		/* currently, only single-digit widths are used */
		if (isdigit(*fmt)) {
			width = fmt++;
		} else width = 0;

		switch (*fmt) {
		case 's':	/* interpolate string */
			string_arg = va_arg(args,char *);

			while (*string_arg) {
				if (outp == outp_last) {
					if (enlarge_out_buffer() == 0) {
						/* FAIL */
						return;
					}
				}
				*outp++ = *string_arg++;
			}
			fmt++;
			break;
		case 'd':	/* interpolate int */
			int_arg = va_arg(args,int);

			if (width) int_fmt = width;
			else int_fmt = fmt;

			sprintf(int_literal,int_fmt,int_arg);

			int_litp = int_literal;
			for (int_litp;*int_litp;) {
				if (enlarge_out_buffer() == 0) return;
				*outp++ = *int_litp++;
			}
			fmt++;
			break;
		default:	/* anything else is literal */
			if (enlarge_out_buffer() == 0) return;	/* FAIL */
			*outp++ = *fmt++;
			break;
		}
	}
}

/* copy input string to exp_output, replacing \r\n sequences by \n */
/* return length of new string */
int
exp_copy_out(char *s)
{
	outp = out_buffer;
	int count = 0;

	while (*s) {
		if ((*s == '\r') && (*(s+1) =='\n')) s++;
		if (enlarge_out_buffer() == 0) {
			/* FAIL */
			break;
		}
		*outp = *s;
		count++;
	}
	return count;
}
#endif
