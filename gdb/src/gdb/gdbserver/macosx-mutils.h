#ifndef __GDBSERVER_MACOSX_MUTILS_H__
#define __GDBSERVER_MACOSX_MUTILS_H__

#include <mach/port.h>

unsigned int child_get_pagesize (void);

int mach_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len, int write, 
		      task_t task);

int macosx_thread_valid (task_t task, thread_t thread);

#endif /* __GDBSERVER_MACOSX_MUTILS_H__ */
