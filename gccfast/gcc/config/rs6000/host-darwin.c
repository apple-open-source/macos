/* Darwin/powerpc host-specific hook definitions.
   Copyright (C) 2003 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 2, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to the
   Free Software Foundation, 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include <signal.h>
#include <sys/mman.h>
#include "hosthooks.h"
#include "hosthooks-def.h"
#include "toplev.h"
#include "diagnostic.h"

static void segv_crash_handler (int);
static void segv_handler (int, siginfo_t *, void *);
static void darwin_rs6000_extra_signals (void);

/* This is from the Darwin /usr/include/mach/ppc/_types.h.  */
struct gcc_ppc_thread_state
{
	unsigned int srr0;      /* Instruction address register (PC) */
	unsigned int srr1;	/* Machine state register (supervisor) */
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int r12;
	unsigned int r13;
	unsigned int r14;
	unsigned int r15;
	unsigned int r16;
	unsigned int r17;
	unsigned int r18;
	unsigned int r19;
	unsigned int r20;
	unsigned int r21;
	unsigned int r22;
	unsigned int r23;
	unsigned int r24;
	unsigned int r25;
	unsigned int r26;
	unsigned int r27;
	unsigned int r28;
	unsigned int r29;
	unsigned int r30;
	unsigned int r31;

	unsigned int cr;        /* Condition register */
	unsigned int xer;	/* User's integer exception register */
	unsigned int lr;	/* Link register */
	unsigned int ctr;	/* Count register */
	unsigned int mq;	/* MQ register (601 only) */

	unsigned int vrsave;	/* Vector Save Register */
};

struct gcc_ppc_float_state
{
	double  fpregs[32];

	unsigned int fpscr_pad; /* fpscr is 64 bits, 32 bits of rubbish */
	unsigned int fpscr;	/* floating point status register */
};

struct gcc_ppc_vector_state
{
	unsigned long	save_vr[32][4];
	unsigned long	save_vscr[4];
	unsigned int	save_pad5[4];
	unsigned int	save_vrvalid;			/* VRs that have been saved */
	unsigned int	save_pad6[7];
};

struct gcc_ppc_exception_state
{
	unsigned long dar;			/* Fault registers for coredump */
	unsigned long dsisr;
	unsigned long exception;	/* number of powerpc exception taken */
	unsigned long pad0;			/* align to 16 bytes */
	unsigned long pad1[4];		/* space in PCB "just in case" */
};

/* This is from the Darwin /usr/include/ppc/ucontext.h.  */
struct gcc_mcontext {
	struct gcc_ppc_exception_state	es;
	struct gcc_ppc_thread_state		ss;
	struct gcc_ppc_float_state		fs;
	struct gcc_ppc_vector_state		vs;
};

#undef HOST_HOOKS_EXTRA_SIGNALS
#define HOST_HOOKS_EXTRA_SIGNALS darwin_rs6000_extra_signals

/* On Darwin/powerpc, hitting the stack limit turns into a SIGSEGV.
   This code detects the difference between hitting the stack limit and
   a true wild pointer dereference by looking at the instruction that
   faulted; only a few kinds of instruction are used to access below
   the previous bottom of the stack.  */

static void
segv_crash_handler (int sig ATTRIBUTE_UNUSED)
{
  internal_error ("Segmentation Fault (code)");
}

static void
segv_handler (int sig ATTRIBUTE_UNUSED,
	      siginfo_t *sip ATTRIBUTE_UNUSED,
	      void *scp)
{
  ucontext_t *uc = (ucontext_t *)scp;
  struct gcc_mcontext *mc;
  unsigned faulting_insn;

  /* The fault might have happened when trying to run some instruction, in
     which case the next lines will segfault _again_.  Handle this case.  */
  signal (SIGSEGV, segv_crash_handler);
  
  mc = (struct gcc_mcontext *)uc->uc_mcontext;
  faulting_insn = *(unsigned *)mc->ss.srr0;

  /* Note that this only has to work for GCC, so we don't have to deal
     with all the possible cases (GCC has no AltiVec code, for
     instance).  It's complicated because Darwin allows stores to
     below the stack pointer, and the prologue code takes advantage of
     this.  */

  if ((faulting_insn & 0xFFFF8000) == 0x94218000  /* stwu %r1, -xxx(%r1) */
      || (faulting_insn & 0xFFFF03FF) == 0x7C21016E /* stwux %r1, xxx, %r1 */
      || (faulting_insn & 0xFC1F8000) == 0x90018000 /* stw xxx, -yyy(%r1) */
      || (faulting_insn & 0xFC1F8000) == 0xD8018000 /* stfd xxx, -yyy(%r1) */
      || (faulting_insn & 0xFC1F8000) == 0xBC018000 /* stmw xxx, -yyy(%r1) */)
    {
      char *shell_name;
      
      fnotice (stderr, "Out of stack space.\n");
      shell_name = getenv ("SHELL");
      if (shell_name != NULL)
	shell_name = strrchr (shell_name, '/');
      if (shell_name != NULL)
	{
	  static const char * shell_commands[][2] = {
	    { "sh", "ulimit -S -s unlimited" },
	    { "bash", "ulimit -S -s unlimited" },
	    { "tcsh", "limit stacksize unlimited" },
	    { "csh", "limit stacksize unlimited" },
	    /* zsh doesn't have "unlimited", this will work under the
	       default configuration.  */
	    { "zsh", "limit stacksize 32m" }
	  };
	  size_t i;
	  
	  for (i = 0; i < ARRAY_SIZE (shell_commands); i++)
	    if (strcmp (shell_commands[i][0], shell_name + 1) == 0)
	      {
		fnotice (stderr, 
			 "Try running `%s' in the shell to raise its limit.\n",
			 shell_commands[i][1]);
	      }
	}
      
      if (global_dc->abort_on_error)
	abort ();

      exit (FATAL_EXIT_CODE);
    }

  fprintf (stderr, "[address=%08lx pc=%08x]\n", mc->es.dar, mc->ss.srr0);
  internal_error ("Segmentation Fault");
  exit (FATAL_EXIT_CODE);
}

static void
darwin_rs6000_extra_signals (void)
{
  struct sigaction sact;
  stack_t sigstk;

  sigstk.ss_sp = xmalloc (SIGSTKSZ);
  sigstk.ss_size = SIGSTKSZ;
  sigstk.ss_flags = 0;
  if (sigaltstack (&sigstk, NULL) < 0)
    fatal_error ("While setting up signal stack: %m");

  sigemptyset(&sact.sa_mask);
  sact.sa_flags = SA_ONSTACK | SA_SIGINFO;
  sact.sa_sigaction = segv_handler;
  if (sigaction (SIGSEGV, &sact, 0) < 0) 
    fatal_error ("While setting up signal handler: %m");
}

static void * darwin_rs6000_gt_pch_get_address (size_t);
static bool darwin_rs6000_gt_pch_use_address (void *, size_t);

#undef HOST_HOOKS_GT_PCH_GET_ADDRESS
#define HOST_HOOKS_GT_PCH_GET_ADDRESS darwin_rs6000_gt_pch_get_address
#undef HOST_HOOKS_GT_PCH_USE_ADDRESS
#define HOST_HOOKS_GT_PCH_USE_ADDRESS darwin_rs6000_gt_pch_use_address


/* Yes, this is really supposed to work.  */
static char pch_address_space[1024*1024*1024] __attribute__((aligned (4096)));

static void *
darwin_rs6000_gt_pch_get_address (size_t sz)
{
  if (sz <= sizeof (pch_address_space))
    return pch_address_space;
  else
    return NULL;
}

static bool
darwin_rs6000_gt_pch_use_address (void *addr, size_t sz)
{
  const size_t pagesize = getpagesize();
  bool result;

  if ((size_t)pch_address_space % pagesize != 0
      || sizeof (pch_address_space) % pagesize != 0)
    abort ();
  
  result = (addr == pch_address_space && sz <= sizeof (pch_address_space));
  if (! result)
    sz = 0;

  /* Round the size to a whole page size.  Normally this is a no-op.  */
  sz = (sz + pagesize - 1) / pagesize * pagesize;

  if (munmap (pch_address_space + sz, sizeof (pch_address_space) - sz) != 0)
    fatal_error ("couldn't unmap pch_address_space: %m\n");

  return result;
}

const struct host_hooks host_hooks = HOST_HOOKS_INITIALIZER;
