/* This is part of libio/iostream, providing -*- C++ -*- input/output.
Copyright (C) 1993 Free Software Foundation

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

/* Written by Per Bothner (bothner@cygnus.com). */

#include "libioP.h"
#include "streambuf.h"
#include <stdio.h>

// The ANSI draft requires that operations on cin/cout/cerr can be
// mixed with operations on stdin/stdout/stderr on a character by
// character basis.  This normally requires that the streambuf's
// used by cin/cout/cerr be stdiostreams.  However, if the stdio
// implementation is the one that is built using this library,
// then we don't need to, since in that case stdin/stdout/stderr
// are identical to _IO_stdin/_IO_stdout/_IO_stderr.

#include "libio.h"

#ifdef _STDIO_USES_IOSTREAM
#define CIN_SBUF _IO_stdin_
#define COUT_SBUF _IO_stdout_
#define CERR_SBUF _IO_stderr_
static int use_stdiobuf = 0;
#else
#define CIN_SBUF _IO_stdin_buf
#define COUT_SBUF _IO_stdout_buf
#define CERR_SBUF _IO_stderr_buf
static int use_stdiobuf = 1;
#endif

#define cin CIN
#define cout COUT
#define cerr CERR
#define clog CLOG
#include "iostream.h"
#undef cin
#undef cout
#undef cerr
#undef clog

#ifdef MACOSX
#include "keymgr.h"

// The _IO_stdxxx variables are initialized here because they need to appear in 
// the same file as cin, cout, and cerr so that initialization is guaranteed to 
// take place in the correct order. There is a private API between stdstrbufs.cc
// and here.
 
extern "C" {
extern _IO_FILE_plus *_IO_init_stdin(), *_IO_init_stdout(), *_IO_init_stderr() ;
struct _IO_FILE_plus *_IO_stdin=_IO_init_stdin(), *_IO_stdout=_IO_init_stdout(), *_IO_stderr=_IO_init_stderr();
}
#endif

#ifdef __GNUG__
#define PAD 0 /* g++ allows 0-length arrays. */
#else
#define PAD 1
#endif
struct _fake_istream {
    struct myfields {
#ifdef __GNUC__
	_ios_fields *vb; /* pointer to virtual base class ios */
	_IO_ssize_t _gcount;
#else
	/* This is supposedly correct for cfront. */
	_IO_ssize_t _gcount;
	void *vptr;
	_ios_fields *vb; /* pointer to virtual base class ios */
#endif
    } mine;
    _ios_fields base;
    char filler[sizeof(struct istream)-sizeof(struct _ios_fields)+PAD];
};
struct _fake_ostream {
    struct myfields {
#ifndef __GNUC__
	void *vptr;
#endif
	_ios_fields *vb; /* pointer to virtual base class ios */
    } mine;
    _ios_fields base;
    char filler[sizeof(struct ostream)-sizeof(struct _ios_fields)+PAD];
};


#ifdef _IO_NEW_STREAMS
#define STD_STR(SBUF, TIE, EXTRA_FLAGS) \
 (streambuf*)&SBUF, TIE, 0, ios::skipws|ios::dec|EXTRA_FLAGS, ' ',0,0,6
#else
#define STD_STR(SBUF, TIE, EXTRA_FLAGS) \
 (streambuf*)&SBUF, TIE, 0, ios::dont_close|ios::dec|ios::skipws|EXTRA_FLAGS, ' ',0,0,6
#endif

#ifdef __GNUC__
#define OSTREAM_DEF(NAME, SBUF, TIE, EXTRA_FLAGS, ASM) \
  _fake_ostream NAME ASM = { {&NAME.base}, {STD_STR(SBUF, TIE, EXTRA_FLAGS) }};
#define ISTREAM_DEF(NAME, SBUF, TIE, EXTRA_FLAGS) \
  _fake_istream NAME = { {&NAME.base}, {STD_STR(SBUF, TIE, EXTRA_FLAGS) }};
#else
#define OSTREAM_DEF(NAME, SBUF, TIE, EXTRA_FLAGS) \
  _fake_ostream NAME = { {0, &NAME.base}, {STD_STR(SBUF, TIE, EXTRA_FLAGS) }};
#define ISTREAM_DEF(NAME, SBUF, TIE, EXTRA_FLAGS) \
  _fake_istream NAME = {{0, 0, &NAME.base}, {STD_STR(SBUF, TIE, EXTRA_FLAGS)}};
#endif

#ifdef MACOSX

_fake_ostream cout, cerr ;
_fake_istream cin ;

extern "C" {

// In the case of MACOSX we do the standard stream initialization (cin, cout,
// cerr) dynamically.  Why?  Because of module initialization routines.
// Since user module initialization routines can potentially run before the C++
// initialization routines for the libraries, they could potentially write to
// streams too early.  This routine is part of a "initialize on demand" scheme.
// So if streams are used before the library initialization can occur,
// this code initializes std streams right in the middle of their use.

void _dynamic_stdstream_init() {
	cout.mine.vb = &cout.base ;
	cerr.mine.vb = &cerr.base ;
	cin.mine.vb = &cin.base ;
	
	cout.base._strbuf = (streambuf *)&COUT_SBUF ;
	cerr.base._strbuf = (streambuf *)&CERR_SBUF ;
	cin.base._strbuf = (streambuf *)&CIN_SBUF ;
	
	cin.base._flags = 	ios::dec|ios::skipws ;
	cout.base._flags = 	ios::dec|ios::skipws ;
	cerr.base._flags = 	ios::dec|ios::skipws|ios::unitbuf ;
	
	cin.base._fill = ' ' ;
	cout.base._fill = ' ' ;
	cerr.base._fill = ' ' ;
	
	cin.base._exceptions = 0 ;
	cout.base._exceptions = 0 ;
	cerr.base._exceptions = 0 ;
	
	cin.base._state = 0 ;
	cout.base._state = 0 ;
	cerr.base._state = 0 ;
	
	cin.base._precision = 6 ;
	cout.base._precision = 6 ;
	cerr.base._precision = 6 ;
	
	cin.base._tie = (ostream*)&cout ;
	cout.base._tie = NULL ;
	cerr.base._tie = (ostream*)&cout ;
	}
} /*extern "C"*/

#else /* !MACOSX */

OSTREAM_DEF(cout, COUT_SBUF, NULL, 0, )
OSTREAM_DEF(cerr, CERR_SBUF,(ostream*)&cout, ios::unitbuf, )
ISTREAM_DEF(cin, CIN_SBUF,  (ostream*)&cout, 0)

#endif /* MACOSX */

/* Only for (partial) compatibility with AT&T's library. */
#if _G_CLOG_CONFLICT
OSTREAM_DEF(clog, CERR_SBUF, (ostream*)&cout, 0, __asm__ ("__IO_clog"))
#else
OSTREAM_DEF(clog, CERR_SBUF, (ostream*)&cout, 0, )
#endif

// Switches between using _IO_std{in,out,err} and __std{in,out,err}_buf
// for standard streams.  This does not normally need to be called
// explicitly, but is provided for AT&T compatibility.

int ios::sync_with_stdio(int new_state)
{
#ifdef MACOSX
  int old_state = ((_io_mode_bits & _IO_OSX_SYNC_STDIO) == _IO_OSX_SYNC_STDIO);
  _io_mode_bits =
    (unsigned int) _keymgr_get_and_lock_processwide_ptr (KEYMGR_IO_MODE_BITS);

  _io_mode_bits = new_state ? (_io_mode_bits |  _IO_OSX_SYNC_STDIO)
			    : (_io_mode_bits & ~_IO_OSX_SYNC_STDIO);

  _keymgr_set_and_unlock_processwide_ptr (KEYMGR_IO_MODE_BITS,
					  (void *) _io_mode_bits);

  return old_state;
#else /* !MACOSX */
#ifdef _STDIO_USES_IOSTREAM
    // It is always synced.
    return 0;
#else
    if (new_state == use_stdiobuf) // The usual case now.
	return use_stdiobuf;
    if (new_state) {
	cin.base._strbuf = (streambuf*)&_IO_stdin_buf;
	cout.base._strbuf = (streambuf*)&_IO_stdout_buf;
	cerr.base._strbuf = (streambuf*)&_IO_stderr_buf;
	clog.base._strbuf = (streambuf*)&_IO_stderr_buf;
    } else {
	cin.base._strbuf = (streambuf*)_IO_stdin;
	cout.base._strbuf = (streambuf*)_IO_stdout;
	cerr.base._strbuf = (streambuf*)_IO_stderr;
	clog.base._strbuf = (streambuf*)_IO_stderr;
    }
    int old_state = use_stdiobuf;
    use_stdiobuf = new_state;
    return old_state;
#endif
#endif /* MACOSX */
}
