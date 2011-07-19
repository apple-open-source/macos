/************************************************************

Copyright 1998 by Thomas E. Dickey <dickey@clark.net>

                        All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
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
IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization.

********************************************************/


#ifndef _DM_ERROR_H_
# define _DM_ERROR_H_ 1

# if defined(GCC_PRINTFLIKE) && defined(__GNUC__)
#  define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
# else
#  define GCC_PRINTFLIKE(fmt,var) /*nothing*/
# endif

extern void Debug        (const char * fmt, ...) GCC_PRINTFLIKE(1,2);
extern void InitErrorLog (void);
extern void LogAppend    (const char * fmt, ...) GCC_PRINTFLIKE(1,2);
extern void LogError     (const char * fmt, ...) GCC_PRINTFLIKE(1,2);
extern void LogInfo      (const char * fmt, ...) GCC_PRINTFLIKE(1,2);
extern void LogOutOfMem  (const char * fmt, ...) GCC_PRINTFLIKE(1,2);
extern void LogPanic     (const char * fmt, ...) GCC_PRINTFLIKE(1,2);


#endif /* _DM_ERROR_H_ */
