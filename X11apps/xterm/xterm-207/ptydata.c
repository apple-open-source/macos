/* $XTermId: ptydata.c,v 1.65 2005/04/22 00:21:54 tom Exp $ */

/*
 * $XFree86: xc/programs/xterm/ptydata.c,v 1.23 2005/04/22 00:21:54 dickey Exp $
 */

/************************************************************

Copyright 1999-2004,2005 by Thomas E. Dickey

                        All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization.

********************************************************/

#include <data.h>

#if OPT_WIDE_CHARS
#include <menu.h>
#endif

/*
 * Check for both EAGAIN and EWOULDBLOCK, because some supposedly POSIX
 * systems are broken and return EWOULDBLOCK when they should return EAGAIN.
 * Note that this macro may evaluate its argument more than once.
 */
#if defined(EAGAIN) && defined(EWOULDBLOCK)
#define E_TEST(err) ((err) == EAGAIN || (err) == EWOULDBLOCK)
#else
#ifdef EAGAIN
#define E_TEST(err) ((err) == EAGAIN)
#else
#define E_TEST(err) ((err) == EWOULDBLOCK)
#endif
#endif

#if OPT_WIDE_CHARS
/*
 * Convert the 8-bit codes in data->buffer[] into Unicode in data->utf_data.
 * The number of bytes converted will be nonzero iff there is data.
 */
static Bool
decodeUtf8(PtyData * data)
{
    int i;
    int length = data->last - data->next;
    int utf_count = 0;
    IChar utf_char = 0;

    data->utf_size = 0;
    for (i = 0; i < length; i++) {
	unsigned c = data->next[i];

	/* Combine UTF-8 into Unicode */
	if (c < 0x80) {
	    /* We received an ASCII character */
	    if (utf_count > 0) {
		data->utf_data = UCS_REPL;	/* prev. sequence incomplete */
		data->utf_size = (i + 1);
	    } else {
		data->utf_data = c;
		data->utf_size = 1;
	    }
	    break;
	} else if (c < 0xc0) {
	    /* We received a continuation byte */
	    if (utf_count < 1) {
		/*
		 * We received a continuation byte before receiving a sequence
		 * state.  Or an attempt to use a C1 control string.  Either
		 * way, it is mapped to the replacement character.
		 */
		data->utf_data = UCS_REPL;	/* ... unexpectedly */
		data->utf_size = (i + 1);
		break;
	    } else {
		/* Check for overlong UTF-8 sequences for which a shorter
		 * encoding would exist and replace them with UCS_REPL.
		 * An overlong UTF-8 sequence can have any of the following
		 * forms:
		 *   1100000x 10xxxxxx
		 *   11100000 100xxxxx 10xxxxxx
		 *   11110000 1000xxxx 10xxxxxx 10xxxxxx
		 *   11111000 10000xxx 10xxxxxx 10xxxxxx 10xxxxxx
		 *   11111100 100000xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
		 */
		if (!utf_char && !((c & 0x7f) >> (7 - utf_count))) {
		    utf_char = UCS_REPL;
		}
		/* characters outside UCS-2 become UCS_REPL */
		if (utf_char > 0x03ff) {
		    /* value would be >0xffff */
		    utf_char = UCS_REPL;
		} else {
		    utf_char <<= 6;
		    utf_char |= (c & 0x3f);
		}
		if ((utf_char >= 0xd800 &&
		     utf_char <= 0xdfff) ||
		    (utf_char == 0xfffe) ||
		    (utf_char == 0xffff)) {
		    utf_char = UCS_REPL;
		}
		utf_count--;
		if (utf_count == 0) {
		    data->utf_data = utf_char;
		    data->utf_size = (i + 1);
		    break;
		}
	    }
	} else {
	    /* We received a sequence start byte */
	    if (utf_count > 0) {
		data->utf_data = UCS_REPL;	/* prev. sequence incomplete */
		data->utf_size = (i + 1);
		break;
	    }
	    if (c < 0xe0) {
		utf_count = 1;
		utf_char = (c & 0x1f);
		if (!(c & 0x1e))
		    utf_char = UCS_REPL;	/* overlong sequence */
	    } else if (c < 0xf0) {
		utf_count = 2;
		utf_char = (c & 0x0f);
	    } else if (c < 0xf8) {
		utf_count = 3;
		utf_char = (c & 0x07);
	    } else if (c < 0xfc) {
		utf_count = 4;
		utf_char = (c & 0x03);
	    } else if (c < 0xfe) {
		utf_count = 5;
		utf_char = (c & 0x01);
	    } else {
		data->utf_data = UCS_REPL;
		data->utf_size = (i + 1);
		break;
	    }
	}
    }
#if OPT_TRACE > 1
    TRACE(("UTF-8 char %04X [%d..%d]\n",
	   data->utf_data,
	   data->next - data->buffer,
	   data->next - data->buffer + data->utf_size - 1));
#endif

    return (data->utf_size != 0);
}
#endif

int
readPtyData(TScreen * screen, PtySelect * select_mask, PtyData * data)
{
    int size = 0;

#ifdef VMS
    if (*select_mask & pty_mask) {
	trimPtyData(screen, data);
	if (read_queue.flink != 0) {
	    size = tt_read(data->next);
	    if (size == 0) {
		Panic("input: read returned zero\n", 0);
	    }
	} else {
	    sys$hiber();
	}
    }
#else /* !VMS */
    if (FD_ISSET(screen->respond, select_mask)) {
	trimPtyData(screen, data);

	size = read(screen->respond, (char *) data->last, FRG_SIZE);
	if (size <= 0) {
	    /*
	     * Yes, I know this is a majorly f*ugly hack, however it seems to
	     * be necessary for Solaris x86.  DWH 11/15/94
	     * Dunno why though..
	     * (and now CYGWIN, alanh@xfree86.org 08/15/01
	     */
#if (defined(i386) && defined(SVR4) && defined(sun)) || defined(__CYGWIN__)
	    if (errno == EIO || errno == 0)
#else
	    if (errno == EIO)
#endif
		Cleanup(0);
	    else if (!E_TEST(errno))
		Panic("input: read returned unexpected error (%d)\n", errno);
	    size = 0;
	} else if (size == 0) {
#if defined(__UNIXOS2__)
	    Cleanup(0);
#else
	    Panic("input: read returned zero\n", 0);
#endif
	}
    }
#endif /* VMS */

    if (size) {
#if OPT_TRACE
	int i;

	TRACE(("read %d bytes from pty\n", size));
	for (i = 0; i < size; i++) {
	    if (!(i % 16))
		TRACE(("%s", i ? "\n    " : "READ"));
	    TRACE((" %02X", data->last[i]));
	}
	TRACE(("\n"));
#endif
	data->last += size;
#ifdef ALLOWLOGGING
	term->screen.logstart = VTbuffer->next;
#endif
    }

    return (size);
}

/*
 * Check if there is more data in the input buffer which can be returned by
 * nextPtyData().  If there is insufficient data to return a completed UTF-8
 * value, return false anyway.
 */
#if OPT_WIDE_CHARS
Bool
morePtyData(TScreen * screen GCC_UNUSED, PtyData * data)
{
    Bool result = (data->last > data->next);
    if (result && screen->utf8_inparse) {
	if (!data->utf_size)
	    result = decodeUtf8(data);
    }
    TRACE2(("morePtyData returns %d\n", result));
    return result;
}
#endif

/*
 * Return the next value from the input buffer.  Note that morePtyData() is
 * always called before this function, so we can do the UTF-8 input conversion
 * in that function and simply return the result here.
 */
#if OPT_WIDE_CHARS
IChar
nextPtyData(TScreen * screen, PtyData * data)
{
    IChar result;
    if (screen->utf8_inparse) {
	result = data->utf_data;
	data->next += data->utf_size;
	data->utf_size = 0;
    } else {
	result = *((data)->next++);
	if (!screen->output_eight_bits)
	    result &= 0x7f;
    }
    TRACE2(("nextPtyData returns %#x\n", result));
    return result;
}
#endif

#if OPT_WIDE_CHARS
/*
 * Called when UTF-8 mode has been turned on/off.
 */
void
switchPtyData(TScreen * screen, int flag)
{
    if (screen->utf8_mode != flag) {
	screen->utf8_mode = flag;
	screen->utf8_inparse = (flag != 0);

	TRACE(("turning UTF-8 mode %s\n", BtoS(flag)));
	update_font_utf8_mode();
    }
}
#endif

void
initPtyData(PtyData ** result)
{
    PtyData *data;

    TRACE(("initPtyData given minBufSize %d, maxBufSize %d\n",
	   FRG_SIZE, BUF_SIZE));

    if (FRG_SIZE < 64)
	FRG_SIZE = 64;
    if (BUF_SIZE < FRG_SIZE)
	BUF_SIZE = FRG_SIZE;
    if (BUF_SIZE % FRG_SIZE)
	BUF_SIZE = BUF_SIZE + FRG_SIZE - (BUF_SIZE % FRG_SIZE);

    TRACE(("initPtyData using minBufSize %d, maxBufSize %d\n",
	   FRG_SIZE, BUF_SIZE));

    data = (PtyData *) XtMalloc(sizeof(*data) + BUF_SIZE + FRG_SIZE);

    memset(data, 0, sizeof(*data));
    data->next = data->buffer;
    data->last = data->buffer;
    *result = data;
}

/*
 * Remove used data by shifting the buffer down, to make room for more data,
 * e.g., a continuation-read.
 */
void
trimPtyData(TScreen * screen GCC_UNUSED, PtyData * data)
{
    int i;

    FlushLog(screen);

    if (data->next != data->buffer) {
	int n = (data->last - data->next);

	TRACE(("shifting buffer down by %d\n", n));
	for (i = 0; i < n; ++i) {
	    data->buffer[i] = data->next[i];
	}
	data->next = data->buffer;
	data->last = data->next + n;
    }

}

/*
 * Insert new data into the input buffer so the next calls to morePtyData()
 * and nextPtyData() will return that.
 */
void
fillPtyData(TScreen * screen, PtyData * data, char *value, int length)
{
    int size;
    int n;

    /* remove the used portion of the buffer */
    trimPtyData(screen, data);

    VTbuffer->last += length;
    size = VTbuffer->last - VTbuffer->next;

    /* shift the unused portion up to make room */
    for (n = size; n >= length; --n)
	VTbuffer->next[n] = VTbuffer->next[n - length];

    /* insert the new bytes to interpret */
    for (n = 0; n < length; n++)
	VTbuffer->next[n] = CharOf(value[n]);
}

#if OPT_WIDE_CHARS
Char *
convertToUTF8(Char * lp, unsigned c)
{
    if (c < 0x80) {		/*  0*******  */
	*lp++ = (c);
    } else if (c < 0x800) {	/*  110***** 10******  */
	*lp++ = (0xc0 | (c >> 6));
	*lp++ = (0x80 | (c & 0x3f));
    } else {			/*  1110**** 10****** 10******  */
	*lp++ = (0xe0 | (c >> 12));
	*lp++ = (0x80 | ((c >> 6) & 0x3f));
	*lp++ = (0x80 | (c & 0x3f));
    }
    /*
     * UTF-8 is defined for words of up to 31 bits, but we need only 16
     * bits here, since that's all that X11R6 supports.
     */
    return lp;
}

/*
 * Write data back to the PTY
 */
void
writePtyData(int f, IChar * d, unsigned len)
{
    static Char *dbuf;
    static unsigned dlen;
    unsigned n = (len << 1);

    if (dlen <= len) {
	dlen = n;
	dbuf = (Char *) XtRealloc((char *) dbuf, dlen);
    }

    for (n = 0; n < len; n++)
	dbuf[n] = d[n];
    v_write(f, dbuf, n);
}
#endif
