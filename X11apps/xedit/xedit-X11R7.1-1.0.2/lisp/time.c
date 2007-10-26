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

/* $XFree86: xc/programs/xedit/lisp/time.c,v 1.9tsi Exp $ */

#include "lisp/time.h"
#include "lisp/bytecode.h"

/*
 * Implementation
*/
LispObj *
Lisp_Time(LispBuiltin *builtin)
/*
 time form
 */
{
#ifndef __UNIXOS2__
    struct itimerval real, virt, prof;
    unsigned long count;
    long sec, usec;
    LispObj *result;
#define MONTH	60 * 60 * 31

    LispObj *form;

    form = ARGUMENT(0);

    real.it_value.tv_sec =
	virt.it_value.tv_sec =
	prof.it_value.tv_sec =
	real.it_interval.tv_sec =
	virt.it_interval.tv_sec =
	prof.it_interval.tv_sec = MONTH;
    real.it_value.tv_usec =
	virt.it_value.tv_usec =
	prof.it_value.tv_usec =
	real.it_interval.tv_usec =
	virt.it_interval.tv_usec =
	prof.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &real, NULL);
    setitimer(ITIMER_VIRTUAL, &virt, NULL);
    setitimer(ITIMER_PROF, &prof, NULL);

    getitimer(ITIMER_REAL, &real);
    getitimer(ITIMER_VIRTUAL, &virt);
    getitimer(ITIMER_PROF, &prof);

    lisp__data.gc.gctime = 0;
    lisp__data.gc.timebits = 1;

    count = lisp__data.gc.count;

#if 0
    form = CONS(form, NIL);
    COD = CONS(form, COD);
    result = LispExecuteBytecode(LispCompileForm(form));
#else
    result = EVAL(form);
#endif

    getitimer(ITIMER_REAL, &real);
    getitimer(ITIMER_VIRTUAL, &virt);
    getitimer(ITIMER_PROF, &prof);

    sec = real.it_interval.tv_sec - real.it_value.tv_sec;
    usec = real.it_interval.tv_usec - real.it_value.tv_usec;
    if (usec < 0) {
	--sec;
	usec += 1000000;
    }
    LispMessage("Real time   : %g sec", sec + usec / 1000000.0);

    sec = virt.it_interval.tv_sec - virt.it_value.tv_sec;
    usec = virt.it_interval.tv_usec - virt.it_value.tv_usec + 10000;
    if (usec < 0) {
	--sec;
	usec += 1000000;
    }
    LispMessage("Virtual time: %g sec", sec + usec / 1000000.0);

    sec = prof.it_interval.tv_sec - prof.it_value.tv_sec;
    usec = prof.it_interval.tv_usec - prof.it_value.tv_usec + 10000;
    if (usec < 0) {
	--sec;
	usec += 1000000;
    }
    LispMessage("Profile time: %g sec", sec + usec / 1000000.0);

    real.it_value.tv_sec =
	virt.it_value.tv_sec =
	prof.it_value.tv_sec =
	real.it_interval.tv_sec =
	virt.it_interval.tv_sec =
	prof.it_interval.tv_sec =
	real.it_value.tv_usec =
	virt.it_value.tv_usec =
	prof.it_value.tv_usec =
	real.it_interval.tv_usec =
	virt.it_interval.tv_usec =
	prof.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &real, NULL);
    setitimer(ITIMER_VIRTUAL, &virt, NULL);
    setitimer(ITIMER_PROF, &prof, NULL);

    LispMessage("GC: %ld times, %g sec",
		lisp__data.gc.count - count, lisp__data.gc.gctime / 1000000.0);
    lisp__data.gc.timebits = 0;

    return (result);
#else
    LispMessage("\"Lisp_Time\" not implemented under OS/2");
    return (NIL);
#endif
}
