
atf_test_case rdar79795987
rdar79795987_head() {
	atf_set "descr" "Verifying that xattr does not exit cleanly on failure"
}
rdar79795987_body() {
	dir_name=$(mktemp -d XXXXXX)
	atf_check -s not-exit:0 -e not-empty \
	    xattr -p com.apple.xattrtest.idontexist ${dir_name}
}

atf_test_case rdar79990691
rdar79990691_head() {
	atf_set "descr" "Verifying that xattr behaves correctly when " \
		"hex values have embedded NULs"
}
rdar79990691_body() {
	dir_name=$(mktemp -d XXXXXX)

	atf_check xattr -w -x com.apple.xattrtest "00 00 00 00 00 00 00 00 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00" ${dir_name}
}

atf_test_case rdar80300336
rdar80300336_head() {
	atf_set "descr" "Verifying that deleting an xattr recursively " \
		"does not exit with an error"
}
rdar80300336_body() {
	dir_name=$(mktemp -d XXXXXX)
	subdir_name=$(mktemp -d ${dir_name}/XXXXXX)
	file_name=$(mktemp ${subdir_name}/XXXXXX)

	atf_check xattr -d -r com.apple.xattrtest ${dir_name}
}

atf_test_case rdar82573904
rdar82573904_head() {
	atf_set "descr" "Verifying that deleting an xattr recursively " \
		"with a symlink directory target does not affect files " \
		"in that directory"
}
rdar82573904_body() {
	dir_name=$(mktemp -d XXXXXX)
	subdir_name=$(mktemp -d ${dir_name}/XXXXXX)
	file_name=$(mktemp ${subdir_name}/XXXXXX)

	# Write the xattr first
	atf_check xattr -w com.apple.xattrtest testvalue ${file_name}

	# Ensure we can read it back
	atf_check -o match:testvalue \
	    xattr -p com.apple.xattrtest ${file_name}

	# Now create a symlink to ${subdir_name} inside ${dir_name}
	symlink_name="xattrtestsymlink"
	subdir_basename=$(basename ${subdir_name})
	atf_check ln -s ${subdir_basename} ${dir_name}/${symlink_name}

	# Delete recursively on the symlink path
	atf_check xattr -d -r com.apple.xattrtest ${dir_name}/${symlink_name}

	# xattr should still be there
	atf_check -o match:testvalue \
	    xattr -p com.apple.xattrtest ${file_name}
}

atf_init_test_cases() {
	atf_add_test_case rdar79795987
	atf_add_test_case rdar79990691
	atf_add_test_case rdar80300336
	atf_add_test_case rdar82573904
}
