atf_test_case bc_shutdown_ebadf
bc_shutdown_ebadf_head() {
	atf_set descr "Trigger EBADF during VM shutdown (rdar://124127836)"
}
bc_shutdown_ebadf_body() {
	(
		bc -e 0 2>stderr
		echo $? >result
	) >&-
	atf_check -o match:"[1-9]" cat result
	atf_check -o match:"I/O error" cat stderr
}

atf_test_case bc_shutdown_sigpipe
bc_shutdown_sigpipe_head() {
	atf_set descr "Trigger SIGPIPE during VM shutdown (rdar://124127836)"
}
bc_shutdown_sigpipe_body() {
	(
		trap "" PIPE
		bc -e 0 2>stderr
		echo $? >result
	) | true
	atf_check -o match:"[1-9]" cat result
	atf_check -o match:"I/O error" cat stderr
}

atf_init_test_cases() {
	atf_add_test_case bc_shutdown_ebadf
	atf_add_test_case bc_shutdown_sigpipe
}
