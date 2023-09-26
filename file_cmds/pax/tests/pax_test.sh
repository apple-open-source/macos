# Appropriate source of test files: a) always present, b) not too big,
# c) the output of `find "${src}" -type f` should be several kB.
src=/usr/lib

atf_compare_files() {
	local a="$1"
	local b="$2"
	atf_check_equal "$(stat -f%p:%m:%z "${a}")" "$(stat -f%p:%m:%z "${b}")"
	atf_check cmp -s "${a}" "${b}"
}

atf_test_case copy_cmdline cleanup
copy_cmdline_head() {
	atf_set descr "Copy mode with files on command line"
}
copy_cmdline_body() {
	atf_check mkdir dst
	atf_check pax -rw -pp "${src}" dst
	find "${src}" -type f | while read file ; do
		atf_compare_files "${file}" "${dst}${file}"
	done
}
copy_cmdline_cleanup() {
	rm -rf dst
}

atf_test_case copy_stdin cleanup
copy_stdin_head() {
	atf_set descr "Copy mode with files on stdin"
}
copy_stdin_body() {
	atf_check mkdir dst
	find "${src}" -type f >input
	atf_check pax -rw -pp dst <input
	find "${src}" -type f | while read file ; do
		atf_compare_files "${file}" "${dst}${file}"
	done
}
copy_stdin_cleanup() {
	rm -rf dst
}

atf_test_case copy_stdin0 cleanup
copy_stdin0_head() {
	atf_set descr "Copy mode with files on stdin using -0"
}
copy_stdin0_body() {
	atf_check mkdir dst
	find "${src}" -type f -print0 >input
	atf_check pax -rw -pp -0 dst <input
	find "${src}" -type f | while read file ; do
		atf_compare_files "${file}" "${dst}${file}"
	done
}
copy_stdin0_cleanup() {
	rm -rf dst
}

atf_init_test_cases()
{
	atf_add_test_case copy_cmdline
	atf_add_test_case copy_stdin
	atf_add_test_case copy_stdin0
}
