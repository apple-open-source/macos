/*
Copyright (C) 1994 Free Software Foundation

This file is part of the GNU IO Library.  This library is free
software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option)
any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this library; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

As a special exception, if you link this library with files
compiled with a GNU compiler to produce an executable, this does not cause
the resulting executable to be covered by the GNU General Public License.
This exception does not however invalidate any other reasons why
the executable file might be covered by the GNU General Public License. */


/* This file provides definitions of _IO_stdin, _IO_stdout, and _IO_stderr
   for C++ code.  Compare stdfiles.c.
   (The difference is that here the vtable field is set to
   point to builtinbuf's vtable, so the objects are effectively
   of class builtinbuf.) */

#include "libioP.h"
#include <stdio.h>

#ifdef MACOSX
#include <stdlib.h>
#include "keymgr.h"
#include <mach-o/dyld.h>
#endif

#if !defined(filebuf_vtable) && defined(__cplusplus)
#ifdef __GNUC__
extern char filebuf_vtable[]
  asm (_G_VTABLE_LABEL_PREFIX
#if _G_VTABLE_LABEL_HAS_LENGTH
       "7"
#endif
       "filebuf");
#else /* !__GNUC__ */
#if _G_VTABLE_LABEL_HAS_LENGTH
#define filebuf_vtable _G_VTABLE_LABEL_PREFIX_ID##7filebuf
#else
#define filebuf_vtable _G_VTABLE_LABEL_PREFIX_ID##filebuf
#endif
extern char filebuf_vtable[];
#endif /* !__GNUC__ */
#endif /* !defined(filebuf_vtable) && defined(__cplusplus) */

#ifndef STD_VTABLE
#define STD_VTABLE (const struct _IO_jump_t *)filebuf_vtable
#endif

#ifdef _IO_MTSAFE_IO
#define DEF_STDFILE(NAME, FD, CHAIN, FLAGS) \
  static _IO_lock_t _IO_stdfile_##FD##_lock = _IO_lock_initializer; \
  struct _IO_FILE_plus NAME \
    = {FILEBUF_LITERAL(CHAIN, FLAGS, FD), STD_VTABLE}
#else
#define DEF_STDFILE(NAME, FD, CHAIN, FLAGS) \
  struct _IO_FILE_plus NAME = {FILEBUF_LITERAL(CHAIN, FLAGS, FD), STD_VTABLE}
#endif

#ifdef MACOSX
#ifdef _IO_MTSAFE_IO
/* check following! */
#define FILEBUF_INIT(FLAGS, FD) \
       { _IO_MAGIC+_IO_IS_FILEBUF+FLAGS, \
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, FD, \
	   0, 0, 0, 0, { 0 }, &_IO_stdfile_##FD##_lock }
#else
/* check following! */
# define FILEBUF_INIT(FLAGS, FD) \
       { _IO_MAGIC+_IO_IS_FILEBUF+FLAGS, \
	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, FD }
#endif

#ifdef _IO_MTSAFE_IO
#define INIT_STDFILE(NAME, FD, FLAGS) \
  static _IO_lock_t _IO_stdfile_##FD##_lock = _IO_lock_initializer; \
  struct _IO_FILE_plus NAME \
    = {FILEBUF_INIT(FLAGS, FD), STD_VTABLE}
#else
#define INIT_STDFILE(NAME, FD, FLAGS) \
  struct _IO_FILE_plus NAME = {FILEBUF_INIT(FLAGS, FD), STD_VTABLE}
#endif

extern "C" {
#define DEF_GLOBAL_NODE(proto,fp)   fp = (_IO_FILE_plus *) malloc (sizeof (_IO_FILE_plus)); \
								 *fp = proto; _IO_link_in (&((fp)->file));

extern _IO_FILE_plus *_IO_init_stdin (), *_IO_init_stdout (), *_IO_init_stderr ();
extern _IO_FILE_plus *_IO_stdin, *_IO_stdout, *_IO_stderr;
unsigned int _io_mode_bits = _IO_OSX_SYNC_STDIO;

extern void _dynamic_stdstream_init ();

static int mutex=0;

void _IO_init_global_stdio (void) {

// The mutex variable insures that this instance of the library
// is only initialized once.

if (mutex)
	return;

INIT_STDFILE(_IO_stdin_V, 0, _IO_NO_WRITES);
INIT_STDFILE(_IO_stdout_V, 1, _IO_NO_READS+_IO_UNBUFFERED);
INIT_STDFILE(_IO_stderr_V, 2, _IO_NO_READS+_IO_UNBUFFERED);

struct _IO_FILE_plus *fp;
int refcnt;

#ifdef MACOSX_DEBUG
	printf ("_IO_init_global_stdio @ 0x%08x\n", &_IO_init_global_stdio);
#endif

// Now we need to force keymgr module to be fully bound before entering
// the locked region. That way, once we set the lock, nothing will 
// interrupt processing of the initialization routine (at least dyld won't)
// until the initialization completes. By binding __init_keymgr fully,
// we bind all references to the keymgr from here. The binding of 
// _dynamic_stream_init is handled manually in the build_gcc script 
// by linking this module and stdstreams.o together with the static linker.

unsigned long address ;
void *module ;
_dyld_lookup_and_bind_fully("__init_keymgr", &address, &module);
_dyld_lookup_and_bind_fully("_malloc", &address, &module) ;

// If this is not the first module to initialize libstdc++,
// then we just need to load the standard file pointers.

if (refcnt = (unsigned int) _keymgr_get_and_lock_processwide_ptr (KEYMGR_IO_REFCNT)) {
	// Prevent race conditions.
	if (mutex) {
		_keymgr_unlock_processwide_ptr (KEYMGR_IO_REFCNT);
		return;
		}

	_IO_stdin = (_IO_FILE_plus *) _keymgr_get_and_lock_processwide_ptr (KEYMGR_IO_STDIN);
	_IO_stdout = (_IO_FILE_plus *) _keymgr_get_and_lock_processwide_ptr (KEYMGR_IO_STDOUT);
	_IO_stderr = (_IO_FILE_plus *) _keymgr_get_and_lock_processwide_ptr (KEYMGR_IO_STDERR);
	_io_mode_bits = (unsigned int) _keymgr_get_and_lock_processwide_ptr (KEYMGR_IO_MODE_BITS);
	refcnt++; mutex++;
	_keymgr_unlock_processwide_ptr (KEYMGR_IO_STDIN);
	_keymgr_unlock_processwide_ptr (KEYMGR_IO_STDOUT);
	_keymgr_unlock_processwide_ptr (KEYMGR_IO_STDERR);
	_keymgr_unlock_processwide_ptr (KEYMGR_IO_MODE_BITS);
	_dynamic_stdstream_init ();
	_keymgr_set_and_unlock_processwide_ptr (KEYMGR_IO_REFCNT, (void *) refcnt);
	return;
	}

// Order is critical here.  KEYMGR_IO_REFCNT is acting like a
// semaphore for the whole change process. So it should not
// be unlocked until all other changes have been completed.

refcnt++; mutex++;
DEF_GLOBAL_NODE(_IO_stdin_V,_IO_stdin);
DEF_GLOBAL_NODE(_IO_stdout_V,_IO_stdout);
DEF_GLOBAL_NODE(_IO_stderr_V,_IO_stderr);
_keymgr_get_and_lock_processwide_ptr (KEYMGR_IO_STDIN);
_keymgr_get_and_lock_processwide_ptr (KEYMGR_IO_STDOUT);
_keymgr_get_and_lock_processwide_ptr (KEYMGR_IO_STDERR);
_keymgr_get_and_lock_processwide_ptr (KEYMGR_IO_MODE_BITS);
_keymgr_set_and_unlock_processwide_ptr (KEYMGR_IO_STDOUT, (void *) _IO_stdout);
_keymgr_set_and_unlock_processwide_ptr (KEYMGR_IO_STDERR, (void *) _IO_stderr);
_keymgr_set_and_unlock_processwide_ptr (KEYMGR_IO_STDIN, (void *) _IO_stdin);
_keymgr_set_and_unlock_processwide_ptr (KEYMGR_IO_MODE_BITS, (void *) _io_mode_bits);
_dynamic_stdstream_init ();
_keymgr_set_and_unlock_processwide_ptr (KEYMGR_IO_REFCNT, (void *) refcnt);
}

// These three functions insure that if the standard file buffers haven't been
// initialized by the time something needs them, that they will be initialized
// then.

_IO_FILE_plus *_IO_init_stdin () {

#ifdef MACOSX_DEBUG
	printf ("_IO_init_stdin @ 0x%08x\n", _IO_init_stdin);
#endif

	_IO_init_global_stdio ();
	return _IO_stdin;
}

_IO_FILE_plus *_IO_init_stdout () {
#ifdef MACOSX_DEBUG
	printf ("_IO_init_stdout @ 0x%08x\n", _IO_init_stdout);
#endif
	_IO_init_global_stdio ();
	return (_IO_stdout);
}

_IO_FILE_plus *_IO_init_stderr () {
#ifdef MACOSX_DEBUG
	printf ("_IO_init_stderr @ 0x%08x\n", _IO_init_stderr);
#endif
	_IO_init_global_stdio ();
	return (_IO_stderr);
}

void _IO_term_global_stdio (void) {
int refcnt;

#ifdef MACOSX_DEBUG
	printf ("_IO_term_global_stdio @ 0x%08x\n", &_IO_term_global_stdio);
#endif

// If this isn't the last user of streams to terminate,
// then we just flush all streams. If this is the last user
// of streams, then flush and unbuffer all streams, then unlink
// them from the master list. The master file list is set to NULL
// then to start the process over again. There is an unavoidable memory
// leak here. The file nodes themselves must be kept around because
// it is possible for another termination routine/destructor to
// still run and produce output.

 if ((refcnt = (unsigned int) _keymgr_get_and_lock_processwide_ptr (KEYMGR_IO_REFCNT)) > 1) {
 	refcnt--; mutex--;
 	_IO_flush_all ();
 	_keymgr_set_and_unlock_processwide_ptr (KEYMGR_IO_REFCNT, (void *)refcnt);
 	return;
 	}

 _IO_cleanup ();
 _keymgr_get_and_lock_processwide_ptr (KEYMGR_IO_LIST);
 _keymgr_set_and_unlock_processwide_ptr (KEYMGR_IO_LIST, NULL);
 mutex--;
 _keymgr_set_and_unlock_processwide_ptr (KEYMGR_IO_REFCNT, 0);
 } /* end of _IO_term_global_stdio () */
} /* end extern "C" */

#pragma CALL_ON_LOAD  _IO_init_global_stdio

#pragma CALL_ON_UNLOAD _IO_term_global_stdio

#else /*! MACOSX*/

DEF_STDFILE(_IO_stdin_, 0, 0, _IO_NO_WRITES);
DEF_STDFILE(_IO_stdout_, 1, &_IO_stdin_.file, _IO_NO_READS);
DEF_STDFILE(_IO_stderr_, 2, &_IO_stdout_.file,
            _IO_NO_READS+_IO_UNBUFFERED);

#ifdef _STDIO_USES_IOSTREAM
_IO_FILE *_IO_list_all = &_IO_stderr_.file;
#else /* !_STDIO_USES_IOSTREAM */
#include "stdiostream.h"

struct _IO_fake_stdiobuf {
  struct {
    _IO_FILE file;
    const void *vtable;
  } s;
  FILE *stdio_file;
};

/* Define stdiobuf_vtable as a name for the virtual function table
   of the stdiobuf class. */
#ifndef stdiobuf_vtable
#ifdef __GNUC__
extern struct _IO_jump_t stdiobuf_vtable
  asm (_G_VTABLE_LABEL_PREFIX
#if _G_VTABLE_LABEL_HAS_LENGTH
       "8"
#endif
       "stdiobuf");
#else /* !__GNUC__ */
#if _G_VTABLE_LABEL_HAS_LENGTH
#define stdiobuf_vtable _G_VTABLE_LABEL_PREFIX_ID##8stdiobuf
#else
#define stdiobuf_vtable _G_VTABLE_LABEL_PREFIX_ID##stdiobuf
#endif
extern struct _IO_jump_t stdiobuf_vtable;
#endif /* !__GNUC__ */
#endif /* !stdiobuf_vtable */

#ifdef _IO_MTSAFE_IO
#define DEF_STDIOFILE(NAME, FD, FILE, FLAGS, CHAIN) \
  static _IO_lock_t _IO_stdfile_##FD##_lock = _IO_lock_initializer; \
  struct _IO_fake_stdiobuf NAME = \
      {{{ _IO_MAGIC+_IO_LINKED+_IO_IS_FILEBUF+_IO_UNBUFFERED+FLAGS, \
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, CHAIN, FD, \
	 0, 0, 0, 0, { 0 }, _IO_stdfile_##FD##_lock},\
         &stdiobuf_vtable}, FILE}
#else
#define DEF_STDIOFILE(NAME, FD, FILE, FLAGS, CHAIN) \
  struct _IO_fake_stdiobuf NAME = \
      {{{ _IO_MAGIC+_IO_LINKED+_IO_IS_FILEBUF+_IO_UNBUFFERED+FLAGS, \
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, CHAIN, FD}, \
         &stdiobuf_vtable}, FILE}
#endif

DEF_STDIOFILE(_IO_stdin_buf, 0, stdin, _IO_NO_WRITES, &_IO_stderr_.file);
DEF_STDIOFILE(_IO_stdout_buf, 1, stdout, _IO_NO_READS, &_IO_stdin_buf.s.file);
DEF_STDIOFILE(_IO_stderr_buf, 2, stderr, _IO_NO_READS, &_IO_stdout_buf.s.file);

_IO_FILE *_IO_list_all = &_IO_stderr_buf.s.file;
#endif  /* !_STDIO_USES_IOSTREAM */
#endif /* MACOSX */
