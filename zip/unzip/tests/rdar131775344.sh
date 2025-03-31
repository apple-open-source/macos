self="$(realpath "$0")"
input="${self%.sh}.in"

atf_test_case allow
allow_head() {
	atf_set "descr" "Test that unzip will follow symlinks that point "\
		"within the extraction directory"
}
allow_body() {
	dst=$(mktemp -d XXXXX)
	mkdir -p ${dst}/bar
	ln -s bar ${dst}/link
	atf_check -o match:"extracting: ${dst}/link/file.txt" \
	    unzip -d ${dst} "${input}"
	atf_check -o inline:"Hello, world!\n" \
	    cat ${dst}/bar/file.txt
}

atf_test_case refuse
refuse_head() {
	atf_set "descr" "Test that unzip will not follow symlinks that point "\
		"outside the original extraction directory"
}
refuse_body() {
	dst=$(mktemp -d XXXXX)
	mkdir bar
	ln -s ../bar ${dst}/link
	bar=$(realpath bar)
	atf_check -s not-exit:0 -o ignore \
	    -e match:"cannot enter ${bar}" \
	    -e match:"Operation not permitted" \
	    -e match:"unable to process link/file.txt" \
	    unzip -d ${dst} "${input}"
	atf_check -s not-exit:0 \
	    test -f bar/file.txt
}

atf_test_case create
create_head() {
	atf_set "descr" "Test that unzip will create the extraction directory "\
		"if it does not already exist"
}
create_body() {
	dst=$(mktemp -du XXXXX)
	atf_check -o match:"extracting: ${dst}/link/file.txt" \
	    unzip -d ${dst} "${input}"
	atf_check test -f ${dst}/link/file.txt
}

atf_init_test_cases() {
	atf_add_test_case allow
	atf_add_test_case refuse
	atf_add_test_case create
}
