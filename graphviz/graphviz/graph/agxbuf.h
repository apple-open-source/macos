/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#ifndef         AGXBUF_H
#define         AGXBUF_H

/* Extensible buffer:
 *  Malloc'ed memory is never released until agxbfree is called.
 */
typedef struct {
    unsigned char*  buf;   /* start of buffer */
    unsigned char*  ptr;   /* next place to write */
    unsigned char*  eptr;  /* end of buffer */
    int             dyna;  /* true if buffer is malloc'ed */
} agxbuf;

/* agxbinit:
 * Initializes new agxbuf; caller provides memory.
 * Assume if init is non-null, hint = sizeof(init[])
 */
extern void agxbinit (agxbuf* xb, unsigned int hint, unsigned char* init);

/* agxbput_n:
 * Append string s of length n into xb
 */
extern int agxbput_n (agxbuf* xb, char* s, unsigned int n);

/* agxbput:
 * Append string s into xb
 */
extern int agxbput (agxbuf* xb, char* s);

/* agxbfree:
 * Free any malloced resources.
 */
extern void agxbfree (agxbuf* xb);

/* agxbpop:
 * Removes last character added, if any.
 */
extern int agxbpop (agxbuf* xb);

/* agxbmore:
 * Expand buffer to hold at least ssz more bytes.
 */
extern int agxbmore (agxbuf* xb, int unsigned ssz);

/* agxbputc:
 * Add character to buffer.
 *  int agxbputc(agxbuf*, char)
 */
#define agxbputc(X,C) ((((X)->ptr >= (X)->eptr) ? agxbmore(X,1) : 0), \
          (int)(*(X)->ptr++ = ((unsigned char)C)))

/* agxbuse:
 * Null-terminates buffer; resets and returns pointer to data;
 *  char* agxbuse(agxbuf* xb)
 */
#define agxbuse(X) (agxbputc(X,'\0'),(char*)((X)->ptr = (X)->buf))

/* agxbstart:
 * Return pointer to beginning of buffer.
 *  char* agxbstart(agxbuf* xb)
 */
#define agxbstart(X) ((char*)((X)->buf))

/* agxblen:
 * Return number of characters currently stored.
 *  int agxblen(agxbuf* xb)
 */
#define agxblen(X) (((X)->ptr)-((X)->buf))

/* agxbclear:
 * Resets pointer to data;
 *  void agxbclear(agxbuf* xb)
 */
#define agxbclear(X) ((void)((X)->ptr = (X)->buf))

/* agxbnext:
 * Next position for writing.
 *  char* agxbnext(agxbuf* xb)
 */
#define agxbnext(X) ((char*)((X)->ptr))

#endif
