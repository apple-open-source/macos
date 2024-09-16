#! /usr/bin/env atf-sh

setup_sshkey()
{
	local ssh_authfile ssh_keydir ssh_keyfile ssh_keyname ssh_lockfile

	if [ -z "$RSYNC_SSHKEY" ]; then
		return 0
	fi

	# We'll allow RSYNC_SSHKEY to use $VARS and we'll expand those, but it
	# can't use tilde expansion.  The most common one will be $HOME, so the
	# constraint is likely ok.
	eval "ssh_keyfile=\"$RSYNC_SSHKEY\""
	ssh_keydir="$(dirname "$ssh_keyfile")"

	# One could specify a key in cwd, or elsewhere.  Make sure it exists so
	# that we can write the lockfile into it.
	mkdir -p "$HOME/.ssh" "$ssh_keydir"

	ssh_authfile="$HOME/.ssh/authorized_keys"
	ssh_lockfile="$ssh_keyfile.lock"

	if [ -s "$ssh_keyfile" ]; then
		# No use even trying to lock if the keyfile is already there.
		return 0
	fi

	if shlock -f "$ssh_lockfile" -p $$; then
		# We'll just assume that if the keyfile exists, whichever
		# invocation won the race completed the whole setup.
		if [ ! -s "$ssh_keyfile" ]; then
			# Generate a key for test purposes, add it to
			# ~/.ssh/authorized_keys.
			ssh-keygen -N '' -f "$ssh_keyfile" -t ed25519
			cat "$ssh_keyfile.pub" >> "$ssh_authfile"
		fi

		rm -f "$ssh_lockfile"
	else
		# Spin until the lockfile is gone, wait up to 5 seconds.
		# If we still haven't generated the key, fail the test.
		time=0
		while [ -e "$ssh_lockfile" ] && [ "$time" -lt 5 ]; do
			time=$((time + 1))
			sleep 1
		done

		if [ ! -s "$ssh_keyfile" ]; then
			atf_fail "Lost the race to the ssh key, winner didn't finish setup"
		fi
	fi

	return 0
}

atf_test_case test0_noslash

test0_noslash_head()
{
    atf_set "descr" "test0_noslash"

}

test0_noslash_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test0_noslash.test" ; then
        atf_pass
    else
        atf_fail "test0_noslash.test failed"
    fi
}


atf_test_case test10_perms

test10_perms_head()
{
    atf_set "descr" "test10_perms"

}

test10_perms_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test10_perms.test" ; then
        atf_pass
    else
        atf_fail "test10_perms.test failed"
    fi
}


atf_test_case test10b_perms

test10b_perms_head()
{
    atf_set "descr" "test10b_perms"

}

test10b_perms_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test10b_perms.test" ; then
        atf_pass
    else
        atf_fail "test10b_perms.test failed"
    fi
}


atf_test_case test11_middlediff

test11_middlediff_head()
{
    atf_set "descr" "test11_middlediff"

}

test11_middlediff_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test11_middlediff.test" ; then
        atf_pass
    else
        atf_fail "test11_middlediff.test failed"
    fi
}


atf_test_case test11b_middlediff

test11b_middlediff_head()
{
    atf_set "descr" "test11b_middlediff"

}

test11b_middlediff_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test11b_middlediff.test" ; then
        atf_pass
    else
        atf_fail "test11b_middlediff.test failed"
    fi
}


atf_test_case test12_inex

test12_inex_head()
{
    atf_set "descr" "test12_inex"

}

test12_inex_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test12_inex.test" ; then
        atf_pass
    else
        atf_fail "test12_inex.test failed"
    fi
}


atf_test_case test12b_inex

test12b_inex_head()
{
    atf_set "descr" "test12b_inex"

}

test12b_inex_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test12b_inex.test" ; then
        atf_pass
    else
        atf_fail "test12b_inex.test failed"
    fi
}


atf_test_case test12c_inex

test12c_inex_head()
{
    atf_set "descr" "test12c_inex"

}

test12c_inex_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test12c_inex.test" ; then
        atf_pass
    else
        atf_fail "test12c_inex.test failed"
    fi
}


atf_test_case test12d_inex

test12d_inex_head()
{
    atf_set "descr" "test12d_inex"

}

test12d_inex_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test12d_inex.test" ; then
        atf_pass
    else
        atf_fail "test12d_inex.test failed"
    fi
}


atf_test_case test13_sparse

test13_sparse_head()
{
    atf_set "descr" "test13_sparse"

}

test13_sparse_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test13_sparse.test" ; then
        atf_pass
    else
        atf_fail "test13_sparse.test failed"
    fi
}


atf_test_case test13b_sparse

test13b_sparse_head()
{
    atf_set "descr" "test13b_sparse"

}

test13b_sparse_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test13b_sparse.test" ; then
        atf_pass
    else
        atf_fail "test13b_sparse.test failed"
    fi
}


atf_test_case test14_hardlinks

test14_hardlinks_head()
{
    atf_set "descr" "test14_hardlinks"

}

test14_hardlinks_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test14_hardlinks.test" ; then
        atf_pass
    else
        atf_fail "test14_hardlinks.test failed"
    fi
}


atf_test_case test14b_hardlinks

test14b_hardlinks_head()
{
    atf_set "descr" "test14b_hardlinks"

}

test14b_hardlinks_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test14b_hardlinks.test" ; then
        atf_pass
    else
        atf_fail "test14b_hardlinks.test failed"
    fi
}


atf_test_case test14c_hardlinks

test14c_hardlinks_head()
{
    atf_set "descr" "test14c_hardlinks"

}

test14c_hardlinks_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test14c_hardlinks.test" ; then
        atf_pass
    else
        atf_fail "test14c_hardlinks.test failed"
    fi
}


atf_test_case test14d_hardlinks

test14d_hardlinks_head()
{
    atf_set "descr" "test14d_hardlinks"

}

test14d_hardlinks_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test14d_hardlinks.test" ; then
        atf_pass
    else
        atf_fail "test14d_hardlinks.test failed"
    fi
}


atf_test_case test14e_hardlinks

test14e_hardlinks_head()
{
    atf_set "descr" "test14e_hardlinks"

}

test14e_hardlinks_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test14e_hardlinks.test" ; then
        atf_pass
    else
        atf_fail "test14e_hardlinks.test failed"
    fi
}


atf_test_case test15_xattrs

test15_xattrs_head()
{
    atf_set "descr" "test15_xattrs"

}

test15_xattrs_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test15_xattrs.test" ; then
        atf_pass
    else
        atf_fail "test15_xattrs.test failed"
    fi
}


atf_test_case test15a_tofile

test15a_tofile_head()
{
    atf_set "descr" "test15a_tofile"

}

test15a_tofile_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test15a_tofile.test" ; then
        atf_pass
    else
        atf_fail "test15a_tofile.test failed"
    fi
}


atf_test_case test15b_tofile

test15b_tofile_head()
{
    atf_set "descr" "test15b_tofile"

}

test15b_tofile_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test15b_tofile.test" ; then
        atf_pass
    else
        atf_fail "test15b_tofile.test failed"
    fi
}


atf_test_case test16_symlinks

test16_symlinks_head()
{
    atf_set "descr" "test16_symlinks"

}

test16_symlinks_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test16_symlinks.test" ; then
        atf_pass
    else
        atf_fail "test16_symlinks.test failed"
    fi
}


atf_test_case test16a_symlinks

test16a_symlinks_head()
{
    atf_set "descr" "test16a_symlinks"

}

test16a_symlinks_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test16a_symlinks.test" ; then
        atf_pass
    else
        atf_fail "test16a_symlinks.test failed"
    fi
}


atf_test_case test16b_symlinks

test16b_symlinks_head()
{
    atf_set "descr" "test16b_symlinks"

}

test16b_symlinks_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test16b_symlinks.test" ; then
        atf_pass
    else
        atf_fail "test16b_symlinks.test failed"
    fi
}


atf_test_case test16c_symlinks

test16c_symlinks_head()
{
    atf_set "descr" "test16c_symlinks"

}

test16c_symlinks_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test16c_symlinks.test" ; then
        atf_pass
    else
        atf_fail "test16c_symlinks.test failed"
    fi
}


atf_test_case test17_existing

test17_existing_head()
{
    atf_set "descr" "test17_existing"

}

test17_existing_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test17_existing.test" ; then
        atf_pass
    else
        atf_fail "test17_existing.test failed"
    fi
}


atf_test_case test17a_existing

test17a_existing_head()
{
    atf_set "descr" "test17a_existing"

}

test17a_existing_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test17a_existing.test" ; then
        atf_pass
    else
        atf_fail "test17a_existing.test failed"
    fi
}


atf_test_case test17b_existing

test17b_existing_head()
{
    atf_set "descr" "test17b_existing"

}

test17b_existing_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test17b_existing.test" ; then
        atf_pass
    else
        atf_fail "test17b_existing.test failed"
    fi
}


atf_test_case test18_nop

test18_nop_head()
{
    atf_set "descr" "test18_nop"

}

test18_nop_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test18_nop.test" ; then
        atf_pass
    else
        atf_fail "test18_nop.test failed"
    fi
}


atf_test_case test19_linkdest

test19_linkdest_head()
{
    atf_set "descr" "test19_linkdest"

}

test19_linkdest_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test19_linkdest.test" ; then
        atf_pass
    else
        atf_fail "test19_linkdest.test failed"
    fi
}


atf_test_case test19b_linkdest_rel

test19b_linkdest_rel_head()
{
    atf_set "descr" "test19b_linkdest-rel"

}

test19b_linkdest_rel_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test19b_linkdest-rel.test" ; then
        atf_pass
    else
        atf_fail "test19b_linkdest-rel.test failed"
    fi
}


atf_test_case test1_minusa

test1_minusa_head()
{
    atf_set "descr" "test1_minusa"

}

test1_minusa_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test1_minusa.test" ; then
        atf_pass
    else
        atf_fail "test1_minusa.test failed"
    fi
}


atf_test_case test20_dlupdates

test20_dlupdates_head()
{
    atf_set "descr" "test20_dlupdates"

}

test20_dlupdates_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test20_dlupdates.test" ; then
        atf_pass
    else
        atf_fail "test20_dlupdates.test failed"
    fi
}


atf_test_case test21_delopts

test21_delopts_head()
{
    atf_set "descr" "test21_delopts"

}

test21_delopts_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test21_delopts.test" ; then
        atf_pass
    else
        atf_fail "test21_delopts.test failed"
    fi
}


atf_test_case test22_inplace

test22_inplace_head()
{
    atf_set "descr" "test22_inplace"

}

test22_inplace_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test22_inplace.test" ; then
        atf_pass
    else
        atf_fail "test22_inplace.test failed"
    fi
}


atf_test_case test23_append

test23_append_head()
{
    atf_set "descr" "test23_append"

}

test23_append_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test23_append.test" ; then
        atf_pass
    else
        atf_fail "test23_append.test failed"
    fi
}


atf_test_case test24_removesource

test24_removesource_head()
{
    atf_set "descr" "test24_removesource"

}

test24_removesource_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test24_removesource.test" ; then
        atf_pass
    else
        atf_fail "test24_removesource.test failed"
    fi
}


atf_test_case test25_filter_basic

test25_filter_basic_head()
{
    atf_set "descr" "test25_filter_basic"

}

test25_filter_basic_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test25_filter_basic.test" ; then
        atf_pass
    else
        atf_fail "test25_filter_basic.test failed"
    fi
}


atf_test_case test25_filter_basic_clear

test25_filter_basic_clear_head()
{
    atf_set "descr" "test25_filter_basic_clear"

}

test25_filter_basic_clear_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test25_filter_basic_clear.test" ; then
        atf_pass
    else
        atf_fail "test25_filter_basic_clear.test failed"
    fi
}


atf_test_case test25_filter_basic_cvs

test25_filter_basic_cvs_head()
{
    atf_set "descr" "test25_filter_basic_cvs"

}

test25_filter_basic_cvs_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test25_filter_basic_cvs.test" ; then
        atf_pass
    else
        atf_fail "test25_filter_basic_cvs.test failed"
    fi
}


atf_test_case test25_filter_clear

test25_filter_clear_head()
{
    atf_set "descr" "test25_filter_clear"

}

test25_filter_clear_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test25_filter_clear.test" ; then
        atf_pass
    else
        atf_fail "test25_filter_clear.test failed"
    fi
}


atf_test_case test25_filter_dir

test25_filter_dir_head()
{
    atf_set "descr" "test25_filter_dir"

}

test25_filter_dir_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test25_filter_dir.test" ; then
        atf_pass
    else
        atf_fail "test25_filter_dir.test failed"
    fi
}


atf_test_case test25_filter_merge

test25_filter_merge_head()
{
    atf_set "descr" "test25_filter_merge"

}

test25_filter_merge_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test25_filter_merge.test" ; then
        atf_pass
    else
        atf_fail "test25_filter_merge.test failed"
    fi
}


atf_test_case test25_filter_merge_cvs

test25_filter_merge_cvs_head()
{
    atf_set "descr" "test25_filter_merge_cvs"

}

test25_filter_merge_cvs_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test25_filter_merge_cvs.test" ; then
        atf_pass
    else
        atf_fail "test25_filter_merge_cvs.test failed"
    fi
}


atf_test_case test25_filter_merge_mods

test25_filter_merge_mods_head()
{
    atf_set "descr" "test25_filter_merge_mods"

}

test25_filter_merge_mods_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test25_filter_merge_mods.test" ; then
        atf_pass
    else
        atf_fail "test25_filter_merge_mods.test failed"
    fi
}


atf_test_case test25_filter_mods

test25_filter_mods_head()
{
    atf_set "descr" "test25_filter_mods"

}

test25_filter_mods_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test25_filter_mods.test" ; then
        atf_pass
    else
        atf_fail "test25_filter_mods.test failed"
    fi
}


atf_test_case test25_filter_receiver

test25_filter_receiver_head()
{
    atf_set "descr" "test25_filter_receiver"

}

test25_filter_receiver_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test25_filter_receiver.test" ; then
        atf_pass
    else
        atf_fail "test25_filter_receiver.test failed"
    fi
}


atf_test_case test25_filter_sender

test25_filter_sender_head()
{
    atf_set "descr" "test25_filter_sender"

}

test25_filter_sender_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test25_filter_sender.test" ; then
        atf_pass
    else
        atf_fail "test25_filter_sender.test failed"
    fi
}


atf_test_case test26_update

test26_update_head()
{
    atf_set "descr" "test26_update"

}

test26_update_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test26_update.test" ; then
        atf_pass
    else
        atf_fail "test26_update.test failed"
    fi
}


atf_test_case test27_checksum

test27_checksum_head()
{
    atf_set "descr" "test27_checksum"

}

test27_checksum_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test27_checksum.test" ; then
        atf_pass
    else
        atf_fail "test27_checksum.test failed"
    fi
}


atf_test_case test28_size_only

test28_size_only_head()
{
    atf_set "descr" "test28_size_only"

}

test28_size_only_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test28_size_only.test" ; then
        atf_pass
    else
        atf_fail "test28_size_only.test failed"
    fi
}


atf_test_case test29_missing_ids

test29_missing_ids_head()
{
    atf_set "descr" "test29_missing_ids"

    atf_set "require.user" "root"
}

test29_missing_ids_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test29_missing_ids.test" ; then
        atf_pass
    else
        atf_fail "test29_missing_ids.test failed"
    fi
}


atf_test_case test2_minusexclude

test2_minusexclude_head()
{
    atf_set "descr" "test2_minusexclude"

}

test2_minusexclude_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test2_minusexclude.test" ; then
        atf_pass
    else
        atf_fail "test2_minusexclude.test failed"
    fi
}


atf_test_case test30_file_update

test30_file_update_head()
{
    atf_set "descr" "test30_file_update"

}

test30_file_update_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test30_file_update.test" ; then
        atf_pass
    else
        atf_fail "test30_file_update.test failed"
    fi
}


atf_test_case test31_rsh

test31_rsh_head()
{
    atf_set "descr" "test31_rsh"

}

test31_rsh_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test31_rsh.test" ; then
        atf_pass
    else
        atf_fail "test31_rsh.test failed"
    fi
}


atf_test_case test32_bigfile

test32_bigfile_head()
{
    atf_set "descr" "test32_bigfile"

}

test32_bigfile_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test32_bigfile.test" ; then
        atf_pass
    else
        atf_fail "test32_bigfile.test failed"
    fi
}


atf_test_case test33_bigid

test33_bigid_head()
{
    atf_set "descr" "test33_bigid"

}

test33_bigid_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test33_bigid.test" ; then
        atf_pass
    else
        atf_fail "test33_bigid.test failed"
    fi
}


atf_test_case test34_desync

test34_desync_head()
{
    atf_set "descr" "test34_desync"

}

test34_desync_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test34_desync.test" ; then
        atf_pass
    else
        atf_fail "test34_desync.test failed"
    fi
}


atf_test_case test35_checksum_seed

test35_checksum_seed_head()
{
    atf_set "descr" "test35_checksum_seed"

}

test35_checksum_seed_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test35_checksum_seed.test" ; then
        atf_pass
    else
        atf_fail "test35_checksum_seed.test failed"
    fi
}


atf_test_case test36_block_size

test36_block_size_head()
{
    atf_set "descr" "test36_block_size"

}

test36_block_size_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test36_block_size.test" ; then
        atf_pass
    else
        atf_fail "test36_block_size.test failed"
    fi
}


atf_test_case test37_chmod

test37_chmod_head()
{
    atf_set "descr" "test37_chmod"

}

test37_chmod_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test37_chmod.test" ; then
        atf_pass
    else
        atf_fail "test37_chmod.test failed"
    fi
}


atf_test_case test38_executability

test38_executability_head()
{
    atf_set "descr" "test38_executability"

}

test38_executability_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test38_executability.test" ; then
        atf_pass
    else
        atf_fail "test38_executability.test failed"
    fi
}


atf_test_case test39_quiet

test39_quiet_head()
{
    atf_set "descr" "test39_quiet"

}

test39_quiet_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test39_quiet.test" ; then
        atf_pass
    else
        atf_fail "test39_quiet.test failed"
    fi
}


atf_test_case test3_minusexclude

test3_minusexclude_head()
{
    atf_set "descr" "test3_minusexclude"

}

test3_minusexclude_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test3_minusexclude.test" ; then
        atf_pass
    else
        atf_fail "test3_minusexclude.test failed"
    fi
}


atf_test_case test3b_minusexclude

test3b_minusexclude_head()
{
    atf_set "descr" "test3b_minusexclude"

}

test3b_minusexclude_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test3b_minusexclude.test" ; then
        atf_pass
    else
        atf_fail "test3b_minusexclude.test failed"
    fi
}


atf_test_case test3c_minusexclude

test3c_minusexclude_head()
{
    atf_set "descr" "test3c_minusexclude"

}

test3c_minusexclude_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test3c_minusexclude.test" ; then
        atf_pass
    else
        atf_fail "test3c_minusexclude.test failed"
    fi
}


atf_test_case test3d_minusexclude

test3d_minusexclude_head()
{
    atf_set "descr" "test3d_minusexclude"

}

test3d_minusexclude_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test3d_minusexclude.test" ; then
        atf_pass
    else
        atf_fail "test3d_minusexclude.test failed"
    fi
}


atf_test_case test3e_minusexclude

test3e_minusexclude_head()
{
    atf_set "descr" "test3e_minusexclude"

}

test3e_minusexclude_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test3e_minusexclude.test" ; then
        atf_pass
    else
        atf_fail "test3e_minusexclude.test failed"
    fi
}


atf_test_case test40_backup

test40_backup_head()
{
    atf_set "descr" "test40_backup"

}

test40_backup_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test40_backup.test" ; then
        atf_pass
    else
        atf_fail "test40_backup.test failed"
    fi
}


atf_test_case test41_backup_dir

test41_backup_dir_head()
{
    atf_set "descr" "test41_backup_dir"

}

test41_backup_dir_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test41_backup_dir.test" ; then
        atf_pass
    else
        atf_fail "test41_backup_dir.test failed"
    fi
}


atf_test_case test42_copy_dest

test42_copy_dest_head()
{
    atf_set "descr" "test42_copy_dest"

}

test42_copy_dest_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test42_copy_dest.test" ; then
        atf_pass
    else
        atf_fail "test42_copy_dest.test failed"
    fi
}


atf_test_case test43_whole_file

test43_whole_file_head()
{
    atf_set "descr" "test43_whole_file"

}

test43_whole_file_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test43_whole_file.test" ; then
        atf_pass
    else
        atf_fail "test43_whole_file.test failed"
    fi
}


atf_test_case test4_excludedir

test4_excludedir_head()
{
    atf_set "descr" "test4_excludedir"

}

test4_excludedir_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test4_excludedir.test" ; then
        atf_pass
    else
        atf_fail "test4_excludedir.test failed"
    fi
}


atf_test_case test50_empty_flist

test50_empty_flist_head()
{
    atf_set "descr" "test50_empty_flist"

}

test50_empty_flist_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test50_empty_flist.test" ; then
        atf_pass
    else
        atf_fail "test50_empty_flist.test failed"
    fi
}


atf_test_case test51_dupe_src

test51_dupe_src_head()
{
    atf_set "descr" "test51_dupe_src"

}

test51_dupe_src_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test51_dupe_src.test" ; then
        atf_pass
    else
        atf_fail "test51_dupe_src.test failed"
    fi
}


atf_test_case test52_version_output

test52_version_output_head()
{
    atf_set "descr" "test52_version_output"

}

test52_version_output_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test52_version_output.test" ; then
        atf_pass
    else
        atf_fail "test52_version_output.test failed"
    fi
}


atf_test_case test5_symlink_kills_dir

test5_symlink_kills_dir_head()
{
    atf_set "descr" "test5_symlink-kills-dir"

}

test5_symlink_kills_dir_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test5_symlink-kills-dir.test" ; then
        atf_pass
    else
        atf_fail "test5_symlink-kills-dir.test failed"
    fi
}


atf_test_case test5b_symlink_kills_dir

test5b_symlink_kills_dir_head()
{
    atf_set "descr" "test5b_symlink-kills-dir"

}

test5b_symlink_kills_dir_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test5b_symlink-kills-dir.test" ; then
        atf_pass
    else
        atf_fail "test5b_symlink-kills-dir.test failed"
    fi
}


atf_test_case test6_perms

test6_perms_head()
{
    atf_set "descr" "test6_perms"

}

test6_perms_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test6_perms.test" ; then
        atf_pass
    else
        atf_fail "test6_perms.test failed"
    fi
}


atf_test_case test6b_perms

test6b_perms_head()
{
    atf_set "descr" "test6b_perms"

}

test6b_perms_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test6b_perms.test" ; then
        atf_pass
    else
        atf_fail "test6b_perms.test failed"
    fi
}


atf_test_case test7_symlinks

test7_symlinks_head()
{
    atf_set "descr" "test7_symlinks"

}

test7_symlinks_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test7_symlinks.test" ; then
        atf_pass
    else
        atf_fail "test7_symlinks.test failed"
    fi
}


atf_test_case test7b_symlinks

test7b_symlinks_head()
{
    atf_set "descr" "test7b_symlinks"

}

test7b_symlinks_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test7b_symlinks.test" ; then
        atf_pass
    else
        atf_fail "test7b_symlinks.test failed"
    fi
}


atf_test_case test8_times

test8_times_head()
{
    atf_set "descr" "test8_times"

}

test8_times_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test8_times.test" ; then
        atf_pass
    else
        atf_fail "test8_times.test failed"
    fi
}


atf_test_case test8b_times

test8b_times_head()
{
    atf_set "descr" "test8b_times"

}

test8b_times_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test8b_times.test" ; then
        atf_pass
    else
        atf_fail "test8b_times.test failed"
    fi
}


atf_test_case test9_norecurse

test9_norecurse_head()
{
    atf_set "descr" "test9_norecurse"

}

test9_norecurse_body()
{
    export tstdir=$(atf_get_srcdir)
    setup_sshkey
    if "$tstdir/test9_norecurse.test" ; then
        atf_pass
    else
        atf_fail "test9_norecurse.test failed"
    fi
}


atf_init_test_cases()
{
    atf_add_test_case test0_noslash
    atf_add_test_case test10_perms
    atf_add_test_case test10b_perms
    atf_add_test_case test11_middlediff
    atf_add_test_case test11b_middlediff
    atf_add_test_case test12_inex
    atf_add_test_case test12b_inex
    atf_add_test_case test12c_inex
    atf_add_test_case test12d_inex
    atf_add_test_case test13_sparse
    atf_add_test_case test13b_sparse
    atf_add_test_case test14_hardlinks
    atf_add_test_case test14b_hardlinks
    atf_add_test_case test14c_hardlinks
    atf_add_test_case test14d_hardlinks
    atf_add_test_case test14e_hardlinks
    atf_add_test_case test15_xattrs
    atf_add_test_case test15a_tofile
    atf_add_test_case test15b_tofile
    atf_add_test_case test16_symlinks
    atf_add_test_case test16a_symlinks
    atf_add_test_case test16b_symlinks
    atf_add_test_case test16c_symlinks
    atf_add_test_case test17_existing
    atf_add_test_case test17a_existing
    atf_add_test_case test17b_existing
    atf_add_test_case test18_nop
    atf_add_test_case test19_linkdest
    atf_add_test_case test19b_linkdest_rel
    atf_add_test_case test1_minusa
    atf_add_test_case test20_dlupdates
    atf_add_test_case test21_delopts
    atf_add_test_case test22_inplace
    atf_add_test_case test23_append
    atf_add_test_case test24_removesource
    atf_add_test_case test25_filter_basic
    atf_add_test_case test25_filter_basic_clear
    atf_add_test_case test25_filter_basic_cvs
    atf_add_test_case test25_filter_clear
    atf_add_test_case test25_filter_dir
    atf_add_test_case test25_filter_merge
    atf_add_test_case test25_filter_merge_cvs
    atf_add_test_case test25_filter_merge_mods
    atf_add_test_case test25_filter_mods
    atf_add_test_case test25_filter_receiver
    atf_add_test_case test25_filter_sender
    atf_add_test_case test26_update
    atf_add_test_case test27_checksum
    atf_add_test_case test28_size_only
    atf_add_test_case test29_missing_ids
    atf_add_test_case test2_minusexclude
    atf_add_test_case test30_file_update
    atf_add_test_case test31_rsh
    atf_add_test_case test32_bigfile
    atf_add_test_case test33_bigid
    atf_add_test_case test34_desync
    atf_add_test_case test35_checksum_seed
    atf_add_test_case test36_block_size
    atf_add_test_case test37_chmod
    atf_add_test_case test38_executability
    atf_add_test_case test39_quiet
    atf_add_test_case test3_minusexclude
    atf_add_test_case test3b_minusexclude
    atf_add_test_case test3c_minusexclude
    atf_add_test_case test3d_minusexclude
    atf_add_test_case test3e_minusexclude
    atf_add_test_case test40_backup
    atf_add_test_case test41_backup_dir
    atf_add_test_case test42_copy_dest
    atf_add_test_case test43_whole_file
    atf_add_test_case test4_excludedir
    atf_add_test_case test50_empty_flist
    atf_add_test_case test51_dupe_src
    atf_add_test_case test52_version_output
    atf_add_test_case test5_symlink_kills_dir
    atf_add_test_case test5b_symlink_kills_dir
    atf_add_test_case test6_perms
    atf_add_test_case test6b_perms
    atf_add_test_case test7_symlinks
    atf_add_test_case test7b_symlinks
    atf_add_test_case test8_times
    atf_add_test_case test8b_times
    atf_add_test_case test9_norecurse
}
