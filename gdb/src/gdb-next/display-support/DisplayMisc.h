#include <Foundation/Foundation.h>

/* pool used by Foundation objects */
extern NSAutoreleasePool *pool;

/* number of times thru window_hook; */
extern int pool_num_times;

/* release pool after this many times thru a hook */
#define POOL_RELEASE_MAX_TIMES	(50)

extern GdbManager *gdbManager;

extern GdbManager *make_gui_gdb_manager ();
extern GdbManager *make_view_gdb_manager ();

void fork_and_start_debugger_controller (GdbManager *);

