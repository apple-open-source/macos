#ifndef HTMLLEX_H
#define HTMLLEX_H

#include <agxbuf.h>

extern void initHTMLlexer (char*, agxbuf*);
extern int htmllex ();
extern int htmllineno ();
extern int clearHTMLlexer ();
void htmlerror(const char *);

#endif
