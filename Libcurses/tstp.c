/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1981, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "curses.h"

/*
 * stop_signal_handler --
 *	Handle stop signals.
 */
void
__stop_signal_handler(signo)
	int signo;
{
	struct termios save;
	sigset_t oset, set;

	/* Get the current terminal state (which the user may have changed). */
	if (tcgetattr(STDIN_FILENO, &save))
		return;

	/*
	 * Block window change and timer signals.  The latter is because
	 * applications use timers to decide when to repaint the screen.
	 */
	(void)sigemptyset(&set);
	(void)sigaddset(&set, SIGALRM);
	(void)sigaddset(&set, SIGWINCH);
	(void)sigprocmask(SIG_BLOCK, &set, &oset);
	
	/*
	 * End the window, which also resets the terminal state to the
	 * original modes.
	 */
	endwin();

	/* Unblock SIGTSTP. */
	(void)sigemptyset(&set);
	(void)sigaddset(&set, SIGTSTP);
	(void)sigprocmask(SIG_UNBLOCK, &set, NULL);

	/* Stop ourselves. */
	__restore_stophandler();
	(void)kill(0, SIGTSTP);

	/* Time passes ... */

	/* Reset the curses SIGTSTP signal handler. */
	__set_stophandler();

	/* save the new "default" terminal state */
	(void)tcgetattr(STDIN_FILENO, &__orig_termios);

	/* Reset the terminal state to the mode just before we stopped. */
	(void)tcsetattr(STDIN_FILENO, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, &save);

	/* Restart the screen. */
	__startwin();

	/* Repaint the screen. */
	wrefresh(curscr);

	/* Reset the signals. */
	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
}

static void (*otstpfn)() = SIG_DFL;

/*
 * Set the TSTP handler.
 */
void
__set_stophandler()
{
#if defined(__DYNAMIC__)
    _dyld_lookup_and_bind_fully("___set_stophandler", NULL, NULL);
    otstpfn = _signal_nobind(SIGTSTP, __stop_signal_handler);
#else
    otstpfn = signal(SIGTSTP, __stop_signal_handler);
#endif
}

/*
 * Restore the TSTP handler.
 */
void
__restore_stophandler()
{
	(void)signal(SIGTSTP, otstpfn);
}
