/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

/* we use a small buffer to minimize stack usage constraints */
#define MYBUFSIZE	32

typedef struct {
    char buf[MYBUFSIZE];
    char *ptr;
    char *end;
    int fd;
} BUF;

/* flush the buffer and reset the pointer */
static inline void
flush(BUF *b)
{
    char *buf = b->buf;
    int n = b->ptr - buf;
    int w;

    while(n > 0) {
	w = write(b->fd, buf, n);
	if(w < 0) {
	    if(errno == EINTR || errno == EAGAIN)
		continue;
	    break;
	}
	n -= w;
	buf += n;
    }
    b->ptr = b->buf;
}

/* output a single character */
static inline void
put_c(BUF *b, int c)
{
    if(b->ptr >= b->end)
	flush(b);
    *b->ptr++ = c;
}

/* output a null-terminated string */
static inline void
put_s(BUF *b, const char *str)
{
    while(*str)
	put_c(b, *str++);
}

/* output a string of the specified size */
static inline void
put_n(BUF *b, const char *str, int n)
{
    while(n-- > 0)
	put_c(b, *str++);
}

/*
 * Output the signed decimal string representing the number in "in".  "width" is
 * the minimum field width, and "zero" is a boolean value, true for zero padding
 * (otherwise blank padding).
 */
static void
dec(BUF *b, long long in, int width, int zero)
{
    char buf[32];
    char *cp = buf + sizeof(buf);
    int pad;
    int neg = 0;
    unsigned long long n = (unsigned long long)in;

    if(in < 0) {
	neg++;
	width--;
	n = ~n + 1;
    }
    *--cp = 0;
    if(n) {
	while(n) {
	    *--cp = (n % 10) + '0';
	    n /= 10;
	}
    } else
	*--cp = '0';
    if(neg && zero) {
	put_c(b, '-');
	neg = 0;
    }
    pad = width - strlen(cp);
    zero = zero ? '0' : ' ';
    while(pad-- > 0)
	put_c(b, zero);
    if(neg)
	put_c(b, '-');
    put_s(b, cp);
}

/*
 * Output the hex string representing the number in "i".  "width" is the
 * minimum field width, and "zero" is a boolean value, true for zero padding
 * (otherwise blank padding).  "upper" is a boolean value, true for upper
 * case hex characters, lower case otherwise.  "p" is a boolean value, true
 * if 0x should be prepended (for %p), otherwise nothing.
 */
static char _h[] = "0123456789abcdef";
static char _H[] = "0123456789ABCDEF";
static char _0x[] = "0x";

static void
hex(BUF *b, unsigned long long n, int width, int zero, int upper, int p)
{
    char buf[32];
    char *cp = buf + sizeof(buf);
    char *h = upper ? _H : _h;

    *--cp = 0;
    if(n) {
	while(n) {
	    *--cp = h[n & 0xf];
	    n >>= 4;
	}
    } else
	*--cp = '0';
    if(p) {
	width -= 2;
	if(zero) {
	    put_s(b, _0x);
	    p = 0;
	}
    }
    width -= strlen(cp);
    zero = zero ? '0' : ' ';
    while(width-- > 0)
	put_c(b, zero);
    if(p)
	put_s(b, _0x);
    put_s(b, cp);
}

/*
 * Output the unsigned decimal string representing the number in "in".  "width"
 * is the minimum field width, and "zero" is a boolean value, true for zero
 * padding (otherwise blank padding).
 */
static void
udec(BUF *b, unsigned long long n, int width, int zero)
{
    char buf[32];
    char *cp = buf + sizeof(buf);
    int pad;

    *--cp = 0;
    if(n) {
	while(n) {
	    *--cp = (n % 10) + '0';
	    n /= 10;
	}
    } else
	*--cp = '0';
    pad = width - strlen(cp);
    zero = zero ? '0' : ' ';
    while(pad-- > 0)
	put_c(b, zero);
    put_s(b, cp);
}

/*
 * A simplified vfprintf variant.  The format string is interpreted with
 * arguments for the va_list, and the results are written to the given
 * file descriptor.
 */
void
_simple_vdprintf(int fd, const char *fmt, va_list ap)
{
    BUF b;

    b.fd = fd;
    b.ptr = b.buf;
    b.end = b.buf + MYBUFSIZE;
    while(*fmt) {
	int lflag, zero, width;
	char *cp;
	if(!(cp = strchr(fmt, '%'))) {
	    put_s(&b, fmt);
	    break;
	}
	put_n(&b, fmt, cp - fmt);
	fmt = cp + 1;
	if(*fmt == '%') {
	    put_c(&b, '%');
	    fmt++;
	    continue;
	}
	lflag = zero = width = 0;
	for(;;) {
	    switch(*fmt) {
	    case '0':
		zero++;
		fmt++;
		/* drop through */
	    case '1': case '2': case '3': case '4': case '5':
	    case '6': case '7': case '8': case '9':
		while(*fmt >= '0' && *fmt <= '9')
		    width = 10 * width + (*fmt++ - '0');
		continue;
	    case 'c':
		zero = zero ? '0' : ' ';
		width--;
		while(width-- > 0)
		    put_c(&b, zero);
		put_c(&b, va_arg(ap, int));
		break;
	    case 'd': case 'i':
		switch(lflag) {
		case 0:
		    dec(&b, va_arg(ap, int), width, zero);
		    break;
		case 1:
		    dec(&b, va_arg(ap, long), width, zero);
		    break;
		default:
		    dec(&b, va_arg(ap, long long), width, zero);
		    break;
		}
		break;
	    case 'l':
		lflag++;
		fmt++;
		continue;
	    case 'p':
		hex(&b, (unsigned long)va_arg(ap, void *), width, zero, 0, 1);
		break;
	    case 's':
		cp = va_arg(ap, char *);
		width -= strlen(cp);
		zero = zero ? '0' : ' ';
		while(width-- > 0)
		    put_c(&b, zero);
		put_s(&b, cp);
		break;
	    case 'u':
		switch(lflag) {
		case 0:
		    udec(&b, va_arg(ap, unsigned int), width, zero);
		    break;
		case 1:
		    udec(&b, va_arg(ap, unsigned long), width, zero);
		    break;
		default:
		    udec(&b, va_arg(ap, unsigned long long), width, zero);
		    break;
		}
		break;
	    case 'X': case 'x':
		switch(lflag) {
		case 0:
		    hex(&b, va_arg(ap, unsigned int), width, zero,
			*fmt == 'X', 0);
		    break;
		case 1:
		    hex(&b, va_arg(ap, unsigned long), width, zero,
			*fmt == 'X', 0);
		    break;
		default:
		    hex(&b, va_arg(ap, unsigned long long), width, zero,
			*fmt == 'X', 0);
		    break;
		}
		break;
	    default:
		put_c(&b, *fmt);
		break;
	    }
	    break;
	}
	fmt++;
    }
    flush(&b);
}

/*
 * A simplified fprintf variant.  The format string is interpreted with
 * arguments for the variable argument list, and the results are written
 * to the given file descriptor.
 */
void
_simple_dprintf(int fd, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    _simple_vdprintf(fd, format, ap);
    va_end(ap);
}
