/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


/* -----------------------------------------------------------------------------
includes
----------------------------------------------------------------------------- */

#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <CoreFoundation/CoreFoundation.h>

#include "ppp_msg.h"
#include "../Family/PPP.kmodproj/ppp.h"
#include "../Family/PPP.kmodproj/ppp_domain.h"

#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "chap.h"
#include "upap.h"
#include "ppp_client.h"
#include "ppp_utils.h"
#include "ppp_manager.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

struct buffer_info {
    char *ptr;
    int len;
};

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static void pr_log __P((struct ppp *, void *, char *, ...));
static void logit __P((struct ppp *, int, char *, va_list));
static void vslp_printer __P((struct ppp *, void *, char *, ...));
static void format_packet __P((struct ppp *, u_char *, int, void (*) (struct ppp *, void *, char *, ...),
                               void *));

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void closeall()
{
    int i;

    for (i = getdtablesize() - 1; i >= 0; i--) close(i);
    open("/dev/null", O_RDWR, 0);
    dup(0);
    dup(0);
    return;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long loadKext(char *kext)
{
    int pid;

    if ((pid = fork()) < 0)
        return 1;

    if (pid == 0) {
        closeall();
        // PPP kernel extension not loaded, try load it...
        execl("/sbin/kextload", "kextload", kext, (char *)0);
        exit(1);
    }

    if (waitpid(pid, 0, 0) < 0) {
        // fail
        return 1;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long unloadKext(char *kext)
{
    int pid;

    if ((pid = fork()) < 0)
        return 1;

    if (pid == 0) {
        closeall();
        execl("/sbin/kextunload", "kextunload", kext, (char *)0);
        exit(1);
    }

    if (waitpid(pid, 0, 0) < 0) {
        return 1;
    }
    return 0;
}


/* -----------------------------------------------------------------------------
* strlcpy - like strcpy/strncpy, doesn't overflow destination buffer,
* always leaves destination null-terminated (for len > 0).
----------------------------------------------------------------------------- */
size_t strlcpy(char *dest, const char *src, size_t len)
{
    size_t ret = strlen(src);

    if (len != 0) {
	if (ret < len)
	    strcpy(dest, src);
	else {
	    strncpy(dest, src, len - 1);
	    dest[len-1] = 0;
	}
    }
    return ret;
}

/* -----------------------------------------------------------------------------
* strlcat - like strcat/strncat, doesn't overflow destination buffer,
* always leaves destination null-terminated (for len > 0).
----------------------------------------------------------------------------- */
size_t strlcat(char *dest, const char *src, size_t len)
{
    size_t dlen = strlen(dest);

    return dlen + strlcpy(dest + dlen, src, (len > dlen? len - dlen: 0));
}

/* -----------------------------------------------------------------------------
* slprintf - format a message into a buffer.  Like sprintf except we
* also specify the length of the output buffer, and we handle
* %r (recursive format), %m (error message), %v (visible string),
* %q (quoted string), %t (current time) and %I (IP address) formats.
* Doesn't do floating-point formats.
* Returns the number of chars put into buf.
----------------------------------------------------------------------------- */
int slprintf __V((struct ppp *ppp, char *buf, int buflen, char *fmt, ...))
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = vslprintf(ppp, buf, buflen, fmt, args);
    va_end(args);
    return n;
}

/* -----------------------------------------------------------------------------
* vslprintf - like slprintf, takes a va_list instead of a list of args.
----------------------------------------------------------------------------- */
#define OUTCHAR(c)	(buflen > 0? (--buflen, *buf++ = (c)): 0)

int vslprintf(struct ppp *ppp, char *buf, int buflen, char *fmt, va_list args)
{
    int c, i, n;
    int width, prec, fillch;
    int base, len, neg, quoted;
    unsigned long val = 0;
    char *str, *f, *buf0;
    unsigned char *p;
    char num[32];
    time_t t;
    u_int32_t ip;
    static char hexchars[] = "0123456789abcdef";
    struct buffer_info bufinfo;

    buf0 = buf;
    --buflen;
    while (buflen > 0) {
	for (f = fmt; *f != '%' && *f != 0; ++f)
	    ;
	if (f > fmt) {
	    len = f - fmt;
	    if (len > buflen)
		len = buflen;
	    memcpy(buf, fmt, len);
	    buf += len;
	    buflen -= len;
	    fmt = f;
	}
	if (*fmt == 0)
	    break;
	c = *++fmt;
	width = 0;
	prec = -1;
	fillch = ' ';
	if (c == '0') {
	    fillch = '0';
	    c = *++fmt;
	}
	if (c == '*') {
	    width = va_arg(args, int);
	    c = *++fmt;
	} else {
	    while (isdigit(c)) {
		width = width * 10 + c - '0';
		c = *++fmt;
	    }
	}
	if (c == '.') {
	    c = *++fmt;
	    if (c == '*') {
		prec = va_arg(args, int);
		c = *++fmt;
	    } else {
		prec = 0;
		while (isdigit(c)) {
		    prec = prec * 10 + c - '0';
		    c = *++fmt;
		}
	    }
	}
	str = 0;
	base = 0;
	neg = 0;
	++fmt;
	switch (c) {
	case 'd':
	    i = va_arg(args, int);
	    if (i < 0) {
		neg = 1;
		val = -i;
	    } else
		val = i;
	    base = 10;
	    break;
	case 'o':
	    val = va_arg(args, unsigned int);
	    base = 8;
	    break;
	case 'x':
	case 'X':
	    val = va_arg(args, unsigned int);
	    base = 16;
	    break;
	case 'p':
	    val = (unsigned long) va_arg(args, void *);
	    base = 16;
	    neg = 2;
	    break;
	case 's':
	    str = va_arg(args, char *);
	    break;
	case 'c':
	    num[0] = va_arg(args, int);
	    num[1] = 0;
	    str = num;
	    break;
	case 'm':
	    str = strerror(errno);
	    break;
	case 'I':
	    ip = va_arg(args, u_int32_t);
	    ip = ntohl(ip);
	    slprintf(ppp, num, sizeof(num), "%d.%d.%d.%d", (ip >> 24) & 0xff,
		     (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
	    str = num;
	    break;
	case 'r':
	    f = va_arg(args, char *);
#ifndef __powerpc__
	    n = vslprintf(ppp, buf, buflen + 1, f, va_arg(args, va_list));
#else
	    /* On the powerpc, a va_list is an array of 1 structure */
	    n = vslprintf(buf, buflen + 1, f, va_arg(args, void *));
#endif
	    buf += n;
	    buflen -= n;
	    continue;
	case 't':
	    time(&t);
	    str = ctime(&t);
	    str += 4;		/* chop off the day name */
	    str[15] = 0;	/* chop off year and newline */
	    break;
	case 'v':		/* "visible" string */
	case 'q':		/* quoted string */
	    quoted = c == 'q';
	    p = va_arg(args, unsigned char *);
	    if (fillch == '0' && prec >= 0) {
		n = prec;
	    } else {
		n = strlen((char *)p);
		if (prec >= 0 && n > prec)
		    n = prec;
	    }
	    while (n > 0 && buflen > 0) {
		c = *p++;
		--n;
		if (!quoted && c >= 0x80) {
		    OUTCHAR('M');
		    OUTCHAR('-');
		    c -= 0x80;
		}
		if (quoted && (c == '"' || c == '\\'))
		    OUTCHAR('\\');
		if (c < 0x20 || (0x7f <= c && c < 0xa0)) {
		    if (quoted) {
			OUTCHAR('\\');
			switch (c) {
			case '\t':	OUTCHAR('t');	break;
			case '\n':	OUTCHAR('n');	break;
			case '\b':	OUTCHAR('b');	break;
			case '\f':	OUTCHAR('f');	break;
			default:
			    OUTCHAR('x');
			    OUTCHAR(hexchars[c >> 4]);
			    OUTCHAR(hexchars[c & 0xf]);
			}
		    } else {
			if (c == '\t')
			    OUTCHAR(c);
			else {
			    OUTCHAR('^');
			    OUTCHAR(c ^ 0x40);
			}
		    }
		} else
		    OUTCHAR(c);
	    }
	    continue;
	case 'P':		/* print PPP packet */
	    bufinfo.ptr = buf;
	    bufinfo.len = buflen + 1;
	    p = va_arg(args, unsigned char *);
	    n = va_arg(args, int);
	    format_packet(ppp, p, n, vslp_printer, &bufinfo);
	    buf = bufinfo.ptr;
	    buflen = bufinfo.len - 1;
	    continue;
	case 'B':
	    p = va_arg(args, unsigned char *);
	    for (n = prec; n > 0; --n) {
		c = *p++;
		if (fillch == ' ')
		    OUTCHAR(' ');
		OUTCHAR(hexchars[(c >> 4) & 0xf]);
		OUTCHAR(hexchars[c & 0xf]);
	    }
	    continue;
	default:
	    *buf++ = '%';
	    if (c != '%')
		--fmt;		/* so %z outputs %z etc. */
	    --buflen;
	    continue;
	}
	if (base != 0) {
	    str = num + sizeof(num);
	    *--str = 0;
	    while (str > num + neg) {
		*--str = hexchars[val % base];
		val = val / base;
		if (--prec <= 0 && val == 0)
		    break;
	    }
	    switch (neg) {
	    case 1:
		*--str = '-';
		break;
	    case 2:
		*--str = 'x';
		*--str = '0';
		break;
	    }
	    len = num + sizeof(num) - 1 - str;
	} else {
	    len = strlen(str);
	    if (prec >= 0 && len > prec)
		len = prec;
	}
	if (width > 0) {
	    if (width > buflen)
		width = buflen;
	    if ((n = width - len) > 0) {
		buflen -= n;
		for (; n > 0; --n)
		    *buf++ = fillch;
	    }
	}
	if (len > buflen)
	    len = buflen;
	memcpy(buf, str, len);
	buf += len;
	buflen -= len;
    }
    *buf = 0;
    return buf - buf0;
}

/* -----------------------------------------------------------------------------
* vslp_printer - used in processing a %P format
----------------------------------------------------------------------------- */
void vslp_printer __V((struct ppp *ppp, void *arg, char *fmt, ...))
{
    int n;
    va_list pvar;
    struct buffer_info *bi;

    va_start(pvar, fmt);
    bi = (struct buffer_info *) arg;
    n = vslprintf(ppp, bi->ptr, bi->len, fmt, pvar);
    va_end(pvar);

    bi->ptr += n;
    bi->len -= n;
}

/* -----------------------------------------------------------------------------
* log_packet - format a packet and log it.
----------------------------------------------------------------------------- */

char line[256];			/* line to be logged accumulated here */
char *linep;

void log_packet(struct ppp *ppp, u_char *p, int len, char *prefix, int level)
{
    strlcpy(line, prefix, sizeof(line));
    linep = line + strlen(line);
    format_packet(ppp, p, len, pr_log, NULL);
    if (linep != line)
	syslog(level, "%s", line);
}

/* -----------------------------------------------------------------------------
* format_packet - make a readable representation of a packet,
* calling `printer(arg, format, ...)' to output it.
----------------------------------------------------------------------------- */
void format_packet(struct ppp *ppp, u_char *p, int len, void (*printer) __P((struct ppp *, void *, char *, ...)), void *arg)
{
    int i, n;
    u_short proto;
    struct protent *protp;

    if (len >= PPP_HDRLEN && p[0] == PPP_ALLSTATIONS && p[1] == PPP_UI) {
	p += 2;
	GETSHORT(proto, p);
	len -= PPP_HDRLEN;
	for (i = 0; (protp = ppp->protocols[i]) != NULL; ++i)
	    if (proto == protp->protocol)
		break;
	if (protp != NULL) {
	    printer(ppp, arg, "[%s", protp->name);
	    n = (*protp->printpkt)(ppp, p, len, printer, arg);
	    printer(ppp, arg, "]");
	    p += n;
	    len -= n;
	} else {
	    for (i = 0; (protp = ppp->protocols[i]) != NULL; ++i)
		if (proto == (protp->protocol & ~0x8000))
		    break;
	    if (protp != 0 && protp->data_name != 0) {
		printer(ppp, arg, "[%s data]", protp->data_name);
		if (len > 8)
		    printer(ppp, arg, "%.8B ...", p);
		else
		    printer(ppp, arg, "%.*B", len, p);
		len = 0;
	    } else
		printer(ppp, arg, "[proto=0x%x]", proto);
	}
    }

    if (len > 32)
	printer(ppp, arg, "%.32B ...", p);
    else
	printer(ppp, arg, "%.*B", len, p);
}

/* -----------------------------------------------------------------------------
* log_packet - format a packet and log it.
----------------------------------------------------------------------------- */
void pr_log __V((struct ppp *ppp, void *arg, char *fmt, ...))
{
    int n;
    va_list pvar;
    char buf[256];

    va_start(pvar, fmt);
    n = vslprintf(ppp, buf, sizeof(buf), fmt, pvar);
    va_end(pvar);

    if (linep + n + 1 > line + sizeof(line)) {
	syslog(LOG_DEBUG, "%s", line);
	linep = line;
    }
    strlcpy(linep, buf, line + sizeof(line) - linep);
    linep += n;
}

/* -----------------------------------------------------------------------------
* print_string - print a readable representation of a string using printer.
----------------------------------------------------------------------------- */
void print_string(struct ppp *ppp, char *p, int len, void (*printer) __P((struct ppp *, void *, char *, ...)), void *arg)
{
    int c;

    printer(ppp, arg, "\"");
    for (; len > 0; --len) {
	c = *p++;
	if (' ' <= c && c <= '~') {
	    if (c == '\\' || c == '"')
		printer(ppp, arg, "\\");
	    printer(ppp, arg, "%c", c);
	} else {
	    switch (c) {
	    case '\n':
		printer(ppp, arg, "\\n");
		break;
	    case '\r':
		printer(ppp, arg, "\\r");
		break;
	    case '\t':
		printer(ppp, arg, "\\t");
		break;
	    default:
		printer(ppp, arg, "\\%.3o", c);
	    }
	}
    }
    printer(ppp, arg, "\"");
}

/* -----------------------------------------------------------------------------
* logit - does the hard work for fatal et al.
----------------------------------------------------------------------------- */
void logit(struct ppp *ppp, int level, char *fmt, va_list args)
{
    int n;
    char buf[256];

    n = vslprintf(ppp, buf, sizeof(buf), fmt, args);

//
    syslog(level, "%s", buf);
//
    printf("%s\n", buf);

    if (ppp->log_to_fd > 0 && (level != LOG_DEBUG || ppp->debug)) {
	if (buf[n-1] != '\n')
	    buf[n++] = '\n';
        if (write(ppp->log_to_fd, buf, n) != n)
            ppp->log_to_fd = 0;
    }
}

/* -----------------------------------------------------------------------------
* error - log an error message.
----------------------------------------------------------------------------- */
void error __V((struct ppp *ppp, char *fmt, ...))
{
    va_list pvar;

    va_start(pvar, fmt);
    logit(ppp, LOG_ERR, fmt, pvar);
    va_end(pvar);
}

/* -----------------------------------------------------------------------------
* warn - log a warning message.
----------------------------------------------------------------------------- */
void warn __V((struct ppp *ppp, char *fmt, ...))
{
    va_list pvar;

    va_start(pvar, fmt);
    logit(ppp, LOG_WARNING, fmt, pvar);
    va_end(pvar);
}

/* -----------------------------------------------------------------------------
* notice - log a notice-level message.
----------------------------------------------------------------------------- */
void notice __V((struct ppp *ppp, char *fmt, ...))
{
    va_list pvar;

    va_start(pvar, fmt);
    logit(ppp, LOG_NOTICE, fmt, pvar);
    va_end(pvar);
}

/* -----------------------------------------------------------------------------
* info - log an informational message.
----------------------------------------------------------------------------- */
void info __V((struct ppp *ppp, char *fmt, ...))
{
    va_list pvar;

    va_start(pvar, fmt);
    logit(ppp, LOG_INFO, fmt, pvar);
    va_end(pvar);
}

/* -----------------------------------------------------------------------------
* dbglog - log a debug message.
----------------------------------------------------------------------------- */
void dbglog __V((struct ppp *ppp, char *fmt, ...))
{
    va_list pvar;

    va_start(pvar, fmt);
    logit(ppp, LOG_INFO /*LOG_DEBUG*/, fmt, pvar);
    va_end(pvar);
}

/* -----------------------------------------------------------------------------
* run a program to talk to the serial device
* (e.g. to run the connector or disconnector script).
program contain only the path to the program
command contains the full commanf to execute (prgrname+parameters)
----------------------------------------------------------------------------- */
pid_t start_program(char *program, char *command, int in, int out, int err)
//pid_t start_program(char *program, char *command)
{
    int 	pid, i, isapp = 0, error;
    u_short 	len;
    char 	cmd[256];

    len = strlen(program);

    pid = fork();
    if (pid < 0) {
        //       error(ppp, "Failed to create child process: %m");
        return 0;
    }

    if (pid == 0) {

            len = strlen(program);
            if ((len > 4)
                && (program[len-4] == '.')
                && (program[len-3] == 'a')
                && (program[len-2] == 'p')
                && (program[len-1] == 'p')) {
                // need to open a cocoa app
                // don't redirect in and out
                isapp = 1;
            }

#define INOUT 1
#ifdef INOUT
          for (i = 0; i < getdtablesize(); i++) {
            if (isapp || ((i != in) && (i != out))) {// && (i != err)) 
                error = close(i);
                }
        }
        fflush(stdin);	// flush in/out, otherwise the previous printf will go to the modem
        fflush(stdout);
        dup2(in, 0);
        dup2(out, 1);
        //dup2(err, 2);
        close(in);
        close(out);
        //close(err);
#else
        closeall();
#endif
        
        // double fork, because configd doesn't handle SIGCHLD
        pid = fork();
        if (pid < 0) {
            return 0;
        }

        if (pid == 0) {

           len = strlen(program);
            if ((len > 4)
                && (program[len-4] == '.')
                && (program[len-3] == 'a')
                && (program[len-2] == 'p')
                && (program[len-1] == 'p')) {
                // need to open a cocoa app
                // fix me : don't know yet how to give parameters to cocao apps
                sprintf(cmd, "/usr/bin/open %s", program);
                system(cmd);
                //execl("/bin/sh", "sh", "-c", "/usr/bin/open", program, (char *)0);
            }
            else {
                // need to exec a tool, with complete parameters list
                execl("/bin/tcsh", "tcsh", "-c", command, (char *)0);

            }

            // grandchild exits
            exit(0);
            /* NOTREACHED */
        }

        // child exits
        exit(0);
        /* NOTREACHED */
    }
   
    // grand parents wait for child's completion, that occurs immediatly since grandchild does the job
    if (waitpid(pid, 0, 0) < 0) {
        return 1;
    }
    return pid;
}

/* -----------------------------------------------------------------------------
return 1 if the file exist, 0 otherwise
----------------------------------------------------------------------------- */
u_char exist_file(char *filename)
{
    int 	ret;
    struct stat	statbuf;
    
    ret = stat(filename, &statbuf);

    return ret >= 0;
}
