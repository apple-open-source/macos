mkfiles()
{
	cat >input <<EOF
##
# User Database
##
nobody:*:-2:-2::0:0:Unprivileged User:/var/empty:/usr/bin/false
root:*:0:0::0:0:System Administrator:/var/root:/bin/sh
daemon:*:1:1::0:0:System Services:/var/root:/usr/bin/false
EOF
	cat >output <<EOF
nobody:*:-2:-2:Unprivileged User:/var/empty:/usr/bin/false
root:*:0:0:System Administrator:/var/root:/bin/sh
daemon:*:1:1:System Services:/var/root:/usr/bin/false
EOF
}

atf_test_case basic
basic_body()
{
	mkfiles
	mkdir etc
	cp input etc/input
	atf_check pwd_mkdb -d $PWD/etc $PWD/etc/input
	atf_check -s exit:1 test -f etc/input
	atf_check cmp input etc/master.passwd
	atf_check -s exit:1 test -f etc/passwd
}

atf_test_case check
check_body()
{
	mkfiles
	atf_check pwd_mkdb -c input
}

atf_test_case makeold
makeold_body()
{
	mkfiles
	mkdir etc
	cp input etc/input
	atf_check pwd_mkdb -d $PWD/etc -p $PWD/etc/input
	atf_check -s exit:1 test -f etc/input
	atf_check cmp input etc/master.passwd
	atf_check cmp output etc/passwd
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case check
	atf_add_test_case makeold
}
