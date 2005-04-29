/* $XFree86: xc/programs/luit/sys.h,v 1.3 2003/10/24 20:38:12 tsi Exp $ */
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

int waitForOutput(int fd);
int waitForInput(int fd1, int fd2);
int setWindowSize(int sfd, int dfd);
int installHandler(int signum, void (*handler)(int));
int copyTermios(int sfd, int dfd);
int saveTermios(void);
int restoreTermios(void);
int setRawTermios(void);
char *my_basename(char *path);
int allocatePty(int *pty_return, char **line_return);
int openTty(char *line);
int droppriv(void);
