int dcc_send_job_corked(int net_fd,
			char **argv,
			pid_t cpp_pid,
			int *status,
			const char *cpp_fname);

int dcc_retrieve_results(int net_fd, int *status, const char *output_fname);
