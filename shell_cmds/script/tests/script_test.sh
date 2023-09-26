#!/bin/sh

atf_test_case script_from_file
script_from_file_head() {
	atf_set "descr" "[rdar://86182963] Ignore tcgetattr() failure when input is a regular file"
}
script_from_file_body() {
	local input="input.$$"
	local output="output_from_file.$$"
	atf_check cp /dev/null "${input}"
	atf_check -o ignore env SHELL=/bin/sh PS1='$ ' script "${output}" <"${input}"
	atf_check rm "${input}" "${output}"
}

atf_test_case script_from_null
script_from_null_head() {
	atf_set "descr" "[rdar://86182963] Ignore tcgetattr() failure when input is a device"
}
script_from_null_body() {
	local output="output_from_null.$$"
	atf_check -o ignore env SHELL=/bin/sh PS1='$ ' script "${output}" </dev/null
	atf_check rm "${output}"
}

atf_test_case script_from_pipe
script_from_pipe_head() {
	atf_set "descr" "[rdar://86182963] Ignore tcgetattr() failure when input is a pipe"
}
script_from_pipe_body() {
	local output="output_from_pipe.$$"
	:| atf_check -o ignore env SHELL=/bin/sh PS1='$ ' script "${output}"
	atf_check rm "${output}"
}

atf_init_test_cases() {
	atf_add_test_case script_from_file
	atf_add_test_case script_from_null
	atf_add_test_case script_from_pipe
}
