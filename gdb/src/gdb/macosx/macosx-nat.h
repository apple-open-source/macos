#ifndef __GDB_MACOSX_NAT_H__
#define __GDB_MACOSX_NAT_H__

int is_pid_classic (pid_t pid);
int can_attach (pid_t target_pid);
int attaching_to_classic_process_p (pid_t target_pid);
void attach_to_classic_process (pid_t pid);
void restore_orig_rlimit (void);

#endif /* __GDB_MACOSX_NAT_H__ */
