#ifndef __GDB_MACOSX_NAT_THREADS_H__
#define __GDB_MACOSX_NAT_THREADS_H__

#include <pthread.h>

typedef void *(*pthread_fn_t) (void *arg);

void gdb_pthread_kill (pthread_t pthread);
pthread_t gdb_pthread_fork (pthread_fn_t function, void *arg);

#define gdb_thread_exit pthread_exit
#define gdb_thread_fork gdb_pthread_fork
#define gdb_thread_kill gdb_pthread_kill

typedef pthread_t gdb_thread_t;
typedef pthread_fn_t gdb_thread_fn_t;

typedef pthread_mutex_t gdb_mutex_t;
typedef pthread_cond_t gdb_cond_t;

#define gdb_mutex_init(m, i) pthread_mutex_init(m, i)
#define gdb_mutex_lock(m) pthread_mutex_lock(m)
#define gdb_mutex_unlock(m) pthread_mutex_unlock(m)
#define gdb_mutex_destroy(m) pthread_mutex_destroy(m)
#define gdb_cond_init(c, i) pthread_cond_init(c, i)
#define gdb_cond_destroy(c) pthread_cond_destroy(c)
#define gdb_cond_wait(c, m) pthread_cond_wait(c, m)
#define gdb_cond_signal(c) pthread_cond_signal(c)

#endif /* __GDB_MACOSX_NAT_THREADS_H__ */
