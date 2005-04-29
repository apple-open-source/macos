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

/* $XFree86: xc/programs/xedit/lisp/stream.c,v 1.22 2003/04/27 18:17:34 tsi Exp $ */

#include "lisp/read.h"
#include "lisp/stream.h"
#include "lisp/pathname.h"
#include "lisp/write.h"
#include "lisp/private.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>

/*
 * Initialization
 */
#define DIR_PROBE		0
#define DIR_INPUT		1
#define DIR_OUTPUT		2
#define DIR_IO			3

#define EXT_NIL			0
#define EXT_ERROR		1
#define EXT_NEW_VERSION		2
#define EXT_RENAME		3
#define EXT_RENAME_DELETE	4
#define EXT_OVERWRITE		5
#define EXT_APPEND		6
#define EXT_SUPERSEDE		7

#define NOEXT_NIL		0
#define NOEXT_ERROR		1
#define NOEXT_CREATE		2
#define NOEXT_NOTHING		3

extern char **environ;

LispObj *Oopen, *Oclose, *Otruename;

LispObj *Kif_does_not_exist, *Kprobe, *Kinput, *Koutput, *Kio,
	*Knew_version, *Krename, *Krename_and_delete, *Koverwrite,
	*Kappend, *Ksupersede, *Kcreate;

/*
 * Implementation
 */
void
LispStreamInit(void)
{
    Oopen		= STATIC_ATOM("OPEN");
    Oclose		= STATIC_ATOM("CLOSE");
    Otruename		= STATIC_ATOM("TRUENAME");

    Kif_does_not_exist	= KEYWORD("IF-DOES-NOT-EXIST");
    Kprobe		= KEYWORD("PROBE");
    Kinput		= KEYWORD("INPUT");
    Koutput		= KEYWORD("OUTPUT");
    Kio			= KEYWORD("IO");
    Knew_version	= KEYWORD("NEW-VERSION");
    Krename		= KEYWORD("RENAME");
    Krename_and_delete	= KEYWORD("RENAME-AND-DELETE");
    Koverwrite		= KEYWORD("OVERWRITE");
    Kappend		= KEYWORD("APPEND");
    Ksupersede		= KEYWORD("SUPERSEDE");
    Kcreate		= KEYWORD("CREATE");
}

LispObj *
Lisp_DeleteFile(LispBuiltin *builtin)
/*
 delete-file filename
 */
{
    GC_ENTER();
    LispObj *filename;

    filename = ARGUMENT(0);

    if (STRINGP(filename)) {
	filename = APPLY1(Oparse_namestring, filename);
	GC_PROTECT(filename);
    }
    else if (STREAMP(filename)) {
	if (filename->data.stream.type != LispStreamFile)
	    LispDestroy("%s: %s is not a FILE-STREAM",
			STRFUN(builtin), STROBJ(filename));
	filename = filename->data.stream.pathname;
    }
    else {
	CHECK_PATHNAME(filename);
    }
    GC_LEAVE();

    return (LispUnlink(THESTR(CAR(filename->data.pathname))) ? NIL : T);
}

LispObj *
Lisp_RenameFile(LispBuiltin *builtin)
/*
 rename-file filename new-name
 */
{
    int code;
    GC_ENTER();
    char *from, *to;
    LispObj *old_truename, *new_truename;

    LispObj *filename, *new_name;

    new_name = ARGUMENT(1);
    filename = ARGUMENT(0);

    if (STRINGP(filename)) {
	filename = APPLY1(Oparse_namestring, filename);
	GC_PROTECT(filename);
    }
    else if (STREAMP(filename)) {
	if (filename->data.stream.type != LispStreamFile)
	    LispDestroy("%s: %s is not a FILE-STREAM",
			STRFUN(builtin), STROBJ(filename));
	filename = filename->data.stream.pathname;
    }
    else {
	CHECK_PATHNAME(filename);
    }
    old_truename = APPLY1(Otruename, filename);
    GC_PROTECT(old_truename);

    if (STRINGP(new_name)) {
	new_name = APPLY3(Oparse_namestring, new_name, NIL, filename);
	GC_PROTECT(new_name);
    }
    else {
	CHECK_PATHNAME(new_name);
    }

    from = THESTR(CAR(filename->data.pathname));
    to = THESTR(CAR(new_name->data.pathname));
    code = LispRename(from, to);
    if (code)
	LispDestroy("%s: rename(%s, %s): %s",
		    STRFUN(builtin), from, to, strerror(errno));
    GC_LEAVE();

    new_truename = APPLY1(Otruename, new_name);
    RETURN_COUNT = 2;
    RETURN(0) = old_truename;
    RETURN(1) = new_truename;

    return (new_name);
}

LispObj *
Lisp_Streamp(LispBuiltin *builtin)
/*
 streamp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (STREAMP(object) ? T : NIL);
}

LispObj *
Lisp_InputStreamP(LispBuiltin *builtin)
/*
 input-stream-p stream
 */
{
    LispObj *stream;

    stream = ARGUMENT(0);

    CHECK_STREAM(stream);

    return (stream->data.stream.readable ? T : NIL);
}

LispObj *
Lisp_OpenStreamP(LispBuiltin *builtin)
/*
 open-stream-p stream
 */
{
   LispObj *stream;

    stream = ARGUMENT(0);

    CHECK_STREAM(stream);

    return (stream->data.stream.readable || stream->data.stream.writable ?
	    T : NIL);
}

LispObj *
Lisp_OutputStreamP(LispBuiltin *builtin)
/*
 output-stream-p stream
 */
{
    LispObj *stream;

    stream = ARGUMENT(0);

    CHECK_STREAM(stream);

    return (stream->data.stream.writable ? T : NIL);
}

LispObj *
Lisp_Open(LispBuiltin *builtin)
/*
 open filename &key direction element-type if-exists if-does-not-exist external-format
 */
{
    GC_ENTER();
    char *string;
    LispObj *stream = NIL;
    int mode, flags, direction, exist, noexist, file_exist;
    LispFile *file;

    LispObj *filename, *odirection, *element_type, *if_exists,
	    *if_does_not_exist, *external_format;

    external_format = ARGUMENT(5);
    if_does_not_exist = ARGUMENT(4);
    if_exists = ARGUMENT(3);
    element_type = ARGUMENT(2);
    odirection = ARGUMENT(1);
    filename = ARGUMENT(0);

    if (STRINGP(filename)) {
	filename = APPLY1(Oparse_namestring, filename);
	GC_PROTECT(filename);
    }
    else if (STREAMP(filename)) {
	if (filename->data.stream.type != LispStreamFile)
	    LispDestroy("%s: %s is not a FILE-STREAM",
			STRFUN(builtin), STROBJ(filename));
	filename = filename->data.stream.pathname;
    }
    else {
	CHECK_PATHNAME(filename);
    }

    if (odirection != UNSPEC) {
	direction = -1;
	if (KEYWORDP(odirection)) {
	    if (odirection == Kprobe)
		direction = DIR_PROBE;
	    else if (odirection == Kinput)
		direction = DIR_INPUT;
	    else if (odirection == Koutput)
		direction = DIR_OUTPUT;
	    else if (odirection == Kio)
		direction = DIR_IO;
	}
	if (direction == -1)
	    LispDestroy("%s: bad :DIRECTION %s",
			STRFUN(builtin), STROBJ(odirection));
    }
    else
	direction = DIR_INPUT;

    if (element_type != UNSPEC) {
	/* just check argument... */
	if (SYMBOLP(element_type) &&
	    ATOMID(element_type) == Scharacter)
	    ;	/* do nothing */
	else if (KEYWORDP(element_type) &&
	    ATOMID(element_type) == Sdefault)
	    ;	/* do nothing */
	else
	    LispDestroy("%s: only :%s and %s supported for :ELEMENT-TYPE, not %s",
			STRFUN(builtin), Sdefault, Scharacter, STROBJ(element_type));
    }

    if (if_exists != UNSPEC) {
	exist = -1;
	if (if_exists == NIL)
	    exist = EXT_NIL;
	else if (KEYWORDP(if_exists)) {
	    if (if_exists == Kerror)
		exist = EXT_ERROR;
	    else if (if_exists == Knew_version)
		exist = EXT_NEW_VERSION;
	    else if (if_exists == Krename)
		exist = EXT_RENAME;
	    else if (if_exists == Krename_and_delete)
		exist = EXT_RENAME_DELETE;
	    else if (if_exists == Koverwrite)
		exist = EXT_OVERWRITE;
	    else if (if_exists == Kappend)
		exist = EXT_APPEND;
	    else if (if_exists == Ksupersede)
		exist = EXT_SUPERSEDE;
	}
	if (exist == -1)
	    LispDestroy("%s: bad :IF-EXISTS %s",
			STRFUN(builtin), STROBJ(if_exists));
    }
    else
	exist = EXT_ERROR;

    if (if_does_not_exist != UNSPEC) {
	noexist = -1;
	if (if_does_not_exist == NIL)
	    noexist = NOEXT_NIL;
	if (KEYWORDP(if_does_not_exist)) {
	    if (if_does_not_exist == Kerror)
		noexist = NOEXT_ERROR;
	    else if (if_does_not_exist == Kcreate)
		noexist = NOEXT_CREATE;
	}
	if (noexist == -1)
	    LispDestroy("%s: bad :IF-DOES-NO-EXISTS %s",
			STRFUN(builtin), STROBJ(if_does_not_exist));
    }
    else
	noexist = direction != DIR_INPUT ? NOEXT_NOTHING : NOEXT_ERROR;

    if (external_format != UNSPEC) {
	/* just check argument... */
	if (SYMBOLP(external_format) &&
	    ATOMID(external_format) == Scharacter)
	    ;	/* do nothing */
	else if (KEYWORDP(external_format) &&
	    ATOMID(external_format) == Sdefault)
	    ;	/* do nothing */
	else
	    LispDestroy("%s: only :%s and %s supported for :EXTERNAL-FORMAT, not %s",
			STRFUN(builtin), Sdefault, Scharacter, STROBJ(external_format));
    }

    /* string representation of pathname */
    string = THESTR(CAR(filename->data.pathname));
    mode = 0;

    file_exist = access(string, F_OK) == 0;
    if (file_exist) {
	if (exist == EXT_NIL) {
	    GC_LEAVE();
	    return (NIL);
	}
    }
    else {
	if (noexist == NOEXT_NIL) {
	    GC_LEAVE();
	    return (NIL);
	}
	if (noexist == NOEXT_ERROR)
	    LispDestroy("%s: file %s does not exist",
			STRFUN(builtin), STROBJ(CAR(filename->data.quote)));
	else if (noexist == NOEXT_CREATE) {
	    LispFile *tmp = LispFopen(string, FILE_WRITE);

	    if (tmp)
		LispFclose(tmp);
	    else
		LispDestroy("%s: cannot create file %s",
			    STRFUN(builtin),
			    STROBJ(CAR(filename->data.quote)));
	}
    }

    if (direction == DIR_OUTPUT || direction == DIR_IO) {
	if (file_exist) {
	    if (exist == EXT_ERROR)
		LispDestroy("%s: file %s already exists",
			    STRFUN(builtin), STROBJ(CAR(filename->data.quote)));
	    if (exist == EXT_RENAME) {
		/* Add an ending '~' at the end of the backup file */
		char tmp[PATH_MAX + 1];

		strcpy(tmp, string);
		if (strlen(tmp) + 1 > PATH_MAX)
		    LispDestroy("%s: backup name for %s too long",
				STRFUN(builtin),
				STROBJ(CAR(filename->data.quote)));
		strcat(tmp, "~");
		if (rename(string, tmp))
		    LispDestroy("%s: rename: %s",
				STRFUN(builtin), strerror(errno));
		mode |= FILE_WRITE;
	    }
	    else if (exist == EXT_OVERWRITE)
		mode |= FILE_WRITE;
	    else if (exist == EXT_APPEND)
		mode |= FILE_APPEND;
	}
	else
	    mode |= FILE_WRITE;
	if (direction == DIR_IO)
	    mode |= FILE_IO;
    }
    else
	mode |= FILE_READ;

    file = LispFopen(string, mode);
    if (file == NULL)
	LispDestroy("%s: open: %s", STRFUN(builtin), strerror(errno));

    flags = 0;
    if (direction == DIR_PROBE) {
	LispFclose(file);
	file = NULL;
    }
    else {
	if (direction == DIR_INPUT || direction == DIR_IO)
	    flags |= STREAM_READ;
	if (direction == DIR_OUTPUT || direction == DIR_IO)
	    flags |= STREAM_WRITE;
    }
    stream = FILESTREAM(file, filename, flags);
    GC_LEAVE();

    return (stream);
}

LispObj *
Lisp_Close(LispBuiltin *builtin)
/*
 close stream &key abort
 */
{
    LispObj *stream, *oabort;

    oabort = ARGUMENT(1);
    stream = ARGUMENT(0);

    CHECK_STREAM(stream);

    if (stream->data.stream.readable || stream->data.stream.writable) {
	stream->data.stream.readable = stream->data.stream.writable = 0;
	if (stream->data.stream.type == LispStreamFile) {
	    LispFclose(stream->data.stream.source.file);
	    stream->data.stream.source.file = NULL;
	}
	else if (stream->data.stream.type == LispStreamPipe) {
	    if (IPSTREAMP(stream)) {
		LispFclose(IPSTREAMP(stream));
		IPSTREAMP(stream) = NULL;
	    }
	    if (OPSTREAMP(stream)) {
		LispFclose(OPSTREAMP(stream));
		OPSTREAMP(stream) = NULL;
	    }
	    if (EPSTREAMP(stream)) {
		LispFclose(EPSTREAMP(stream));
		EPSTREAMP(stream) = NULL;
	    }
	    if (PIDPSTREAMP(stream) > 0) {
		kill(PIDPSTREAMP(stream),
		     oabort == UNSPEC || oabort == NIL ? SIGTERM : SIGKILL);
		waitpid(PIDPSTREAMP(stream), NULL, 0);
	    }
	}
	return (T);
    }

    return (NIL);
}

LispObj *
Lisp_Listen(LispBuiltin *builtin)
/*
 listen &optional input-stream
 */
{
    LispFile *file = NULL;
    LispObj *result = NIL;

    LispObj *stream;

    stream = ARGUMENT(0);

    if (stream == UNSPEC)
	stream = NIL;
    else if (stream != NIL) {
	CHECK_STREAM(stream);
    }
    else
	stream = lisp__data.standard_input;

    if (stream->data.stream.readable) {
	switch (stream->data.stream.type) {
	    case LispStreamString:
		if (SSTREAMP(stream)->input < SSTREAMP(stream)->length)
		    result = T;
		break;
	    case LispStreamFile:
		file = FSTREAMP(stream);
		break;
	    case LispStreamStandard:
		file = FSTREAMP(stream);
		break;
	    case LispStreamPipe:
		file = IPSTREAMP(stream);
		break;
	}

	if (file != NULL) {
	    if (file->available || file->offset < file->length)
		result = T;
	    else {
		unsigned char c;

		if (!file->nonblock) {
		    if (fcntl(file->descriptor, F_SETFL, O_NONBLOCK) < 0)
			LispDestroy("%s: fcntl: %s",
				    STRFUN(builtin), strerror(errno));
		    file->nonblock = 1;
		}
		if (read(file->descriptor, &c, 1) == 1) {
		    LispFungetc(file, c);
		    result = T;
		}
	    }
	}
    }

    return (result);
}

LispObj *
Lisp_MakeStringInputStream(LispBuiltin *builtin)
/*
 make-string-input-stream string &optional start end
 */
{
    char *string;
    long start, end, length;

    LispObj *ostring, *ostart, *oend, *result;

    oend = ARGUMENT(2);
    ostart = ARGUMENT(1);
    ostring = ARGUMENT(0);

    start = end = 0;
    CHECK_STRING(ostring);
    LispCheckSequenceStartEnd(builtin, ostring, ostart, oend,
			      &start, &end, &length);
    string = THESTR(ostring);

    if (end - start != length)
	length = end - start;
    result = LSTRINGSTREAM(string + start, STREAM_READ, length);

    return (result);
}

LispObj *
Lisp_MakeStringOutputStream(LispBuiltin *builtin)
/*
 make-string-output-stream &key element-type
 */
{
    LispObj *element_type;

    element_type = ARGUMENT(0);

    if (element_type != UNSPEC) {
	/* just check argument... */
	if (SYMBOLP(element_type) && ATOMID(element_type) == Scharacter)
	    ;	/* do nothing */
	else if (KEYWORDP(element_type) &&
	    ATOMID(element_type) == Sdefault)
	    ;	/* do nothing */
	else
	    LispDestroy("%s: only :%s and %s supported for :ELEMENT-TYPE, not %s",
			STRFUN(builtin), Sdefault, Scharacter, STROBJ(element_type));
    }

    return (LSTRINGSTREAM("", STREAM_WRITE, 1));
}

LispObj *
Lisp_GetOutputStreamString(LispBuiltin *builtin)
/*
 get-output-stream-string string-output-stream
 */
{
    int length;
    char *string;
    LispObj *string_output_stream, *result;

    string_output_stream = ARGUMENT(0);

    if (!STREAMP(string_output_stream) ||
	string_output_stream->data.stream.type != LispStreamString ||
	string_output_stream->data.stream.readable ||
	!string_output_stream->data.stream.writable)
	LispDestroy("%s: %s is not an output string stream",
		    STRFUN(builtin), STROBJ(string_output_stream));

    string = LispGetSstring(SSTREAMP(string_output_stream), &length);
    result = LSTRING(string, length);

    /* reset string */
    SSTREAMP(string_output_stream)->output =
	SSTREAMP(string_output_stream)->length =
	SSTREAMP(string_output_stream)->column = 0;

    return (result);
}


/* XXX Non standard functions below
 */
LispObj *
Lisp_MakePipe(LispBuiltin *builtin)
/*
 make-pipe command-line &key :direction :element-type :external-format
 */
{
    char *string;
    LispObj *stream = NIL;
    int flags, direction;
    LispFile *error_file;
    LispPipe *program;
    int ifd[2];
    int ofd[2];
    int efd[2];
    char *argv[4];

    LispObj *command_line, *odirection, *element_type, *external_format;

    external_format = ARGUMENT(3);
    element_type = ARGUMENT(2);
    odirection = ARGUMENT(1);
    command_line = ARGUMENT(0);

    if (PATHNAMEP(command_line))
	command_line = CAR(command_line->data.quote);
    else if (!STRINGP(command_line))
	LispDestroy("%s: %s is a bad pathname",
		    STRFUN(builtin), STROBJ(command_line));

    if (odirection != UNSPEC) {
	direction = -1;
	if (KEYWORDP(odirection)) {
	    if (odirection == Kprobe)
		direction = DIR_PROBE;
	    else if (odirection == Kinput)
		direction = DIR_INPUT;
	    else if (odirection == Koutput)
		direction = DIR_OUTPUT;
	    else if (odirection == Kio)
		direction = DIR_IO;
	}
	if (direction == -1)
	    LispDestroy("%s: bad :DIRECTION %s",
			STRFUN(builtin), STROBJ(odirection));
    }
    else
	direction = DIR_INPUT;

    if (element_type != UNSPEC) {
	/* just check argument... */
	if (SYMBOLP(element_type) && ATOMID(element_type) == Scharacter)
	    ;	/* do nothing */
	else if (KEYWORDP(element_type) && ATOMID(element_type) == Sdefault)
	    ;	/* do nothing */
	else
	    LispDestroy("%s: only :%s and %s supported for :ELEMENT-TYPE, not %s",
			STRFUN(builtin), Sdefault, Scharacter, STROBJ(element_type));
    }

    if (external_format != UNSPEC) {
	/* just check argument... */
	if (SYMBOLP(external_format) && ATOMID(external_format) == Scharacter)
	    ;	/* do nothing */
	else if (KEYWORDP(external_format) &&
		 ATOMID(external_format) == Sdefault)
	    ;	/* do nothing */
	else
	    LispDestroy("%s: only :%s and %s supported for :EXTERNAL-FORMAT, not %s",
			STRFUN(builtin), Sdefault, Scharacter, STROBJ(external_format));
    }

    string = THESTR(command_line);
    program = LispMalloc(sizeof(LispPipe));
    if (direction != DIR_PROBE) {
	argv[0] = "sh";
	argv[1] = "-c";
	argv[2] = string;
	argv[3] = NULL;
	pipe(ifd);
	pipe(ofd);
	pipe(efd);
	if ((program->pid = fork()) == 0) {
	    close(0);
	    close(1);
	    close(2);
	    dup2(ofd[0], 0);
	    dup2(ifd[1], 1);
	    dup2(efd[1], 2);
	    close(ifd[0]);
	    close(ifd[1]);
	    close(ofd[0]);
	    close(ofd[1]);
	    close(efd[0]);
	    close(efd[1]);
	    execve("/bin/sh", argv, environ);
	    exit(-1);
	}
	else if (program->pid < 0)
	    LispDestroy("%s: fork: %s", STRFUN(builtin), strerror(errno));

	program->input = LispFdopen(ifd[0], FILE_READ | FILE_UNBUFFERED);
	close(ifd[1]);
	program->output = LispFdopen(ofd[1], FILE_WRITE | FILE_UNBUFFERED);
	close(ofd[0]);
	error_file = LispFdopen(efd[0], FILE_READ | FILE_UNBUFFERED);
	close(efd[1]);
    }
    else {
	program->pid = -1;
	program->input = program->output = error_file = NULL;
    }

    flags = direction == DIR_PROBE ? 0 : STREAM_READ;
    program->errorp = FILESTREAM(error_file, command_line, flags);

    flags = 0;
    if (direction != DIR_PROBE) {
	if (direction == DIR_INPUT || direction == DIR_IO)
	    flags |= STREAM_READ;
	if (direction == DIR_OUTPUT || direction == DIR_IO)
	    flags |= STREAM_WRITE;
    }
    stream = PIPESTREAM(program, command_line, flags);
    LispMused(program);

    return (stream);
}

/* Helper function, primarily for use with the xt module
 */
LispObj *
Lisp_PipeBroken(LispBuiltin *builtin)
/*
 pipe-broken pipe-stream
 */
{
    int pid, status, retval;
    LispObj *result = NIL;

    LispObj *pipe_stream;

    pipe_stream = ARGUMENT(0);

    if (!STREAMP(pipe_stream) ||
	pipe_stream->data.stream.type != LispStreamPipe)
	LispDestroy("%s: %s is not a pipe stream",
		    STRFUN(builtin), STROBJ(pipe_stream));

    if ((pid = PIDPSTREAMP(pipe_stream)) > 0) {
	retval = waitpid(pid, &status, WNOHANG | WUNTRACED);
	if (retval == pid || (retval == -1 && errno == ECHILD))
	    result = T;
    }

    return (result);
}

/*
 Helper function, so that it is not required to redirect error output
 */
LispObj *
Lisp_PipeErrorStream(LispBuiltin *builtin)
/*
 pipe-error-stream pipe-stream
 */
{
    LispObj *pipe_stream;

    pipe_stream = ARGUMENT(0);

    if (!STREAMP(pipe_stream) ||
	pipe_stream->data.stream.type != LispStreamPipe)
	LispDestroy("%s: %s is not a pipe stream",
		    STRFUN(builtin), STROBJ(pipe_stream));

    return (pipe_stream->data.stream.source.program->errorp);
}

/*
 Helper function, primarily for use with the xt module
 */
LispObj *
Lisp_PipeInputDescriptor(LispBuiltin *builtin)
/*
 pipe-input-descriptor pipe-stream
 */
{
    LispObj *pipe_stream;

    pipe_stream = ARGUMENT(0);

    if (!STREAMP(pipe_stream) ||
	pipe_stream->data.stream.type != LispStreamPipe)
	LispDestroy("%s: %s is not a pipe stream",
		    STRFUN(builtin), STROBJ(pipe_stream));
    if (!IPSTREAMP(pipe_stream))
	LispDestroy("%s: pipe %s is unreadable",
		    STRFUN(builtin), STROBJ(pipe_stream));

    return (INTEGER(LispFileno(IPSTREAMP(pipe_stream))));
}

/*
 Helper function, primarily for use with the xt module
 */
LispObj *
Lisp_PipeErrorDescriptor(LispBuiltin *builtin)
/*
 pipe-error-descriptor pipe-stream
 */
{
    LispObj *pipe_stream;

    pipe_stream = ARGUMENT(0);

    if (!STREAMP(pipe_stream) ||
	pipe_stream->data.stream.type != LispStreamPipe)
	LispDestroy("%s: %s is not a pipe stream",
		    STRFUN(builtin), STROBJ(pipe_stream));
    if (!EPSTREAMP(pipe_stream))
	LispDestroy("%s: pipe %s is closed",
		    STRFUN(builtin), STROBJ(pipe_stream));

    return (INTEGER(LispFileno(EPSTREAMP(pipe_stream))));
}
