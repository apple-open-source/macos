/*
 * This is derived from i386-xdep.c
 */

/* Intel 386 stuff.
   Copyright (C) 1988, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"

#ifdef GDB414

/* This is broken for cross debugging.  Possible solutions are:

1.  Don't worry about whether the thing compiles for cross-debugging.
Go ahead and call them from i386-tdep.c.
1a.  Same thing but use some macros in xm-i386.h so that gdb will
compile for cross-debugging but just give an error (or some such behavior)
when you attempt to convert floats.

2.  Write a portable (in the sense of running on any machine; it would
always be for i387 floating-point formats) extended<->double converter
(which just deals with the values as arrays of char).

3.  Assume the host machine has *some* IEEE chip.  However, IEEE does
not standardize formats for extended floats (387 is 10 bytes, 68881 is
12 bytes), so this won't work.  */

void 
i387_to_double (from, to)
     char *from;
     char *to;
{
  long *lp;
  /* push extended mode on 387 stack, then pop in double mode
   *
   * first, set exception masks so no error is generated -
   * number will be rounded to inf or 0, if necessary 
   */
  asm ("pushl %eax"); 		/* grab a stack slot */
  asm ("fstcw (%esp)");		/* get 387 control word */
  asm ("movl (%esp),%eax");	/* save old value */
  asm ("orl $0x3f,%eax");		/* mask all exceptions */
  asm ("pushl %eax");
  asm ("fldcw (%esp)");		/* load new value into 387 */
  
  asm ("movl 8(%ebp),%eax");
  asm ("fldt (%eax)");		/* push extended number on 387 stack */
  asm ("fwait");
  asm ("movl 12(%ebp),%eax");
  asm ("fstpl (%eax)");		/* pop double */
  asm ("fwait");
  
  asm ("popl %eax");		/* flush modified control word */
  asm ("fnclex");			/* clear exceptions */
  asm ("fldcw (%esp)");		/* restore original control word */
  asm ("popl %eax");		/* flush saved copy */
}

void 
double_to_i387 (from, to)
     char *from;
     char *to;
{
  /* push double mode on 387 stack, then pop in extended mode
   * no errors are possible because every 64-bit pattern
   * can be converted to an extended
   */
  asm ("movl 8(%ebp),%eax");
  asm ("fldl (%eax)");
  asm ("fwait");
  asm ("movl 12(%ebp),%eax");
  asm ("fstpt (%eax)");
  asm ("fwait");
}

#endif /* GDB414 */

static void 
print_387_control_word (control)
    fp_control_t *control;
{
  printf ("control 0x%04x: ", *((unsigned short *) control));
  printf ("compute to ");
  switch (control->pc) {
    case FP_PREC_24B:	printf ("24 bits; "); break;
    case FP_PREC_53B:	printf ("53 bits; "); break;
    case FP_PREC_64B:	printf ("64 bits; "); break;
    default:		printf ("(bad); "); break;
  }

  printf ("round ");
  switch (control->rc) {
    case FP_RND_NEAR:	printf ("NEAREST; "); break;
    case FP_RND_DOWN:	printf ("DOWN; "); break;
    case FP_RND_UP:	printf ("UP; "); break;
    case FP_CHOP:	printf ("CHOP; "); break;
  }
  printf ("mask:");
  if (control->invalid)	printf (" INVALID");
  if (control->denorm)	printf (" DENORM");
  if (control->zdiv)	printf (" DIVZ");
  if (control->ovrfl)	printf (" OVERF");
  if (control->undfl)	printf (" UNDERF");
  if (control->precis)	printf (" LOS");
  printf (";");

  printf ("\n");
}

static void 
print_387_status_word (status)
     fp_status_t *status;
{
  printf ("status 0x%04x: ", *((unsigned short *)status));

  printf ("exceptions:");
  if (status->invalid)	printf (" INVALID");
  if (status->denorm)	printf (" DENORM");
  if (status->zdiv)	printf (" DIVZ");
  if (status->ovrfl)	printf (" OVERF");
  if (status->undfl)	printf (" UNDERF");
  if (status->precis)	printf (" LOS");
  if (status->stkflt)	printf (" FPSTACK");
  if (status->errsumm)	printf (" ERRSUMM");
  printf ("; ");

  printf ("flags: %d%d%d%d; ",
	  status->c0, status->c1, status->c2, status->c3);
  
  printf ("top %d\n", status->tos);
}

static void 
print_387_status (fp)
     i386_thread_fpstate_t *fp;
{
  int i;
  int top;
  int fpreg;
  unsigned short tag;
  unsigned char *p;

  print_387_status_word  (&fp->environ.status);
  print_387_control_word (&fp->environ.control);
  printf ("last exception: ");
  printf ("opcode 0x%x; ", fp->environ.opcode);
  printf ("pc 0x%x:0x%x; ",
	  *((unsigned short *) &fp->environ.cs), fp->environ.ip);
  printf ("operand 0x%x:0x%x\n",
	  *((unsigned short *) &fp->environ.ds), fp->environ.dp);

  top = fp->environ.status.tos;
  tag = *((unsigned short *)   &fp->environ.tag);
  
  printf ("regno  tag  msb              lsb  value\n");
  for (fpreg = 7; fpreg >= 0; fpreg--) {
    double val;

    printf ("%s %d: ", fpreg == top ? "=>" : "  ", fpreg);

    switch ((tag >> (fpreg * 2)) & 3) {
      case 0: printf ("valid "); break;
      case 1: printf ("zero  "); break;
      case 2: printf ("trap  "); break;
      case 3: printf ("empty "); break;
    }
    p = (char *) &(fp->stack.ST[fpreg]);
    for (i = 9; i >= 0; i--)
      printf ("%02x", p[i]);

    i387_to_double ((char *)&fp->stack.ST[fpreg], (char *)&val);
    printf ("  %g\n", val);
  }
}

void 
i386_float_info ()
{
  i386_thread_fpstate_t fpstate;
  unsigned int fpstate_size = i386_THREAD_FPSTATE_COUNT;
  thread_t current_thread = next_current_thread ();

  if (current_thread) {
    mach_check_ret(thread_get_state(current_thread, i386_THREAD_FPSTATE, \
			       (thread_state_t)&fpstate, &fpstate_size),
	      "thread_get_state", "i386_float_info");
    print_387_status (&fpstate);
  }
  return;
}
