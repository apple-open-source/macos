#!/bin/sh

: ${PATH_HELPER:=/usr/libexec/path_helper}

mkpath() {
	local p
	if [ $# -gt 0 ] ; then
		p="$1"
		shift
		while [ $# -gt 0 ] ; do
			p="$p:$1"
			shift
		done
	fi
	echo "PATH=\"$p\"; export PATH;"
}

mkmanpath() {
	local p
	if [ $# -gt 0 ] ; then
		p="$1"
		shift
		while [ $# -gt 0 ] ; do
			p="$p:$1"
			shift
		done
	fi
	echo "MANPATH=\"$p:\"; export MANPATH;"
}

atf_test_case path_helper_empty
path_helper_empty_head() {
	atf_set "descr" "empty PATH"
}
path_helper_empty_body() {
	root="$(mktemp -d empty.XXXXXXXX)"
	mkpath >output.empty
	atf_check -o file:output.empty \
		  env -uMANPATH PATH= PATH_HELPER_ROOT="${root}" \
		  "${PATH_HELPER}"
}

atf_test_case path_helper_empty2
path_helper_empty2_head() {
	atf_set "descr" "empty PATH and MANPATH"
}
path_helper_empty2_body() {
	root="$(mktemp -d empty2.XXXXXXXX)"
	(mkpath; mkmanpath) >output.empty2
	atf_check -o file:output.empty2 \
		  env MANPATH= PATH= PATH_HELPER_ROOT="${root}" \
		  "${PATH_HELPER}"
}

atf_test_case path_helper_preserve
path_helper_preserve_head() {
	atf_set "descr" "preserve existing values"
}
path_helper_preserve_body() {
	root="$(mktemp -d preserve.XXXXXXXX)"
	(mkpath a b; mkmanpath c d) >output.preserve
	atf_check -o file:output.preserve \
		  env MANPATH=c:d PATH=a:b PATH_HELPER_ROOT="${root}" \
		  "${PATH_HELPER}"
}

atf_test_case path_helper_combine
path_helper_combine_head() {
	atf_set "descr" "combine defaults and add-ons in that order"
}
path_helper_combine_body() {
	root="$(mktemp -d combine.XXXXXXXX)"
	mkdir -p "${root}"/etc/paths.d
	(echo a; echo b) >"${root}"/etc/paths
	(echo c; echo d) >"${root}"/etc/paths.d/add-ons
	mkpath a b c d >output.combine
	atf_check -o file:output.combine \
		  env -uMANPATH PATH= PATH_HELPER_ROOT="${root}" \
		  "${PATH_HELPER}"
}

atf_test_case path_helper_order
path_helper_order_head() {
	atf_set "descr" "read add-ons in correct order"
}
path_helper_order_body() {
	root="$(mktemp -d order.XXXXXXXX)"
	mkdir -p "${root}"/etc/paths.d
	(echo a; echo b) >"${root}"/etc/paths
	echo z >"${root}"/etc/paths.d/a
	echo y >"${root}"/etc/paths.d/1000
	echo x >"${root}"/etc/paths.d/0400-b
	echo w >"${root}"/etc/paths.d/400-a
	(echo d; echo e; echo f) >"${root}"/etc/paths.d/70def
	echo c >"${root}"/etc/paths.d/9
	# expected order is:
	# - defaults
	# - add-ons in lexicographical order, except those that start
	#   with a number are sorted numerically, hence:
	#   - 9
	#   - 70def
	#   - 400-a
	#   - 0400-b
	#   - 1000
	#   - a
	# - prior value
	mkpath a b c d e f w x y z g h >output.order
	atf_check -o file:output.order \
		  env -uMANPATH PATH=g:h PATH_HELPER_ROOT="${root}" \
		  "${PATH_HELPER}"
}

atf_init_test_cases() {
	atf_add_test_case path_helper_empty
	atf_add_test_case path_helper_empty2
	atf_add_test_case path_helper_preserve
	atf_add_test_case path_helper_combine
	atf_add_test_case path_helper_order
}
