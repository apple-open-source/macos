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

atf_test_case mod_time_preserve
mod_time_preserve_head() {
	atf_set "descr" "Verifying that pax(1) preserves modification time " \
		"with nanosecond resolution."
}
mod_time_preserve_body() {
	src_dir_name=$(mktemp -d "${PWD}/XXXXXX")
	src_file_name=$(mktemp "${src_dir_name}/XXXXXX")
	src_dir_mtime=$(stat -f %Fm "${src_dir_name}")
	src_file_mtime=$(stat -f %Fm "${src_file_name}")

	tgt_dir_name=$(mktemp -d "${PWD}/XXXXXX")
	tgt_dir_mtime1=$(stat -f %Fm "${tgt_dir_name}")
	tgt_file_name=${tgt_dir_name}/$(basename "${src_file_name}")

	cd "${src_dir_name}"
	atf_check -o ignore pax -rw . "${tgt_dir_name}"
	tgt_dir_mtime2=$(stat -f %Fm "${tgt_dir_name}")
	tgt_file_mtime=$(stat -f %Fm "${tgt_file_name}")

	atf_check_not_equal "${tgt_dir_mtime1}" "${tgt_dir_mtime2}"
	atf_check_equal "${src_dir_mtime}" "${tgt_dir_mtime2}"
	atf_check_equal "${src_file_mtime}" "${tgt_file_mtime}"
}

atf_test_case mod_time_set
mod_time_set_head() {
	atf_set "descr" "Verifying that pax(1) sets modification time " \
		"with nanosecond resolution."
}
mod_time_set_body() {
	# modification time can legitimately have 000 as the ns component, with 1/1000 chance.
	# trying 5 times pretty much guarantees no false positives
	num_tries=5

	src_dir_name=$(mktemp -d "${PWD}/XXXXXX")
	src_file_name=$(mktemp "${src_dir_name}/XXXXXX")

	tgt_dir_name=$(mktemp -d "${PWD}/XXXXXX")
	tgt_file_name=${tgt_dir_name}/$(basename "${src_file_name}")

	cd "${src_dir_name}"
	for i in $(seq 1 ${num_tries}); do
		atf_check -o ignore pax -rw -p m . "${tgt_dir_name}"
		case $(stat -f %Fm "${tgt_file_name}") in
		*.*000)
			# try again...
			;;
		*)
			atf_pass
			;;
		esac
		rm -f "${tgt_file_name}"
	done
	atf_fail "pax(1) does not set modification time with nanosecond resolution."
}

atf_init_test_cases()
{
	atf_add_test_case copy_cmdline
	atf_add_test_case copy_stdin
	atf_add_test_case copy_stdin0
	atf_add_test_case mod_time_preserve
	atf_add_test_case mod_time_set
}
