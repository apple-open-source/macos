/*
 * Copyright (c) 2002 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/io.c,v 1.16tsi Exp $ */

#include "lisp/io.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Match the FILE_XXX flags */
#define READ_BIT	0x01
#define WRITE_BIT	0x02
#define APPEND_BIT	0x04
#define BUFFERED_BIT	0x08
#define UNBUFFERED_BIT	0x10
#define BINARY_BIT	0x20

/*
 * Prototypes
 */
static int calculate_line(void*, int);
static int calculate_column(void*, int, int);

/*
 * Initialization
 */
extern int pagesize;

/*
 * Implementation
 */
int
LispGet(void)
{
    int ch = EOF;
    LispUngetInfo *unget = lisp__data.unget[lisp__data.iunget];

    if (unget->offset)
	ch = ((unsigned char*)unget->buffer)[--unget->offset];
    else if (SINPUT->data.stream.readable) {
	LispFile *file = NULL;

	switch (SINPUT->data.stream.type) {
	    case LispStreamStandard:
	    case LispStreamFile:
		file = FSTREAMP(SINPUT);
		break;
	    case LispStreamPipe:
		file = IPSTREAMP(SINPUT);
		break;
	    case LispStreamString:
		ch = LispSgetc(SSTREAMP(SINPUT));
		break;
	    default:
		ch = EOF;
		break;
	}
	if (file != NULL) {
	    if (file->nonblock) {
		if (fcntl(file->descriptor, F_SETFL, 0) < 0)
		    LispDestroy("fcntl: %s", strerror(errno));
		file->nonblock = 0;
	    }
	    ch = LispFgetc(file);
	}
    }
    else
	LispDestroy("cannot read from *STANDARD-INPUT*");

    if (ch == EOF)
	lisp__data.eof = 1;

    return (ch);
}

int
LispUnget(int ch)
{
    LispUngetInfo *unget = lisp__data.unget[lisp__data.iunget];

    if (unget->offset == sizeof(unget->buffer)) {
	LispWarning("character %c lost at LispUnget()", unget->buffer[0]);
	memmove(unget->buffer, unget->buffer + 1, unget->offset - 1);
	unget->buffer[unget->offset - 1] = ch;
    }
    else
	unget->buffer[unget->offset++] = ch;

    return (ch);
}

void
LispPushInput(LispObj *stream)
{
    if (!STREAMP(stream) || !stream->data.stream.readable)
	LispDestroy("bad stream at PUSH-INPUT");
    lisp__data.input_list = CONS(stream, lisp__data.input_list);
    SINPUT = stream;
    if (lisp__data.iunget + 1 == lisp__data.nunget) {
	LispUngetInfo **info =
	    realloc(lisp__data.unget,
		    sizeof(LispUngetInfo) * (lisp__data.nunget + 1));

	if (!info ||
	    (info[lisp__data.nunget] =
	     calloc(1, sizeof(LispUngetInfo))) == NULL)
	    LispDestroy("out of memory");
	lisp__data.unget = info;
	++lisp__data.nunget;
    }
    ++lisp__data.iunget;
    memset(lisp__data.unget[lisp__data.iunget], '\0', sizeof(LispUngetInfo));
    lisp__data.eof = 0;
}

void
LispPopInput(LispObj *stream)
{
    if (!CONSP(lisp__data.input_list) || stream != CAR(lisp__data.input_list))
	LispDestroy("bad stream at POP-INPUT");
    lisp__data.input_list = CDR(lisp__data.input_list);
    SINPUT = CONSP(lisp__data.input_list) ?
    CAR(lisp__data.input_list) : lisp__data.input_list;
    --lisp__data.iunget;
    lisp__data.eof = 0;
}

/*
 * Low level functions
 */
static int
calculate_line(void *data, int size)
{
    int line = 0;
    char *str, *ptr;

    for (str = (char*)data, ptr = (char*)data + size; str < ptr; str++)
	if (*ptr == '\n')
	    ++line;

    return (line);
}

static int
calculate_column(void *data, int size, int column)
{
    char *str, *ptr;

    /* search for newline in data */
    for (str = (char*)data, ptr = (char*)data + size - 1; ptr >= str; ptr--)
	if (*ptr == '\n')
	    break;

    /* newline found */
    if (ptr >= str)
	return (size - (ptr - str) - 1);

    /* newline not found */
    return (column + size);
}

LispFile *
LispFdopen(int descriptor, int mode)
{
    LispFile *file = calloc(1, sizeof(LispFile));

    if (file) {
	struct stat st;

	file->descriptor = descriptor;
	file->readable = (mode & READ_BIT) != 0;
	file->writable = (mode & WRITE_BIT) != 0;

	if (fstat(descriptor, &st) == 0)
	    file->regular = S_ISREG(st.st_mode);
	else
	    file->regular = 0;

	file->buffered = (mode & BUFFERED_BIT) != 0;
	if ((mode & UNBUFFERED_BIT) == 0)
	    file->buffered = file->regular;

	if (file->buffered) {
	    file->buffer = malloc(pagesize);
	    if (file->buffer == NULL)
		file->buffered = 0;
	}
	file->line = 1;
	file->binary = (mode & BINARY_BIT) != 0;
	file->io_write = write;
    }

    return (file);
}

LispFile *
LispFopen(char *path, int mode)
{
    LispFile *file;
    int descriptor;
    int flags = O_NOCTTY;

    /* check read/write attributes */
    if ((mode & (READ_BIT | WRITE_BIT)) == (READ_BIT | WRITE_BIT))
	flags |= O_RDWR;
    else if (mode & READ_BIT)
	flags |= O_RDONLY;
    else if (mode & WRITE_BIT)
	flags |= O_WRONLY;

    /* create if does not exist */
    if (mode & WRITE_BIT) {
	flags |= O_CREAT;

	/* append if exists? */
	if (mode & APPEND_BIT)
	    flags |= O_APPEND;
	else
	    flags |= O_TRUNC;
    }

    /* open file */
    descriptor = open(path, flags, 0666);
    if (descriptor < 0)
	return (NULL);

    /* initialize LispFile structure */
    file = LispFdopen(descriptor, mode);
    if (file == NULL)
	close(descriptor);

    return (file);
}

void
LispFclose(LispFile *file)
{
    /* flush any pending output */
    LispFflush(file);
    /* cleanup */
    close(file->descriptor);
    if (file->buffer)
	free(file->buffer);
    free(file);
}

io_write_fn
LispSetFileWrite(LispFile *file, io_write_fn new_write)
{
    io_write_fn old_write = file->io_write;

    file->io_write = new_write;

    return (old_write);
}

int
LispFflush(LispFile *file)
{
    if (file->writable && file->length) {
	int length = (*file->io_write)(file->descriptor,
				       file->buffer, file->length);

	if (length > 0) {
	    if (file->length > length)
		memmove(file->buffer, file->buffer + length,
			file->length - length);
	    file->length -= length;
	}
	return (length);
    }

    return (0);
}

int
LispFungetc(LispFile *file, int ch)
{
    if (file->readable) {
	file->available = 1;
	file->unget = ch;
	/* this should never happen */
	if (ch == '\n' && !file->binary)
	    --file->line;
    }

    return (ch);
}

int
LispFgetc(LispFile *file)
{
    int ch;

    if (file->readable) {
	unsigned char c;

	if (file->available) {
	    ch = file->unget;
	    file->available = 0;
	}
	else if (file->buffered) {
	    if (file->writable) {
		LispFflush(file);
		if (read(file->descriptor, &c, 1) == 1)
		    ch = c;
		else
		    ch = EOF;
	    }
	    else {
		if (file->offset < file->length)
		    ch = ((unsigned char*)file->buffer)[file->offset++];
		else {
		    int length = read(file->descriptor,
				      file->buffer, pagesize);

		    if (length >= 0)
			file->length = length;
		    else
			file->length = 0;
		    file->offset = 0;
		    if (file->length)
			ch = ((unsigned char*)file->buffer)[file->offset++];
		    else
			ch = EOF;
		}
	    }
	}
	else if (read(file->descriptor, &c, 1) == 1)
	    ch = c;
	else
	    ch = EOF;
    }
    else
	ch = EOF;

    if (ch == '\n' && !file->binary)
	++file->line;

    return (ch);
}

int
LispFputc(LispFile *file, int ch)
{
    if (file->writable) {
	unsigned char c = ch;

	if (file->buffered) {
	    if (file->length + 1 >= pagesize)
		LispFflush(file);
	    file->buffer[file->length++] = c;
	}
	else if ((*file->io_write)(file->descriptor, &c, 1) != 1)
	    ch = EOF;

	if (!file->binary) {
	    /* update column number */
	    if (ch == '\n')
		file->column = 0;
	    else
		++file->column;
	}
    }

    return (ch);
}

int
LispSgetc(LispString *string)
{
    int ch;

    if (string->input >= string->length)
	return (EOF);			/* EOF reading from string */

    ch = ((unsigned char*)string->string)[string->input++];
    if (ch == '\n' && !string->binary)
	++string->line;

    return (ch);
}

int
LispSputc(LispString *string, int ch)
{
    if (string->output + 1 >= string->space) {
	if (string->fixed)
	    return (EOF);
	else {
	    char *tmp = realloc(string->string, string->space + pagesize);

	    if (tmp == NULL)
		return (EOF);
	    string->string = tmp;
	    string->space += pagesize;
	}
    }

    string->string[string->output++] = ch;
    if (string->length < string->output)
	string->length = string->output;

    /* update column number */
    if (!string->binary) {
	if (ch == '\n')
	    string->column = 0;
	else
	    ++string->column;
    }

    return (ch);
}

char *
LispFgets(LispFile *file, char *string, int size)
{
    int ch, offset = 0;

    if (size < 1)
	return (string);

    for (;;) {
	if (offset + 1 >= size)
	    break;
	if ((ch = LispFgetc(file)) == EOF)
	    break;
	string[offset++] = ch;
	/* line number is calculated in LispFgetc */
	if (ch == '\n')
	    break;
    }
    string[offset] = '\0';

    return (offset ? string : NULL);
}

int
LispFputs(LispFile *file, char *buffer)
{
    return (LispFwrite(file, buffer, strlen(buffer)));
}

int
LispSputs(LispString *string, char *buffer)
{
    return (LispSwrite(string, buffer, strlen(buffer)));
}

int
LispFread(LispFile *file, void *data, int size)
{
    int bytes, length;
    char *buffer;

    if (!file->readable)
	return (EOF);

    if (size <= 0)
	return (size);

    length = 0;
    buffer = (char*)data;

    /* check if there is an unget character */
    if (file->available) {
	*buffer++ = file->unget;
	file->available = 0;
	if (--size == 0) {
	    if (file->unget == '\n' && !file->binary)
		++file->line;

	    return (1);
	}

	length = 1;
    }

    if (file->buffered) {
	void *base_data = (char*)data - length;

	if (file->writable) {
	    LispFflush(file);
	    bytes = read(file->descriptor, buffer, size);
	    if (bytes < 0)
		bytes = 0;
	    if (!file->binary)
		file->line += calculate_line(base_data, length + bytes);

	    return (length + bytes);
	}

	/* read anything that is in the buffer */
	if (file->offset < file->length) {
	    bytes = file->length - file->offset;
	    if (bytes > size)
		bytes = size;
	    memcpy(buffer, file->buffer + file->offset, bytes);
	    buffer += bytes;
	    file->offset += bytes;
	    size -= bytes;
	}

	/* if there is still something to read */
	if (size) {
	    bytes = read(file->descriptor, buffer, size);
	    if (bytes < 0)
		bytes = 0;

	    length += bytes;
	}

	if (!file->binary)
	    file->line += calculate_line(base_data, length);

	return (length);
    }

    bytes = read(file->descriptor, buffer, size);
    if (bytes < 0)
	bytes = 0;
    if (!file->binary)
	file->line += calculate_line(buffer - length, length + bytes);

    return (length + bytes);
}

int
LispFwrite(LispFile *file, void *data, int size)
{
    if (!file->writable || size < 0)
	return (EOF);

    if (!file->binary)
	file->column = calculate_column(data, size, file->column);

    if (file->buffered) {
	int length, bytes;
	char *buffer = (char*)data;

	length = 0;
	if (size + file->length > pagesize) {
	    /* fill remaining space in buffer and flush */
	    bytes = pagesize - file->length;
	    memcpy(file->buffer + file->length, buffer, bytes);
	    file->length += bytes;
	    LispFflush(file);

	    /* check if all data was written */
	    if (file->length)
		return (pagesize - file->length);

	    length = bytes;
	    buffer += bytes;
	    size -= bytes;
	}

	while (size > pagesize) {
	    /* write multiple of pagesize */
	    bytes = (*file->io_write)(file->descriptor, buffer,
				      size - (size % pagesize));
	    if (bytes <= 0)
		return (length);

	    length += bytes;
	    buffer += bytes;
	    size -= bytes;
	}

	if (size) {
	    /* keep remaining data in buffer */
	    switch (size) {
		case 8:
		    file->buffer[file->length++] = *buffer++;
		case 7:
		    file->buffer[file->length++] = *buffer++;
		case 6:
		    file->buffer[file->length++] = *buffer++;
		case 5:
		    file->buffer[file->length++] = *buffer++;
		case 4:
		    file->buffer[file->length++] = *buffer++;
		case 3:
		    file->buffer[file->length++] = *buffer++;
		case 2:
		    file->buffer[file->length++] = *buffer++;
		case 1:
		    file->buffer[file->length++] = *buffer++;
		    break;
		default:
		    memcpy(file->buffer + file->length, buffer, size);
		    file->length += size;
		    break;
	    }
	    length += size;
	}

	return (length);
    }

    return ((*file->io_write)(file->descriptor, data, size));
}

int
LispSwrite(LispString *string, void *data, int size)
{
    int bytes;

    if (size < 0)
	return (EOF);

    if (string->output + size >= string->space) {
	if (string->fixed) {
	    /* leave space for a ending nul character */
	    bytes = string->space - string->output - 1;

	    if (bytes < size)
		size = bytes;

	    if (size <= 0)
		return (-1);
	}
	else {
	    char *tmp;

	    bytes = string->space + size;
	    bytes += pagesize - (bytes % pagesize);
	    tmp = realloc(string->string, bytes);

	    if (tmp == NULL)
		return (-1);

	    string->string = tmp;
	    string->space = bytes;
	}
    }
    memcpy(string->string + string->output, data, size);
    string->output += size;
    if (string->length < string->output)
	string->length = string->output;

    if (!string->binary)
	string->column = calculate_column(data, size, string->column);

    return (size);
}

char *
LispGetSstring(LispString *string, int *length)
{
    if (string->string == NULL || string->length <= 0) {
	*length = 0;

	return ("");
    }
    *length = string->length;
    if (string->string[string->length -1] != '\0') {
	if (string->length < string->space)
	    string->string[string->length] = '\0';
	else if (string->fixed && string->space)
	    string->string[string->space - 1] = '\0';
	else {
	    char *tmp = realloc(string->string, string->space + pagesize);

	    if (tmp == NULL)
		string->string[string->space - 1] = '\0';
	    else {
		string->string = tmp;
		string->space += pagesize;
		string->string[string->length] = '\0';
	    }
	}
    }

    return (string->string);
}

int
LispRename(char *from, char *to)
{
    return (rename(from, to));
}

int
LispUnlink(char *name)
{
    return (unlink(name));
}
