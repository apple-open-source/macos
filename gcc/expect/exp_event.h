/* exp_event.h - event definitions */

int exp_get_next_event _ANSI_ARGS_((Tcl_Interp *,int *, int, int *, int, int));
int exp_get_next_event_info _ANSI_ARGS_((Tcl_Interp *, int, int));
int exp_dsleep _ANSI_ARGS_((Tcl_Interp *, double));
void exp_init_event _ANSI_ARGS_((void));

extern void (*exp_event_exit) _ANSI_ARGS_((Tcl_Interp *));

void exp_event_disarm _ANSI_ARGS_((int));

void exp_arm_background_filehandler _ANSI_ARGS_((int));
void exp_disarm_background_filehandler _ANSI_ARGS_((int));
void exp_disarm_background_filehandler_force _ANSI_ARGS_((int));
void exp_unblock_background_filehandler _ANSI_ARGS_((int));
void exp_block_background_filehandler _ANSI_ARGS_((int));

void exp_background_filehandler _ANSI_ARGS_((ClientData,int));

