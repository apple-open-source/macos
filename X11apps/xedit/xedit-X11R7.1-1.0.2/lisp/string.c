/*
 * Copyright (c) 2001 by The XFree86 Project, Inc.
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

/* $XdotOrg: app/xedit/lisp/string.c,v 1.3 2004/12/04 00:43:13 kuhn Exp $ */
/* $XFree86: xc/programs/xedit/lisp/string.c,v 1.24tsi Exp $ */

#include "lisp/helper.h"
#include "lisp/read.h"
#include "lisp/string.h"
#include "lisp/private.h"
#include <ctype.h>

#define CHAR_LESS		1
#define CHAR_LESS_EQUAL		2
#define CHAR_EQUAL		3
#define CHAR_GREATER_EQUAL	4
#define CHAR_GREATER		5
#define CHAR_NOT_EQUAL		6

#define CHAR_ALPHAP		1
#define CHAR_DOWNCASE		2
#define CHAR_UPCASE		3
#define CHAR_INT		4
#define CHAR_BOTHP		5
#define CHAR_UPPERP		6
#define CHAR_LOWERP		7
#define CHAR_GRAPHICP		8

#ifndef MIN
#define MIN(a, b)		((a) < (b) ? (a) : (b))
#endif

/*
 * Prototypes
 */
static LispObj *LispCharCompare(LispBuiltin*, int, int);
static LispObj *LispStringCompare(LispBuiltin*, int, int);
static LispObj *LispCharOp(LispBuiltin*, int);
static LispObj *LispStringTrim(LispBuiltin*, int, int, int);
static LispObj *LispStringUpcase(LispBuiltin*, int);
static LispObj *LispStringDowncase(LispBuiltin*, int);
static LispObj *LispStringCapitalize(LispBuiltin*, int);

/*
 * Implementation
 */
static LispObj *
LispCharCompare(LispBuiltin *builtin, int operation, int ignore_case)
{
    LispObj *object;
    int cmp, value, next_value;

    LispObj *character, *more_characters;

    more_characters = ARGUMENT(1);
    character = ARGUMENT(0);

    CHECK_SCHAR(character);
    value = SCHAR_VALUE(character);
    if (ignore_case && islower(value))
	value = toupper(value);

    if (!CONSP(more_characters))
	return (T);

    /* First check if all parameters are characters */
    for (object = more_characters; CONSP(object); object = CDR(object))
	CHECK_SCHAR(CAR(object));

    /* All characters in list must be different */
    if (operation == CHAR_NOT_EQUAL) {
	/* Compare all characters */
	do {
	    for (object = more_characters; CONSP(object); object = CDR(object)) {
		character = CAR(object);
		next_value = SCHAR_VALUE(character);
		if (ignore_case && islower(next_value))
		    next_value = toupper(next_value);
		if (value == next_value)
		    return (NIL);
	    }
	    value = SCHAR_VALUE(CAR(more_characters));
	    if (ignore_case && islower(value))
		value = toupper(value);
	    more_characters = CDR(more_characters);
	} while (CONSP(more_characters));

	return (T);
    }

    /* Linearly compare characters */
    for (; CONSP(more_characters); more_characters = CDR(more_characters)) {
	character = CAR(more_characters);
	next_value = SCHAR_VALUE(character);
	if (ignore_case && islower(next_value))
	    next_value = toupper(next_value);

	switch (operation) {
	    case CHAR_LESS:		cmp = value < next_value;	break;
	    case CHAR_LESS_EQUAL:	cmp = value <= next_value;	break;
	    case CHAR_EQUAL:		cmp = value == next_value;	break;
	    case CHAR_GREATER_EQUAL:	cmp = value >= next_value;	break;
	    case CHAR_GREATER:		cmp = value > next_value;	break;
	    default:			cmp = 0;			break;
	}

	if (!cmp)
	    return (NIL);
	value = next_value;
    }

    return (T);
}

LispObj *
Lisp_CharLess(LispBuiltin *builtin)
/*
 char< character &rest more-characters
 */
{
    return (LispCharCompare(builtin, CHAR_LESS, 0));
}

LispObj *
Lisp_CharLessEqual(LispBuiltin *builtin)
/*
 char<= character &rest more-characters
 */
{
    return (LispCharCompare(builtin, CHAR_LESS_EQUAL, 0));
}

LispObj *
Lisp_CharEqual_(LispBuiltin *builtin)
/*
 char= character &rest more-characters
 */
{
    return (LispCharCompare(builtin, CHAR_EQUAL, 0));
}

LispObj *
Lisp_CharGreater(LispBuiltin *builtin)
/*
 char> character &rest more-characters
 */
{
    return (LispCharCompare(builtin, CHAR_GREATER, 0));
}

LispObj *
Lisp_CharGreaterEqual(LispBuiltin *builtin)
/*
 char>= character &rest more-characters
 */
{
    return (LispCharCompare(builtin, CHAR_GREATER_EQUAL, 0));
}

LispObj *
Lisp_CharNotEqual_(LispBuiltin *builtin)
/*
 char/= character &rest more-characters
 */
{
    return (LispCharCompare(builtin, CHAR_NOT_EQUAL, 0));
}

LispObj *
Lisp_CharLessp(LispBuiltin *builtin)
/*
 char-lessp character &rest more-characters
 */
{
    return (LispCharCompare(builtin, CHAR_LESS, 1));
}

LispObj *
Lisp_CharNotGreaterp(LispBuiltin *builtin)
/*
 char-not-greaterp character &rest more-characters
 */
{
    return (LispCharCompare(builtin, CHAR_LESS_EQUAL, 1));
}

LispObj *
Lisp_CharEqual(LispBuiltin *builtin)
/*
 char-equalp character &rest more-characters
 */
{
    return (LispCharCompare(builtin, CHAR_EQUAL, 1));
}

LispObj *
Lisp_CharGreaterp(LispBuiltin *builtin)
/*
 char-greaterp character &rest more-characters
 */
{
    return (LispCharCompare(builtin, CHAR_GREATER, 1));
}

LispObj *
Lisp_CharNotLessp(LispBuiltin *builtin)
/*
 char-not-lessp &rest more-characters
 */
{
    return (LispCharCompare(builtin, CHAR_GREATER_EQUAL, 1));
}

LispObj *
Lisp_CharNotEqual(LispBuiltin *builtin)
/*
 char-not-equal character &rest more-characters
 */
{
    return (LispCharCompare(builtin, CHAR_NOT_EQUAL, 1));
}

static LispObj *
LispCharOp(LispBuiltin *builtin, int operation)
{
    int value;
    LispObj *result, *character;

    character = ARGUMENT(0);
    CHECK_SCHAR(character);
    value = (int)SCHAR_VALUE(character);

    switch (operation) {
	case CHAR_ALPHAP:
	    result = isalpha(value) ? T : NIL;
	    break;
	case CHAR_DOWNCASE:
	    result = SCHAR(tolower(value));
	    break;
	case CHAR_UPCASE:
	    result = SCHAR(toupper(value));
	    break;
	case CHAR_INT:
	    result = FIXNUM(value);
	    break;
	case CHAR_BOTHP:
	    result = isupper(value) || islower(value) ? T : NIL;
	    break;
	case CHAR_UPPERP:
	    result = isupper(value) ? T : NIL;
	    break;
	case CHAR_LOWERP:
	    result = islower(value) ? T : NIL;
	    break;
	case CHAR_GRAPHICP:
	    result = value == ' ' || isgraph(value) ? T : NIL;
	    break;
	default:
	    result = NIL;
	    break;
    }

    return (result);
}

LispObj *
Lisp_AlphaCharP(LispBuiltin *builtin)
/*
 alpha-char-p char
 */
{
    return (LispCharOp(builtin, CHAR_ALPHAP));
}

LispObj *
Lisp_CharDowncase(LispBuiltin *builtin)
/*
 char-downcase character
 */
{
    return (LispCharOp(builtin, CHAR_DOWNCASE));
}

LispObj *
Lisp_CharInt(LispBuiltin *builtin)
/*
 char-int character
 char-code character
 */
{
    return (LispCharOp(builtin, CHAR_INT));
}

LispObj *
Lisp_CharUpcase(LispBuiltin *builtin)
/*
 char-upcase character
 */
{
    return (LispCharOp(builtin, CHAR_UPCASE));
}

LispObj *
Lisp_BothCaseP(LispBuiltin *builtin)
/*
 both-case-p character
 */
{
    return (LispCharOp(builtin, CHAR_BOTHP));
}

LispObj *
Lisp_UpperCaseP(LispBuiltin *builtin)
/*
 upper-case-p character
 */
{
    return (LispCharOp(builtin, CHAR_UPPERP));
}

LispObj *
Lisp_LowerCaseP(LispBuiltin *builtin)
/*
 upper-case-p character
 */
{
    return (LispCharOp(builtin, CHAR_LOWERP));
}

LispObj *
Lisp_GraphicCharP(LispBuiltin *builtin)
/*
 graphic-char-p char
 */
{
    return (LispCharOp(builtin, CHAR_GRAPHICP));
}

LispObj *
Lisp_Char(LispBuiltin *builtin)
/*
 char string index
 schar simple-string index
 */
{
    unsigned char *string;
    long offset, length;

    LispObj *ostring, *oindex;

    oindex = ARGUMENT(1);
    ostring = ARGUMENT(0);

    CHECK_STRING(ostring);
    CHECK_INDEX(oindex);
    offset = FIXNUM_VALUE(oindex);
    string = (unsigned char*)THESTR(ostring);
    length = STRLEN(ostring);

    if (offset >= length)
	LispDestroy("%s: index %ld too large for string length %ld",
		    STRFUN(builtin), offset, length);

    return (SCHAR(string[offset]));
}

/* helper function for setf
 *	DONT explicitly call. Non standard function
 */
LispObj *
Lisp_XeditCharStore(LispBuiltin *builtin)
/*
 xedit::char-store string index value
 */
{
    int character;
    long offset, length;
    LispObj *ostring, *oindex, *ovalue;

    ovalue = ARGUMENT(2);
    oindex = ARGUMENT(1);
    ostring = ARGUMENT(0);

    CHECK_STRING(ostring);
    CHECK_INDEX(oindex);
    length = STRLEN(ostring);
    offset = FIXNUM_VALUE(oindex);
    if (offset >= length)
	LispDestroy("%s: index %ld too large for string length %ld",
		    STRFUN(builtin), offset, length);
    CHECK_SCHAR(ovalue);
    CHECK_STRING_WRITABLE(ostring);

    character = SCHAR_VALUE(ovalue);

    if (character < 0 || character > 255)
	LispDestroy("%s: cannot represent character %d",
		    STRFUN(builtin), character);

    THESTR(ostring)[offset] = character;

    return (ovalue);
}

LispObj *
Lisp_Character(LispBuiltin *builtin)
/*
 character object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (LispCharacterCoerce(builtin, object));
}

LispObj *
Lisp_Characterp(LispBuiltin *builtin)
/*
 characterp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (SCHARP(object) ? T : NIL);
}

LispObj *
Lisp_DigitChar(LispBuiltin *builtin)
/*
 digit-char weight &optional radix
 */
{
    long radix = 10, weight;
    LispObj *oweight, *oradix, *result = NIL;

    oradix = ARGUMENT(1);
    oweight = ARGUMENT(0);

    CHECK_FIXNUM(oweight);
    weight = FIXNUM_VALUE(oweight);

    if (oradix != UNSPEC) {
	CHECK_INDEX(oradix);
	radix = FIXNUM_VALUE(oradix);
    }
    if (radix < 2 || radix > 36)
	LispDestroy("%s: radix must be >= 2 and <= 36, not %ld",
		    STRFUN(builtin), radix);

    if (weight >= 0 && weight < radix) {
	if (weight < 9)
	    weight += '0';
	else
	    weight += 'A' - 10;
	result = SCHAR(weight);
    }

    return (result);
}

LispObj *
Lisp_DigitCharP(LispBuiltin *builtin)
/*
 digit-char-p character &optional radix
 */
{
    long radix = 10, character;
    LispObj *ochar, *oradix, *result = NIL;

    oradix = ARGUMENT(1);
    ochar = ARGUMENT(0);

    CHECK_SCHAR(ochar);
    character = SCHAR_VALUE(ochar);
    if (oradix != UNSPEC) {
	CHECK_INDEX(oradix);
	radix = FIXNUM_VALUE(oradix);
    }
    if (radix < 2 || radix > 36)
	LispDestroy("%s: radix must be >= 2 and <= 36, not %ld",
		    STRFUN(builtin), radix);

    if (character >= '0' && character <= '9')
	character -= '0';
    else if (character >= 'A' && character <= 'Z')
	character -= 'A' - 10;
    else if (character >= 'a' && character <= 'z')
	character -= 'a' - 10;
    if (character < radix)
	result = FIXNUM(character);

    return (result);
}

LispObj *
Lisp_IntChar(LispBuiltin *builtin)
/*
 int-char integer
 code-char integer
 */
{
    long character = 0;
    LispObj *integer;

    integer = ARGUMENT(0);

    CHECK_FIXNUM(integer);
    character = FIXNUM_VALUE(integer);

    return (character >= 0 && character < 0xff ? SCHAR(character) : NIL);
}

/* XXX ignoring element-type */
LispObj *
Lisp_MakeString(LispBuiltin *builtin)
/*
 make-string size &key initial-element element-type
 */
{
    long length;
    char *string, initial;

    LispObj *size, *initial_element;

    initial_element = ARGUMENT(1);
    size = ARGUMENT(0);

    CHECK_INDEX(size);
    length = FIXNUM_VALUE(size);
    if (initial_element != UNSPEC) {
	CHECK_SCHAR(initial_element);
	initial = SCHAR_VALUE(initial_element);
    }
    else
	initial = 0;

    string = LispMalloc(length + 1);
    memset(string, initial, length);
    string[length] = '\0';

    return (LSTRING2(string, length));
}

LispObj *
Lisp_ParseInteger(LispBuiltin *builtin)
/*
 parse-integer string &key start end radix junk-allowed
 */
{
    GC_ENTER();
    char *ptr, *string;
    int character, junk, sign, overflow;
    long i, start, end, radix, length, integer, check;
    LispObj *result;

    LispObj *ostring, *ostart, *oend, *oradix, *junk_allowed;

    junk_allowed = ARGUMENT(4);
    oradix = ARGUMENT(3);
    oend = ARGUMENT(2);
    ostart = ARGUMENT(1);
    ostring = ARGUMENT(0);

    start = end = radix = 0;
    result = NIL;

    CHECK_STRING(ostring);
    LispCheckSequenceStartEnd(builtin, ostring, ostart, oend,
			      &start, &end, &length);
    string = THESTR(ostring);
    if (oradix == UNSPEC)
	radix = 10;
    else {
	CHECK_INDEX(oradix);
	radix = FIXNUM_VALUE(oradix);
    }
    if (radix < 2 || radix > 36)
	LispDestroy("%s: :RADIX %ld must be in the range 2 to 36",
		    STRFUN(builtin), radix);

    integer = check = 0;
    ptr = string + start;
    sign = overflow = 0;

    /* Skip leading white spaces */
    for (i = start; i < end && *ptr && isspace(*ptr); ptr++, i++)
	;

    /* Check for sign specification */
    if (i < end && (*ptr == '-' || *ptr == '+')) {
	sign = *ptr == '-';
	++ptr;
	++i;
    }

    for (junk = 0; i < end; i++, ptr++) {
	character = *ptr;
	if (islower(character))
	    character = toupper(character);
	if (character >= '0' && character <= '9') {
	    if (character - '0' >= radix)
		junk = 1;
	    else {
		check = integer;
		integer = integer * radix + character - '0';
	    }
	}
	else if (character >= 'A' && character <= 'Z') {
	    if (character - 'A' + 10 >= radix)
		junk = 1;
	    else {
		check = integer;
		integer = integer * radix + character - 'A' + 10;
	    }
	}
	else {
	    if (isspace(character))
		break;
	    junk = 1;
	}

	if (junk)
	    break;

	if (!overflow && check > integer)
	    overflow = 1;
	/* keep looping just to count read bytes */
    }

    if (!junk)
	/* Skip white spaces */
	for (; i < end && *ptr && isspace(*ptr); ptr++, i++)
	    ;

    if ((junk || ptr == string) &&
	(junk_allowed == UNSPEC || junk_allowed == NIL))
	LispDestroy("%s: %s has a bad integer representation",
		    STRFUN(builtin), STROBJ(ostring));
    else if (ptr == string)
	result = NIL;
    else if (overflow) {
	mpi *bigi = LispMalloc(sizeof(mpi));
	char *str;

	length = end - start + sign;
	str = LispMalloc(length + 1);

	strncpy(str, string - sign, length + sign);
	str[length + sign] = '\0';
	mpi_init(bigi);
	mpi_setstr(bigi, str, radix);
	LispFree(str);
	result = BIGNUM(bigi);
    }
    else
	result = INTEGER(sign ? -integer : integer);

    GC_PROTECT(result);
    RETURN(0) = FIXNUM(i);
    RETURN_COUNT = 1;
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_String(LispBuiltin *builtin)
/*
 string object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (LispStringCoerce(builtin, object));
}

LispObj *
Lisp_Stringp(LispBuiltin *builtin)
/*
 stringp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (STRINGP(object) ? T : NIL);
}

/* XXX preserve-whitespace is being ignored */
LispObj *
Lisp_ReadFromString(LispBuiltin *builtin)
/*
 read-from-string string &optional eof-error-p eof-value &key start end preserve-whitespace
 */
{
    GC_ENTER();
    char *string;
    LispObj *stream, *result;
    long length, start, end, bytes_read;

    LispObj *ostring, *eof_error_p, *eof_value, *ostart, *oend;

    oend = ARGUMENT(4);
    ostart = ARGUMENT(3);
    eof_value = ARGUMENT(2);
    eof_error_p = ARGUMENT(1);
    ostring = ARGUMENT(0);

    CHECK_STRING(ostring);
    string = THESTR(ostring);
    LispCheckSequenceStartEnd(builtin, ostring, ostart, oend,
			      &start, &end, &length);

    if (start > 0 || end < length)
	length = end - start;
    stream = LSTRINGSTREAM(string + start, STREAM_READ, length);

    if (eof_value == UNSPEC)
	eof_value = NIL;

    LispPushInput(stream);
    result = LispRead();
    /* stream->data.stream.source.string->input is
     * the offset of the last byte read in string */
    bytes_read = stream->data.stream.source.string->input;
    LispPopInput(stream);

    if (result == NULL) {
	if (eof_error_p == NIL)
	    result = eof_value;
	else
	    LispDestroy("%s: unexpected end of input", STRFUN(builtin));
    }

    GC_PROTECT(result);
    RETURN(0) = FIXNUM(start + bytes_read);
    RETURN_COUNT = 1;
    GC_LEAVE();

    return (result);
}

static LispObj *
LispStringTrim(LispBuiltin *builtin, int left, int right, int inplace)
/*
 string-{,left-,right-}trim character-bag string
*/
{
    unsigned char *string;
    long start, end, length;

    LispObj *ochars, *ostring;

    ostring = ARGUMENT(1);
    ochars = ARGUMENT(0);

    if (!POINTERP(ochars) || !(XSTRINGP(ochars) || XCONSP(ochars))) {
	if (ARRAYP(ochars) && ochars->data.array.rank == 1)
	    ochars = ochars->data.array.list;
	else
	    LispDestroy("%s: %s is not a sequence",
			STRFUN(builtin), STROBJ(ochars));
    }
    CHECK_STRING(ostring);

    string = (unsigned char*)THESTR(ostring);
    length = STRLEN(ostring);

    start = 0;
    end = length;

    if (XSTRINGP(ochars)) {
	unsigned char *chars = (unsigned char*)THESTR(ochars);
	long i, clength = STRLEN(ochars);

	if (left) {
	    for (; start < end; start++) {
		for (i = 0; i < clength; i++)
		    if (string[start] == chars[i])
			break;
		if (i >= clength)
		    break;
	    }
	}
	if (right) {
	    for (--end; end >= 0; end--) {
		for (i = 0; i < clength; i++)
		    if (string[end] == chars[i])
			break;
		if (i >= clength)
		    break;
	    }
	    ++end;
	}
    }
    else {
	LispObj *ochar, *list;

	if (left) {
	    for (; start < end; start++) {
		for (list = ochars; CONSP(list); list = CDR(list)) {
		    ochar = CAR(list);
		    if (SCHARP(ochar) && string[start] == SCHAR_VALUE(ochar))
			break;
		}
		if (!CONSP(list))
		    break;
	    }
	}
	if (right) {
	    for (--end; end >= 0; end--) {
		for (list = ochars; CONSP(list); list = CDR(list)) {
		    ochar = CAR(list);
		    if (SCHARP(ochar) && string[end] == SCHAR_VALUE(ochar))
			break;
		}
		if (!CONSP(list))
		    break;
	    }
	    ++end;
	}
    }

    if (start == 0 && end == length)
	return (ostring);

    length = end - start;

    if (inplace) {
	CHECK_STRING_WRITABLE(ostring);
	memmove(string, string + start, length);
	string[length] = '\0';
	STRLEN(ostring) = length;
    }
    else {
	string = LispMalloc(length + 1);
	memcpy(string, THESTR(ostring) + start, length);
	string[length] = '\0';
	ostring = LSTRING2((char*)string, length);
    }

    return (ostring);
}

LispObj *
Lisp_StringTrim(LispBuiltin *builtin)
/*
 string-trim character-bag string
 */
{
    return (LispStringTrim(builtin, 1, 1, 0));
}

LispObj *
Lisp_NstringTrim(LispBuiltin *builtin)
/*
 ext::nstring-trim character-bag string
 */
{
    return (LispStringTrim(builtin, 1, 1, 1));
}

LispObj *
Lisp_StringLeftTrim(LispBuiltin *builtin)
/*
 string-left-trim character-bag string
 */
{
    return (LispStringTrim(builtin, 1, 0, 0));
}

LispObj *
Lisp_NstringLeftTrim(LispBuiltin *builtin)
/*
 ext::nstring-left-trim character-bag string
 */
{
    return (LispStringTrim(builtin, 1, 0, 1));
}

LispObj *
Lisp_StringRightTrim(LispBuiltin *builtin)
/*
 string-right-trim character-bag string
 */
{
    return (LispStringTrim(builtin, 0, 1, 0));
}

LispObj *
Lisp_NstringRightTrim(LispBuiltin *builtin)
/*
 ext::nstring-right-trim character-bag string
 */
{
    return (LispStringTrim(builtin, 0, 1, 1));
}

static LispObj *
LispStringCompare(LispBuiltin *builtin, int function, int ignore_case)
{
    int cmp1, cmp2;
    LispObj *fixnum;
    unsigned char *string1, *string2;
    long start1, end1, start2, end2, offset, length;

    LispGetStringArgs(builtin, (char**)&string1, (char**)&string2,
		      &start1, &end1, &start2, &end2);

    string1 += start1;
    string2 += start2;

    if (function == CHAR_EQUAL) {
	length = end1 - start1;

	if (length != (end2 - start2))
	    return (NIL);

	if (!ignore_case)
	    return (memcmp(string1, string2, length) ? NIL : T);

	for (; length; length--, string1++, string2++)
	    if (toupper(*string1) != toupper(*string2))
		return (NIL);
	return (T);
    }

    end1 -= start1;
    end2 -= start2;
    length = MIN(end1, end2);
    for (offset = 0;
	 offset < length;
	 string1++, string2++, offset++, start1++, start2++) {
	cmp1 = *string1;
	cmp2 = *string2;
	if (ignore_case) {
	    cmp1 = toupper(cmp1);
	    cmp2 = toupper(cmp2);
	}
	if (cmp1 != cmp2) {
	    fixnum = FIXNUM(start1);
	    switch (function) {
		case CHAR_LESS:
		    return ((cmp1 < cmp2) ? fixnum : NIL);
		case CHAR_LESS_EQUAL:
		    return ((cmp1 <= cmp2) ? fixnum : NIL);
		case CHAR_NOT_EQUAL:
		    return (fixnum);
		case CHAR_GREATER_EQUAL:
		    return ((cmp1 >= cmp2) ? fixnum : NIL);
		case CHAR_GREATER:
		    return ((cmp1 > cmp2) ? fixnum : NIL);
	    }
	}
    }

    fixnum = FIXNUM(start1);
    switch (function) {
	case CHAR_LESS:
	    return (start1 >= end1 && start2 < end2 ? fixnum : NIL);
	case CHAR_LESS_EQUAL:
	    return (start1 >= end1 ? fixnum : NIL);
	case CHAR_NOT_EQUAL:
	    return (start1 >= end1 && start2 >= end2 ? NIL : fixnum);
	case CHAR_GREATER_EQUAL:
	    return (start2 >= end2 ? fixnum : NIL);
	case CHAR_GREATER:
	    return (start2 >= end2 && start1 < end1 ? fixnum : NIL);
    }

    return (NIL);
}

LispObj *
Lisp_StringEqual_(LispBuiltin *builtin)
/*
 string= string1 string2 &key start1 end1 start2 end2
 */
{
    return (LispStringCompare(builtin, CHAR_EQUAL, 0));
}

LispObj *
Lisp_StringLess(LispBuiltin *builtin)
/*
 string< string1 string2 &key start1 end1 start2 end2
 */
{
    return (LispStringCompare(builtin, CHAR_LESS, 0));
}

LispObj *
Lisp_StringGreater(LispBuiltin *builtin)
/*
 string> string1 string2 &key start1 end1 start2 end2
 */
{
    return (LispStringCompare(builtin, CHAR_GREATER, 0));
}

LispObj *
Lisp_StringLessEqual(LispBuiltin *builtin)
/*
 string<= string1 string2 &key start1 end1 start2 end2
 */
{
    return (LispStringCompare(builtin, CHAR_LESS_EQUAL, 0));
}

LispObj *
Lisp_StringGreaterEqual(LispBuiltin *builtin)
/*
 string>= string1 string2 &key start1 end1 start2 end2
 */
{
    return (LispStringCompare(builtin, CHAR_GREATER_EQUAL, 0));
}

LispObj *
Lisp_StringNotEqual_(LispBuiltin *builtin)
/*
 string/= string1 string2 &key start1 end1 start2 end2
 */
{
    return (LispStringCompare(builtin, CHAR_NOT_EQUAL, 0));
}

LispObj *
Lisp_StringEqual(LispBuiltin *builtin)
/*
 string-equal string1 string2 &key start1 end1 start2 end2
 */
{
    return (LispStringCompare(builtin, CHAR_EQUAL, 1));
}

LispObj *
Lisp_StringLessp(LispBuiltin *builtin)
/*
 string-lessp string1 string2 &key start1 end1 start2 end2
 */
{
    return (LispStringCompare(builtin, CHAR_LESS, 1));
}

LispObj *
Lisp_StringGreaterp(LispBuiltin *builtin)
/*
 string-greaterp string1 string2 &key start1 end1 start2 end2
 */
{
    return (LispStringCompare(builtin, CHAR_GREATER, 1));
}

LispObj *
Lisp_StringNotGreaterp(LispBuiltin *builtin)
/*
 string-not-greaterp string1 string2 &key start1 end1 start2 end2
 */
{
    return (LispStringCompare(builtin, CHAR_LESS_EQUAL, 1));
}

LispObj *
Lisp_StringNotLessp(LispBuiltin *builtin)
/*
 string-not-lessp string1 string2 &key start1 end1 start2 end2
 */
{
    return (LispStringCompare(builtin, CHAR_GREATER_EQUAL, 1));
}

LispObj *
Lisp_StringNotEqual(LispBuiltin *builtin)
/*
 string-not-equal string1 string2 &key start1 end1 start2 end2
 */
{
    return (LispStringCompare(builtin, CHAR_NOT_EQUAL, 1));
}

LispObj *
LispStringUpcase(LispBuiltin *builtin, int inplace)
/*
 string-upcase string &key start end
 nstring-upcase string &key start end
 */
{
    LispObj *result;
    char *string, *newstring;
    long start, end, length, offset;

    LispObj *ostring, *ostart, *oend;

    oend = ARGUMENT(2);
    ostart = ARGUMENT(1);
    ostring = ARGUMENT(0);
    CHECK_STRING(ostring);
    LispCheckSequenceStartEnd(builtin, ostring, ostart, oend,
			      &start, &end, &offset);
    result = ostring;
    string = THESTR(ostring);
    length = STRLEN(ostring);

    /* first check if something need to be done */
    for (offset = start; offset < end; offset++)
	if (string[offset] != toupper(string[offset]))
	    break;

    if (offset >= end)
	return (result);

    if (inplace) {
	CHECK_STRING_WRITABLE(ostring);
	newstring = string;
    }
    else {
	/* upcase a copy of argument */
	newstring = LispMalloc(length + 1);
	if (offset)
	    memcpy(newstring, string, offset);
	if (length > end)
	    memcpy(newstring + end, string + end, length - end);
	newstring[length] = '\0';
    }

    for (; offset < end; offset++)
	newstring[offset] = toupper(string[offset]);

    if (!inplace)
	result = LSTRING2(newstring, length);

    return (result);
}

LispObj *
Lisp_StringUpcase(LispBuiltin *builtin)
/*
 string-upcase string &key start end
 */
{
    return (LispStringUpcase(builtin, 0));
}

LispObj *
Lisp_NstringUpcase(LispBuiltin *builtin)
/*
 nstring-upcase string &key start end
 */
{
    return (LispStringUpcase(builtin, 1));
}

LispObj *
LispStringDowncase(LispBuiltin *builtin, int inplace)
/*
 string-downcase string &key start end
 nstring-downcase string &key start end
 */
{
    LispObj *result;
    char *string, *newstring;
    long start, end, length, offset;

    LispObj *ostring, *ostart, *oend;

    oend = ARGUMENT(2);
    ostart = ARGUMENT(1);
    ostring = ARGUMENT(0);
    CHECK_STRING(ostring);
    LispCheckSequenceStartEnd(builtin, ostring, ostart, oend,
			      &start, &end, &offset);
    result = ostring;
    string = THESTR(ostring);
    length = STRLEN(ostring);

    /* first check if something need to be done */
    for (offset = start; offset < end; offset++)
	if (string[offset] != tolower(string[offset]))
	    break;

    if (offset >= end)
	return (result);

    if (inplace) {
	CHECK_STRING_WRITABLE(ostring);
	newstring = string;
    }
    else {
	/* downcase a copy of argument */
	newstring = LispMalloc(length + 1);
	if (offset)
	    memcpy(newstring, string, offset);
	if (length > end)
	    memcpy(newstring + end, string + end, length - end);
	newstring[length] = '\0';
    }
    for (; offset < end; offset++)
	newstring[offset] = tolower(string[offset]);

    if (!inplace)
	result = LSTRING2(newstring, length);

    return (result);
}

LispObj *
Lisp_StringDowncase(LispBuiltin *builtin)
/*
 string-downcase string &key start end
 */
{
    return (LispStringDowncase(builtin, 0));
}

LispObj *
Lisp_NstringDowncase(LispBuiltin *builtin)
/*
 nstring-downcase string &key start end
 */
{
    return (LispStringDowncase(builtin, 1));
}

LispObj *
LispStringCapitalize(LispBuiltin *builtin, int inplace)
/*
 string-capitalize string &key start end
 nstring-capitalize string &key start end
 */
{
    LispObj *result;
    char *string, *newstring;
    long start, end, length, offset, upcase;

    LispObj *ostring, *ostart, *oend;

    oend = ARGUMENT(2);
    ostart = ARGUMENT(1);
    ostring = ARGUMENT(0);
    CHECK_STRING(ostring);
    LispCheckSequenceStartEnd(builtin, ostring, ostart, oend,
			      &start, &end, &offset);
    result = ostring;
    string = THESTR(ostring);
    length = STRLEN(ostring);

    /* first check if something need to be done */
    for (upcase = 1, offset = start; offset < end; offset++) {
	if (upcase) {
	    if (!isalnum(string[offset]))
		continue;
	    if (string[offset] != toupper(string[offset]))
		break;
	    upcase = 0;
	}
	else {
	    if (isalnum(string[offset])) {
		if (string[offset] != tolower(string[offset]))
		    break;
	    }
	    else
		upcase = 1;
	}
    }

    if (offset >= end)
	return (result);

    if (inplace) {
	CHECK_STRING_WRITABLE(ostring);
	newstring = string;
    }
    else {
	/* capitalize a copy of argument */
	newstring = LispMalloc(length + 1);
	memcpy(newstring, string, length);
	newstring[length] = '\0';
    }
    for (; offset < end; offset++) {
	if (upcase) {
	    if (!isalnum(string[offset]))
		continue;
	    newstring[offset] = toupper(string[offset]);
	    upcase = 0;
	}
	else {
	    if (isalnum(newstring[offset]))
		newstring[offset] = tolower(string[offset]);
	    else
		upcase = 1;
	}
    }

    if (!inplace)
	result = LSTRING2(newstring, length);

    return (result);
}

LispObj *
Lisp_StringCapitalize(LispBuiltin *builtin)
/*
 string-capitalize string &key start end
 */
{
    return (LispStringCapitalize(builtin, 0));
}

LispObj *
Lisp_NstringCapitalize(LispBuiltin *builtin)
/*
 nstring-capitalize string &key start end
 */
{
    return (LispStringCapitalize(builtin, 1));
}

LispObj *
Lisp_StringConcat(LispBuiltin *builtin)
/*
 string-concat &rest strings
 */
{
    char *buffer;
    long size, length;
    LispObj *object, *string;

    LispObj *strings;

    strings = ARGUMENT(0);

    if (strings == NIL)
	return (STRING(""));

    for (length = 1, object = strings; CONSP(object); object = CDR(object)) {
	string = CAR(object);
	CHECK_STRING(string);
	length += STRLEN(string);
    }

    buffer = LispMalloc(length);

    for (length = 0, object = strings; CONSP(object); object = CDR(object)) {
	string = CAR(object);
	size = STRLEN(string);
	memcpy(buffer + length, THESTR(string), size);
	length += size;
    }
    buffer[length] = '\0';
    object = LSTRING2(buffer, length);

    return (object);
}
