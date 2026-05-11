self="$(realpath "$0")"
input="${self%.sh}.in"

atf_test_case basic_cwd
basic_cwd_head() {
	atf_set "descr" "Extract a basic zip file in current working directory"
}
basic_cwd_body() {
	atf_check \
	    -o match:"Archive: +${input}" \
	    -o match:"extracting: hello.txt" \
	    -o match:"creating: dir" \
	    -o match:"extracting: dir/file.txt" \
	    unzip "${input}"
	atf_check grep -q "Hello, world" hello.txt
	atf_check grep -q "Text" dir/file.txt
}

atf_test_case basic_dst
basic_dst_head() {
	atf_set "descr" "Extract a basic zip file with explicit destination"
}
basic_dst_body() {
	dst=$(mktemp -d XXXXX)
	atf_check \
	    -o match:"Archive: +${input}" \
	    -o match:"extracting: ${dst}/hello.txt" \
	    -o match:"creating: ${dst}/dir" \
	    -o match:"extracting: ${dst}/dir/file.txt" \
	    unzip -d "${dst}" "${input}"
	atf_check grep -q "Hello, world" "${dst}"/hello.txt
	atf_check grep -q "Text" "${dst}"/dir/file.txt
}

atf_test_case basic_dst_direxist
basic_dst_direxist_head() {
	atf_set "descr" "Extract a basic zip file with explicit destination" \
		" (directory exists)"
}
basic_dst_direxist_body() {
	dst=$(mktemp -d XXXXX)
	mkdir "${dst}"/dir
	atf_check \
	    -o match:"Archive: +${input}" \
	    -o match:"extracting: ${dst}/hello.txt" \
	    -o not-match:"creating: ${dst}/dir" \
	    -o match:"extracting: ${dst}/dir/file.txt" \
	    unzip -d "${dst}" "${input}"
}

atf_test_case basic_fileexist
basic_fileexist_head() {
	atf_set "descr" "Extract a basic zip file (file exists, A)"
}
basic_fileexist_body() {
	mkdir dir
	echo "Subtext" >dir/file.txt
	echo A > input
	atf_check \
	    -o match:"Archive: +${input}" \
	    -o match:"extracting: hello.txt" \
	    -o not-match:"creating: dir" \
	    -e match:"replace dir/file.txt" \
	    -o match:"extracting: dir/file.txt" \
	    unzip "${input}" <input
	atf_check grep -q "Hello, world" hello.txt
	atf_check grep -q "Text" dir/file.txt
}

atf_test_case basic_fileexist_replace
basic_fileexist_replace_head() {
	atf_set "descr" "Extract a basic zip file (file exists, -o)"
}
basic_fileexist_replace_body() {
	mkdir dir
	echo "Subtext" >dir/file.txt
	atf_check \
	    -o match:"Archive: +${input}" \
	    -o match:"extracting: hello.txt" \
	    -o not-match:"creating: dir" \
	    -o match:"extracting: dir/file.txt" \
	    unzip -o "${input}"
	atf_check grep -q "Hello, world" hello.txt
	atf_check grep -q "Text" dir/file.txt
}

atf_test_case basic_fileexist_noreplace
basic_fileexist_noreplace_head() {
	atf_set "descr" "Extract a basic zip file (file exists, -n)"
}
basic_fileexist_noreplace_body() {
	mkdir dir
	echo "Subtext" >dir/file.txt
	atf_check \
	    -o match:"Archive: +${input}" \
	    -o match:"extracting: hello.txt" \
	    -o not-match:"creating: dir" \
	    -o not-match:"extracting: dir/file.txt" \
	    unzip -n "${input}"
	atf_check grep -q "Hello, world" hello.txt
	atf_check grep -q "Subtext" dir/file.txt
}

atf_test_case basic_fileexist_backup
basic_fileexist_backup_head() {
	atf_set "descr" "Extract a basic zip file (file exists, -B)"
}
basic_fileexist_backup_body() {
	mkdir dir
	echo "Subtext" >dir/file.txt
	atf_check \
	    -o match:"Archive: +${input}" \
	    -o match:"extracting: hello.txt" \
	    -o not-match:"creating: dir" \
	    -o match:"extracting: dir/file.txt" \
	    unzip -B "${input}"
	atf_check grep -q "Hello, world" hello.txt
	atf_check grep -q "Text" dir/file.txt
	atf_check grep -q "Subtext" dir/file.txt~
}

atf_test_case basic_fileexist_backupexist
basic_fileexist_backupexist_head() {
	atf_set "descr" "Extract a basic zip file (file exists, backup exists, -B)"
}
basic_fileexist_backupexist_body() {
	mkdir dir
	echo "Subtext" >dir/file.txt
	echo "Context" >dir/file.txt~
	atf_check \
	    -o match:"Archive: +${input}" \
	    -o match:"extracting: hello.txt" \
	    -o not-match:"creating: dir" \
	    -o match:"extracting: dir/file.txt" \
	    unzip -B "${input}"
	atf_check grep -q "Hello, world" hello.txt
	atf_check grep -q "Text" dir/file.txt
	atf_check grep -q "Context" dir/file.txt~
	atf_check grep -q "Subtext" dir/file.txt~1
}

atf_test_case basic_fileexist_replacebackup
basic_fileexist_replacebackup_head() {
	atf_set "descr" "Extract a basic zip file (file exists, backup exists, -oB)"
}
basic_fileexist_replacebackup_body() {
	mkdir dir
	echo "Subtext" >dir/file.txt
	echo "Context" >dir/file.txt~
	atf_check \
	    -o match:"Archive: +${input}" \
	    -o match:"extracting: hello.txt" \
	    -o not-match:"creating: dir" \
	    -o match:"extracting: dir/file.txt" \
	    unzip -oB "${input}"
	atf_check grep -q "Hello, world" hello.txt
	atf_check grep -q "Text" dir/file.txt
	atf_check grep -q "Subtext" dir/file.txt~
}

atf_init_test_cases() {
	atf_add_test_case basic_cwd
	atf_add_test_case basic_dst
	atf_add_test_case basic_dst_direxist
	atf_add_test_case basic_fileexist
	atf_add_test_case basic_fileexist_replace
	atf_add_test_case basic_fileexist_noreplace
	atf_add_test_case basic_fileexist_backup
	atf_add_test_case basic_fileexist_backupexist
	atf_add_test_case basic_fileexist_replacebackup
}
