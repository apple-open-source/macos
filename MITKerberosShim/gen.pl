#!/usr/bin/perl

use strict;

my @rewritesyms = (
    "krb5_cc_end_seq_get",
    "krb5_config_get_string",
    "krb5_set_default_in_tkt_etypes",
    "krb5_get_pw_salt",
    "krb5_free_salt",
    "krb5_string_to_key_data_salt",
    "krb5_free_keyblock_contents",
    "krb5_set_real_time",
    "krb5_mk_req_extended",
    "krb5_free_keyblock",
    "krb5_auth_con_getremotesubkey",
    "krb5_auth_con_getlocalsubkey",
    "krb5_set_password_using_ccache",
    "krb5_realm_compare",
    "krb5_get_renewed_creds",
    "krb5_get_validated_creds",
    "krb5_get_init_creds_keytab",
    "krb5_prompter_posix",
    "krb5_string_to_deltat",
    "krb5_get_all_client_addrs",
    "krb5_kt_get_type",
    "krb5_kt_add_entry",
    "krb5_kt_remove_entry",
    "krb5_mk_req",
    "krb5_kt_get_name",
    "krb5_rd_req",
    "krb5_free_ticket",
    "krb5_build_principal_va",
    "krb5_build_principal_va_ext",
    "krb5_cc_cache_match",
    "krb5_cc_close",
    "krb5_cc_default",
    "krb5_cc_get_config",
    "krb5_cc_get_full_name",
    "krb5_cc_get_name",
    "krb5_cc_get_principal",
    "krb5_cc_get_type",
    "krb5_cc_initialize",
    "krb5_cc_move",
    "krb5_cc_new_unique",
    "krb5_cc_resolve",
    "krb5_cc_store_cred",
    "krb5_cc_switch",
    "krb5_cc_retrieve_cred",
    "krb5_cc_remove_cred",
    "krb5_cc_get_kdc_offset",
    "krb5_cc_set_kdc_offset",
    "krb5_cc_next_cred",
    "krb5_cccol_last_change_time",
    "krb5_crypto_init",
    "krb5_crypto_getblocksize",
    "krb5_crypto_destroy",
    "krb5_decrypt_ivec",
    "krb5_encrypt_ivec",
    "krb5_crypto_getenctype",
    "krb5_generate_random_keyblock",
    "krb5_get_wrapped_length",
    "krb5_copy_creds_contents",
    "krb5_copy_data",
    "krb5_copy_principal",
    "krb5_data_copy",
    "krb5_data_free",
    "krb5_data_zero",
    "krb5_free_context",
    "krb5_free_cred_contents",
    "krb5_free_creds",
    "krb5_free_principal",
    "krb5_sname_to_principal",
    "krb5_get_credentials",
    "krb5_get_error_string",
    "krb5_get_init_creds_opt_alloc",
    "krb5_get_init_creds_opt_free",
    "krb5_get_init_creds_opt_set_canonicalize",
    "krb5_get_init_creds_opt_set_forwardable",
    "krb5_get_init_creds_opt_set_proxiable",
    "krb5_get_init_creds_opt_set_renew_life",
    "krb5_get_init_creds_opt_set_tkt_life",
    "krb5_get_init_creds_password",
    "krb5_get_kdc_cred",
    "krb5_get_kdc_sec_offset",
    "krb5_init_context",
    "krb5_make_principal",
    "krb5_parse_name",
    "krb5_principal_compare",
    "krb5_principal_get_realm",
    "krb5_timeofday",
    "krb5_unparse_name",
    "krb5_us_timeofday",
    "krb5_kt_start_seq_get",
    "krb5_kt_end_seq_get",
    "krb5_xfree",
    "krb5_kt_next_entry",
    "krb5_kt_free_entry",
    "gsskrb5_extract_authz_data_from_sec_context",
    "krb5_sendauth",
    "krb5_free_ap_rep_enc_part",
    "krb5_free_error",
    "krb5_recvauth",
    "krb5_recvauth_match_version",
    "krb5_mk_priv",
    "krb5_rd_priv",
    "krb5_mk_safe",
    "krb5_rd_safe",
    "krb5_set_home_dir_access",
    "krb5_verify_init_creds",
    "krb5_verify_init_creds_opt_init",
    "krb5_verify_init_creds_opt_set_ap_req_nofail",
    "krb5_kuserok",
    "com_right",
    "com_right_r",
    "gss_import_name",
    );

my @proxysyms = (
    "gss_accept_sec_context",
    "gss_acquire_cred",
    "gss_add_cred",
    "gss_add_oid_set_member",
    "gss_canonicalize_name",
    "gss_compare_name",
    "gss_context_time",
    "gss_create_empty_oid_set",
    "gss_delete_sec_context",
    "gss_display_name",
    "gss_display_status",
    "gss_duplicate_name",
    "gss_export_name",
    "gss_export_sec_context",
    "gss_get_mic",
    "gss_import_sec_context",
    "gss_indicate_mechs",
    "gss_init_sec_context",
    "gss_inquire_context",
    "gss_inquire_cred",
    "gss_inquire_cred_by_mech",
    "gss_inquire_names_for_mech",
    "gss_krb5_ccache_name",
    "gss_krb5_copy_ccache",
    "gss_krb5_export_lucid_sec_context",
    "gss_krb5_free_lucid_sec_context",
    "gss_krb5_set_allowable_enctypes",
    "gss_oid_equal",
    "gss_oid_to_str",
    "gss_process_context_token",
    "gss_release_buffer",
    "gss_release_cred",
    "gss_release_name",
    "gss_release_oid",
    "gss_release_oid_set",
    "gss_seal",
    "gss_test_oid_set_member",
    "gss_unseal",
    "gss_unwrap",
    "gss_verify_mic",
    "gss_wrap",
    "gss_wrap_size_limit",
    "krb5_cc_start_seq_get",
    "krb5_cc_default_name",
    "krb5_cc_destroy",
    "krb5_cccol_cursor_free",
    "krb5_cccol_cursor_new",
    "krb5_cccol_cursor_next",
    "krb5_free_host_realm",
    "krb5_get_default_realm",
    "krb5_get_host_realm",
    "krb5_gss_register_acceptor_identity",
    "krb5_cc_set_default_name",
    "krb5_kt_resolve",
    "krb5_kt_default",
    "krb5_kt_default_name",
    "krb5_kt_close",
    "krb5_kt_destroy",
    "krb5_auth_con_free",
    "krb5_auth_con_init",
    "krb5_auth_con_genaddrs",
    "krb5_auth_con_getlocalseqnumber",
    "krb5_auth_con_getremoteseqnumber",
    "krb5_auth_con_setflags",
    "krb5_auth_con_getflags",
    "krb5_clear_error_message",
    "krb5_free_error_message",
    "krb5_get_error_message",
    "krb5_set_default_realm",
    "krb5_set_error_message",
    "krb5_vset_error_message",
    "com_err",
    "com_err_va",
    "reset_com_err_hook",
    "set_com_err_hook",
    );


sub gen_header {
    my $sym;
    my $syms = shift;
    print "void __attribute__((constructor)) heim_load_frameworks(void);\n";
    print "void heim_load_functions(void);\n";
    foreach $sym (@$syms) {
	print "extern void (*fun_${sym})();\n";
    }
}

sub gen_loader {
    my $sym;
    my $syms = shift;
    print "#include <dlfcn.h>\n";
    print "#include <stdio.h>\n";
    print "#include <stdlib.h>\n";
    print "#include <syslog.h>\n";
    print "#include <dispatch/dispatch.h>\n";
    print "#include \"heim-sym.h\"\n";
    print "\n";
    print "static void *hf = NULL;\n";
    print "static void *gf = NULL;\n";
    print "void heim_load_frameworks(void) {\n";
    print "hf = dlopen(\"/System/Library/PrivateFrameworks/Heimdal.framework/Heimdal\", RTLD_LAZY|RTLD_LOCAL);\n";
    print "gf = dlopen(\"/System/Library/Frameworks/GSS.framework/GSS\", RTLD_LAZY|RTLD_LOCAL);\n";
    printf "}\n";
    printf "\n";

    print "void load_functions(void) {\n";
    foreach $sym (@$syms) {
	my $lib;
	if ($sym =~ /gss/) {
	    $lib = "gf";
	} else {
	    $lib = "hf";
	}
	print "fun_${sym} = dlsym(${lib}, \"$sym\");\n";
	print "if (!fun_${sym}) { syslog(LOG_ERR, \"${sym} failed loading\"); }\n";
    }
    printf "}\n";
    printf "\n";
    print "void heim_load_functions(void) {\n";
    print "static dispatch_once_t once = 0;\n";
    print "dispatch_once(&once, ^{ load_functions(); });\n";
    print ("}\n\n");
}

sub gen_32 {

    my $sym;
    my $syms = shift;
    my $prefix = shift;

    my $num = 0;

    print "#ifdef __i386__\n";
    print "	.text\n\n";

    foreach $sym (@$syms) {
	$num++;
	print ".globl ${prefix}_${sym}\n";
	print "${prefix}_${sym}:\n";
	print "\tpushl	%ebp\n";
	print "\tmovl	%esp, %ebp\n";
	print "\tsubl	\$72, %esp\n";
	print "\tcall _heim_load_functions\n";
	print "\taddl	\$72, %esp\n";
	print "\tmovl	%ebp, %esp\n";
	print "\popl	%ebp\n";
	print "\tcall L0${num}\n";
	print "L0${num}:\n";
	print "\tpopl %edx\n";
	print "\tleal L_fun_${sym}\$non_lazy_ptr-\"L0${num}\"(%edx), %eax\n";
	print "\tmovl (%eax), %edx\n";
	print "\tmovl (%edx), %edx\n";
	print "\tjmp *%edx\n";
	print "\n";
    }
    
    foreach $sym (@$syms) {
	print ".comm _fun_${sym},4,2\n";
    }
    print "	.section __IMPORT,__pointers,non_lazy_symbol_pointers\n";
    foreach $sym (@$syms) {
	print "L_fun_${sym}\$non_lazy_ptr:\n";
	print "	.indirect_symbol _fun_${sym}\n";
	print "\t.long 0\n";
    }
    print "\t.subsections_via_symbols\n";

    print "#endif\n";
}

sub gen_64 {

    my $sym;
    my $syms = shift;
    my $prefix = shift;

    print "#ifdef __x86_64__\n";
    print ".text\n\n";

    foreach $sym (@$syms) {
	print "	.globl ${prefix}_${sym}\n";
	print "${prefix}_${sym}:\n";
	print "\tpushq	%rbp\n";
	print "\tmovq	%rsp, %rbp\n";
	print "\tsubq	\$208, %rsp\n";
	print "\tpushq %rdi\n";
	print "\tpushq %rsi\n";
        print "\tpushq %rdx\n";
	print "\tpushq %rcx\n";
	print "\tpushq %r8\n";
	print "\tpushq %r9\n";
	print "\tcall	_heim_load_functions\n";
	print "\tpopq %r9\n";
	print "\tpopq %r8\n";
	print "\tpopq %rcx\n";
        print "\tpopq %rdx\n";
	print "\tpopq %rsi\n";
	print "\tpopq %rdi\n";
	print "\taddq	\$208, %rsp\n";
	print "\tmovq	%rbp, %rsp\n";
	print "\tpopq   %rbp\n";
	print "\tmovq    _fun_${sym}\@GOTPCREL(%rip), %r11\n";
	print "\tmovq    (%r11), %r11\n";
	print "\tjmp	*%r11\n";
	print "\n";
    }
    
    foreach $sym (@$syms) {
	print ".comm _fun_${sym},8,3\n";
    }
    print "#endif\n";
}

sub gen_ppc {

    my $sym;
    my $syms = shift;
    my $prefix = shift;

    print "#ifdef __ppc__\n";
    print ".section __TEXT,__text,regular,pure_instructions\n";
    print ".section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32\n";
    print ".machine ppc7400\n";
    print ".text\n";

    foreach $sym (@$syms) {
	print ".globl ${prefix}_${sym}\n";
	print "${prefix}_${sym}:\n";
	print "\tbl	_abort\n";
	print "\n";
    }

    foreach $sym (@$syms) {
	print ".comm _fun_${sym},4,2\n";
    }

    print "#endif\n";
}


print "/* generated file, no dont edit */\n";

my @all = (@rewritesyms, @proxysyms);

gen_32(\@rewritesyms, "_heim") if ($ARGV[0] eq '3');
gen_64(\@rewritesyms, "_heim") if ($ARGV[0] eq '6');
gen_ppc(\@rewritesyms, "_heim") if ($ARGV[0] eq 'p');
gen_32(\@proxysyms, "") if ($ARGV[0] eq '3p');
gen_64(\@proxysyms, "") if ($ARGV[0] eq '6p');
gen_ppc(\@proxysyms, "") if ($ARGV[0] eq 'pp');

gen_header(\@all) if ($ARGV[0] eq 'h');
gen_loader(\@all) if ($ARGV[0] eq 'l');

