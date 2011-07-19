/* $XConsortium: lncmd.h,v 10.8 94/04/17 20:44:03 rws Exp $ */
/*

Copyright (c) 1985  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
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
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/


/* lncmd.h - Command sequences DEC printers, in particular LN0x laser
   printers */

/*
#define LN_RIS             "\033c"  Obsolete; causes LN03+ problems
*/
#define LN_STR             "\033[!p"
#define LN_SSU             "\033[%d I"
#define LN_PUM_SET         "\033[11h"
#define LN_PFS             "\033[%s J"
#define LN_DECSLRM         "\033[%d;%ds"
#define LN_HPA             "\033[%d`"
#define LN_VPA             "\033[%dd"
#define LN_SIXEL_GRAPHICS  "\033P%d;%d;%dq"
#define LN_ST              "\033\\"
#define LN_DECOPM_SET      "\033[?52h"
#define LN_DECOPM_RESET    "\033[?52I"
#define LN_SGR             "\033[1%dm"
#define LN_PUM             "\033[11I"
#define LN_LNM             "\033[20h"
