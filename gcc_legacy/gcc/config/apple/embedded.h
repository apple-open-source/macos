/* This file contains defines needed when producing libraries for 
   an embedded target. An embedded target is defined as one which
   does not necessarily have I/O, C++ exceptions, or threads.  */
 
#if !defined(_EMBEDDED_H) && defined(APPLE_KERNEL_EXTENSION)
#define _EMBEDDED_H
 
#ifdef __cplusplus
extern "C" {
#endif
 
 /* Monitor error codes. When a runtime exception or error is
    encountered, the embedded monitor/RTOS is informed by a call
    to MONITOR_ERROR() with one of the error codes below.
    It is up to the target monitor to supply its own copy of 
    MONITOR_ERROR for the runtime to call.  */
  
#define MON_ERROR_RUN_INTERNAL	1	/* internal runtime error  */
#define MON_ERROR_PURE_VIRTUAL	2	/* pure virtual function called  */
#define MON_ERROR_NO_MEM	3	/* memory allocation failed  */

/* MONITOR_ERROR() must be supplied by the external implementation.
   It would typically be a subroutine in the embedded monitor.  */

/* Define the error handler and memory allocator for your embedded environment
   here.  */

#ifdef MACOSX

#define MONITOR_ERROR(code) panic(code)
#undef  MON_ERROR_RUN_INTERNAL
#define MON_ERROR_RUN_INTERNAL "Internal C++ runtime error"
#undef  MON_ERROR_PURE_VIRTUAL
#define MON_ERROR_PURE_VIRTUAL "C++ pure virtual function called"
#undef  MON_ERROR_NO_MEM
#define MON_ERROR_NO_MEM "C/C++ runtime memory allocation failed"

#define ABORT MONITOR_ERROR (MON_ERROR_RUN_INTERNAL)
#define TERMINATE MONITOR_ERROR (MON_ERROR_PURE_VIRTUAL)

/* These prototypes must be kept in sync with the declarations
   in the kernel headers. The types of the arguments here are 
   not the same as in kernel headers. They must be equivalent though.  */

extern void *kalloc (unsigned int size) ;
extern void kfree (void * data, unsigned int size) ;
extern void panic (const char *, ...) ;

static inline void *malloc (long unsigned int size)
{
  void *ptr = (void *) kalloc (size+4);
  if (ptr)
    {
      *(unsigned int *) ptr = size + 4 ;
      ptr = (void *) ((unsigned char *) ptr + 4) ;
    }
  return ptr;
}

static inline void free (void *ptr)
{
  if (ptr)
    {
      ptr = (void *) ((unsigned char *) ptr - 4);
      kfree (ptr, *(unsigned int *) ptr);
    }
}

#endif /* MACOSX */

/* Provide a definition of PRIVATE_EXTERN if the target doesn't define one.*/

#ifndef PRIVATE_EXTERN
#define PRIVATE_EXTERN
#endif


#ifdef __cplusplus
}
#endif

#endif /* _EMBEDDED_H */
