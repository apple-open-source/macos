self="$(realpath "$0")"
input="${self%.sh}.in"

atf_test_case quarantine
quarantine_head() {
	atf_set "descr" "Test that unzip will propagate a zip file's "\
		"quarantine status when extracting it"
}
quarantine_body() {
	local attr="com.apple.quarantine"
	local val="0082;688909f7;Test;"
	atf_check install -m 0644 "${input}" input.zip
	atf_check xattr -w "${attr}" "${val}" input.zip
	atf_check -o inline:"${attr}: ${val}\n" xattr -sl input.zip
	atf_check -o ignore unzip input.zip -d dst
	atf_check test -d dst
	atf_check test -d dst/dir
	atf_check test -d dst/dir/subdir
	atf_check test -f dst/dir/subdir/file
	atf_check -o inline:"file\n" readlink dst/dir/subdir/link
	atf_check -o inline:"${attr}: ${val}\n" xattr -sl dst
	atf_check -o inline:"${attr}: ${val}\n" xattr -sl dst/dir
	atf_check -o inline:"${attr}: ${val}\n" xattr -sl dst/dir/subdir
	atf_check -o inline:"${attr}: ${val}\n" xattr -sl dst/dir/subdir/file
	atf_check -o inline:"${attr}: ${val}\n" xattr -sl dst/dir/subdir/link
}

atf_test_case no_quarantine
no_quarantine_head() {
	atf_set "descr" "Test that unzip will not set a quarantine status on "\
		"extracted files and directories if the zip file itself is not "\
		"quarantined"
}
no_quarantine_body() {
	local attr="com.apple.quarantine"
	atf_check install -m 0644 "${input}" input.zip
	xattr -d "${attr}" input.zip # just in case
	atf_check -o not-match:"^${attr}:" xattr -sl input.zip
	atf_check -o ignore unzip input.zip -d dst
	atf_check test -d dst
	atf_check test -d dst/dir
	atf_check test -d dst/dir/subdir
	atf_check test -f dst/dir/subdir/file
	atf_check -o inline:"file\n" readlink dst/dir/subdir/link
	atf_check -o not-match:"^${attr}:" xattr -sl dst
	atf_check -o not-match:"^${attr}:" xattr -sl dst/dir
	atf_check -o not-match:"^${attr}:" xattr -sl dst/dir/subdir
	atf_check -o not-match:"^${attr}:" xattr -sl dst/dir/subdir/file
	atf_check -o not-match:"^${attr}:" xattr -sl dst/dir/subdir/link
}

atf_init_test_cases() {
	atf_add_test_case quarantine
	atf_add_test_case no_quarantine
}
