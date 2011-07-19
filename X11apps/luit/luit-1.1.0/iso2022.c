/*
Copyright (c) 2001 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include "luit.h"
#include "sys.h"
#include "other.h"
#include "iso2022.h"

#define BUFFERED_INPUT_SIZE 4
static unsigned char buffered_input[BUFFERED_INPUT_SIZE];
static int buffered_input_count = 0;

static void
FatalError(const char *f,...)
{
    va_list args;
    va_start(args, f);
    vfprintf(stderr, f, args);
    va_end(args);
    ExitProgram(1);
}

static void
ErrorF(const char *f,...)
{
    va_list args;
    va_start(args, f);
    vfprintf(stderr, f, args);
    va_end(args);
}

#define OUTBUF_FREE(is, count) ((is)->outbuf_count + (count) <= BUFFER_SIZE)
#define OUTBUF_MAKE_FREE(is, fd, count) \
    if(!OUTBUF_FREE((is), (count))) outbuf_flush((is), (fd))

static void
outbuf_flush(Iso2022Ptr is, int fd)
{
    int rc;
    unsigned i = 0;

    if (olog >= 0)
	IGNORE_RC(write(olog, is->outbuf, is->outbuf_count));

    while (i < is->outbuf_count) {
	rc = (int) write(fd, is->outbuf + i, is->outbuf_count - i);
	if (rc > 0) {
	    i += (unsigned) rc;
	} else {
	    if (rc < 0 && errno == EINTR)
		continue;
	    else if ((rc == 0) || ((rc < 0) && (errno == EAGAIN))) {
		waitForOutput(fd);
		continue;
	    } else
		break;
	}
    }
    is->outbuf_count = 0;
}

static void
outbufOne(Iso2022Ptr is, int fd, unsigned c)
{
    OUTBUF_MAKE_FREE(is, fd, 1);
    is->outbuf[is->outbuf_count++] = UChar(c);
}

/* Discards null codepoints */
static void
outbufUTF8(Iso2022Ptr is, int fd, unsigned c)
{
    if (c == 0)
	return;

    if (c <= 0x7F) {
	OUTBUF_MAKE_FREE(is, fd, 1);
	is->outbuf[is->outbuf_count++] = UChar(c);
    } else if (c <= 0x7FF) {
	OUTBUF_MAKE_FREE(is, fd, 2);
	is->outbuf[is->outbuf_count++] = UChar(0xC0 | ((c >> 6) & 0x1F));
	is->outbuf[is->outbuf_count++] = UChar(0x80 | (c & 0x3F));
    } else {
	OUTBUF_MAKE_FREE(is, fd, 3);
	is->outbuf[is->outbuf_count++] = UChar(0xE0 | ((c >> 12) & 0x0F));
	is->outbuf[is->outbuf_count++] = UChar(0x80 | ((c >> 6) & 0x3F));
	is->outbuf[is->outbuf_count++] = UChar(0x80 | (c & 0x3F));
    }
}

static void
buffer(Iso2022Ptr is, unsigned c)
{
    if (is->buffered == NULL) {
	is->buffered_len = 10;
	is->buffered = malloc(is->buffered_len);
	if (is->buffered == NULL)
	    FatalError("Couldn't allocate buffered.\n");
    }

    if (is->buffered_count >= is->buffered_len) {
	is->buffered = realloc(is->buffered, 2 * is->buffered_len + 1);
	if (is->buffered == NULL) {
	    FatalError("Couldn't grow buffered.\n");
	}
	is->buffered_len = 2 * is->buffered_len + 1;
    }

    is->buffered[is->buffered_count++] = UChar(c);
}

static void
outbuf_buffered_carefully(Iso2022Ptr is, int fd)
{
    /* This should never happen in practice */
    unsigned i = 0;

    while (i < is->buffered_count) {
	OUTBUF_MAKE_FREE(is, fd, 1);
	is->outbuf[is->outbuf_count++] = is->buffered[i++];
    }
    is->buffered_count = 0;
}

static void
outbuf_buffered(Iso2022Ptr is, int fd)
{
    if (is->buffered_count > BUFFER_SIZE)
	outbuf_buffered_carefully(is, fd);

    OUTBUF_MAKE_FREE(is, fd, is->buffered_count);
    memcpy(is->outbuf + is->outbuf_count, is->buffered, is->buffered_count);
    is->outbuf_count += is->buffered_count;
    is->buffered_count = 0;
}

static void
discard_buffered(Iso2022Ptr is)
{
    is->buffered_count = 0;
}

Iso2022Ptr
allocIso2022(void)
{
    Iso2022Ptr is;
    is = malloc(sizeof(Iso2022Rec));
    if (!is)
	return NULL;
    is->glp = is->grp = NULL;
    G0(is) = G1(is) = G2(is) = G3(is) = OTHER(is) = NULL;

    is->parserState = P_NORMAL;
    is->shiftState = S_NORMAL;

    is->inputFlags = IF_EIGHTBIT | IF_SS | IF_SSGR;
    is->outputFlags = OF_SS | OF_LS | OF_SELECT;

    is->buffered = NULL;
    is->buffered_len = 0;
    is->buffered_count = 0;

    is->buffered_ku = -1;

    is->outbuf = malloc((size_t) BUFFER_SIZE);
    if (!is->outbuf) {
	free(is);
	return NULL;
    }
    is->outbuf_count = 0;

    return is;
}

void
destroyIso2022(Iso2022Ptr is)
{
    if (is->buffered)
	free(is->buffered);
    if (is->outbuf)
	free(is->outbuf);
    free(is);
}

static int
identifyCharset(Iso2022Ptr i, const CharsetRec * *p)
{
    if (p == &G0(i)) {
	return 0;
    } else if (p == &G1(i)) {
	return 1;
    } else if (p == &G2(i)) {
	return 2;
    } else if (p == &G3(i)) {
	return 3;
    } else {
	abort();
	/* NOTREACHED */
    }
}

void
reportIso2022(Iso2022Ptr i)
{
    if (OTHER(i) != NULL) {
	fprintf(stderr, "%s, non-ISO-2022 encoding.\n", OTHER(i)->name);
	return;
    }
    fprintf(stderr, "G0 is %s, ", G0(i)->name);
    fprintf(stderr, "G1 is %s, ", G1(i)->name);
    fprintf(stderr, "G2 is %s, ", G2(i)->name);
    fprintf(stderr, "G3 is %s.\n", G3(i)->name);
    fprintf(stderr, "GL is G%d, ", identifyCharset(i, i->glp));
    fprintf(stderr, "GR is G%d.\n", identifyCharset(i, i->grp));
}

int
initIso2022(const char *locale, const char *charset, Iso2022Ptr i)
{
    int gl = 0, gr = 2;
    const CharsetRec *g0 = NULL;
    const CharsetRec *g1 = NULL;
    const CharsetRec *g2 = NULL;
    const CharsetRec *g3 = NULL;
    const CharsetRec *other = NULL;
    int rc;

    rc = getLocaleState(locale, charset, &gl, &gr, &g0, &g1, &g2, &g3, &other);
    if (rc < 0) {
	if (charset)
	    ErrorF("Warning: couldn't find charset %s; "
		   "using ISO 8859-1.\n", charset);
	else
	    ErrorF("Warning: couldn't find charset data for locale %s; "
		   "using ISO 8859-1.\n", locale);
    }

    if (g0)
	G0(i) = g0;
    else
	G0(i) = getCharsetByName("ASCII");

    if (g1)
	G1(i) = g1;
    else
	G1(i) = getUnknownCharset(T_94);

    if (g2)
	G2(i) = g2;
    else
	G2(i) = getCharsetByName("ISO 8859-1");

    if (g3)
	G3(i) = g3;
    else
	G3(i) = getUnknownCharset(T_94);

    if (other)
	OTHER(i) = other;
    else
	OTHER(i) = NULL;

    i->glp = &i->g[gl];
    i->grp = &i->g[gr];
    return 0;
}

int
mergeIso2022(Iso2022Ptr d, Iso2022Ptr s)
{
    if (G0(d) == NULL)
	G0(d) = G0(s);
    if (G1(d) == NULL)
	G1(d) = G1(s);
    if (G2(d) == NULL)
	G2(d) = G2(s);
    if (G3(d) == NULL)
	G3(d) = G3(s);
    if (OTHER(d) == NULL)
	OTHER(d) = OTHER(s);
    if (d->glp == NULL)
	d->glp = &(d->g[identifyCharset(s, s->glp)]);
    if (d->grp == NULL)
	d->grp = &(d->g[identifyCharset(s, s->grp)]);
    return 0;
}

static int
utf8Count(unsigned c)
{
    /* All return values must be less than BUFFERED_INPUT_SIZE */
    if ((c & 0x80) == 0)
	return 1;
    else if ((c & 0x40) == 0)
	return 1;		/* incorrect UTF-8 */
    else if ((c & 0x60) == 0x40)
	return 2;
    else if ((c & 0x70) == 0x60)
	return 3;
    else if ((c & 0x78) == 0x70)
	return 4;
    else
	return 1;
}

static int
fromUtf8(unsigned char *b)
{
    if ((b[0] & 0x80) == 0)
	return b[0];
    else if ((b[0] & 0x40) == 0)
	return -1;		/* incorrect UTF-8 */
    else if ((b[0] & 0x60) == 0x40)
	return ((b[0] & 0x1F) << 6) | (b[1] & 0x3F);
    else if ((b[0] & 0x70) == 0x60)
	return (((b[0] & 0x0F) << 12) |
		((b[1] & 0x3F) << 6) |
		((b[2] & 0x3F)));
    else if ((b[0] & 0x78) == 0x70)
	return (((b[0] & 0x03) << 18) |
		((b[1] & 0x3F) << 12) |
		((b[2] & 0x3F) << 6) |
		((b[3] & 0x3F)));
    else
	return -1;
}

void
copyIn(Iso2022Ptr is, int fd, unsigned char *buf, int count)
{
    unsigned char *c;
    int codepoint, rem;

    c = buf;
    rem = count;

#define NEXT do {c++; rem--;} while(0)

    while (rem) {
	codepoint = -1;
	if (is->parserState == P_ESC) {
	    assert(buffered_input_count == 0);
	    codepoint = *c;
	    NEXT;
	    if (*c == CSI_7)
		is->parserState = P_CSI;
	    else if (IS_FINAL_ESC(codepoint))
		is->parserState = P_NORMAL;
	} else if (is->parserState == P_CSI) {
	    assert(buffered_input_count == 0);
	    codepoint = *c;
	    NEXT;
	    if (IS_FINAL_CSI(codepoint))
		is->parserState = P_NORMAL;
	} else if (!(*c & 0x80)) {
	    if (buffered_input_count > 0) {
		buffered_input_count = 0;
		continue;
	    } else {
		codepoint = *c;
		NEXT;
		if (codepoint == ESC)
		    is->parserState = P_ESC;
	    }
	} else if ((*c & 0x40)) {
	    if (buffered_input_count > 0) {
		buffered_input_count = 0;
		continue;
	    } else {
		buffered_input[buffered_input_count] = *c;
		buffered_input_count++;
		NEXT;
	    }
	} else {
	    if (buffered_input_count <= 0) {
		buffered_input_count = 0;
		NEXT;
		continue;
	    } else {
		buffered_input[buffered_input_count] = *c;
		buffered_input_count++;
		NEXT;
		if (buffered_input_count >= utf8Count(buffered_input[0])) {
		    codepoint = fromUtf8(buffered_input);
		    buffered_input_count = 0;
		    if (codepoint == CSI)
			is->parserState = P_CSI;
		}
	    }
	}
#undef NEXT

	if (codepoint >= 0) {
	    int i;
	    unsigned ucode = (unsigned) codepoint;
	    unsigned char obuf[4];

#define WRITE_1(i) do { \
	    obuf[0] = UChar(i); \
	    IGNORE_RC(write(fd, obuf, (size_t) 1)); \
	} while(0)
#define WRITE_2(i) do { \
	    obuf[0] = UChar(((i) >> 8) & 0xFF); \
	    obuf[1] = UChar((i) & 0xFF); \
	    IGNORE_RC(write(fd, obuf, (size_t) 2)); \
	} while(0)

#define WRITE_3(i) do { \
	    obuf[0] = UChar(((i) >> 16) & 0xFF); \
	    obuf[1] = UChar(((i) >>  8) & 0xFF); \
	    obuf[2] = UChar((i) & 0xFF); \
	    IGNORE_RC(write(fd, obuf, (size_t) 3)); \
	} while(0)

#define WRITE_4(i) do { \
	    obuf[0] = UChar(((i) >> 24) & 0xFF); \
	    obuf[1] = UChar(((i) >> 16) & 0xFF); \
	    obuf[2] = UChar(((i) >>  8) & 0xFF); \
	    obuf[3] = UChar((i) & 0xFF); \
	    IGNORE_RC(write(fd, obuf, (size_t) 4)); \
       } while(0)

#define WRITE_1_P_8bit(p, i) { \
	    obuf[0] = UChar(p); \
	    obuf[1] = UChar(i); \
	    IGNORE_RC(write(fd, obuf, (size_t) 2)); \
	}

#define WRITE_1_P_7bit(p, i) { \
	    obuf[0] = ESC; \
	    obuf[1] = UChar((p) - 0x40); \
	    obuf[2] = UChar(i); \
	    IGNORE_RC(write(fd, obuf, (size_t) 3)); \
	}

#define WRITE_1_P(p,i) do { \
	if(is->inputFlags & IF_EIGHTBIT) \
	    WRITE_1_P_8bit(p,i) else \
	    WRITE_1_P_7bit(p,i) \
	} while(0)

#define WRITE_2_P_8bit(p, i) { \
	    obuf[0] = UChar(p); \
	    obuf[1] = UChar(((i) >> 8) & 0xFF); \
	    obuf[2] = UChar((i) & 0xFF); \
	    IGNORE_RC(write(fd, obuf, (size_t) 3)); \
	}

#define WRITE_2_P_7bit(p, i) { \
	    obuf[0] = ESC; \
	    obuf[1] = UChar((p) - 0x40); \
	    obuf[2] = UChar(((i) >> 8) & 0xFF); \
	    obuf[3] = UChar((i) & 0xFF); \
	    IGNORE_RC(write(fd, obuf, (size_t) 4)); \
	}

#define WRITE_2_P(p,i) do { \
	    if(is->inputFlags & IF_EIGHTBIT) \
		WRITE_2_P_8bit(p,i) \
	    else \
		WRITE_2_P_7bit(p,i) \
	} while(0)

#define WRITE_1_P_S(p,i,s) do { \
	    obuf[0] = UChar(p); \
	    obuf[1] = UChar((i) & 0xFF); \
	    obuf[2] = UChar(s); \
	    IGNORE_RC(write(fd, obuf, (size_t) 3)); \
	} while(0)

#define WRITE_2_P_S(p,i,s) do { \
	    obuf[0] = UChar(p); \
	    obuf[1] = UChar(((i) >> 8) & 0xFF); \
	    obuf[2] = UChar((i) & 0xFF); \
	    obuf[3] = UChar(s); \
	    IGNORE_RC(write(fd, obuf, (size_t) 4)); \
	} while(0)

	    if (ucode < 0x20 ||
		(OTHER(is) == NULL && CHARSET_REGULAR(GR(is)) &&
		 (ucode >= 0x80 && ucode < 0xA0))) {
		WRITE_1(ucode);
		continue;
	    }
	    if (OTHER(is) != NULL) {
		unsigned int c2;
		c2 = OTHER(is)->other_reverse(ucode, OTHER(is)->other_aux);
		if (c2 >> 24)
		    WRITE_4(c2);
		else if (c2 >> 16)
		    WRITE_3(c2);
		else if (c2 >> 8)
		    WRITE_2(c2);
		else if (c2)
		    WRITE_1(c2);
		continue;
	    }
	    i = (GL(is)->reverse) (ucode, GL(is));
	    if (i >= 0) {
		switch (GL(is)->type) {
		case T_94:
		case T_96:
		case T_128:
		    if (i >= 0x20)
			WRITE_1(i);
		    break;
		case T_9494:
		case T_9696:
		case T_94192:
		    if (i >= 0x2020)
			WRITE_2(i);
		    break;
		default:
		    abort();
		    /* NOTREACHED */
		}
		continue;
	    }
	    if (is->inputFlags & IF_EIGHTBIT) {
		i = GR(is)->reverse(ucode, GR(is));
		if (i >= 0) {
		    switch (GR(is)->type) {
		    case T_94:
		    case T_96:
		    case T_128:
			/* we allow C1 characters if T_128 in GR */
			WRITE_1(i | 0x80);
			break;
		    case T_9494:
		    case T_9696:
			WRITE_2(i | 0x8080);
			break;
		    case T_94192:
			WRITE_2(i | 0x8000);
			break;
		    default:
			abort();
			/* NOTREACHED */
		    }
		    continue;
		}
	    }
	    if (is->inputFlags & IF_SS) {
		i = G2(is)->reverse(ucode, G2(is));
		if (i >= 0) {
		    switch (GR(is)->type) {
		    case T_94:
		    case T_96:
		    case T_128:
			if (i >= 0x20) {
			    if ((is->inputFlags & IF_EIGHTBIT) &&
				(is->inputFlags & IF_SSGR))
				i |= 0x80;
			    WRITE_1_P(SS2, i);
			}
			break;
		    case T_9494:
		    case T_9696:
			if (i >= 0x2020) {
			    if ((is->inputFlags & IF_EIGHTBIT) &&
				(is->inputFlags & IF_SSGR))
				i |= 0x8080;
			    WRITE_2_P(SS2, i);
			}
			break;
		    case T_94192:
			if (i >= 0x2020) {
			    if ((is->inputFlags & IF_EIGHTBIT) &&
				(is->inputFlags & IF_SSGR))
				i |= 0x8000;
			    WRITE_2_P(SS2, i);
			}
			break;
		    default:
			abort();
			/* NOTREACHED */
		    }
		    continue;
		}
	    }
	    if (is->inputFlags & IF_SS) {
		i = G3(is)->reverse(ucode, G3(is));
		switch (GR(is)->type) {
		case T_94:
		case T_96:
		case T_128:
		    if (i >= 0x20) {
			if ((is->inputFlags & IF_EIGHTBIT) &&
			    (is->inputFlags & IF_SSGR))
			    i |= 0x80;
			WRITE_1_P(SS3, i);
		    }
		    break;
		case T_9494:
		case T_9696:
		    if (i >= 0x2020) {
			if ((is->inputFlags & IF_EIGHTBIT) &&
			    (is->inputFlags & IF_SSGR))
			    i |= 0x8080;
			WRITE_2_P(SS3, i);
		    }
		    break;
		case T_94192:
		    if (i >= 0x2020) {
			if ((is->inputFlags & IF_EIGHTBIT) &&
			    (is->inputFlags & IF_SSGR))
			    i |= 0x8000;
			WRITE_2_P(SS3, i);
		    }
		    break;
		default:
		    abort();
		    /* NOTREACHED */
		}
		continue;
	    }
	    if (is->inputFlags & IF_LS) {
		i = GR(is)->reverse(ucode, GR(is));
		if (i >= 0) {
		    switch (GR(is)->type) {
		    case T_94:
		    case T_96:
		    case T_128:
			WRITE_1_P_S(LS1, i, LS0);
			break;
		    case T_9494:
		    case T_9696:
			WRITE_2_P_S(LS1, i, LS0);
			break;
		    case T_94192:
			WRITE_2_P_S(LS1, i, LS0);
			break;
		    default:
			abort();
			/* NOTREACHED */
		    }
		    continue;
		}
	    }
#undef WRITE_1
#undef WRITE_2
#undef WRITE_1_P
#undef WRITE_1_P_7bit
#undef WRITE_1_P_8bit
#undef WRITE_2_P
#undef WRITE_2_P_7bit
#undef WRITE_2_P_8bit
	}
    }
}

#define PAIR(a,b) ((unsigned) ((a) << 8) | (b))

void
copyOut(Iso2022Ptr is, int fd, unsigned char *buf, unsigned count)
{
    unsigned char *s = buf;

    if (ilog >= 0)
	IGNORE_RC(write(ilog, buf, (size_t) count));

    while (s < buf + count) {
	switch (is->parserState) {
	case P_NORMAL:
	  resynch:
	    if (is->buffered_ku < 0) {
		if (*s == ESC) {
		    buffer(is, *s++);
		    is->parserState = P_ESC;
		} else if (OTHER(is) != NULL) {
		    int c = OTHER(is)->other_stack(*s, OTHER(is)->other_aux);
		    if (c >= 0) {
			unsigned ucode = (unsigned) c;
			outbufUTF8(is, fd,
				   OTHER(is)->other_recode(ucode, OTHER(is)->other_aux));
			is->shiftState = S_NORMAL;
		    }
		    s++;
		} else if (*s == CSI && CHARSET_REGULAR(GR(is))) {
		    buffer(is, *s++);
		    is->parserState = P_CSI;
		} else if ((*s == SS2 ||
			    *s == SS3 ||
			    *s == LS0 ||
			    *s == LS1) &&
			   CHARSET_REGULAR(GR(is))) {
		    buffer(is, *s++);
		    terminate(is, fd);
		    is->parserState = P_NORMAL;
		} else if (*s <= 0x20 && is->shiftState == S_NORMAL) {
		    /* Pass through C0 when GL is not regular */
		    outbufOne(is, fd, *s);
		    s++;
		} else {
		    const CharsetRec *charset;
		    unsigned char code = 0;
		    if (*s <= 0x7F) {
			switch (is->shiftState) {
			case S_NORMAL:
			    charset = GL(is);
			    break;
			case S_SS2:
			    charset = G2(is);
			    break;
			case S_SS3:
			    charset = G3(is);
			    break;
			default:
			    abort();
			    /* NOTREACHED */
			}
			code = *s;
		    } else {
			switch (is->shiftState) {
			case S_NORMAL:
			    charset = GR(is);
			    break;
			case S_SS2:
			    charset = G2(is);
			    break;
			case S_SS3:
			    charset = G3(is);
			    break;
			default:
			    abort();
			    /* NOTREACHED */
			}
			code = UChar(*s - 0x80);
		    }

		    switch (charset->type) {
		    case T_94:
			if (code >= 0x21 && code <= 0x7E)
			    outbufUTF8(is, fd, charset->recode(code, charset));
			else
			    outbufUTF8(is, fd, *s);
			s++;
			is->shiftState = S_NORMAL;
			break;
		    case T_96:
			if (code >= 0x20)
			    outbufUTF8(is, fd, charset->recode(code, charset));
			else
			    outbufUTF8(is, fd, *s);
			is->shiftState = S_NORMAL;
			s++;
			break;
		    case T_128:
			outbufUTF8(is, fd, charset->recode(code, charset));
			is->shiftState = S_NORMAL;
			s++;
			break;
		    default:
			/* First byte of a multibyte sequence */
			is->buffered_ku = *s;
			s++;
		    }
		}
	    } else {		/* buffered_ku */
		const CharsetRec *charset;
		unsigned char ku_code;
		unsigned code = 0;
		if (is->buffered_ku <= 0x7F) {
		    switch (is->shiftState) {
		    case S_NORMAL:
			charset = GL(is);
			break;
		    case S_SS2:
			charset = G2(is);
			break;
		    case S_SS3:
			charset = G3(is);
			break;
		    default:
			abort();
			/* NOTREACHED */
		    }
		    ku_code = UChar(is->buffered_ku);
		    if (*s < 0x80)
			code = *s;
		} else {
		    switch (is->shiftState) {
		    case S_NORMAL:
			charset = GR(is);
			break;
		    case S_SS2:
			charset = G2(is);
			break;
		    case S_SS3:
			charset = G3(is);
			break;
		    default:
			abort();
			/* NOTREACHED */
		    }
		    ku_code = UChar(is->buffered_ku - 0x80);
		    if (*s >= 0x80)
			code = UChar(*s - 0x80);
		}
		switch (charset->type) {
		case T_94:
		case T_96:
		case T_128:
		    abort();
		    /* NOTREACHED */
		    break;
		case T_9494:
		    if (code >= 0x21 && code <= 0x7E) {
			outbufUTF8(is, fd,
				   charset->recode(PAIR(ku_code, code), charset));
			is->buffered_ku = -1;
			is->shiftState = S_NORMAL;
		    } else {
			is->buffered_ku = -1;
			is->shiftState = S_NORMAL;
			goto resynch;
		    }
		    s++;
		    break;
		case T_9696:
		    if (code >= 0x20) {
			outbufUTF8(is, fd,
				   charset->recode(PAIR(ku_code, code), charset));
			is->buffered_ku = -1;
			is->shiftState = S_NORMAL;
		    } else {
			is->buffered_ku = -1;
			is->shiftState = S_NORMAL;
			goto resynch;
		    }
		    s++;
		    break;
		case T_94192:
		    /* Use *s, not code */
		    if (((*s >= 0x21) && (*s <= 0x7E)) ||
			((*s >= 0xA1) && (*s <= 0xFE))) {
			unsigned ucode = PAIR(ku_code, *s);
			outbufUTF8(is, fd,
				   charset->recode(ucode, charset));
			is->buffered_ku = -1;
			is->shiftState = S_NORMAL;
		    } else {
			is->buffered_ku = -1;
			is->shiftState = S_NORMAL;
			goto resynch;
		    }
		    s++;
		    break;
		default:
		    abort();
		    /* NOTREACHED */
		}
	    }
	    break;
	case P_ESC:
	    assert(is->buffered_ku == -1);
	    if (*s == CSI_7) {
		buffer(is, *s++);
		is->parserState = P_CSI;
	    } else if (IS_FINAL_ESC(*s)) {
		buffer(is, *s++);
		terminate(is, fd);
		is->parserState = P_NORMAL;
	    } else {
		buffer(is, *s++);
	    }
	    break;
	case P_CSI:
	    if (IS_FINAL_CSI(*s)) {
		buffer(is, *s++);
		terminate(is, fd);
		is->parserState = P_NORMAL;
	    } else {
		buffer(is, *s++);
	    }
	    break;
	default:
	    abort();
	    /* NOTREACHED */
	}
    }
    outbuf_flush(is, fd);
}

void
terminate(Iso2022Ptr is, int fd)
{
    if (is->outputFlags & OF_PASSTHRU) {
	outbuf_buffered(is, fd);
	return;
    }

    switch (is->buffered[0]) {
    case SS2:
	if (is->outputFlags & OF_SS)
	    is->shiftState = S_SS2;
	discard_buffered(is);
	return;
    case SS3:
	if (is->outputFlags & OF_SS)
	    is->shiftState = S_SS3;
	discard_buffered(is);
	return;
    case LS0:
	if (is->outputFlags & OF_LS)
	    is->glp = &G0(is);
	discard_buffered(is);
	return;
    case LS1:
	if (is->outputFlags & OF_LS)
	    is->glp = &G1(is);
	discard_buffered(is);
	return;
    case ESC:
	assert(is->buffered_count >= 2);
	switch (is->buffered[1]) {
	case SS2_7:
	    if (is->outputFlags & OF_SS)
		is->shiftState = S_SS2;
	    discard_buffered(is);
	    return;
	case SS3_7:
	    if (is->outputFlags & OF_SS)
		is->shiftState = S_SS3;
	    discard_buffered(is);
	    return;
	case LS2_7:
	    if (is->outputFlags & OF_SS)
		is->glp = &G2(is);
	    discard_buffered(is);
	    return;
	case LS3_7:
	    if (is->outputFlags & OF_LS)
		is->glp = &G3(is);
	    discard_buffered(is);
	    return;
	case LS1R_7:
	    if (is->outputFlags & OF_LS)
		is->grp = &G1(is);
	    discard_buffered(is);
	    return;
	case LS2R_7:
	    if (is->outputFlags & OF_LS)
		is->grp = &G2(is);
	    discard_buffered(is);
	    return;
	case LS3R_7:
	    if (is->outputFlags & OF_LS)
		is->grp = &G3(is);
	    discard_buffered(is);
	    return;
	default:
	    terminateEsc(is, fd,
			 is->buffered + 1,
			 (unsigned) (is->buffered_count - 1));
	    break;
	}
	return;
    default:
	outbuf_buffered(is, fd);
    }
}

void
terminateEsc(Iso2022Ptr is, int fd, unsigned char *s_start, unsigned count)
{
    const CharsetRec *charset;

    /* ISO 2022 doesn't allow 2C, but Emacs/MULE uses it in 7-bit
       mode */

    if ((s_start[0] == 0x28 || s_start[0] == 0x29 ||
	 s_start[0] == 0x2A || s_start[0] == 0x2B ||
	 s_start[0] == 0x2C || s_start[0] == 0x2D ||
	 s_start[0] == 0x2E || s_start[0] == 0x2F) &&
	count >= 2) {
	if (is->outputFlags & OF_SELECT) {
	    if (s_start[0] <= 0x2B)
		charset = getCharset(s_start[1], T_94);
	    else
		charset = getCharset(s_start[1], T_96);
	    switch (s_start[0]) {
	    case 0x28:
	    case 0x2C:
		G0(is) = charset;
		break;
	    case 0x29:
	    case 0x2D:
		G1(is) = charset;
		break;
	    case 0x2A:
	    case 0x2E:
		G2(is) = charset;
		break;
	    case 0x2B:
	    case 0x2F:
		G3(is) = charset;
		break;
	    }
	}
	discard_buffered(is);
    } else if (s_start[0] == 0x24 && count == 2) {
	if (is->outputFlags & OF_SELECT) {
	    charset = getCharset(s_start[1], T_9494);
	    G0(is) = charset;
	}
	discard_buffered(is);
    } else if (s_start[0] == 0x24 && count >= 2 &&
	       (s_start[1] == 0x28 || s_start[1] == 0x29 ||
		s_start[1] == 0x2A || s_start[1] == 0x2B ||
		s_start[1] == 0x2D || s_start[1] == 0x2E ||
		s_start[1] == 0x2F) &&
	       count >= 3) {
	if (is->outputFlags & OF_SELECT) {
	    if (s_start[1] <= 0x2B)
		charset = getCharset(s_start[2], T_9494);
	    else
		charset = getCharset(s_start[2], T_9696);
	    switch (s_start[1]) {
	    case 0x28:
		G0(is) = charset;
		break;
	    case 0x29:
	    case 0x2D:
		G1(is) = charset;
		break;
	    case 0x2A:
	    case 0x2E:
		G2(is) = charset;
		break;
	    case 0x2B:
	    case 0x2F:
		G3(is) = charset;
		break;
	    }
	}
	discard_buffered(is);
    } else
	outbuf_buffered(is, fd);
}

#ifdef NO_LEAKS
void
iso2022_leaks(void)
{
}
#endif
