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

/* $XFree86: xc/programs/xedit/lisp/read.c,v 1.36tsi Exp $ */

#include <errno.h>
#include "lisp/read.h"
#include "lisp/package.h"
#include "lisp/write.h"
#include <fcntl.h>
#include <stdarg.h>

/* This should be visible only in read.c, but if an error is generated,
 * the current code in write.c will print it as #<ERROR> */
#define LABEL_BIT_COUNT		8
#define LABEL_BIT_MASK		0xff
#define MAX_LABEL_VALUE		((1L << (sizeof(long) * 8 - 9)) - 1)
#define READLABEL(label)						\
    (LispObj*)(((label) << LABEL_BIT_COUNT) | READLABEL_MASK)
#define READLABELP(object)						\
    (((unsigned long)(object) & LABEL_BIT_MASK) == READLABEL_MASK)
#define READLABEL_VALUE(object)						\
    ((long)(object) >> LABEL_BIT_COUNT)

#define READ_ENTER()							\
    LispObj *read__stream = SINPUT;					\
    int read__line = LispGetLine(read__stream)
#define READ_ERROR0(format)						\
    LispReadError(read__stream, read__line, format)
#define READ_ERROR1(format, arg1)					\
    LispReadError(read__stream, read__line, format, arg1)
#define READ_ERROR2(format, arg1, arg2)					\
    LispReadError(read__stream, read__line, format, arg1, arg2)

#define READ_ERROR_EOF()	READ_ERROR0("unexpected end of input")
#define READ_ERROR_FIXNUM()	READ_ERROR0("number is not a fixnum")
#define READ_ERROR_INVARG()	READ_ERROR0("invalid argument")

#ifdef __UNIXOS2__
# define finite(x) isfinite(x)
#endif

/*
 * Types
 */
typedef struct _object_info {
    long label;		/* the read label of this object */
    LispObj *object;	/* the resulting object */
    long num_circles;	/* references to object before it was completely read */
} object_info;

typedef struct _read_info {
    int level;		/* level of open parentheses */

    int nodot;		/* flag set when reading a "special" list */

    int discard;	/* flag used when reading an unavailable feature */

    long circle_count;	/* if non zero, must resolve some labels */

    /* information for #<number>= and #<number># */
    object_info *objects;
    long num_objects;

    /* could use only the objects field as all circular data is known,
     * but check every object so that circular/shared references generated
     * by evaluations would not cause an infinite loop at read time */
    LispObj **circles;
    long num_circles;
} read_info;

/*
 * Protypes
 */
static LispObj *LispReadChar(LispBuiltin*, int);

static int LispGetLine(LispObj*);
#ifdef __GNUC__
#define PRINTF_FORMAT	__attribute__ ((format (printf, 3, 4)))
#else
#define PRINTF_FORMAT	/**/
#endif
static void LispReadError(LispObj*, int, char*, ...);
#undef PRINTF_FORMAT
static void LispReadFixCircle(LispObj*, read_info*);
static LispObj *LispReadLabelCircle(LispObj*, read_info*);
static int LispReadCheckCircle(LispObj*, read_info*);
static LispObj *LispDoRead(read_info*);
static int LispSkipWhiteSpace(void);
static LispObj *LispReadList(read_info*);
static LispObj *LispReadQuote(read_info*);
static LispObj *LispReadBackquote(read_info*);
static LispObj *LispReadCommaquote(read_info*);
static LispObj *LispReadObject(int, read_info*);
static LispObj *LispParseAtom(char*, char*, int, int, LispObj*, int);
static LispObj *LispParseNumber(char*, int, LispObj*, int);
static int StringInRadix(char*, int, int);
static int AtomSeparator(int, int, int);
static LispObj *LispReadVector(read_info*);
static LispObj *LispReadMacro(read_info*);
static LispObj *LispReadFunction(read_info*);
static LispObj *LispReadRational(int, read_info*);
static LispObj *LispReadCharacter(read_info*);
static void LispSkipComment(void);
static LispObj *LispReadEval(read_info*);
static LispObj *LispReadComplex(read_info*);
static LispObj *LispReadPathname(read_info*);
static LispObj *LispReadStruct(read_info*);
static LispObj *LispReadMacroArg(read_info*);
static LispObj *LispReadArray(long, read_info*);
static LispObj *LispReadFeature(int, read_info*);
static LispObj *LispEvalFeature(LispObj*);

/*
 * Initialization
 */
static char *Char_Nul[] = {"Null", "Nul", NULL};
static char *Char_Soh[] = {"Soh", NULL};
static char *Char_Stx[] = {"Stx", NULL};
static char *Char_Etx[] = {"Etx", NULL};
static char *Char_Eot[] = {"Eot", NULL};
static char *Char_Enq[] = {"Enq", NULL};
static char *Char_Ack[] = {"Ack", NULL};
static char *Char_Bel[] = {"Bell", "Bel", NULL};
static char *Char_Bs[]  = {"Backspace", "Bs", NULL};
static char *Char_Tab[] = {"Tab", NULL};
static char *Char_Nl[]  = {"Newline", "Nl", "Lf", "Linefeed", NULL};
static char *Char_Vt[]  = {"Vt", NULL};
static char *Char_Np[]  = {"Page", "Np", NULL};
static char *Char_Cr[]  = {"Return", "Cr", NULL};
static char *Char_Ff[]  = {"So", "Ff", NULL};
static char *Char_Si[]  = {"Si", NULL};
static char *Char_Dle[] = {"Dle", NULL};
static char *Char_Dc1[] = {"Dc1", NULL};
static char *Char_Dc2[] = {"Dc2", NULL};
static char *Char_Dc3[] = {"Dc3", NULL};
static char *Char_Dc4[] = {"Dc4", NULL};
static char *Char_Nak[] = {"Nak", NULL};
static char *Char_Syn[] = {"Syn", NULL};
static char *Char_Etb[] = {"Etb", NULL};
static char *Char_Can[] = {"Can", NULL};
static char *Char_Em[]  = {"Em", NULL};
static char *Char_Sub[] = {"Sub", NULL};
static char *Char_Esc[] = {"Escape", "Esc", NULL};
static char *Char_Fs[]  = {"Fs", NULL};
static char *Char_Gs[]  = {"Gs", NULL};
static char *Char_Rs[]  = {"Rs", NULL};
static char *Char_Us[]  = {"Us", NULL};
static char *Char_Sp[]  = {"Space", "Sp", NULL};
static char *Char_Del[] = {"Rubout", "Del", "Delete", NULL};

LispCharInfo LispChars[256] = {
    {Char_Nul},
    {Char_Soh},
    {Char_Stx},
    {Char_Etx},
    {Char_Eot},
    {Char_Enq},
    {Char_Ack},
    {Char_Bel},
    {Char_Bs},
    {Char_Tab},
    {Char_Nl},
    {Char_Vt},
    {Char_Np},
    {Char_Cr},
    {Char_Ff},
    {Char_Si},
    {Char_Dle},
    {Char_Dc1},
    {Char_Dc2},
    {Char_Dc3},
    {Char_Dc4},
    {Char_Nak},
    {Char_Syn},
    {Char_Etb},
    {Char_Can},
    {Char_Em},
    {Char_Sub},
    {Char_Esc},
    {Char_Fs},
    {Char_Gs},
    {Char_Rs},
    {Char_Us},
    {Char_Sp},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {Char_Del},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
    {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}
    
};

Atom_id Sand, Sor, Snot;


/*
 * Implementation
 */
LispObj *
Lisp_Read(LispBuiltin *builtin)
/*
 read &optional input-stream eof-error-p eof-value recursive-p
 */
{
    LispObj *result;

    LispObj *input_stream, *eof_error_p, *eof_value;

    eof_value = ARGUMENT(2);
    eof_error_p = ARGUMENT(1);
    input_stream = ARGUMENT(0);

    if (input_stream == UNSPEC)
	input_stream = NIL;
    else if (input_stream != NIL) {
	CHECK_STREAM(input_stream);
	else if (!input_stream->data.stream.readable)
	    LispDestroy("%s: stream %s is not readable",
			STRFUN(builtin), STROBJ(input_stream));
	LispPushInput(input_stream);
    }
    else if (CONSP(lisp__data.input_list)) {
	input_stream = STANDARD_INPUT;
	LispPushInput(input_stream);
    }

    if (eof_value == UNSPEC)
	eof_value = NIL;

    result = LispRead();
    if (input_stream != NIL)
	LispPopInput(input_stream);

    if (result == NULL) {
	if (eof_error_p != NIL)
	    LispDestroy("%s: EOF reading stream %s",
			STRFUN(builtin), STROBJ(input_stream));
	else
	    result = eof_value;
    }

    return (result);
}

static LispObj *
LispReadChar(LispBuiltin *builtin, int nohang)
{
    int character;

    LispObj *input_stream, *eof_error_p, *eof_value;

    eof_value = ARGUMENT(2);
    eof_error_p = ARGUMENT(1);
    input_stream = ARGUMENT(0);

    if (input_stream == UNSPEC)
	input_stream = NIL;
    else if (input_stream != NIL) {
	CHECK_STREAM(input_stream);
    }
    else
	input_stream = lisp__data.input;

    if (eof_value == UNSPEC)
	eof_value = NIL;

    character = EOF;

    if (input_stream->data.stream.readable) {
	LispFile *file = NULL;

	switch (input_stream->data.stream.type) {
	    case LispStreamStandard:
	    case LispStreamFile:
		file = FSTREAMP(input_stream);
		break;
	    case LispStreamPipe:
		file = IPSTREAMP(input_stream);
		break;
	    case LispStreamString:
		character = LispSgetc(SSTREAMP(input_stream));
		break;
	    default:
		break;
	}
	if (file != NULL) {
	    if (file->available || file->offset < file->length)
		character = LispFgetc(file);
	    else {
		if (nohang && !file->nonblock) {
		    if (fcntl(file->descriptor, F_SETFL, O_NONBLOCK) < 0)
			LispDestroy("%s: fcntl(%d): %s",
				    STRFUN(builtin), file->descriptor,
				    strerror(errno));
		    file->nonblock = 1;
		}
		else if (!nohang && file->nonblock) {
		    if (fcntl(file->descriptor, F_SETFL, 0) < 0)
			LispDestroy("%s: fcntl(%d): %s",
				    STRFUN(builtin), file->descriptor,
				    strerror(errno));
		    file->nonblock = 0;
		}
		if (nohang) {
		    unsigned char ch;

		    if (read(file->descriptor, &ch, 1) == 1)
			character = ch;
		    else if (errno == EAGAIN)
			return (NIL);	/* XXX no character available */
		    else
			character = EOF;
		}
		else
		    character = LispFgetc(file);
	    }
	}
    }
    else
	LispDestroy("%s: stream %s is unreadable",
		    STRFUN(builtin), STROBJ(input_stream));

    if (character == EOF) {
	if (eof_error_p != NIL)
	    LispDestroy("%s: EOF reading stream %s",
			STRFUN(builtin), STROBJ(input_stream));

	return (eof_value);
    }

    return (SCHAR(character));
}

LispObj *
Lisp_ReadChar(LispBuiltin *builtin)
/*
 read-char &optional input-stream eof-error-p eof-value recursive-p
 */
{
    return (LispReadChar(builtin, 0));
}

LispObj *
Lisp_ReadCharNoHang(LispBuiltin *builtin)
/*
 read-char-no-hang &optional input-stream eof-error-p eof-value recursive-p
 */
{
    return (LispReadChar(builtin, 1));
}

LispObj *
Lisp_ReadLine(LispBuiltin *builtin)
/*
 read-line &optional input-stream eof-error-p eof-value recursive-p
 */
{
    char *string;
    int ch, length;
    LispObj *result, *status = NIL;

    LispObj *input_stream, *eof_error_p, *eof_value;

    eof_value = ARGUMENT(2);
    eof_error_p = ARGUMENT(1);
    input_stream = ARGUMENT(0);

    if (input_stream == UNSPEC)
	input_stream = NIL;
    else if (input_stream == NIL)
	input_stream = STANDARD_INPUT;
    else {
	CHECK_STREAM(input_stream);
    }

    if (eof_value == UNSPEC)
	eof_value = NIL;

    result = NIL;
    string = NULL;
    length = 0;

    if (!input_stream->data.stream.readable)
	LispDestroy("%s: stream %s is unreadable",
		    STRFUN(builtin), STROBJ(input_stream));
    if (input_stream->data.stream.type == LispStreamString) {
	char *start, *end, *ptr;

	if (SSTREAMP(input_stream)->input >=
	    SSTREAMP(input_stream)->length) {
	    if (eof_error_p != NIL)
		LispDestroy("%s: EOS found reading %s",
			    STRFUN(builtin), STROBJ(input_stream));

	    status = T;
	    result = eof_value;
	    goto read_line_done;
	}

	start = SSTREAMP(input_stream)->string +
		SSTREAMP(input_stream)->input;
	end = SSTREAMP(input_stream)->string +
	      SSTREAMP(input_stream)->length;
	/* Search for a newline */
	for (ptr = start; *ptr != '\n' && ptr < end; ptr++)
	    ;
	if (ptr == end)
	    status = T;
	else if (!SSTREAMP(input_stream)->binary)
	    ++SSTREAMP(input_stream)->line;
	length = ptr - start;
	string = LispMalloc(length + 1);
	memcpy(string, start, length);
	string[length] = '\0';
	result = LSTRING2(string, length);
	/* macro LSTRING2 does not make a copy of it's arguments, and
	 * calls LispMused on it. */
	SSTREAMP(input_stream)->input += length + (status == NIL);
    }
    else /*if (input_stream->data.stream.type == LispStreamFile ||
	     input_stream->data.stream.type == LispStreamStandard ||
	     input_stream->data.stream.type == LispStreamPipe)*/ {
	LispFile *file;

	if (input_stream->data.stream.type == LispStreamPipe)
	    file = IPSTREAMP(input_stream);
	else
	    file = FSTREAMP(input_stream);

	if (file->nonblock) {
	    if (fcntl(file->descriptor, F_SETFL, 0) < 0)
		LispDestroy("%s: fcntl: %s",
			    STRFUN(builtin), strerror(errno));
	    file->nonblock = 0;
	}

	while (1) {
	    ch = LispFgetc(file);
	    if (ch == EOF) {
		if (length)
		    break;
		if (eof_error_p != NIL)
		    LispDestroy("%s: EOF found reading %s",
				STRFUN(builtin), STROBJ(input_stream));
		if (string)
		    LispFree(string);

		status = T;
		result = eof_value;
		goto read_line_done;
	    }
	    else if (ch == '\n')
		break;
	    else if ((length % 64) == 0)
		string = LispRealloc(string, length + 64);
	    string[length++] = ch;
	}
	if (string) {
	    if ((length % 64) == 0)
		string = LispRealloc(string, length + 1);
	    string[length] = '\0';
	    result = LSTRING2(string, length);
	}
	else
	    result = STRING("");
    }

read_line_done:
    RETURN(0) = status;
    RETURN_COUNT = 1;

    return (result);
}

LispObj *
LispRead(void)
{
    READ_ENTER();
    read_info info;
    LispObj *result, *code = COD;

    info.level = info.nodot = info.discard = 0;
    info.circle_count = 0;
    info.objects = NULL;
    info.num_objects = 0;

    result = LispDoRead(&info);

    /* fix circular/shared lists, note that this is done when returning to
     * the toplevel, so, if some circular/shared reference was evaluated,
     * it should have generated an expected error */
    if (info.num_objects) {
	if (info.circle_count) {
	    info.circles = NULL;
	    info.num_circles = 0;
	    LispReadFixCircle(result, &info);
	    if (info.num_circles)
		LispFree(info.circles);
	}
	LispFree(info.objects);
    }

    if (result == EOLIST)
	READ_ERROR0("object cannot start with #\\)");
    else if (result == DOT)
	READ_ERROR0("dot allowed only on lists");

    if (result != NULL && POINTERP(result)) {
	if (code == NIL)
	    COD = result;
	else
	    COD = CONS(COD, result);
    }

    return (result);
}

static int
LispGetLine(LispObj *stream)
{
    int line = -1;

    if (STREAMP(stream)) {
	switch (stream->data.stream.type) {
	    case LispStreamStandard:
	    case LispStreamFile:
		if (!FSTREAMP(stream)->binary)
		    line = FSTREAMP(stream)->line;
		break;
	    case LispStreamPipe:
		if (!IPSTREAMP(stream)->binary)
		    line = IPSTREAMP(stream)->line;
		break;
	    case LispStreamString:
		if (!SSTREAMP(stream)->binary)
		    line = SSTREAMP(stream)->line;
		break;
	    default:
		break;
	}
    }
    else if (stream == NIL && !Stdin->binary)
	line = Stdin->line;

    return (line);
}

static void
LispReadError(LispObj *stream, int line, char *fmt, ...)
{
    char string[128], *buffer_string;
    LispObj *buffer = LSTRINGSTREAM("", STREAM_READ | STREAM_WRITE, 0);
    int length;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(string, sizeof(string), fmt, ap);
    va_end(ap);

    LispFwrite(Stderr, "*** Reading ", 12);
    LispWriteObject(buffer, stream);
    buffer_string = LispGetSstring(SSTREAMP(buffer), &length);
    LispFwrite(Stderr, buffer_string, length);
    LispFwrite(Stderr, " at line ", 9);
    if (line < 0)
	LispFwrite(Stderr, "?\n", 2);
    else {
	char str[32];

	sprintf(str, "%d\n", line);
	LispFputs(Stderr, str);
    }

    LispDestroy("READ: %s", string);
}

static void
LispReadFixCircle(LispObj *object, read_info *info)
{
    LispObj *cons;

fix_again:
    switch (OBJECT_TYPE(object)) {
	case LispCons_t:
	    for (cons = object;
		 CONSP(object);
		 cons = object, object = CDR(object)) {
		if (READLABELP(CAR(object)))
		    CAR(object) = LispReadLabelCircle(CAR(object), info);
		else if (LispReadCheckCircle(object, info))
		    return;
		else
		    LispReadFixCircle(CAR(object), info);
	    }
	    if (READLABELP(object))
		CDR(cons) = LispReadLabelCircle(object, info);
	    else
		goto fix_again;
	    break;
	case LispArray_t:
	    if (READLABELP(object->data.array.list))
		object->data.array.list =
		    LispReadLabelCircle(object->data.array.list, info);
	    else if (!LispReadCheckCircle(object, info)) {
		object = object->data.array.list;
		goto fix_again;
	    }
	    break;
	case LispStruct_t:
	    if (READLABELP(object->data.struc.fields))
		object->data.struc.fields =
		    LispReadLabelCircle(object->data.struc.fields, info);
	    else if (!LispReadCheckCircle(object, info)) {
		object = object->data.struc.fields;
		goto fix_again;
	    }
	    break;
	case LispQuote_t:
	case LispBackquote_t:
	case LispFunctionQuote_t:
	    if (READLABELP(object->data.quote))
		object->data.quote =
		    LispReadLabelCircle(object->data.quote, info);
	    else {
		object = object->data.quote;
		goto fix_again;
	    }
	    break;
	case LispComma_t:
	    if (READLABELP(object->data.comma.eval))
		object->data.comma.eval =
		    LispReadLabelCircle(object->data.comma.eval, info);
	    else {
		object = object->data.comma.eval;
		goto fix_again;
	    }
	    break;
	case LispLambda_t:
	    if (READLABELP(object->data.lambda.code))
		object->data.lambda.code =
		    LispReadLabelCircle(object->data.lambda.code, info);
	    else if (!LispReadCheckCircle(object, info)) {
		object = object->data.lambda.code;
		goto fix_again;
	    }
	    break;
	default:
	    break;
    }
}

static LispObj *
LispReadLabelCircle(LispObj *label, read_info *info)
{
    long i, value = READLABEL_VALUE(label);

    for (i = 0; i < info->num_objects; i++)
	if (info->objects[i].label == value)
	    return (info->objects[i].object);

    LispDestroy("READ: internal error");
    /*NOTREACHED*/
    return (label);
}

static int
LispReadCheckCircle(LispObj *object, read_info *info)
{
    long i;

    for (i = 0; i < info->num_circles; i++)
	if (info->circles[i] == object)
	    return (1);

    if ((info->num_circles % 16) == 0)
	info->circles = LispRealloc(info->circles, sizeof(LispObj*) *
				    (info->num_circles + 16));
    info->circles[info->num_circles++] = object;

    return (0);
}

static LispObj *
LispDoRead(read_info *info)
{
    LispObj *object;
    int ch = LispSkipWhiteSpace();

    switch (ch) {
	case '(':
	    object = LispReadList(info);
	    break;
	case ')':
	    for (ch = LispGet(); ch != EOF && ch != '\n'; ch = LispGet()) {
		if (!isspace(ch)) {
		    LispUnget(ch);
		    break;
		}
	    }
	    return (EOLIST);
	case EOF:
	    return (NULL);
	case '\'':
	    object = LispReadQuote(info);
	    break;
	case '`':
	    object = LispReadBackquote(info);
	    break;
	case ',':
	    object = LispReadCommaquote(info);
	    break;
	case '#':
	    object = LispReadMacro(info);
	    break;
	default:
	    LispUnget(ch);
	    object = LispReadObject(0, info);
	    break;
    }

    return (object);
}

static LispObj *
LispReadMacro(read_info *info)
{
    READ_ENTER();
    LispObj *result = NULL;
    int ch = LispGet();

    switch (ch) {
	case '(':
	    result = LispReadVector(info);
	    break;
	case '\'':
	   result = LispReadFunction(info);
	   break;
	case 'b':
	case 'B':
	    result = LispReadRational(2, info);
	    break;
	case 'o':
	case 'O':
	    result = LispReadRational(8, info);
	    break;
	case 'x':
	case 'X':
	    result = LispReadRational(16, info);
	    break;
	case '\\':
	    result = LispReadCharacter(info);
	    break;
	case '|':
	    LispSkipComment();
	    result = LispDoRead(info);
	    break;
	case '.':	/* eval when compiling */
	case ',':	/* eval when loading */
	    result = LispReadEval(info);
	    break;
	case 'c':
	case 'C':
	    result = LispReadComplex(info);
	    break;
	case 'p':
	case 'P':
	    result = LispReadPathname(info);
	    break;
	case 's':
	case 'S':
	    result = LispReadStruct(info);
	    break;
	case '+':
	    result = LispReadFeature(1, info);
	    break;
	case '-':
	    result = LispReadFeature(0, info);
	    break;
	case ':':
	    /* Uninterned symbol */
	    result = LispReadObject(1, info);
	    break;
	default:
	    if (isdigit(ch)) {
		LispUnget(ch);
		result = LispReadMacroArg(info);
	    }
	    else if (!info->discard)
		READ_ERROR1("undefined dispatch macro character #%c", ch);
	    break;
    }

    return (result);
}

static LispObj *
LispReadMacroArg(read_info *info)
{
    READ_ENTER();
    LispObj *result = NIL;
    long i, integer;
    int ch;

    /* skip leading zeros */
    while (ch = LispGet(), ch != EOF && isdigit(ch) && ch == '0')
	;

    if (ch == EOF)
	READ_ERROR_EOF();

    /* if ch is not a number the argument was zero */
    if (isdigit(ch)) {
	char stk[32], *str;
	int len = 1;

	stk[0] = ch;
	for (;;) {
	    ch = LispGet();
	    if (!isdigit(ch))
		break;
	    if (len + 1 >= sizeof(stk))
		READ_ERROR_FIXNUM();
	    stk[len++] = ch;
	}
	stk[len] = '\0';
	errno = 0;
	integer = strtol(stk, &str, 10);
	/* number is positive because sign is not processed here */
	if (*str || errno == ERANGE || integer > MOST_POSITIVE_FIXNUM)
	    READ_ERROR_FIXNUM();
    }
    else
	integer = 0;

    switch (ch) {
	case 'a':
	case 'A':
	    if (integer == 1) {
		/* LispReadArray and LispReadList expect
		 * the '(' being already read  */
		if ((ch = LispSkipWhiteSpace()) != '(') {
		    if (info->discard)
			return (ch == EOF ? NULL : NIL);
		    READ_ERROR0("bad array specification");
		}
		result = LispReadVector(info);
	    }
	    else
		result = LispReadArray(integer, info);
	    break;
	case 'r':
	case 'R':
	    result = LispReadRational(integer, info);
	    break;
	case '=':
	    if (integer > MAX_LABEL_VALUE)
		READ_ERROR_FIXNUM();
	    if (!info->discard) {
		long num_objects = info->num_objects;

		/* check for duplicated label */
		for (i = 0; i < info->num_objects; i++) {
		    if (info->objects[i].label == integer)
			READ_ERROR1("label #%ld# defined more than once",
				    integer);
		}
		info->objects = LispRealloc(info->objects,
					    sizeof(object_info) *
					    (num_objects + 1));
		/* if this label is referenced it is a shared/circular object */
		info->objects[num_objects].label = integer;
		info->objects[num_objects].object = NULL;
		info->objects[num_objects].num_circles = 0;
		++info->num_objects;
		result = LispDoRead(info);
		if (READLABELP(result) && READLABEL_VALUE(result) == integer)
		    READ_ERROR2("incorrect syntax #%ld= #%ld#",
				integer, integer);
		/* any reference to it now is not shared/circular */
		info->objects[num_objects].object = result;
	    }
	    else
		result = LispDoRead(info);
	    break;
	case '#':
	    if (integer > MAX_LABEL_VALUE)
		READ_ERROR_FIXNUM();
	    if (!info->discard) {
		/* search object */
		for (i = 0; i < info->num_objects; i++) {
		    if (info->objects[i].label == integer) {
			result = info->objects[i].object;
			if (result == NULL) {
			    ++info->objects[i].num_circles;
			    ++info->circle_count;
			    result = READLABEL(integer);
			}
			break;
		    }
		}
		if (i == info->num_objects)
		    READ_ERROR1("undefined label #%ld#", integer);
	    }
	    break;
	default:
	    if (!info->discard)
		READ_ERROR1("undefined dispatch macro character #%c", ch);
	    break;
    }

    return (result);
}

static int
LispSkipWhiteSpace(void)
{
    int ch;

    for (;;) {
	while (ch = LispGet(), isspace(ch) && ch != EOF)
	    ;
	if (ch == ';') {
	    while (ch = LispGet(), ch != '\n' && ch != EOF)
		;
	    if (ch == EOF)
		return (EOF);
	}
	else
	    break;
    }

    return (ch);
}

/* any data in the format '(' FORM ')' is read here */
static LispObj *
LispReadList(read_info *info)
{
    READ_ENTER();
    GC_ENTER();
    LispObj *result, *cons, *object;
    int dot = 0;

    ++info->level;
    /* check for () */
    object = LispDoRead(info);
    if (object == EOLIST) {
	--info->level;

	return (NIL);
    }

    if (object == DOT)
	READ_ERROR0("illegal start of dotted list");

    result = cons = CONS(object, NIL);

    /* make sure GC will not release data being read */
    GC_PROTECT(result);

    while ((object = LispDoRead(info)) != EOLIST) {
	if (object == NULL)
	    READ_ERROR_EOF();
	if (object == DOT) {
	    if (info->nodot == info->level)
		READ_ERROR0("dotted list not allowed");
	    /* this is a dotted list */
	    if (dot)
		READ_ERROR0("more than one . in list");
	    dot = 1;
	}
	else {
	    if (dot) {
		/* only one object after a dot */
		if (++dot > 2)
		    READ_ERROR0("more than one object after . in list");
		RPLACD(cons, object);
	    }
	    else {
		RPLACD(cons, CONS(object, NIL));
		cons = CDR(cons);
	    }
	}
    }

    /* this will happen if last list element was a dot */
    if (dot == 1)
	READ_ERROR0("illegal end of dotted list");

    --info->level;
    GC_LEAVE();

    return (result);
}

static LispObj *
LispReadQuote(read_info *info)
{
    READ_ENTER();
    LispObj *quote = LispDoRead(info), *result;

    if (INVALIDP(quote))
	READ_ERROR_INVARG();

    result = QUOTE(quote);

    return (result);
}

static LispObj *
LispReadBackquote(read_info *info)
{
    READ_ENTER();
    LispObj *backquote = LispDoRead(info), *result;

    if (INVALIDP(backquote))
	READ_ERROR_INVARG();

    result = BACKQUOTE(backquote);

    return (result);
}

static LispObj *
LispReadCommaquote(read_info *info)
{
    READ_ENTER();
    LispObj *comma, *result;
    int atlist = LispGet();

    if (atlist == EOF)
	READ_ERROR_EOF();
    else if (atlist != '@' && atlist != '.')
	LispUnget(atlist);

    comma = LispDoRead(info);
    if (comma == DOT) {
	atlist = '@';
	comma = LispDoRead(info);
    }
    if (INVALIDP(comma))
	READ_ERROR_INVARG();

    result = COMMA(comma, atlist == '@' || atlist == '.');

    return (result);
}

/*
 * Read anything that is not readily identifiable by it's first character
 * and also put the code for reading atoms, numbers and strings together.
 */
static LispObj *
LispReadObject(int unintern, read_info *info)
{
    READ_ENTER();
    LispObj *object;
    char stk[128], *string, *package, *symbol;
    int ch, length, backslash, size, quote, unreadable, collon;

    package = symbol = string = stk;
    size = sizeof(stk);
    backslash = quote = unreadable = collon = 0;
    length = 0;

    ch = LispGet();
    if (unintern && (ch == ':' || ch == '"'))
	READ_ERROR0("syntax error after #:");
    else if (ch == '"' || ch == '|')
	quote = ch;
    else if (ch == '\\') {
	unreadable = backslash = 1;
	string[length++] = ch;
    }
    else if (ch == ':') {
	collon = 1;
	string[length++] = ch;
	symbol = string + 1;
	ch = LispGet();
	if (ch == '|') {
	    quote = ch;
	    unreadable = 1;
	}
	else if (ch != EOF)
	    LispUnget(ch);
    }
    else if (ch) {
	if (islower(ch))
	    ch = toupper(ch);
	string[length++] = ch;
    }
    else
	unreadable = 1;

    /* read remaining data */
    for (; ch;) {
	ch = LispGet();

	if (ch == EOF) {
	    if (quote) {
		/* if quote, file ended with an open quoted object */
		if (string != stk)
		    LispFree(string);
		return (NULL);
	    }
	    break;
	}
	else if (ch == '\0')
	    break;

	if (ch == '\\') {
	    backslash = !backslash;
	    if (quote == '"') {
		/* only remove backslashs from strings */
		if (backslash)
		    continue;
	    }
	    else
		unreadable = 1;
	}
	else if (backslash)
	    backslash = 0;
	else if (ch == quote)
	    break;
	else if (!quote && !backslash) {
	    if (islower(ch))
		ch = toupper(ch);
	    else if (isspace(ch))
		break;
	    else if (AtomSeparator(ch, 0, 0)) {
		LispUnget(ch);
		break;
	    }
	    else if (ch == ':') {
		if (collon == 0 ||
		    (collon == (1 - unintern) && symbol == string + length)) {
		    ++collon;
		    symbol = string + length + 1;
		}
		else
		    READ_ERROR0("too many collons");
	    }
	}

	if (length + 2 >= size) {
	    if (string == stk) {
		size = 1024;
		string = LispMalloc(size);
		strcpy(string, stk);
	    }
	    else {
		size += 1024;
		string = LispRealloc(string, size);
	    }
	    symbol = string + (symbol - package);
	    package = string;
	}
	string[length++] = ch;
    }

    if (info->discard) {
	if (string != stk)
	    LispFree(string);

	return (ch == EOF ? NULL : NIL);
    }

    string[length] = '\0';

    if (unintern) {
	if (length == 0)
	    READ_ERROR0("syntax error after #:");
	object = UNINTERNED_ATOM(string);
    }

    else if (quote == '"')
	object = LSTRING(string, length);

    else if (collon) {
	/* Package specified in object name */
	symbol[-1] = '\0';
	if (collon > 1)
	    symbol[-2] = '\0';
	object = LispParseAtom(package, symbol,
			       collon == 2, unreadable,
			       read__stream, read__line);
    }

    else if (quote == '|' || (unreadable && !collon)) {
	/* Set unreadable field, this atom needs quoting to be read back */
	object = ATOM(string);
	object->data.atom->unreadable = 1;
    }

    /* Check some common symbols */
    else if (length == 1 && string[0] == 'T')
	/* The T */
	object = T;

    else if (length == 1 && string[0] == '.')
	/* The dot */
	object = DOT;

    else if (length == 3 &&
	     string[0] == 'N' && string[1] == 'I' && string[2] == 'L')
	/* The NIL */
	object = NIL;

    else if (isdigit(string[0]) || string[0] == '.' ||
	     ((string[0] == '-' || string[0] == '+') && string[1]))
	/* Looks like a number */
	object = LispParseNumber(string, 10, read__stream, read__line);

    else
	/* A normal atom */
	object = ATOM(string);

    if (string != stk)
	LispFree(string);

    return (object);
}

static LispObj *
LispParseAtom(char *package, char *symbol, int intern, int unreadable,
	      LispObj *read__stream, int read__line)
{
    LispObj *object = NULL, *thepackage = NULL;
    LispPackage *pack = NULL;

    if (!unreadable) {
	/* Until NIL and T be treated as normal symbols */
	if (symbol[0] == 'N' && symbol[1] == 'I' &&
	    symbol[2] == 'L' && symbol[3] == '\0')
	    return (NIL);
	if (symbol[0] == 'T' && symbol[1] == '\0')
	    return (T);
	unreadable = !LispCheckAtomString(symbol);
    }

    /* If package is empty, it is a keyword */
    if (package[0] == '\0') {
	thepackage = lisp__data.keyword;
	pack = lisp__data.key;
    }

    else {
	/* Else, search it in the package list */
	thepackage = LispFindPackageFromString(package);

	if (thepackage == NIL)
	    READ_ERROR1("the package %s is not available", package);

	pack = thepackage->data.package.package;
    }

    if (pack == lisp__data.pack && intern) {
	/* Redundant package specification, since requesting a
	 * intern symbol, create it if does not exist */

	object = ATOM(symbol);
	if (unreadable)
	    object->data.atom->unreadable = 1;
    }

    else if (intern || pack == lisp__data.key) {
	/* Symbol is created, or just fetched from the specified package */

	LispPackage *savepack;
	LispObj *savepackage = PACKAGE;

	/* Remember curent package */
	savepack = lisp__data.pack;

	/* Temporarily set another package */
	lisp__data.pack = pack;
	PACKAGE = thepackage;

	/* Get the object pointer */
	if (pack == lisp__data.key)
	    object = KEYWORD(LispDoGetAtom(symbol, 0)->key->value);
	else
	    object = ATOM(symbol);
	if (unreadable)
	    object->data.atom->unreadable = 1;

	/* Restore current package */
	lisp__data.pack = savepack;
	PACKAGE = savepackage;
    }

    else {
	/* Symbol must exist (and be extern) in the specified package */

	LispAtom *atom;

	atom = (LispAtom *)hash_check(pack->atoms, symbol, strlen(symbol));
	if (atom)
	    object = atom->object;

	/* No object found */
	if (object == NULL || object->data.atom->ext == 0)
	    READ_ERROR2("no extern symbol %s in package %s", symbol, package);
    }

    return (object);
}

static LispObj *
LispParseNumber(char *str, int radix, LispObj *read__stream, int read__line)
{
    int len;
    long integer;
    double dfloat;
    char *ratio, *ptr;
    LispObj *number;
    mpi *bignum;
    mpr *bigratio;

    if (radix < 2 || radix > 36)
	READ_ERROR1("radix %d is not in the range 2 to 36", radix);

    if (*str == '\0')
	return (NULL);

    ratio = strchr(str, '/');
    if (ratio) {
	/* check if looks like a correctly specified ratio */
	if (ratio[1] == '\0' || strchr(ratio + 1, '/') != NULL)
	    return (ATOM(str));

	/* ratio must point to an integer in radix base */
	*ratio++ = '\0';
    }
    else if (radix == 10) {
	int dot = 0;
	int type = 0;

	/* check if it is a floating point number */
	ptr = str;
	if (*ptr == '-' || *ptr == '+')
	    ++ptr;
	else if (*ptr == '.') {
	    dot = 1;
	    ++ptr;
	}
	while (*ptr) {
	    if (*ptr == '.') {
		if (dot)
		    return (ATOM(str));
		/* ignore it if last char is a dot */
		if (ptr[1] == '\0') {
		    *ptr = '\0';
		    break;
		}
		dot = 1;
	    }
	    else if (!isdigit(*ptr))
		break;
	    ++ptr;
	}

	switch (*ptr) {
	    case '\0':
		if (dot)		/* if dot, it is default float */
		    type = 'E';
		break;
	    case 'E': case 'S': case 'F': case 'D': case 'L':
		type = *ptr;
		*ptr = 'E';
		break;
	    default:
		return (ATOM(str));	/* syntax error */
	}

	/* if type set, it is not an integer specification */
	if (type) {
	    if (*ptr) {
		int itype = *ptr;
		char *ptype = ptr;

		++ptr;
		if (*ptr == '+' || *ptr == '-')
		    ++ptr;
		while (*ptr && isdigit(*ptr))
		    ++ptr;
		if (*ptr) {
		    *ptype = itype;

		    return (ATOM(str));
		}
	    }

	    dfloat = strtod(str, NULL);
	    if (!finite(dfloat))
		READ_ERROR0("floating point overflow");

	    return (DFLOAT(dfloat));
	}
    }

    /* check if correctly specified in the given radix */
    len = strlen(str) - 1;
    if (!ratio && radix != 10 && str[len] == '.')
	str[len] = '\0';

    if (ratio || radix != 10) {
	if (!StringInRadix(str, radix, 1)) {
	    if (ratio)
		ratio[-1] = '/';
	    return (ATOM(str));
	}
	if (ratio && !StringInRadix(ratio, radix, 0)) {
	    ratio[-1] = '/';
	    return (ATOM(str));
	}
    }

    bignum = NULL;
    bigratio = NULL;

    errno = 0;
    integer = strtol(str, NULL, radix);

    /* if does not fit in a long */
    if (errno == ERANGE &&
	((*str == '-' && integer == LONG_MIN) ||
	 (*str != '-' && integer == LONG_MAX))) {
	bignum = LispMalloc(sizeof(mpi));
	mpi_init(bignum);
	mpi_setstr(bignum, str, radix);
    }


    if (ratio && integer != 0) {
	long denominator;

	errno = 0;
	denominator = strtol(ratio, NULL, radix);
	if (denominator == 0)
	    READ_ERROR0("divide by zero");

	if (bignum == NULL) {
	    if (integer == MINSLONG ||
		(denominator == LONG_MAX && errno == ERANGE)) {
		bigratio = LispMalloc(sizeof(mpr));
		mpr_init(bigratio);
		mpi_seti(mpr_num(bigratio), integer);
		mpi_setstr(mpr_den(bigratio), ratio, radix);
	    }
	}
	else {
	    bigratio = LispMalloc(sizeof(mpr));
	    mpr_init(bigratio);
	    mpi_set(mpr_num(bigratio), bignum);
	    mpi_clear(bignum);
	    LispFree(bignum);
	    mpi_setstr(mpr_den(bigratio), ratio, radix);
	}

	if (bigratio) {
	    mpr_canonicalize(bigratio);
	    if (mpi_fiti(mpr_num(bigratio)) &&
		mpi_fiti(mpr_den(bigratio))) {
		integer = mpi_geti(mpr_num(bigratio));
		denominator = mpi_geti(mpr_den(bigratio));
		mpr_clear(bigratio);
		LispFree(bigratio);
		if (denominator == 1)
		    number = INTEGER(integer);
		else
		    number = RATIO(integer, denominator);
	    }
	    else
		number = BIGRATIO(bigratio);
	}
	else {
	    long num = integer, den = denominator, rest;

	    if (num < 0)
		num = -num;
	    for (;;) {
		if ((rest = den % num) == 0)
		    break;
		den = num;
		num = rest;
	    }
	    if (den != 1) {
		denominator /= num;
		integer /= num;
	    }
	    if (denominator < 0) {
		integer = -integer;
		denominator = -denominator;
	    }
	    if (denominator == 1)
		number = INTEGER(integer);
	    else
		number = RATIO(integer, denominator);
	}
    }
    else if (bignum)
	number = BIGNUM(bignum);
    else
	number = INTEGER(integer);

    return (number);
}

static int
StringInRadix(char *str, int radix, int skip_sign)
{
    if (skip_sign && (*str == '-' || *str == '+'))
	++str;
    while (*str) {
	if (*str >= '0' && *str <= '9') {
	    if (*str - '0' >= radix)
		return (0);
	}
	else if (*str >= 'A' && *str <= 'Z') {
	    if (radix <= 10 || *str - 'A' + 10 >= radix)
		return (0);
	}
	else
	    return (0);
	str++;
    }

    return (1);
}

static int
AtomSeparator(int ch, int check_space, int check_backslash)
{
    if (check_space && isspace(ch))
	return (1);
    if (check_backslash && ch == '\\')
	return (1);
    return (strchr("(),\";'`#|,", ch) != NULL);
}

static LispObj *
LispReadVector(read_info *info)
{
    LispObj *objects;
    int nodot = info->nodot;

    info->nodot = info->level + 1;
    objects = LispReadList(info);
    info->nodot = nodot;

    if (info->discard)
	return (objects);

    return (VECTOR(objects));
}

static LispObj *
LispReadFunction(read_info *info)
{
    READ_ENTER();
    int nodot = info->nodot;
    LispObj *function;

    info->nodot = info->level + 1;
    function = LispDoRead(info);
    info->nodot = nodot;

    if (info->discard)
	return (function);

    if (INVALIDP(function)) 
	READ_ERROR_INVARG();
    else if (CONSP(function)) {
	if (CAR(function) != Olambda)
	    READ_ERROR_INVARG();

	return (FUNCTION_QUOTE(function));
    }
    else if (!SYMBOLP(function))
	READ_ERROR_INVARG();

    return (FUNCTION_QUOTE(function));
}

static LispObj *
LispReadRational(int radix, read_info *info)
{
    READ_ENTER();
    LispObj *number;
    int ch, len, size;
    char stk[128], *str;

    len = 0;
    str = stk;
    size = sizeof(stk);

    for (;;) {
	ch = LispGet();
	if (ch == EOF || isspace(ch))
	    break;
	else if (AtomSeparator(ch, 0, 1)) {
	    LispUnget(ch);
	    break;
	}
	else if (islower(ch))
	    ch = toupper(ch);
	if ((ch < '0' || ch > '9') && (ch < 'A' || ch > 'Z') &&
	    ch != '+' && ch != '-' && ch != '/') {
	    if (str != stk)
		LispFree(str);
	    if (!info->discard)
		READ_ERROR1("bad character %c for rational number", ch);
	}
	if (len + 1 >= size) {
	    if (str == stk) {
		size = 512;
		str = LispMalloc(size);
		strcpy(str + 1, stk + 1);
	    }
	    else {
		size += 512;
		str = LispRealloc(str, size);
	    }
	}
	str[len++] = ch;
    }

    if (info->discard) {
	if (str != stk)
	    LispFree(str);

	return (ch == EOF ? NULL : NIL);
    }

    str[len] = '\0';

    number = LispParseNumber(str, radix, read__stream, read__line);
    if (str != stk)
	LispFree(str);

    if (!RATIONALP(number))
	READ_ERROR0("bad rational number specification");

    return (number);
}

static LispObj *
LispReadCharacter(read_info *info)
{
    READ_ENTER();
    long c;
    int ch, len;
    char stk[64];

    ch = LispGet();
    if (ch == EOF)
	return (NULL);

    stk[0] = ch;
    len = 1;

    for (;;) {
	ch = LispGet();
	if (ch == EOF)
	    break;
	else if (ch != '-' && !isalnum(ch)) {
	    LispUnget(ch);
	    break;
	}
	if (len + 1 < sizeof(stk))
	    stk[len++] = ch;
    }
    if (len > 1) {
	char **names;
	int found = 0;
	stk[len] = '\0';

	for (c = ch = 0; ch <= ' ' && !found; ch++) {
	    for (names = LispChars[ch].names; *names; names++)
		if (strcasecmp(*names, stk) == 0) {
		    c = ch;
		    found = 1;
		    break;
		}
	}
	if (!found) {
	    for (names = LispChars[0177].names; *names; names++)
		if (strcasecmp(*names, stk) == 0) {
		    c = 0177;
		    found = 1;
		    break;
		}
	}

	if (!found) {
	    if (info->discard)
		return (NIL);
	    READ_ERROR1("unkwnown character %s", stk);
	}
    }
    else
	c = stk[0];

    return (SCHAR(c));
}

static void
LispSkipComment(void)
{
    READ_ENTER();
    int ch, comm = 1;

    for (;;) {
	ch = LispGet();
	if (ch == '#') {
	    ch = LispGet();
	    if (ch == '|')
		++comm;
	    continue;
	}
	while (ch == '|') {
	    ch = LispGet();
	    if (ch == '#' && --comm == 0)
		return;
	}
	if (ch == EOF)
	    READ_ERROR_EOF();
    }
}

static LispObj *
LispReadEval(read_info *info)
{
    READ_ENTER();
    int nodot = info->nodot;
    LispObj *code;

    info->nodot = info->level + 1;
    code = LispDoRead(info);
    info->nodot = nodot;

    if (info->discard)
	return (code);

    if (INVALIDP(code))
	READ_ERROR_INVARG();

    return (EVAL(code));
}

static LispObj *
LispReadComplex(read_info *info)
{
    READ_ENTER();
    GC_ENTER();
    int nodot = info->nodot;
    LispObj *number, *arguments;

    info->nodot = info->level + 1;
    arguments = LispDoRead(info);
    info->nodot = nodot;

    /* form read */
    if (info->discard)
	return (arguments);

    if (INVALIDP(arguments) || !CONSP(arguments))
	READ_ERROR_INVARG();

    GC_PROTECT(arguments);
    number = APPLY(Ocomplex, arguments);
    GC_LEAVE();

    return (number);
}

static LispObj *
LispReadPathname(read_info *info)
{
    READ_ENTER();
    GC_ENTER();
    int nodot = info->nodot;
    LispObj *path, *arguments;

    info->nodot = info->level + 1;
    arguments = LispDoRead(info);
    info->nodot = nodot;

    /* form read */
    if (info->discard)
	return (arguments);

    if (INVALIDP(arguments))
	READ_ERROR_INVARG();

    GC_PROTECT(arguments);
    path = APPLY1(Oparse_namestring, arguments);
    GC_LEAVE();

    return (path);
}

static LispObj *
LispReadStruct(read_info *info)
{
    READ_ENTER();
    GC_ENTER();
    int len, nodot = info->nodot;
    char stk[128], *str;
    LispObj *struc, *fields;

    info->nodot = info->level + 1;
    fields = LispDoRead(info);
    info->nodot = nodot;

    /* form read */
    if (info->discard)
	return (fields);

    if (INVALIDP(fields) || !CONSP(fields) || !SYMBOLP(CAR(fields)))
	READ_ERROR_INVARG();

    GC_PROTECT(fields);

    len = ATOMID(CAR(fields))->length;
	   /* MAKE- */
    if (len + 6 > sizeof(stk))
	str = LispMalloc(len + 6);
    else
	str = stk;
    sprintf(str, "MAKE-%s", ATOMID(CAR(fields))->value);
    RPLACA(fields, ATOM(str));
    if (str != stk)
	LispFree(str);
    struc = APPLY(Omake_struct, fields);
    GC_LEAVE();

    return (struc);
}

/* XXX This is broken, needs a rewritten as soon as true vector/arrays be
 * implemented. */
static LispObj *
LispReadArray(long dimensions, read_info *info)
{
    READ_ENTER();
    GC_ENTER();
    long count;
    int nodot = info->nodot;
    LispObj *arguments, *initial, *dim, *cons, *array, *data;

    info->nodot = info->level + 1;
    data = LispDoRead(info);
    info->nodot = nodot;

    /* form read */
    if (info->discard)
	return (data);

    if (INVALIDP(data))
	READ_ERROR_INVARG();

    initial = Kinitial_contents;

    dim = cons = NIL;
    if (dimensions) {
	LispObj *array;

	for (count = 0, array = data; count < dimensions; count++) {
	    long length;
	    LispObj *item;

	    if (!CONSP(array))
		READ_ERROR0("bad array for given dimension");
	    item = array;
	    array = CAR(array);

	    for (length = 0; CONSP(item); item = CDR(item), length++)
		;

	    if (dim == NIL) {
		dim = cons = CONS(FIXNUM(length), NIL);
		GC_PROTECT(dim);
	    }
	    else {
		RPLACD(cons, CONS(FIXNUM(length), NIL));
		cons = CDR(cons);
	    }
	}
    }

    arguments = CONS(dim, CONS(initial, CONS(data, NIL)));
    GC_PROTECT(arguments);
    array = APPLY(Omake_array, arguments);
    GC_LEAVE();

    return (array);
}

static LispObj *
LispReadFeature(int with, read_info *info)
{
    READ_ENTER();
    LispObj *status;
    LispObj *feature = LispDoRead(info);

    /* form read */
    if (info->discard)
	return (feature);

    if (INVALIDP(feature))
	READ_ERROR_INVARG();

    /* paranoia check, features must be a list, possibly empty */
    if (!CONSP(FEATURES) && FEATURES != NIL)
	READ_ERROR1("%s is not a list", STROBJ(FEATURES));

    status = LispEvalFeature(feature);

    if (with) {
	if (status == T)
	    return (LispDoRead(info));

	/* need to use the field discard because the following expression
	 * may be #.FORM or #,FORM or any other form that may generate
	 * side effects */
	info->discard = 1;
	LispDoRead(info);
	info->discard = 0;

	return (LispDoRead(info));
    }

    if (status == NIL)
	return (LispDoRead(info));

    info->discard = 1;
    LispDoRead(info);
    info->discard = 0;

    return (LispDoRead(info));
}

/*
 * A very simple eval loop with AND, NOT, and OR functions for testing
 * the available features.
 */
static LispObj *
LispEvalFeature(LispObj *feature)
{
    READ_ENTER();
    Atom_id test;
    LispObj *object;

    if (CONSP(feature)) {
	LispObj *function = CAR(feature), *arguments = CDR(feature);

	if (!SYMBOLP(function))
	    READ_ERROR1("bad feature test function %s", STROBJ(function));
	if (!CONSP(arguments))
	    READ_ERROR1("bad feature test arguments %s", STROBJ(arguments));
	test = ATOMID(function);
	if (test == Sand) {
	    for (; CONSP(arguments); arguments = CDR(arguments)) {
		if (LispEvalFeature(CAR(arguments)) == NIL)
		    return (NIL);
	    }
	    return (T);
	}
	else if (test == Sor) {
	    for (; CONSP(arguments); arguments = CDR(arguments)) {
		if (LispEvalFeature(CAR(arguments)) == T)
		    return (T);
	    }
	    return (NIL);
	}
	else if (test == Snot) {
	    if (CONSP(CDR(arguments)))
		READ_ERROR0("too many arguments to NOT");

	    return (LispEvalFeature(CAR(arguments)) == NIL ? T : NIL);
	}
	else
	    READ_ERROR1("unimplemented feature test function %s", test);
    }

    if (KEYWORDP(feature))
	feature = feature->data.quote;
    else if (!SYMBOLP(feature))
	READ_ERROR1("bad feature specification %s", STROBJ(feature));

    test = ATOMID(feature);

    for (object = FEATURES; CONSP(object); object = CDR(object)) {
	/* paranoia check, elements in the feature list must ge keywords */
	if (!KEYWORDP(CAR(object)))
	    READ_ERROR1("%s is not a keyword", STROBJ(CAR(object)));
	if (ATOMID(CAR(object)) == test)
	    return (T);
    }

    /* unknown feature */
    return (NIL);
}
