/* where.c */
int dcc_pick_host_from_env(struct dcc_hostdef **,
			   int *xmit_lock_fd,
			   int *cpu_lock_fd);

int dcc_lock_local(int *xmit_lock_fd, int *cpu_lock_fd);
