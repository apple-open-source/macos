/*
Copyright (c) 2001 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef LUIT_PARSER_H
#define LUIT_PARSER_H 1

#ifndef LOCALE_ALIAS_FILE
#define LOCALE_ALIAS_FILE "/usr/X11R6/lib/X11/locale/locale.alias"
#endif

#define MAX_KEYWORD_LENGTH 1024

#define TOK_ERROR (-2)
#define TOK_EOF (-1)
#define TOK_EOL 0
#define TOK_EQUALS 1
#define TOK_KEYWORD 2

char *resolveLocale(const char *locale);

#endif /* LUIT_PARSER_H */
