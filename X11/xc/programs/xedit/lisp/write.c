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

/* $XFree86: xc/programs/xedit/lisp/write.c,v 1.32 2003/05/27 22:27:04 tsi Exp $ */

#include "lisp/write.h"
#include "lisp/hash.h"
#include <math.h>
#include <ctype.h>

#define	FLOAT_PREC	17

#define UPCASE		0
#define DOWNCASE	1
#define CAPITALIZE	2

#define INCDEPTH()							\
    if (++info->depth > MAX_STACK_DEPTH / 2)				\
	LispDestroy("stack overflow")
#define DECDEPTH()	--info->depth

/*
 * Types
 */
typedef struct _circle_info {
    long circle_nth;		/* nth circular list */
    LispObj *object;		/* the circular object */
} circle_info;

typedef struct _write_info {
    long depth;
    long level;			/* current level */
    long length;		/* current length */
    long print_level;		/* *print-level* when started printing */
    long print_length;		/* *print-length* when started printing */

    int print_escape;
    int print_case;

    long circle_count;
    /* used while building circle info */
    LispObj **objects;
    long num_objects;
    /* the circular lists */
    circle_info *circles;
    long num_circles;
} write_info;

/*
 * Prototypes
 */
static void check_stream(LispObj*, LispFile**, LispString**, int);
static void parse_double(char*, int*, double, int);
static int float_string_inc(char*, int);
static void format_integer(char*, long, int);
static int LispWriteCPointer(LispObj*, void*);
static int LispWriteCString(LispObj*, char*, long, write_info*);
static int LispDoFormatExponentialFloat(LispObj*, LispObj*,
					int, int, int*, int, int,
					int, int, int, int);

static int LispWriteInteger(LispObj*, LispObj*);
static int LispWriteCharacter(LispObj*, LispObj*, write_info*);
static int LispWriteString(LispObj*, LispObj*, write_info*);
static int LispWriteFloat(LispObj*, LispObj*);
static int LispWriteAtom(LispObj*, LispObj*, write_info*);
static int LispDoWriteAtom(LispObj*, char*, int, int);
static int LispWriteList(LispObj*, LispObj*, write_info*, int);
static int LispWriteArray(LispObj*, LispObj*, write_info*);
static int LispWriteStruct(LispObj*, LispObj*, write_info*);
static int LispDoWriteObject(LispObj*, LispObj*, write_info*, int);
static void LispBuildCircle(LispObj*, write_info*);
static void LispDoBuildCircle(LispObj*, write_info*);
static long LispCheckCircle(LispObj*, write_info*);
static int LispPrintCircle(LispObj*, LispObj*, long, int*, write_info*);
static int LispWriteAlist(LispObj*, LispArgList*, write_info*);

/*
 * Initialization
 */
LispObj *Oprint_level, *Oprint_length, *Oprint_circle,
	*Oprint_escape, *Oprint_case;
LispObj *Kupcase, *Kdowncase, *Kcapitalize;

/*
 * Implementation
 */
void
LispWriteInit(void)
{
    Oprint_level	= STATIC_ATOM("*PRINT-LEVEL*");
    LispProclaimSpecial(Oprint_level, NIL, NIL);
    LispExportSymbol(Oprint_level);

    Oprint_length	= STATIC_ATOM("*PRINT-LENGTH*");
    LispProclaimSpecial(Oprint_length, NIL, NIL);
    LispExportSymbol(Oprint_length);

    Oprint_circle	= STATIC_ATOM("*PRINT-CIRCLE*");
    LispProclaimSpecial(Oprint_circle, T, NIL);
    LispExportSymbol(Oprint_circle);

    Oprint_escape	= STATIC_ATOM("*PRINT-ESCAPE*");
    LispProclaimSpecial(Oprint_escape, T, NIL);
    LispExportSymbol(Oprint_escape);

    Kupcase		= KEYWORD("UPCASE");
    Kdowncase		= KEYWORD("DOWNCASE");
    Kcapitalize		= KEYWORD("CAPITALIZE");
    Oprint_case		= STATIC_ATOM("*PRINT-CASE*");
    LispProclaimSpecial(Oprint_case, Kupcase, NIL);
    LispExportSymbol(Oprint_case);
}

LispObj *
Lisp_FreshLine(LispBuiltin *builtin)
/*
 fresh-line &optional output-stream
 */
{
    LispObj *output_stream;

    output_stream = ARGUMENT(0);

    if (output_stream == UNSPEC)
	output_stream = NIL;
    else if (output_stream != NIL) {
	CHECK_STREAM(output_stream);
    }
    if (LispGetColumn(output_stream)) {
	LispWriteChar(output_stream, '\n');
	if (output_stream == NIL ||
	    (output_stream->data.stream.type == LispStreamStandard &&
	     output_stream->data.stream.source.file == Stdout))
	    LispFflush(Stdout);
	return (T);
    }

    return (NIL);
}

LispObj *
Lisp_Prin1(LispBuiltin *builtin)
/*
 prin1 object &optional output-stream
 */
{
    LispObj *object, *output_stream;

    output_stream = ARGUMENT(1);
    object = ARGUMENT(0);

    if (output_stream == UNSPEC)
	output_stream = NIL;
    LispPrint(object, output_stream, 0);

    return (object);
}

LispObj *
Lisp_Princ(LispBuiltin *builtin)
/*
 princ object &optional output-stream
 */
{
    int head;
    LispObj *object, *output_stream;

    output_stream = ARGUMENT(1);
    object = ARGUMENT(0);

    if (output_stream == UNSPEC)
	output_stream = NIL;
    head = lisp__data.env.length;
    LispAddVar(Oprint_escape, NIL);
    ++lisp__data.env.head;
    LispPrint(object, output_stream, 0);
    lisp__data.env.head = lisp__data.env.length = head;

    return (object);
}

LispObj *
Lisp_Print(LispBuiltin *builtin)
/*
 print object &optional output-stream
 */
{
    LispObj *object, *output_stream;

    output_stream = ARGUMENT(1);
    object = ARGUMENT(0);

    if (output_stream == UNSPEC)
	output_stream = NIL;
    LispWriteChar(output_stream, '\n');
    LispPrint(object, output_stream, 0);
    LispWriteChar(output_stream, ' ');

    return (object);
}

LispObj *
Lisp_Terpri(LispBuiltin *builtin)
/*
 terpri &optional output-stream
 */
{
    LispObj *output_stream;

    output_stream = ARGUMENT(0);

    if (output_stream == UNSPEC)
	output_stream = NIL;
    else if (output_stream != NIL) {
	CHECK_STREAM(output_stream);
    }
    LispWriteChar(output_stream, '\n');
    if (output_stream == NIL ||
	(output_stream->data.stream.type == LispStreamStandard &&
	 output_stream->data.stream.source.file == Stdout))
	LispFflush(Stdout);

    return (NIL);
}

LispObj *
Lisp_Write(LispBuiltin *builtin)
/*
 write object &key case circle escape length level lines pretty readably right-margin stream
 */
{
    int head = lisp__data.env.length;

    LispObj *object, *ocase, *circle, *escape, *length, *level, *stream;

    stream = ARGUMENT(10);
    level = ARGUMENT(5);
    length = ARGUMENT(4);
    escape = ARGUMENT(3);
    circle = ARGUMENT(2);
    ocase = ARGUMENT(1);
    object = ARGUMENT(0);

    if (stream == UNSPEC)
	stream = NIL;
    else if (stream != NIL) {
	CHECK_STREAM(stream);
    }

    /* prepare the printer environment */
    if (circle != UNSPEC)
	LispAddVar(Oprint_circle, circle);
    if (length != UNSPEC)
	LispAddVar(Oprint_length, length);
    if (level != UNSPEC)
	LispAddVar(Oprint_level, level);
    if (ocase != UNSPEC)
	LispAddVar(Oprint_case, ocase);
    if (escape != UNSPEC)
	LispAddVar(Oprint_escape, escape);

    lisp__data.env.head = lisp__data.env.length;

    (void)LispWriteObject(stream, object);

    lisp__data.env.head = lisp__data.env.length = head;

    return (object);
}

LispObj *
Lisp_WriteChar(LispBuiltin *builtin)
/*
 write-char character &optional output-stream
 */
{
    int ch;

    LispObj *character, *output_stream;

    output_stream = ARGUMENT(1);
    character = ARGUMENT(0);

    if (output_stream == UNSPEC)
	output_stream = NIL;
    CHECK_SCHAR(character);
    ch = SCHAR_VALUE(character);

    LispWriteChar(output_stream, ch);

    return (character);
}

LispObj *
Lisp_WriteLine(LispBuiltin *builtin)
/*
 write-line string &optional output-stream &key start end
 */
{
    return (LispWriteString_(builtin, 1));
}

LispObj *
Lisp_WriteString(LispBuiltin *builtin)
/*
 write-string string &optional output-stream &key start end
 */
{
    return (LispWriteString_(builtin, 0));
}


int
LispWriteObject(LispObj *stream, LispObj *object)
{
    write_info info;
    int bytes;
    LispObj *level, *length, *circle, *oescape, *ocase;

    /* current state */
    info.depth = info.level = info.length = 0;

    /* maximum level to descend */
    level = LispGetVar(Oprint_level);
    if (level && INDEXP(level))
	info.print_level = FIXNUM_VALUE(level);
    else
	info.print_level = -1;

    /* maximum list length */
    length = LispGetVar(Oprint_length);
    if (length && INDEXP(length))
	info.print_length = FIXNUM_VALUE(length);
    else
	info.print_length = -1;

    /* detect circular/shared objects? */
    circle = LispGetVar(Oprint_circle);
    info.circle_count = 0;
    info.objects = NULL;
    info.num_objects = 0;
    info.circles = NULL;
    info.num_circles = 0;
    if (circle && circle != NIL) {
	LispBuildCircle(object, &info);
	/* free this data now */
	if (info.num_objects) {
	    LispFree(info.objects);
	    info.num_objects = 0;
	}
    }

    /* escape characters and strings? */
    oescape = LispGetVar(Oprint_escape);
    if (oescape != NULL)
	info.print_escape = oescape == NIL;
    else
	info.print_escape = -1;

    /* don't use the default case printing? */
    ocase = LispGetVar(Oprint_case);
    if (ocase == Kdowncase)
	info.print_case = DOWNCASE;
    else if (ocase == Kcapitalize)
	info.print_case = CAPITALIZE;
    else
	info.print_case = UPCASE;

    bytes = LispDoWriteObject(stream, object, &info, 1);
    if (circle && circle != NIL && info.num_circles)
	LispFree(info.circles);

    return (bytes);
}

static void
LispBuildCircle(LispObj *object, write_info *info)
{
    LispObj *list;

    switch (OBJECT_TYPE(object)) {
	case LispCons_t:
	    LispDoBuildCircle(object, info);
	    break;
	case LispArray_t:
	    /* Currently arrays are implemented as lists, but only
	     * the elements could/should be circular */
	    if (LispCheckCircle(object, info) >= 0)
		return;
	    LispDoBuildCircle(object, info);
	    for (list = object->data.array.list;
		 CONSP(list); list = CDR(list))
		LispBuildCircle(CAR(list), info);
	    break;
	case LispStruct_t:
	    /* Like arrays, structs are currently implemented as lists,
	     * but only the elements could/should be circular */
	    if (LispCheckCircle(object, info) >= 0)
		return;
	    LispDoBuildCircle(object, info);
	    for (list = object->data.struc.fields;
		 CONSP(list); list = CDR(list))
		LispBuildCircle(CAR(list), info);
	    break;
	case LispQuote_t:
	case LispBackquote_t:
	case LispFunctionQuote_t:
	    LispDoBuildCircle(object, info);
	    LispBuildCircle(object->data.quote, info);
	    break;
	case LispComma_t:
	    LispDoBuildCircle(object, info);
	    LispBuildCircle(object->data.comma.eval, info);
	    break;
	case LispLambda_t:
	    /* Circularity in a function body should fail elsewhere... */
	    if (LispCheckCircle(object, info) >= 0)
		return;
	    LispDoBuildCircle(object, info);
	    LispBuildCircle(object->data.lambda.code, info);
	    break;
	default:
	    break;
    }
}

static void
LispDoBuildCircle(LispObj *object, write_info *info)
{
    long i;

    if (LispCheckCircle(object, info) >= 0)
	return;

    for (i = 0; i < info->num_objects; i++)
	if (info->objects[i] == object) {
	    /* circularity found */
	    info->circles = LispRealloc(info->circles, sizeof(circle_info) *
					(info->num_circles + 1));
	    info->circles[info->num_circles].circle_nth = 0;
	    info->circles[info->num_circles].object = object;
	    ++info->num_circles;
	    return;
	}

    /* object pointer not yet recorded */
    if ((i % 16) == 0)
	info->objects = LispRealloc(info->objects, sizeof(LispObj*) *
				    (info->num_objects + 16));
    info->objects[info->num_objects++] = object;

    if (CONSP(object)) {
	if (CONSP(CAR(object)))
	    LispDoBuildCircle(CAR(object), info);
	else
	    LispBuildCircle(CAR(object), info);
	if (CONSP(CDR(object)))
	    LispDoBuildCircle(CDR(object), info);
	else
	    LispBuildCircle(CDR(object), info);
    }
}

static long
LispCheckCircle(LispObj *object, write_info *info)
{
    long i;

    for (i = 0; i < info->num_circles; i++)
	if (info->circles[i].object == object)
	    return (i);

    return (-1);
}

static int
LispPrintCircle(LispObj *stream, LispObj *object, long circle,
		int *length, write_info *info)
{
    char stk[32];

    if (!info->circles[circle].circle_nth) {
	sprintf(stk, "#%ld=", ++info->circle_count);
	*length += LispWriteStr(stream, stk, strlen(stk));
	info->circles[circle].circle_nth = info->circle_count;

	return (1);
    }
    sprintf(stk, "#%ld#", info->circles[circle].circle_nth);
    *length += LispWriteStr(stream, stk, strlen(stk));

    return (0);
}

static int
LispWriteAlist(LispObj *stream, LispArgList *alist, write_info *info)
{
    char *name;
    int i, length = 0, need_space = 0;

#define WRITE_ATOM(object)						\
    name = ATOMID(object);						\
    length += LispDoWriteAtom(stream, name, strlen(name),		\
			      info->print_case)
#define WRITE_STRING(string)						\
    length += LispDoWriteAtom(stream, string, strlen(string),		\
			      info->print_case)
#define WRITE_OBJECT(object)						\
    length += LispDoWriteObject(stream, object, info, 1)
#define WRITE_OPAREN()							\
    length += LispWriteChar(stream, '(')
#define WRITE_SPACE()							\
    length += LispWriteChar(stream, ' ')
#define WRITE_CPAREN()							\
    length += LispWriteChar(stream, ')')

    WRITE_OPAREN();
    for (i = 0; i < alist->normals.num_symbols; i++) {
	WRITE_ATOM(alist->normals.symbols[i]);
	if (i + 1 < alist->normals.num_symbols)
	    WRITE_SPACE();
	else
	    need_space = 1;
    }
    if (alist->optionals.num_symbols) {
	if (need_space)
	    WRITE_SPACE();
	WRITE_STRING(Soptional);
	WRITE_SPACE();
	for (i = 0; i < alist->optionals.num_symbols; i++) {
	    WRITE_OPAREN();
	    WRITE_ATOM(alist->optionals.symbols[i]);
	    WRITE_SPACE();
	    WRITE_OBJECT(alist->optionals.defaults[i]);
	    if (alist->optionals.sforms[i]) {
		WRITE_SPACE();
		WRITE_ATOM(alist->optionals.sforms[i]);
	    }
	    WRITE_CPAREN();
	    if (i + 1 < alist->optionals.num_symbols)
		WRITE_SPACE();
	}
	need_space = 1;
    }
    if (alist->keys.num_symbols) {
	if (need_space)
	    WRITE_SPACE();
	length += LispDoWriteAtom(stream, Skey, 4, info->print_case);
	WRITE_SPACE();
	for (i = 0; i < alist->keys.num_symbols; i++) {
	    WRITE_OPAREN();
	    if (alist->keys.keys[i]) {
		WRITE_OPAREN();
		WRITE_ATOM(alist->keys.keys[i]);
		WRITE_SPACE();
	    }
	    WRITE_ATOM(alist->keys.symbols[i]);
	    if (alist->keys.keys[i])
		WRITE_CPAREN();
	    WRITE_SPACE();
	    WRITE_OBJECT(alist->keys.defaults[i]);
	    if (alist->keys.sforms[i]) {
		WRITE_SPACE();
		WRITE_ATOM(alist->keys.sforms[i]);
	    }
	    WRITE_CPAREN();
	    if (i + 1 < alist->keys.num_symbols)
		WRITE_SPACE();
	}
	need_space = 1;
    }
    if (alist->rest) {
	if (need_space)
	    WRITE_SPACE();
	WRITE_STRING(Srest);
	WRITE_SPACE();
	WRITE_ATOM(alist->rest);
	need_space = 1;
    }
    if (alist->auxs.num_symbols) {
	if (need_space)
	    WRITE_SPACE();
	WRITE_STRING(Saux);
	WRITE_SPACE();
	for (i = 0; i < alist->auxs.num_symbols; i++) {
	    WRITE_OPAREN();
	    WRITE_ATOM(alist->auxs.symbols[i]);
	    WRITE_SPACE();
	    WRITE_OBJECT(alist->auxs.initials[i]);
	    WRITE_CPAREN();
	    if (i + 1 < alist->auxs.num_symbols)
		WRITE_SPACE();
	}
    }
    WRITE_CPAREN();

#undef WRITE_ATOM
#undef WRITE_STRING
#undef WRITE_OBJECT
#undef WRITE_OPAREN
#undef WRITE_SPACE
#undef WRITE_CPAREN

    return (length);
}

static void
check_stream(LispObj *stream,
	     LispFile **file, LispString **string, int check_writable)
{
    /* NIL is UNIX stdout, *STANDARD-OUTPUT* may not be UNIX stdout */
    if (stream == NIL) {
	*file = Stdout;
	*string = NULL;
    }
    else {
	if (!STREAMP(stream))
	    LispDestroy("%s is not a stream", STROBJ(stream));
	if (check_writable && !stream->data.stream.writable)
	    LispDestroy("%s is not writable", STROBJ(stream));
	else if (stream->data.stream.type == LispStreamString) {
	    *string = SSTREAMP(stream);
	    *file = NULL;
	}
	else {
	    if (stream->data.stream.type == LispStreamPipe)
		*file = OPSTREAMP(stream);
	    else
		*file = stream->data.stream.source.file;
	    *string = NULL;
	}
    }
}

/* Assumes buffer has enough storage, 64 bytes should be more than enough */
static void
parse_double(char *buffer, int *exponent, double value, int d)
{
    char stk[64], fmt[32], *ptr, *fract = NULL;
    int positive = value >= 0.0;

parse_double_again:
    if (d >= 8) {
	double dcheck;
	int icheck, count;

	/* this should to do the correct rounding */
	for (count = 2;  count >= 0; count--) {
	    icheck = d <= 0 ? 0 : d > FLOAT_PREC ? FLOAT_PREC - count : d - count;
	    sprintf(fmt, "%%.%de", icheck);
	    sprintf(stk, fmt, value);
	    if (count) {
		/* if the value read back is the same formatted */
		sscanf(stk, "%lf", &dcheck);
		if (dcheck == value)
		    break;
	    }
	}
    }
    else {
	sprintf(fmt, "%%.%de", d <= 0 ? 0 : d > FLOAT_PREC ? FLOAT_PREC : d);
	sprintf(stk, fmt, value);
    }

    /* this "should" never fail */
    ptr = strchr(stk, 'e');
    if (ptr) {
	*ptr++ = '\0';
	*exponent = atoi(ptr);
    }
    else
	*exponent = 0;

    /* find start of number representation */
    for (ptr = stk; *ptr && !isdigit(*ptr); ptr++)
	;

    /* check if did not trim any significant digit,
     * this may happen because '%.e' puts only one digit before the '.' */
    if (d > 0 && d < FLOAT_PREC && fabs(value) >= 10.0 &&
	strlen(ptr) - 1 - !positive <= *exponent) {
	d += *exponent - (strlen(ptr) - 1 - !positive) + 1;
	goto parse_double_again;
    }

    /* this "should" never fail */
    fract = strchr(ptr, '.');
    if (fract)
	*fract++ = '\0';

    /* store number representation in buffer */
    *buffer = positive ? '+' : '-';
    strcpy(buffer + 1, ptr);
    if (fract)
	strcpy(buffer + strlen(buffer), fract);
}

static void
format_integer(char *buffer, long value, int radix)
{
    if (radix == 10)
	sprintf(buffer, "%ld", value);
    else if (radix == 16)
	sprintf(buffer, "%lx", value);
    else if (radix == 8)
	sprintf(buffer, "%lo", value);
    else {
	/* use bignum routine to convert number to string */
	mpi integer;

	mpi_init(&integer);
	mpi_seti(&integer, value);
	mpi_getstr(buffer, &integer, radix);
	mpi_clear(&integer);
    }
}

static int
LispWriteCPointer(LispObj *stream, void *data)
{
    char stk[32];

#ifdef LONG64
    sprintf(stk, "0x%016lx", (long)data);
#else
    sprintf(stk, "0x%08lx", (long)data);
#endif

    return (LispWriteStr(stream, stk, strlen(stk)));
}

static int
LispWriteCString(LispObj *stream, char *string, long length, write_info *info)
{
    int result;

    if (!info->print_escape) {
	char *base, *ptr, *end;

	result = LispWriteChar(stream, '"');
	for (base = ptr = string, end = string + length; ptr < end; ptr++) {
	    if (*ptr == '\\' || *ptr == '"') {
		result += LispWriteStr(stream, base, ptr - base);
		result += LispWriteChar(stream, '\\');
		result += LispWriteChar(stream, *ptr);
		base = ptr + 1;
	    }
	}
	result += LispWriteStr(stream, base, end - base);
	result += LispWriteChar(stream, '"');
    }
    else
	result = LispWriteStr(stream, string, length);

    return (result);
}

static int
LispWriteList(LispObj *stream, LispObj *object, write_info *info, int paren)
{
    int length = 0;
    long circle = 0;

    INCDEPTH();
    if (info->print_level < 0 || info->level <= info->print_level) {
	LispObj *car, *cdr;
	long print_length = info->length;

	if (info->circles && (circle = LispCheckCircle(object, info)) >= 0) {
	    if (!paren) {
		length += LispWriteStr(stream, ". ", 2);
		paren = 1;
	    }
	    if (LispPrintCircle(stream, object, circle, &length, info) == 0) {
		DECDEPTH();

		return (length);
	    }
	}

	car = CAR(object);
	cdr = CDR(object);

	if (cdr == NIL) {
	    if (paren)
		length += LispWriteChar(stream, '(');
	    if (info->print_length < 0 || info->length < info->print_length) {
		info->length = 0;
		length += LispDoWriteObject(stream, car, info, 1);
		info->length = print_length + 1;
	    }
	    else
		length += LispWriteStr(stream, "...", 3);
	    if (paren)
		length += LispWriteChar(stream, ')');
	}
	else {
	    if (paren)
		length += LispWriteChar(stream, '(');
	    if (info->print_length < 0 || info->length < info->print_length) {
		info->length = 0;
		length += LispDoWriteObject(stream, car, info, 1);
		info->length = print_length + 1;
		if (!CONSP(cdr)) {
		    length += LispWriteStr(stream, " . ", 3);
		    info->length = 0;
		    length += LispDoWriteObject(stream, cdr, info, 0);
		}
		else {
		    length += LispWriteChar(stream, ' ');
		    if (info->print_length < 0 ||
			info->length < info->print_length)
			length += LispWriteList(stream, cdr, info, 0);
		    else
			length += LispWriteStr(stream, "...", 3);
		}
	    }
	    else
		length += LispWriteStr(stream, "...", 3);
	    if (paren)
		length += LispWriteChar(stream, ')');
	}
	info->length = print_length;
    }
    else
	length += LispWriteChar(stream, '#');
    DECDEPTH();

    return (length);
}

static int
LispDoWriteObject(LispObj *stream, LispObj *object, write_info *info, int paren)
{
    long print_level;
    int length = 0;
    char stk[64], *string = NULL;

write_again:
    switch (OBJECT_TYPE(object)) {
	case LispNil_t:
	    if (object == NIL)
		string = Snil;
	    else if (object == T)
		string = St;
	    else if (object == DOT)
		string = "#<DOT>";
	    else if (object == UNSPEC)
		string = "#<UNSPEC>";
	    else if (object == UNBOUND)
		string = "#<UNBOUND>";
	    else
		string = "#<ERROR>";
	    length += LispDoWriteAtom(stream, string, strlen(string),
				      info->print_case);
	    break;
	case LispOpaque_t: {
	    char *desc = LispIntToOpaqueType(object->data.opaque.type);

	    length += LispWriteChar(stream, '#');
	    length += LispWriteCPointer(stream, object->data.opaque.data);
	    length += LispWriteStr(stream, desc, strlen(desc));
	}   break;
	case LispAtom_t:
	    length += LispWriteAtom(stream, object, info);
	    break;
	case LispFunction_t:
	    if (object->data.atom->a_function) {
		object = object->data.atom->property->fun.function;
		goto write_lambda;
	    }
	    length += LispWriteStr(stream, "#<", 2);
	    if (object->data.atom->a_compiled)
		LispDoWriteAtom(stream, "COMPILED", 8, info->print_case);
	    else if (object->data.atom->a_builtin)
		LispDoWriteAtom(stream, "BUILTIN", 7, info->print_case);
	    /* XXX the function does not exist anymore */
	    /* FIXME not sure if I want this fixed... */
	    else
		LispDoWriteAtom(stream, "UNBOUND", 7, info->print_case);
	    LispDoWriteAtom(stream, "-FUNCTION", 9, info->print_case);
	    length += LispWriteChar(stream, ' ');
	    length += LispWriteAtom(stream, object->data.atom->object, info);
	    length += LispWriteChar(stream, '>');
	    break;
	case LispString_t:
	    length += LispWriteString(stream, object, info);
	    break;
	case LispSChar_t:
	    length += LispWriteCharacter(stream, object, info);
	    break;
	case LispDFloat_t:
	    length += LispWriteFloat(stream, object);
	    break;
	case LispFixnum_t:
	case LispInteger_t:
	case LispBignum_t:
	    length += LispWriteInteger(stream, object);
	    break;
	case LispRatio_t:
	    format_integer(stk, object->data.ratio.numerator, 10);
	    length += LispWriteStr(stream, stk, strlen(stk));
	    length += LispWriteChar(stream, '/');
	    format_integer(stk, object->data.ratio.denominator, 10);
	    length += LispWriteStr(stream, stk, strlen(stk));
	    break;
	case LispBigratio_t: {
	    int sz;
	    char *ptr;

	    sz = mpi_getsize(mpr_num(object->data.mp.ratio), 10) + 1 +
		 mpi_getsize(mpr_den(object->data.mp.ratio), 10) + 1 +
		 (mpi_sgn(mpr_num(object->data.mp.ratio)) < 0);
	    if (sz > sizeof(stk))
		ptr = LispMalloc(sz);
	    else
		ptr = stk;
	    mpr_getstr(ptr, object->data.mp.ratio, 10);
	    length += LispWriteStr(stream, ptr, sz - 1);
	    if (ptr != stk)
		LispFree(ptr);
	}   break;
	case LispComplex_t:
	    length += LispWriteStr(stream, "#C(", 3);
	    length += LispDoWriteObject(stream,
					object->data.complex.real, info, 0);
	    length += LispWriteChar(stream, ' ');
	    length += LispDoWriteObject(stream,
					object->data.complex.imag, info, 0);
	    length += LispWriteChar(stream, ')');
	    break;
	case LispCons_t:
	    print_level = info->level;
	    ++info->level;
	    length += LispWriteList(stream, object, info, paren);
	    info->level = print_level;
	    break;
	case LispQuote_t:
	    length += LispWriteChar(stream, '\'');
	    paren = 1;
	    object = object->data.quote;
	    goto write_again;
	case LispBackquote_t:
	    length += LispWriteChar(stream, '`');
	    paren = 1;
	    object = object->data.quote;
	    goto write_again;
	case LispComma_t:
	    if (object->data.comma.atlist)
		length += LispWriteStr(stream, ",@", 2);
	    else
		length += LispWriteChar(stream, ',');
	    paren = 1;
	    object = object->data.comma.eval;
	    goto write_again;
	    break;
	case LispFunctionQuote_t:
	    length += LispWriteStr(stream, "#'", 2);
	    paren = 1;
	    object = object->data.quote;
	    goto write_again;
	case LispArray_t:
	    length += LispWriteArray(stream, object, info);
	    break;
	case LispStruct_t:
	    length += LispWriteStruct(stream, object, info);
	    break;
	case LispLambda_t:
	write_lambda:
	    switch (object->funtype) {
		case LispLambda:
		    string = "#<LAMBDA ";
		    break;
		case LispFunction:
		    string = "#<FUNCTION ";
		    break;
		case LispMacro:
		    string = "#<MACRO ";
		    break;
		case LispSetf:
		    string = "#<SETF ";
		    break;
	    }
	    length += LispDoWriteAtom(stream, string, strlen(string),
				      info->print_case);
	    if (object->funtype != LispLambda) {
		length += LispWriteAtom(stream, object->data.lambda.name, info);
		length += LispWriteChar(stream, ' ');
		length += LispWriteAlist(stream, object->data.lambda.name
					 ->data.atom->property->alist, info);
	    }
	    else {
		length += LispDoWriteAtom(stream, Snil, 3, info->print_case);
		length += LispWriteChar(stream, ' ');
		length += LispWriteAlist(stream, (LispArgList*)object->
					 data.lambda.name->data.opaque.data,
					 info);
	    }
	    length += LispWriteChar(stream, ' ');
	    length += LispDoWriteObject(stream,
					object->data.lambda.code, info, 0);
	    length += LispWriteChar(stream, '>');
	    break;
	case LispStream_t:
	    length += LispWriteStr(stream, "#<", 2);
	    if (object->data.stream.type == LispStreamFile)
		string = "FILE-STREAM ";
	    else if (object->data.stream.type == LispStreamString)
		string = "STRING-STREAM ";
	    else if (object->data.stream.type == LispStreamStandard)
		string = "STANDARD-STREAM ";
	    else if (object->data.stream.type == LispStreamPipe)
		string = "PIPE-STREAM ";
	    length += LispDoWriteAtom(stream, string, strlen(string),
				      info->print_case);

	    if (!object->data.stream.readable && !object->data.stream.writable)
		length += LispDoWriteAtom(stream, "CLOSED",
					  6, info->print_case);
	    else {
		if (object->data.stream.readable)
		    length += LispDoWriteAtom(stream, "READ",
					      4, info->print_case);
		if (object->data.stream.writable) {
		    if (object->data.stream.readable)
			length += LispWriteChar(stream, '-');
		    length += LispDoWriteAtom(stream, "WRITE",
					      5, info->print_case);
		}
	    }
	    if (object->data.stream.type != LispStreamString) {
		length += LispWriteChar(stream, ' ');
		length += LispDoWriteObject(stream,
					    object->data.stream.pathname,
					    info, 1);
		/* same address/size for pipes */
		length += LispWriteChar(stream, ' ');
		length += LispWriteCPointer(stream,
					    object->data.stream.source.file);
		if (object->data.stream.readable &&
		    object->data.stream.type == LispStreamFile &&
		    !object->data.stream.source.file->binary) {
		    length += LispWriteStr(stream, " @", 2);
		    format_integer(stk, object->data.stream.source.file->line, 10);
		    length += LispWriteStr(stream, stk, strlen(stk));
		}
	    }
	    length += LispWriteChar(stream, '>');
	    break;
	case LispPathname_t:
	    length += LispWriteStr(stream, "#P", 2);
	    paren = 1;
	    object = CAR(object->data.quote);
	    goto write_again;
	case LispPackage_t:
	    length += LispDoWriteAtom(stream, "#<PACKAGE ",
				      10, info->print_case);
	    length += LispWriteStr(stream,
				   THESTR(object->data.package.name),
				   STRLEN(object->data.package.name));
	    length += LispWriteChar(stream, '>');
	    break;
	case LispRegex_t:
	    length += LispDoWriteAtom(stream, "#<REGEX ",
				      8, info->print_case);
	    length += LispDoWriteObject(stream,
					object->data.regex.pattern, info, 1);
	    if (object->data.regex.options & RE_NOSPEC)
		length += LispDoWriteAtom(stream, " :NOSPEC",
					  8, info->print_case);
	    if (object->data.regex.options & RE_ICASE)
		length += LispDoWriteAtom(stream, " :ICASE",
					  7, info->print_case);
	    if (object->data.regex.options & RE_NOSUB)
		length += LispDoWriteAtom(stream, " :NOSUB",
					  7, info->print_case);
	    if (object->data.regex.options & RE_NEWLINE)
		length += LispDoWriteAtom(stream, " :NEWLINE",
					  9, info->print_case);
	    length += LispWriteChar(stream, '>');
	    break;
	case LispBytecode_t:
	    length += LispDoWriteAtom(stream, "#<BYTECODE ",
				      11, info->print_case);
	    length += LispWriteCPointer(stream,
					object->data.bytecode.bytecode);
	    length += LispWriteChar(stream, '>');
	    break;
	case LispHashTable_t:
	    length += LispDoWriteAtom(stream, "#<HASH-TABLE ",
				      13, info->print_case);
	    length += LispWriteAtom(stream, object->data.hash.test, info);
	    snprintf(stk, sizeof(stk), " %g %g",
		     object->data.hash.table->rehash_size,
		     object->data.hash.table->rehash_threshold);
	    length += LispWriteStr(stream, stk, strlen(stk));
	    snprintf(stk, sizeof(stk), " %ld/%ld>",
		     object->data.hash.table->count,
		     object->data.hash.table->num_entries);
	    length += LispWriteStr(stream, stk, strlen(stk));
	    break;
    }

    return (length);
}

/* return current column number in stream */
int
LispGetColumn(LispObj *stream)
{
    LispFile *file;
    LispString *string;

    check_stream(stream, &file, &string, 0);
    if (file != NULL)
	return (file->column);
    return (string->column);
}

/* write a character to stream */
int
LispWriteChar(LispObj *stream, int character)
{
    LispFile *file;
    LispString *string;

    check_stream(stream, &file, &string, 1);
    if (file != NULL)
	return (LispFputc(file, character));

    return (LispSputc(string, character));
}

/* write a character count times to stream */
int
LispWriteChars(LispObj *stream, int character, int count)
{
    int length = 0;

    if (count > 0) {
	char stk[64];
	LispFile *file;
	LispString *string;

	check_stream(stream, &file, &string, 1);
	if (count >= sizeof(stk)) {
	    memset(stk, character, sizeof(stk));
	    for (; count >= sizeof(stk); count -= sizeof(stk)) {
		if (file != NULL)
		    length += LispFwrite(file, stk, sizeof(stk));
		else
		    length += LispSwrite(string, stk, sizeof(stk));
	    }
	}
	else
	    memset(stk, character, count);

	if (count) {
	    if (file != NULL)
		length += LispFwrite(file, stk, count);
	    else
		length += LispSwrite(string, stk, count);
	}
    }

    return (length);
}

/* write a string to stream */
int
LispWriteStr(LispObj *stream, char *buffer, long length)
{
    LispFile *file;
    LispString *string;

    check_stream(stream, &file, &string, 1);
    if (file != NULL)
	return (LispFwrite(file, buffer, length));
    return (LispSwrite(string, buffer, length));
}

static int
LispDoWriteAtom(LispObj *stream, char *string, int length, int print_case)
{
    int bytes = 0, cap = 0;
    char buffer[128], *ptr;

    switch (print_case) {
	case DOWNCASE:
	    for (ptr = buffer; length > 0; length--, string++) {
		if (isupper(*string))
		    *ptr = tolower(*string);
		else
		    *ptr = *string;
		++ptr;
		if (ptr - buffer >= sizeof(buffer)) {
		    bytes += LispWriteStr(stream, buffer, ptr - buffer);
		    ptr = buffer;
		}
	    }
	    if (ptr > buffer)
		bytes += LispWriteStr(stream, buffer, ptr - buffer);
	    break;
	case CAPITALIZE:
	    for (ptr = buffer; length > 0; length--, string++) {
		if (isalnum(*string)) {
		    if (cap && isupper(*string))
			*ptr = tolower(*string);
		    else
			*ptr = *string;
		    cap = 1;
		}
		else {
		    *ptr = *string;
		    cap = 0;
		}
		++ptr;
		if (ptr - buffer >= sizeof(buffer)) {
		    bytes += LispWriteStr(stream, buffer, ptr - buffer);
		    ptr = buffer;
		}
	    }
	    if (ptr > buffer)
		bytes += LispWriteStr(stream, buffer, ptr - buffer);
	    break;
	default:
	    /* Strings are already stored upcase/quoted */
	    bytes += LispWriteStr(stream, string, length);
	    break;
    }

    return (bytes);
}

static int
LispWriteAtom(LispObj *stream, LispObj *object, write_info *info)
{
    int length = 0;
    LispAtom *atom = object->data.atom;
    Atom_id id = atom->string;

    if (atom->package != PACKAGE) {
	if (atom->package == lisp__data.keyword)
	    length += LispWriteChar(stream, ':');
	else if (atom->package == NULL)
	    length += LispWriteStr(stream, "#:", 2);
	else {
	    /* Check if the symbol is visible */
	    int i, visible = 0;

	    if (atom->ext) {
		for (i = lisp__data.pack->use.length - 1; i >= 0; i--) {
		    if (lisp__data.pack->use.pairs[i] == atom->package) {
			visible = 1;
			break;
		    }
		}
	    }

	    if (!visible) {
		/* XXX this assumes that package names are always "readable" */
		length +=
		    LispDoWriteAtom(stream,
				    THESTR(atom->package->data.package.name),
				    STRLEN(atom->package->data.package.name),
				    info->print_case);
		length += LispWriteChar(stream, ':');
		if (!atom->ext)
		    length += LispWriteChar(stream, ':');
	    }
	}
    }
    if (atom->unreadable)
	length += LispWriteChar(stream, '|');
    length += LispDoWriteAtom(stream, id, strlen(id),
			      atom->unreadable ? UPCASE : info->print_case);
    if (atom->unreadable)
	length += LispWriteChar(stream, '|');

    return (length);
}

static int
LispWriteInteger(LispObj *stream, LispObj *object)
{
    return (LispFormatInteger(stream, object, 10, 0, 0, 0, 0, 0, 0));
}

static int
LispWriteCharacter(LispObj *stream, LispObj *object, write_info *info)
{
    return (LispFormatCharacter(stream, object, !info->print_escape, 0));
}

static int
LispWriteString(LispObj *stream, LispObj *object, write_info *info)
{
    return (LispWriteCString(stream, THESTR(object), STRLEN(object), info));
}

static int
LispWriteFloat(LispObj *stream, LispObj *object)
{
    double value = DFLOAT_VALUE(object);

    if (value == 0.0 || (fabs(value) < 1.0E7 && fabs(value) > 1.0E-4))
	return (LispFormatFixedFloat(stream, object, 0, 0, NULL, 0, 0, 0));

    return (LispDoFormatExponentialFloat(stream, object, 0, 0, NULL,
					 0, 1, 0, ' ', 'E', 0));
}

static int
LispWriteArray(LispObj *stream, LispObj *object, write_info *info)
{
    int length = 0;
    long print_level = info->level, circle;

    if (info->circles && (circle = LispCheckCircle(object, info)) >= 0 &&
	LispPrintCircle(stream, object, circle, &length, info) == 0)
	return (length);

    if (object->data.array.rank == 0) {
	length += LispWriteStr(stream, "#0A", 3);
	length += LispDoWriteObject(stream, object->data.array.list, info, 1);
	return (length);
    }

    INCDEPTH();
    ++info->level;
    if (info->print_level < 0 || info->level <= info->print_level) {
	if (object->data.array.rank == 1)
	    length += LispWriteStr(stream, "#(", 2);
	else {
	    char stk[32];

	    format_integer(stk, object->data.array.rank, 10);
	    length += LispWriteChar(stream, '#');
	    length += LispWriteStr(stream, stk, strlen(stk));
	    length += LispWriteStr(stream, "A(", 2);
	}

	if (!object->data.array.zero) {
	    long print_length = info->length, local_length = 0;

	    if (object->data.array.rank == 1) {
		LispObj *ary;
		long count;

		for (ary = object->data.array.dim, count = 1;
		     ary != NIL; ary = CDR(ary))
		    count *= FIXNUM_VALUE(CAR(ary));
		for (ary = object->data.array.list; count > 0;
		     ary = CDR(ary), count--) {
		    if (info->print_length < 0 ||
			++local_length <= info->print_length) {
			info->length = 0;
			length += LispDoWriteObject(stream, CAR(ary), info, 1);
		    }
		    else {
			length += LispWriteStr(stream, "...", 3);
			break;
		    }
		    if (count - 1 > 0)
			length += LispWriteChar(stream, ' ');
		}
	    }
	    else {
		LispObj *ary;
		int i, k, rank, *dims, *loop;

		rank = object->data.array.rank;
		dims = LispMalloc(sizeof(int) * rank);
		loop = LispCalloc(1, sizeof(int) * (rank - 1));

		/* fill dim */
		for (i = 0, ary = object->data.array.dim; ary != NIL;
		     i++, ary = CDR(ary))
		    dims[i] = FIXNUM_VALUE(CAR(ary));

		i = 0;
		ary = object->data.array.list;
		while (loop[0] < dims[0]) {
		    if (info->print_length < 0 ||
			local_length < info->print_length) {
			for (; i < rank - 1; i++)
			    length += LispWriteChar(stream, '(');
			--i;
			for (;;) {
			    ++loop[i];
			    if (i && loop[i] >= dims[i])
				loop[i] = 0;
			    else
				break;
			    --i;
			}
			for (k = 0; k < dims[rank - 1] - 1;
			     k++, ary = CDR(ary)) {
			    if (info->print_length < 0 ||
				k < info->print_length) {
				++local_length;
				info->length = 0;
				length += LispDoWriteObject(stream,
							    CAR(ary), info, 1);
				length += LispWriteChar(stream, ' ');
			    }
			}
			if (info->print_length < 0 || k < info->print_length) {
			    ++local_length;
			    info->length = 0;
			    length += LispDoWriteObject(stream,
							CAR(ary), info, 0);
			}
			else
			    length += LispWriteStr(stream,  "...", 3);
			for (k = rank - 1; k > i; k--)
			    length += LispWriteChar(stream, ')');
			if (loop[0] < dims[0])
			    length += LispWriteChar(stream,  ' ');
			ary = CDR(ary);
		    }
		    else {
			++local_length;
			length += LispWriteStr(stream,	"...)", 4);
			for (; local_length < dims[0] - 1; local_length++)
			    length += LispWriteStr(stream,  " ...)", 5);
			if (local_length <= dims[0])
			    length += LispWriteStr(stream,  " ...", 4);
			break;
		    }
		}
		LispFree(dims);
		LispFree(loop);
	    }
	    info->length = print_length;
	}
	length += LispWriteChar(stream, ')');
    }
    else
	length += LispWriteChar(stream, '#');
    info->level = print_level;
    DECDEPTH();

    return (length);
}

static int
LispWriteStruct(LispObj *stream, LispObj *object, write_info *info)
{
    int length;
    long circle;
    LispObj *symbol;
    LispObj *def = object->data.struc.def;
    LispObj *field = object->data.struc.fields;

    if (info->circles && (circle = LispCheckCircle(object, info)) >= 0 &&
	LispPrintCircle(stream, object, circle, &length, info) == 0)
	return (length);

    INCDEPTH();
    length = LispWriteStr(stream, "#S(", 3);
    symbol = SYMBOLP(CAR(def)) ? CAR(def) : CAAR(def);
    length += LispWriteAtom(stream, symbol, info);
    def = CDR(def);
    for (; def != NIL; def = CDR(def), field = CDR(field)) {
	length += LispWriteChar(stream, ' ');
	symbol = SYMBOLP(CAR(def)) ? CAR(def) : CAAR(def);
	length += LispWriteAtom(stream, symbol, info);
	length += LispWriteChar(stream, ' ');
	length += LispDoWriteObject(stream, CAR(field), info, 1);
    }
    length += LispWriteChar(stream, ')');
    DECDEPTH();

    return (length);
}

int
LispFormatInteger(LispObj *stream, LispObj *object, int radix,
		  int atsign, int collon, int mincol,
		  int padchar, int commachar, int commainterval)
{
    char stk[128], *str = stk;
    int i, length, sign, intervals;

    if (LONGINTP(object))
	format_integer(stk, LONGINT_VALUE(object), radix);
    else {
	if (mpi_getsize(object->data.mp.integer, radix) >= sizeof(stk))
	    str = mpi_getstr(NULL, object->data.mp.integer, radix);
	else
	    mpi_getstr(str, object->data.mp.integer, radix);
    }

    sign = *str == '-';
    length = strlen(str);

    /* if collon, update length for the number of commachars to be printed */
    if (collon && commainterval > 0 && commachar) {
	intervals = length / commainterval;
	length += intervals;
    }
    else
	intervals = 0;

    /* if sign must be printed, and number is positive */
    if (atsign && !sign)
	++length;

    /* if need padding */
    if (padchar && mincol > length)
	LispWriteChars(stream, padchar, mincol - length);

    /* if need to print number sign */
    if (sign || atsign)
	LispWriteChar(stream, sign ? '-' : '+');

    /* if need to print commas to separate groups of numbers */
    if (intervals) {
	int j;
	char *ptr;

	i = (length - atsign) - intervals;
	j = i % commainterval;
	/* make the loop below easier */
	if (j == 0)
	    j = commainterval;
	i -= j;
	ptr = str + sign;
	for (; j > 0; j--, ptr++)
	    LispWriteChar(stream, *ptr);
	for (; i > 0; i -= commainterval) {
	    LispWriteChar(stream, commachar);
	    for (j = 0; j < commainterval; j++, ptr++)
		LispWriteChar(stream, *ptr);
	}
    }
    /* else, just print the string */
    else
	LispWriteStr(stream, str + sign, length - sign);

    /* if number required more than sizeof(stk) bytes */
    if (str != stk)
	LispFree(str);

    return (length);
}

int
LispFormatRomanInteger(LispObj *stream, long value, int new_roman)
{
    char stk[32];
    int length;

    length = 0;
    while (value > 1000) {
	stk[length++] = 'M';
	value -= 1000;
    }
    if (new_roman) {
	if (value >= 900) {
	    strcpy(stk + length, "CM");
	    length += 2,
	    value -= 900;
	}
	else if (value < 500 && value >= 400) {
	    strcpy(stk + length, "CD");
	    length += 2;
	    value -= 400;
	}
    }
    if (value >= 500) {
	stk[length++] = 'D';
	value -= 500;
    }
    while (value >= 100) {
	stk[length++] = 'C';
	value -= 100;
    }
    if (new_roman) {
	if (value >= 90) {
	    strcpy(stk + length, "XC");
	    length += 2,
	    value -= 90;
	}
	else if (value < 50 && value >= 40) {
	    strcpy(stk + length, "XL");
	    length += 2;
	    value -= 40;
	}
    }
    if (value >= 50) {
	stk[length++] = 'L';
	value -= 50;
    }
    while (value >= 10) {
	stk[length++] = 'X';
	value -= 10;
    }
    if (new_roman) {
	if (value == 9) {
	    strcpy(stk + length, "IX");
	    length += 2,
	    value -= 9;
	}
	else if (value == 4) {
	    strcpy(stk + length, "IV");
	    length += 2;
	    value -= 4;
	}
    }
    if (value >= 5) {
	stk[length++] = 'V';
	value -= 5;
    }
    while (value) {
	stk[length++] = 'I';
	--value;
    }

    stk[length] = '\0';

    return (LispWriteStr(stream, stk, length));
}

int
LispFormatEnglishInteger(LispObj *stream, long number, int ordinal)
{
    static char *ds[] = {
	"",	      "one",	   "two",	 "three",      "four",
	"five",       "six",	   "seven",	 "eight",      "nine",
	"ten",	      "eleven",    "twelve",	 "thirteen",   "fourteen",
	"fifteen",    "sixteen",   "seventeen",  "eighteen",   "nineteen"
    };
    static char *dsth[] = {
	"",	      "first",	   "second",	  "third",	"fourth",
	"fifth",      "sixth",	   "seventh",	  "eighth",	"ninth",
	"tenth",      "eleventh",  "twelfth",	  "thirteenth", "fourteenth",
	 "fifteenth", "sixteenth", "seventeenth", "eighteenth", "nineteenth"
    };
    static char *hs[] = {
	"",	      "",	   "twenty",	  "thirty",	"forty",
	"fifty",      "sixty",	   "seventy",	  "eighty",	"ninety"
    };
    static char *hsth[] = {
	"",	      "",	   "twentieth",   "thirtieth",	"fortieth",
       "fiftieth",    "sixtieth",  "seventieth",  "eightieth",	"ninetieth"
    };
    static char *ts[] = {
	"",	      "thousand",   "million"
    };
    static char *tsth[] = {
	"",	     "thousandth", "millionth"
    };
    char stk[256];
    int length, sign;

    sign = number < 0;
    if (sign)
	number = -number;
    length = 0;

#define SIGNLEN		6	/* strlen("minus ") */
    if (sign) {
	strcpy(stk, "minus ");
	length += SIGNLEN;
    }
    else if (number == 0) {
	if (ordinal) {
	    strcpy(stk, "zeroth");
	    length += 6;	/* strlen("zeroth") */
	}
	else {
	    strcpy(stk, "zero");
	    length += 4;	/* strlen("zero") */
	}
    }
    for (;;) {
	int count, temp;
	char *t, *h, *d;
	long value = number;

	for (count = 0; value >= 1000; value /= 1000, count++)
	    ;

	t = ds[value / 100];
	if (ordinal && !count && (value % 10) == 0)
	    h = hsth[(value % 100) / 10];
	else
	    h = hs[(value % 100) / 10];

	if (ordinal && !count)
	    d = *h ? dsth[value % 10] : dsth[value % 20];
	else
	    d = *h ? ds[value % 10] : ds[value % 20];

	if (((!sign && length) || length > SIGNLEN) && (*t || *h || *d)) {
	    if (!ordinal || count || *h || *t) {
		strcpy(stk + length, ", ");
		length += 2;
	    }
	    else {
		strcpy(stk + length, " ");
		++length;
	    }
	}

	if (*t) {
	    if (ordinal && !count && (value % 100) == 0)
		temp = sprintf(stk + length, "%s hundredth", t);
	    else
		temp = sprintf(stk + length, "%s hundred", t);
	    length += temp;
	}

	if (*h) {
	    if (*t) {
		if (ordinal && !count) {
		    strcpy(stk + length, " ");
		    ++length;
		}
		else {
		    strcpy(stk + length, " and ");
		    length += 5;	/* strlen(" and ") */
		}
	    }
	    strcpy(stk + length, h);
	    length += strlen(h);
	}

	if (*d) {
	    if (*h) {
		strcpy(stk + length, "-");
		++length;
	    }
	    else if (*t) {
		if (ordinal && !count) {
		    strcpy(stk + length, " ");
		    ++length;
		}
		else {
		    strcpy(stk + length, " and ");
		    length += 5;	/* strlen(" and ") */
		}
	    }
	    strcpy(stk + length, d);
	    length += strlen(d);
	}

	if (!count)
	    break;
	else
	    temp = count;

	if (count > 1) {
	    value *= 1000;
	    while (--count)
		value *= 1000;
	    number -= value;
	}
	else
	    number %= 1000;

	if (ordinal && number == 0 && !*t && !*h)
	    temp = sprintf(stk + length, " %s", tsth[temp]);
	else
	    temp = sprintf(stk + length, " %s", ts[temp]);
	length += temp;

	if (!number)
	    break;
    }

    return (LispWriteStr(stream, stk, length));
}

int
LispFormatCharacter(LispObj *stream, LispObj *object,
		    int atsign, int collon)
{
    int length = 0;
    int ch = SCHAR_VALUE(object);

    if (atsign && !collon)
	length += LispWriteStr(stream, "#\\", 2);
    if ((atsign || collon) && (ch <= ' ' || ch == 0177)) {
	char *name = LispChars[ch].names[0];

	length += LispWriteStr(stream, name, strlen(name));
    }
    else
	length += LispWriteChar(stream, ch);

    return (length);
}

/* returns 1 if string size must grow, done inplace */
static int
float_string_inc(char *buffer, int offset)
{
    int i;

    for (i = offset; i >= 0; i--) {
	if (buffer[i] == '9')
	    buffer[i] = '0';
	else if (buffer[i] != '.') {
	    ++buffer[i];
	    break;
	}
    }
    if (i < 0) {
	int length = strlen(buffer);

	/* string size must change */
	memmove(buffer + 1, buffer, length + 1);
	buffer[0] = '1';

	return (1);
    }

    return (0);
}

int
LispFormatFixedFloat(LispObj *stream, LispObj *object,
		     int atsign, int w, int *pd, int k, int overflowchar,
		     int padchar)
{
    char buffer[512], stk[64];
    int sign, exponent, length, offset, d = pd ? *pd : FLOAT_PREC, again;
    double value = DFLOAT_VALUE(object);

    if (value == 0.0) {
	exponent = k = 0;
	strcpy(stk, "+0");
    }
    else
	/* calculate format parameters, adjusting scale factor */
	parse_double(stk, &exponent, value, d + 1 + k);

    /* make sure k won't cause overflow */
    if (k > 128)
	k = 128;
    else if (k < -128)
	k = -128;

    /* make sure d won't cause overflow */
    if (d > 128)
	d = 128;
    else if (d < -128)
	d = -128;

    /* adjust scale factor, exponent is used as an index in stk */
    exponent += k + 1;

    /* how many bytes in float representation */
    length = strlen(stk) - 1;

    /* need to print a sign? */
    sign = atsign || (stk[0] == '-');

    /* format number, cannot overflow, as control variables were checked */
    offset = 0;
    if (sign)
	buffer[offset++] = stk[0];
    if (exponent > 0) {
	if (exponent > length) {
	    memcpy(buffer + offset, stk + 1, length);
	    memset(buffer + offset + length, '0', exponent - length);
	}
	else
	    memcpy(buffer + offset, stk + 1, exponent);
	offset += exponent;
	buffer[offset++] = '.';
	if (length > exponent) {
	    memcpy(buffer + offset, stk + 1 + exponent, length - exponent);
	    offset += length - exponent;
	}
	else
	    buffer[offset++] = '0';
    }
    else {
	buffer[offset++] = '0';
	buffer[offset++] = '.';
	while (exponent < 0) {
	    buffer[offset++] = '0';
	    exponent++;
	}
	memcpy(buffer + offset, stk + 1, length);
	offset += length;
    }
    buffer[offset] = '\0';

    again = 0;
fixed_float_check_again:
    /* make sure only d digits are printed after decimal point */
    if (d > 0) {
	char *dptr = strchr(buffer, '.');

	length = strlen(dptr) - 1;
	/* check if need to remove excess digits */
	if (length > d) {
	    int digit;

	    offset = (dptr - buffer) + 1 + d;
	    digit = buffer[offset];

	    /* remove extra digits */
	    buffer[offset] = '\0';

	    /* check if need to round */
	    if (!again && offset > 1 && isdigit(digit) && digit >= '5' &&
		isdigit(buffer[offset - 1]) &&
		float_string_inc(buffer, offset - 1))
		++offset;
	}
	/* check if need to add extra zero digits to fill space */
	else if (length < d) {
	    offset += d - length;
	    for (++length; length <= d; length++)
		dptr[length] = '0';
	    dptr[length] = '\0';
	}
    }
    else {
	/* no digits after decimal point */
	int digit, inc = 0;
	char *dptr = strchr(buffer, '.') + 1;

	digit = *dptr;
	if (!again && digit >= '5' && dptr >= buffer + 2 && isdigit(dptr[-2]))
	    inc = float_string_inc(buffer, dptr - buffer - 2);

	offset = (dptr - buffer) + inc;
	buffer[offset] = '\0';
    }

    /* if d was not specified, remove any extra zeros */
    if (pd == NULL) {
	while (offset > 2 && buffer[offset - 2] != '.' &&
	       buffer[offset - 1] == '0')
	    --offset;
	buffer[offset] = '\0';
    }

    if (w > 0 && offset > w) {
	/* first check if can remove extra fractional digits */
	if (pd == NULL) {
	    char *ptr = strchr(buffer, '.') + 1;

	    if (ptr - buffer < w) {
		d = w - (ptr - buffer);
		goto fixed_float_check_again;
	    }
	}

	/* remove leading "zero" to save space */
 	if ((!sign && buffer[0] == '0') || (sign && buffer[1] == '0')) {
	    /* ending nul also copied */
	    memmove(buffer + sign, buffer + sign + 1, offset);
	    --offset;
	}
	/* remove leading '+' to "save" space */
	if (offset > w && buffer[0] == '+') {
	    /* ending nul also copied */
	    memmove(buffer, buffer + 1, offset);
	    --offset;
	}
    }

    /* if cannot represent number in given width */
    if (overflowchar && offset > w) {
	again = 1;
	goto fixed_float_overflow;
    }

    length = 0;
    /* print padding if required */
    if (w > offset)
	length += LispWriteChars(stream, padchar, w - offset);

    /* print float number representation */
    return (LispWriteStr(stream, buffer, offset) + length);

fixed_float_overflow:
    return (LispWriteChars(stream, overflowchar, w));
}

int
LispFormatExponentialFloat(LispObj *stream, LispObj *object,
			   int atsign, int w, int *pd, int e, int k,
			   int overflowchar, int padchar, int exponentchar)
{
    return (LispDoFormatExponentialFloat(stream, object, atsign, w,
					 pd, e, k, overflowchar, padchar,
					 exponentchar, 1));
}

int
LispDoFormatExponentialFloat(LispObj *stream, LispObj *object,
			     int atsign, int w, int *pd, int e, int k,
			     int overflowchar, int padchar, int exponentchar,
			     int format)
{
    char buffer[512], stk[64];
    int sign, exponent, length, offset, d = pd ? *pd : FLOAT_PREC;
    double value = DFLOAT_VALUE(object);

    if (value == 0.0) {
	exponent = 0;
	k = 1;
	strcpy(stk, "+0");
    }
    else
	/* calculate format parameters, adjusting scale factor */
	parse_double(stk, &exponent, value, d + k - 1);

    /* set e to a value that won't overflow */
    if (e > 16)
	e = 16;

    /* set k to a value that won't overflow */
    if (k > 128)
	k = 128;
    else if (k < -128)
	k = -128;

    /* set d to a value that won't overflow */
    if (d > 128)
	d = 128;
    else if (d < -128)
	d = -128;

    /* how many bytes in float representation */
    length = strlen(stk) - 1;

    /* need to print a sign? */
    sign = atsign || (stk[0] == '-');

    /* adjust number of digits after decimal point */
    if (k > 0)
	d -= k - 1;

    /* adjust exponent, based on scale factor */
    exponent -= k - 1;

    /* format number, cannot overflow, as control variables were checked */
    offset = 0;
    if (sign)
	buffer[offset++] = stk[0];
    if (k > 0) {
	if (k > length) {
	    memcpy(buffer + offset, stk + 1, length);
	    offset += length;
	}
	else {
	    memcpy(buffer + offset, stk + 1, k);
	    offset += k;
	}
	buffer[offset++] = '.';
	if (length > k) {
	    memcpy(buffer + offset, stk + 1 + k, length - k);
	    offset += length - k;
	}
 	else
	    buffer[offset++] = '0';
    }
    else {
	int tmp = k;

	buffer[offset++] = '0';
	buffer[offset++] = '.';
	while (tmp < 0) {
	    buffer[offset++] = '0';
	    tmp++;
	}
	memcpy(buffer + offset, stk + 1, length);
	offset += length;
    }

    /* if format, then always add a sign to exponent */
    buffer[offset++] = exponentchar;
    if (format || exponent < 0)
	buffer[offset++] = exponent < 0 ? '-' : '+';

    /* XXX destroy stk contents */
    sprintf(stk, "%%0%dd", e);
    /* format scale factor*/
    length = sprintf(buffer + offset, stk,
		     exponent < 0 ? -exponent : exponent);
    /* check for overflow in exponent */
    if (length > e && overflowchar)
	goto exponential_float_overflow;
    offset += length;

    /* make sure only d digits are printed after decimal point */
    if (d > 0) {
	int currd;
	char *dptr = strchr(buffer, '.'),
	     *eptr = strchr(dptr, exponentchar);

	currd = eptr - dptr - 1;
	length = strlen(eptr);

	/* check if need to remove excess digits */
	if (currd > d) {
	    int digit, dpos;

	    dpos = offset = (dptr - buffer) + 1 + d;
	    digit = buffer[offset];

	    memmove(buffer + offset, eptr, length + 1);
	    /* also copy ending nul character */

	    /* adjust offset to length of total string */
	    offset += length;

	    /* check if need to round */
	    if (dpos > 1 && isdigit(digit) && digit >= '5' &&
		isdigit(buffer[dpos - 1]) &&
		float_string_inc(buffer, dpos - 1))
		++offset;
	}
	/* check if need to add extra zero digits to fill space */
	else if (pd && currd < d) {
	    memmove(eptr + d - currd, eptr, length + 1);
	    /* also copy ending nul character */

	    offset += d - currd;
	    for (++currd; currd <= d; currd++)
		dptr[currd] = '0';
	}
	/* check if need to remove zeros */
	else if (pd == NULL) {
	    int zeros = 1;

	    while (eptr[-zeros] == '0')
		++zeros;
	    if (eptr[-zeros] == '.')
		--zeros;
	    if (zeros > 1) {
		memmove(eptr - zeros + 1, eptr, length + 1);
		offset -= zeros - 1;
	    }
	}
    }
    else {
	/* no digits after decimal point */
	int digit, inc = 0;
	char *dptr = strchr(buffer, '.'),
	     *eptr = strchr(dptr, exponentchar);

	digit = dptr[1];

	offset = (dptr - buffer) + 1;
	length = strlen(eptr);
	memmove(buffer + offset, eptr, length + 1);
	/* also copy ending nul character */

 	if (digit >= '5' && dptr >= buffer + 2 &&
	    isdigit(dptr[-2]))
	    inc = float_string_inc(buffer, dptr - buffer - 2);

	/* adjust offset to length of total string */
	offset += length + inc;
    }

    if (w > 0 && offset > w) {
	/* remove leading "zero" to save space */
	if ((!sign && buffer[0] == '0') || (sign && buffer[1] == '0')) {
	    /* ending nul also copied */
	    memmove(buffer + sign, buffer + sign + 1, offset);
	    --offset;
	}
	/* remove leading '+' to "save" space */
	if (offset > w && buffer[0] == '+') {
	    /* ending nul also copied */
	    memmove(buffer, buffer + 1, offset);
	    --offset;
	}
    }

    /* if cannot represent number in given width */
    if (overflowchar && offset > w)
	goto exponential_float_overflow;

    length = 0;
    /* print padding if required */
    if (w > offset)
	length += LispWriteChars(stream, padchar, w - offset);

    /* print float number representation */
    return (LispWriteStr(stream, buffer, offset) + length);

exponential_float_overflow:
    return (LispWriteChars(stream, overflowchar, w));
}

int
LispFormatGeneralFloat(LispObj *stream, LispObj *object,
		       int atsign, int w, int *pd, int e, int k,
		       int overflowchar, int padchar, int exponentchar)
{
    char stk[64];
    int length, exponent, n, dd, ee, ww, d = pd ? *pd : FLOAT_PREC;
    double value = DFLOAT_VALUE(object);

    if (value == 0.0) {
	exponent = 0;
	n = 0;
	d = 1;
	strcpy(stk, "+0");
    }
    else {
	/* calculate format parameters, adjusting scale factor */
	parse_double(stk, &exponent, value, d + k - 1);
	n = exponent + 1;
    }

    /* Let ee equal e+2, or 4 if e is omitted. */
    if (e)
	ee = e + 2;
    else
	ee = 4;

    /* Let ww equal w-ee, or nil if w is omitted. */
    if (w)
	ww = w - ee;
    else
	ww = 0;

    dd = d - n;
    if (d >= dd && dd >= 0) {
	length = LispFormatFixedFloat(stream, object, atsign, ww,
				      &dd, 0, overflowchar, padchar);

	/* ~ee@T */
	length += LispWriteChars(stream, padchar, ee);
    }
    else
	length = LispFormatExponentialFloat(stream, object, atsign,
					    w, pd, e, k, overflowchar,
					    padchar, exponentchar);

    return (length);
}

int
LispFormatDollarFloat(LispObj *stream, LispObj *object,
		      int atsign, int collon, int d, int n, int w, int padchar)
{
    char buffer[512], stk[64];
    int sign, exponent, length, offset;
    double value = DFLOAT_VALUE(object);

    if (value == 0.0) {
	exponent = 0;
	strcpy(stk, "+0");
    }
    else
	/* calculate format parameters, adjusting scale factor */
	parse_double(stk, &exponent, value, d == 0 ? FLOAT_PREC : d + 1);

    /* set d to a "sane" value */
    if (d > 128)
	d = 128;

    /* set n to a "sane" value */
    if (n > 128)
	n = 128;

    /* use exponent as index in stk */
    ++exponent;

    /* don't put sign in buffer,
     * if collon specified, must go before padding */
    sign = atsign || (stk[0] == '-');

    offset = 0;

    /* pad with zeros if required */
    if (exponent > 0)
	n -= exponent;
    while (n > 0) {
	buffer[offset++] = '0';
	n--;
    }

    /* how many bytes in float representation */
    length = strlen(stk) - 1;

    if (exponent > 0) {
	if (exponent > length) {
	    memcpy(buffer + offset, stk + 1, length);
	    memset(buffer + offset + length, '0', exponent - length);
	}
	else
	    memcpy(buffer + offset, stk + 1, exponent);
	offset += exponent;
	buffer[offset++] = '.';
	if (length > exponent) {
	    memcpy(buffer + offset, stk + 1 + exponent, length - exponent);
	    offset += length - exponent;
	}
	else
	    buffer[offset++] = '0';
    }
    else {
	if (n > 0)
	    buffer[offset++] = '0';
	buffer[offset++] = '.';
	while (exponent < 0) {
	    buffer[offset++] = '0';
	    exponent++;
	}
	memcpy(buffer + offset, stk + 1, length);
	offset += length;
    }
    buffer[offset] = '\0';

    /* make sure only d digits are printed after decimal point */
    if (d > 0) {
	char *dptr = strchr(buffer, '.');

	length = strlen(dptr) - 1;
	/* check if need to remove excess digits */
	if (length > d) {
	    int digit;

	    offset = (dptr - buffer) + 1 + d;
	    digit = buffer[offset];

	    /* remove extra digits */
	    buffer[offset] = '\0';

	    /* check if need to round */
	    if (offset > 1 && isdigit(digit) && digit >= '5' &&
		isdigit(buffer[offset - 1]) &&
		float_string_inc(buffer, offset - 1))
		++offset;
	}
	/* check if need to add extra zero digits to fill space */
	else if (length < d) {
	    offset += d - length;
	    for (++length; length <= d; length++)
		dptr[length] = '0';
	    dptr[length] = '\0';
	}
    }
    else {
	/* no digits after decimal point */
	int digit, inc = 0;
	char *dptr = strchr(buffer, '.') + 1;

	digit = *dptr;
	if (digit >= '5' && dptr >= buffer + 2 && isdigit(dptr[-2]))
	    inc = float_string_inc(buffer, dptr - buffer - 2);

	offset = (dptr - buffer) + inc;
	buffer[offset] = '\0';
    }

    length = 0;
    if (sign) {
	++offset;
	if (atsign && collon)
	    length += LispWriteChar(stream, value >= 0.0 ? '+' : '-');
    }

    /* print padding if required */
    if (w > offset)
	length += LispWriteChars(stream, padchar, w - offset);

    if (atsign && !collon)
	length += LispWriteChar(stream, value >= 0.0 ? '+' : '-');

    /* print float number representation */
    return (LispWriteStr(stream, buffer, offset) + length);
}
