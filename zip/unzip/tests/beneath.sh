self="$(realpath "$0")"
input="${self%.sh}.in"

atf_test_case beneath_allow
beneath_allow_head() {
	atf_set "descr" "Test that unzip will follow symlinks that point "\
		"within the extraction directory"
}
beneath_allow_body() {
	dst=$(mktemp -d XXXXX)
	mkdir -p ${dst}/bar
	ln -s bar ${dst}/link
	atf_check -o match:"extracting: ${dst}/link/file.txt" \
	    unzip -d ${dst} "${input}"
	atf_check -o inline:"Hello, world!\n" \
	    cat ${dst}/bar/file.txt
}

atf_test_case beneath_dead
beneath_dead_head() {
	atf_set "descr" "Test that unzip will handle a dead symlink"
}
beneath_dead_body() {
	dst=$(mktemp -d XXXXX)
	ln -s dead ${dst}/link
	atf_check -s not-exit:0 -o ignore \
	    -e match:"cannot enter ${dst}/link" \
	    -e match:"No such file or directory" \
	    unzip -d ${dst} "${input}"
	atf_check -s not-exit:0 \
	    test -f bar/file.txt
}

atf_test_case beneath_refuse
beneath_refuse_head() {
	atf_set "descr" "Test that unzip will not follow symlinks that point "\
		"outside the original extraction directory"
}
beneath_refuse_body() {
	dst=$(mktemp -d XXXXX)
	mkdir bar
	ln -s ../bar ${dst}/link
	atf_check -s not-exit:0 -o ignore \
	    -e match:"cannot enter ${dst}/link" \
	    -e match:"Permission denied" \
	    unzip -d ${dst} "${input}"
	atf_check -s not-exit:0 \
	    test -f bar/file.txt
}

atf_test_case beneath_create
beneath_create_head() {
	atf_set "descr" "Test that unzip will create the extraction directory "\
		"if it does not already exist"
}
beneath_create_body() {
	dst=$(mktemp -du XXXXX)
	atf_check -o match:"extracting: ${dst}/link/file.txt" \
	    unzip -d ${dst} "${input}"
	atf_check test -f ${dst}/link/file.txt
}

atf_init_test_cases() {
	atf_add_test_case beneath_allow
	atf_add_test_case beneath_dead
	atf_add_test_case beneath_refuse
	atf_add_test_case beneath_create
}
