/* OpenStep/Rhapsody/MacOSX thread routines for GDB, the GNU debugger.
   Copyright 1997-1999 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "nextstep-nat-inferior.h"
#include "nextstep-nat-mutils.h"

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "gdbthread.h"

#include <stdio.h>
#include <sys/param.h>
#include <sys/dir.h>

#include <mach/machine/thread_status.h>

#if defined (USE_PTHREADS)

void gdb_pthread_kill (pthread_t pthread)
{
  mach_port_t mthread;
  kern_return_t kret;
  int ret;

  mthread = pthread_mach_thread_np (pthread);

  kret = thread_suspend (mthread);
  MACH_CHECK_ERROR (kret);

  ret = pthread_cancel (pthread);
  if (ret != 0) {
    warning ("Unable to cancel thread: %s (%d)", strerror (errno), errno);
    thread_terminate (mthread);
  }

  kret = thread_abort (mthread);
  MACH_CHECK_ERROR (kret);

  kret = thread_resume (mthread);
  MACH_CHECK_ERROR (kret);

  ret = pthread_join (pthread, NULL);
  if (ret != 0) {
    warning ("Unable to join to canceled thread: %s (%d)", strerror (errno), errno);
  }
}

pthread_t gdb_pthread_fork (pthread_fn_t function, void *arg)
{
  int result;
  pthread_t pthread = NULL;
  pthread_attr_t attr;

  result = pthread_attr_init (&attr);
  if (result != 0) {
    error ("Unable to initialize thread attributes: %s (%d)", strerror (errno), errno);
  }
    
  result = pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);
  if (result != 0) {
    error ("Unable to initialize thread attributes: %s (%d)", strerror (errno), errno);
  }

  result = pthread_create (&pthread, &attr, function, arg);
  if (result != 0)  {
    error ("Unable to create thread: %s (%d)", strerror (errno), errno);
  }
  
  result = pthread_attr_destroy (&attr);
  if (result != 0) {
    warning ("Unable to deallocate thread attributes: %s (%d)", strerror (errno), errno);
  }

  return pthread;
}

#else /* ! USE_PTHREADS */

#if defined (TARGET_I386)

#define THREAD_STRUCT i386_thread_state_t
#define THREAD_STATE i386_THREAD_STATE
#define THREAD_STRUCT i386_thread_state_t
#define THREAD_COUNT i386_THREAD_STATE_COUNT

#elif defined (TARGET_POWERPC)

#define THREAD_STRUCT struct ppc_thread_state
#define THREAD_STATE PPC_THREAD_STATE
#define THREAD_STRUCT struct ppc_thread_state
#define THREAD_COUNT PPC_THREAD_STATE_COUNT

#else
#error unknown architecture
#endif

void gdb_cthread_kill (cthread_t cthread)
{
  mach_port_t mthread;
  THREAD_STRUCT state;
  mach_msg_type_number_t state_count;
  kern_return_t	ret;

  mthread = cthread_thread (cthread);

  ret = thread_suspend (mthread);
  MACH_CHECK_ERROR (ret);

  ret = thread_abort (mthread);
  MACH_CHECK_ERROR (ret);

  state_count = THREAD_COUNT;
  ret = thread_get_state
    (mthread, THREAD_STATE, (thread_state_t) &state, &state_count);
  MACH_CHECK_ERROR (ret);

#if defined (TARGET_I386)
#warning gdb_cthread_kill unable to set exit value on ia86 architecture
  state.eip = (unsigned int) &cthread_exit;
#elif defined (TARGET_POWERPC)
  state.srr0 = (unsigned int) &cthread_exit;
  state.r3 = 0;
#else
#error unknown architecture
#endif

  ret = thread_set_state 
    (mthread, THREAD_STATE, (thread_state_t) &state, THREAD_COUNT);
  MACH_CHECK_ERROR (ret);

  ret = thread_resume (mthread);
  MACH_CHECK_ERROR (ret);
 
  cthread_join (cthread);
}

#endif /* USE_PTHREADS */

