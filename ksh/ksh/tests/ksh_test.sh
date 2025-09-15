atf_test_case search_path
search_path_head()
{
	atf_set "descr" "Verify that ksh can search PATH"
}
search_path_body()
{
	tmpdir=$(mktemp -d)
	cd "${tmpdir}"
	touch foo
	atf_check -o inline:"foo\n" ksh -c 'PATH=/bin; ls'
}

atf_test_case builtin_test_d
builtin_test_d_head()
{
	atf_set "descr" "Verify that builtin test -d works"
}
builtin_test_d_body()
{
	tmpdir=$(mktemp -d)
	cd "${tmpdir}"
	mkdir foo
	atf_check ksh -c 'PATH=.; test -d foo'
	atf_check ksh -c 'PATH=.; [ -d foo ]'
	atf_check -s exit:1 ksh -c 'PATH=.; test -d bar'
	atf_check -s exit:1 ksh -c 'PATH=.; [ -d bar ]'
}

atf_test_case builtin_test_f
builtin_test_f_head()
{
	atf_set "descr" "Verify that builtin test -f works"
}
builtin_test_f_body()
{
	tmpdir=$(mktemp -d)
	cd "${tmpdir}"
	touch foo
	atf_check ksh -c 'PATH=.; test -f foo'
	atf_check ksh -c 'PATH=.; [ -f foo ]'
	atf_check -s exit:1 ksh -c 'PATH=.; test -f bar'
	atf_check -s exit:1 ksh -c 'PATH=.; [ -f bar ]'
}

atf_init_test_cases()
{
	atf_add_test_case search_path
	atf_add_test_case builtin_test_d
	atf_add_test_case builtin_test_f
}
