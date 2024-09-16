#! /bin/sh -
#	@(#)makesyscalls.sh	8.1 (Berkeley) 6/10/93
# $FreeBSD: src/sys/kern/makesyscalls.sh,v 1.60 2003/04/01 01:12:24 jeff Exp $
#
# Copyright (c) 2004-2008 Apple Inc. All rights reserved.
#
# @APPLE_OSREFERENCE_LICENSE_HEADER_START@
# 
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
# 
# @APPLE_OSREFERENCE_LICENSE_HEADER_END@
#

set -e

input_file="" # first argument

# output type:
output_syscallnamesfile=0
output_sysprotofile=0
output_syshdrfile=0
output_syscalltablefile=0
output_auditevfile=0
output_tracecodes=0
output_systrace=0

use_stdout=0

# output files:
syscallnamesfile="syscalls.c"
sysprotofile="sysproto.h"
sysproto_h=_SYS_SYSPROTO_H_
syshdrfile="syscall.h"
syscall_h=_SYS_SYSCALL_H_
syscalltablefile="init_sysent.c"
auditevfile="audit_kevents.c"
syscallprefix="SYS_"
switchname="sysent"
namesname="syscallnames"
tracecodename="syscall.codes"
systraceargsfile="systrace_args.c"
# tmp files:
syslegal="sysent.syslegal.$$"
sysent="sysent.switch.$$"
sysinc="sysinc.switch.$$"
sysarg="sysarg.switch.$$"
sysprotoend="sysprotoend.$$"
syscallnamestempfile="syscallnamesfile.$$"
syshdrtempfile="syshdrtempfile.$$"
audittempfile="audittempfile.$$"
tracecodetempfile="tracecodetempfile.$$"
systraceargstempfile="systraceargstempfile.$$"
systraceargdesctempfile="systraceargdesctempfile.$$"
systracerettempfile="systracerettempfile.$$"

trap "rm $syslegal $sysent $sysinc $sysarg $sysprotoend $syscallnamestempfile $syshdrtempfile $audittempfile $tracecodetempfile $systraceargstempfile $systraceargdesctempfile $systracerettempfile" 0

touch $syslegal $sysent $sysinc $sysarg $sysprotoend $syscallnamestempfile $syshdrtempfile $audittempfile $tracecodetempfile $systraceargstempfile $systraceargdesctempfile $systracerettempfile

case $# in
    0)
	echo "usage: $0 input-file [<names|proto|header|table|audit|trace> [<config-file>]]" 1>&2
	exit 1
	;;
esac

input_file="$1"
shift

if [ -n "$1" ]; then
    case $1 in
	names)
	    output_syscallnamesfile=1
	    ;;
	proto)
	    output_sysprotofile=1
	    ;;
	header)
	    output_syshdrfile=1
	    ;;
	table)
	    output_syscalltablefile=1
	    ;;
	audit)
	    output_auditevfile=1
	    ;;
	systrace)
	    output_systrace=1
	    ;;
	trace)
	    output_tracecodes=1
	    use_stdout=1
	    ;;
    esac
    shift;
else
    output_syscallnamesfile=1
    output_sysprotofile=1
    output_syshdrfile=1
    output_syscalltablefile=1
    output_auditevfile=1
    output_tracecodes=1
fi

if [ -n "$1" -a -f "$1" ]; then
	. $1
fi



sed -e '
s/\$//g
:join
	/\\$/{a\

	N
	s/\\\n//
	b join
	}
2,${
	/^#/!s/\([{}()*,;]\)/ \1 /g
}
' < "$input_file" | awk "
	BEGIN {
		syslegal = \"$syslegal\"
		sysprotofile = \"$sysprotofile\"
		sysprotoend = \"$sysprotoend\"
		sysproto_h = \"$sysproto_h\"
		syscall_h = \"$syscall_h\"
		sysent = \"$sysent\"
		syscalltablefile = \"$syscalltablefile\"
		sysinc = \"$sysinc\"
		sysarg = \"$sysarg\"
		syscallnamesfile = \"$syscallnamesfile\"
		syscallnamestempfile = \"$syscallnamestempfile\"
		syshdrfile = \"$syshdrfile\"
		syshdrtempfile = \"$syshdrtempfile\"
		systraceargstempfile = \"$systraceargstempfile\"
		systraceargdesctempfile = \"$systraceargdesctempfile\"
		systracerettempfile = \"$systracerettempfile\"
		audittempfile = \"$audittempfile\"
		tracecodetempfile = \"$tracecodetempfile\"
		syscallprefix = \"$syscallprefix\"
		switchname = \"$switchname\"
		namesname = \"$namesname\"
		infile = \"$input_file\"
		infilepretty = \"${input_file#"$TARGET"}\"
		"'

		printf "/*\n" > syslegal
		printf " * Copyright (c) 2004-2008 Apple Inc. All rights reserved.\n" > syslegal
		printf " * \n" > syslegal
		printf " * @APPLE_OSREFERENCE_LICENSE_HEADER_START@\n" > syslegal
		printf " * \n" > syslegal
		printf " * This file contains Original Code and/or Modifications of Original Code\n" > syslegal
		printf " * as defined in and that are subject to the Apple Public Source License\n" > syslegal
		printf " * Version 2.0 (the \047License\047). You may not use this file except in\n" > syslegal
		printf " * compliance with the License. The rights granted to you under the License\n" > syslegal
		printf " * may not be used to create, or enable the creation or redistribution of,\n" > syslegal
		printf " * unlawful or unlicensed copies of an Apple operating system, or to\n" > syslegal
		printf " * circumvent, violate, or enable the circumvention or violation of, any\n" > syslegal
		printf " * terms of an Apple operating system software license agreement.\n" > syslegal
		printf " * \n" > syslegal
		printf " * Please obtain a copy of the License at\n" > syslegal
		printf " * http://www.opensource.apple.com/apsl/ and read it before using this file.\n" > syslegal
		printf " * \n" > syslegal
		printf " * The Original Code and all software distributed under the License are\n" > syslegal
		printf " * distributed on an \047AS IS\047 basis, WITHOUT WARRANTY OF ANY KIND, EITHER\n" > syslegal
		printf " * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,\n" > syslegal
		printf " * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,\n" > syslegal
		printf " * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.\n" > syslegal
		printf " * Please see the License for the specific language governing rights and\n" > syslegal
		printf " * limitations under the License.\n" > syslegal
		printf " * \n" > syslegal
		printf " * @APPLE_OSREFERENCE_LICENSE_HEADER_END@\n" > syslegal
		printf " * \n" > syslegal
		printf " * \n" > syslegal
		printf " * System call switch table.\n *\n" > syslegal
		printf " * DO NOT EDIT-- this file is automatically generated.\n" > syslegal
		printf " * created from %s\n */\n\n", infilepretty > syslegal
	}
	NR == 1 {
		printf "\n/* The casts are bogus but will do for now. */\n" > sysent
		printf "__private_extern__ const struct sysent %s[] = {\n",switchname > sysent

		printf "#ifndef %s\n", sysproto_h > sysarg
		printf "#define\t%s\n\n", sysproto_h > sysarg
		printf "#ifndef %s\n", syscall_h > syshdrtempfile
		printf "#define\t%s\n\n", syscall_h > syshdrtempfile
		printf "#include <sys/appleapiopts.h>\n" > syshdrtempfile
		printf "#ifdef __APPLE_API_PRIVATE\n" > syshdrtempfile
		printf "#include <sys/appleapiopts.h>\n" > sysarg
		printf "#include <sys/cdefs.h>\n" > sysarg
		printf "#include <sys/mount_internal.h>\n" > sysarg
		printf "#include <sys/types.h>\n" > sysarg
		printf "#include <sys/sem_internal.h>\n" > sysarg
		printf "#include <sys/semaphore.h>\n" > sysarg
		printf "#include <sys/wait.h>\n" > sysarg
		printf "#include <mach/shared_region.h>\n" > sysarg
		printf "\n#ifdef KERNEL\n" > sysarg
		printf "#ifdef __APPLE_API_PRIVATE\n" > sysarg
		printf "/*\n" > sysarg
		printf " * The kernel may support multiple userspace ABIs, and must use\n" > sysarg
		printf " * argument structures with elements large enough for any of them.\n" > sysarg
		printf "*/\n" > sysarg
		printf "\n" > sysarg
		printf "#if CONFIG_REQUIRES_U32_MUNGING\n" > sysarg
		printf "#define\tPAD_(t)\t(sizeof(uint64_t) <= sizeof(t) \\\n " > sysarg
		printf "\t\t? 0 : sizeof(uint64_t) - sizeof(t))\n" > sysarg
		printf "#else\n" > sysarg
		printf "#define\tPAD_(t)\t(sizeof(uint32_t) <= sizeof(t) \\\n" > sysarg
		printf " 		? 0 : sizeof(uint32_t) - sizeof(t))\n" > sysarg
		printf "#endif\n" > sysarg
		printf "#if BYTE_ORDER == LITTLE_ENDIAN\n"> sysarg
		printf "#define\tPADL_(t)\t0\n" > sysarg
		printf "#define\tPADR_(t)\tPAD_(t)\n" > sysarg
		printf "#else\n" > sysarg
		printf "#define\tPADL_(t)\tPAD_(t)\n" > sysarg
		printf "#define\tPADR_(t)\t0\n" > sysarg
		printf "#endif\n" > sysarg
		printf "\n__BEGIN_DECLS\n" > sysarg
		printf "#include <sys/munge.h>\n" > sysarg
		
		printf "\n" > sysarg

		printf "const char *%s[] = {\n", namesname > syscallnamestempfile

		printf "#include <sys/param.h>\n" > audittempfile
		printf "#include <sys/types.h>\n\n" > audittempfile
		printf "#include <bsm/audit.h>\n" > audittempfile
		printf "#include <bsm/audit_kevents.h>\n\n" > audittempfile
		printf "#if CONFIG_AUDIT\n\n" > audittempfile
		printf "au_event_t sys_au_event[] = {\n" > audittempfile

		printf "/*\n * System call argument to DTrace register array conversion.\n */\n" > systraceargstempfile
		printf "#include <sys/systrace_args.h>\n" > systraceargstempfile
		printf "void\nsystrace_args(int sysnum, void *params, uint64_t *uarg)\n{\n" > systraceargstempfile
		printf "\tint64_t *iarg  = (int64_t *) uarg;\n" > systraceargstempfile
		printf "\tswitch (sysnum) {\n" > systraceargstempfile

		printf "void\nsystrace_entry_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)\n{\n\tconst char *p = NULL;\n" > systraceargdesctempfile
		printf "\tswitch (sysnum) {\n" > systraceargdesctempfile

		printf "void\nsystrace_return_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)\n{\n\tconst char *p = NULL;\n" > systracerettempfile
		printf "\tswitch (sysnum) {\n" > systracerettempfile

		next
	}
	NF == 0 || $1 ~ /^;/ {
		next
	}
	$1 ~ /^#[ 	]*include/ {
		print > sysinc
		next
	}
	$1 ~ /^#[ 	]*if/ {
		print > sysent
		print > sysarg
		print > syscallnamestempfile
		print > sysprotoend
		print > audittempfile
		print > systraceargstempfile
		print > systraceargdesctempfile
		print > systracerettempfile
		savesyscall = syscall_num
		skip_for_header = 0
		next
	}
	$1 ~ /^#[ 	]*else/ {
		print > sysent
		print > sysarg
		print > syscallnamestempfile
		print > sysprotoend
		print > audittempfile
		print > systraceargstempfile
		print > systraceargdesctempfile
		print > systracerettempfile
		syscall_num = savesyscall
		skip_for_header = 1
		next
	}
	$1 ~ /^#/ {
		print > sysent
		print > sysarg
		print > syscallnamestempfile
		print > sysprotoend
		print > audittempfile
		print > systraceargstempfile
		print > systraceargdesctempfile
		print > systracerettempfile
		skip_for_header = 0
		next
	}
	syscall_num != $1 {
		printf "%s: line %d: syscall number out of sync at %d\n",
		    infile, NR, syscall_num
		printf "line is:\n"
		print
		exit 1
	}
	function align_comment(linesize, location, thefile) {
		printf(" ") > thefile
		while (linesize < location) {
			printf(" ") > thefile
			linesize++
		}
	}
	function parserr(was, wanted) {
		printf "%s: line %d: unexpected %s (expected %s)\n",
		    infile, NR, was, wanted
		exit 1
	}
	
	function parseline() {
		funcname = ""
		current_field = 4	# skip number, audit event, type
		args_start = 0
		args_end = 0
		comments_start = 0
		comments_end = 0
		argc = 0
		argssize = "0"
		additional_comments = " "
		obs_comments = "_"

		# find start and end of call name and arguments
		if ($current_field != "{")
			parserr($current_field, "{")
		args_start = current_field
		current_field++
		while (current_field <= NF) {
			if ($current_field == "}") {
				args_end = current_field
				break
			}
			current_field++
		}
		if (args_end == 0) {
			printf "%s: line %d: invalid call name and arguments\n",
		    	infile, NR
			exit 1
		}

		# find start and end of optional comments
		current_field++
		if (current_field < NF && $current_field == "{") {
			comments_start = current_field
			while (current_field <= NF) {
				if ($current_field == "}") {
					comments_end = current_field
					break
				}
				current_field++
			}
			if (comments_end == 0) {
				printf "%s: line %d: invalid comments \n",
					infile, NR
				exit 1
			}
		}

		if ($args_end != "}")
			parserr($args_end, "}")
		args_end--
		if ($args_end != ";")
			parserr($args_end, ";")
		args_end--

		# skip any NO_SYSCALL_STUB qualifier
		if ($args_end == "NO_SYSCALL_STUB")
			args_end--

		if ($args_end != ")")
			parserr($args_end, ")")
		args_end--

		# extract additional comments
		if (comments_start != 0) {
			current_field = comments_start + 1
			while (current_field < comments_end) {
				additional_comments = additional_comments $current_field " "
				obs_comments = obs_comments $current_field "_"
				current_field++
			}
		}
		sub(/old/, "obs", obs_comments)
		obs_comments = substr(obs_comments, 1, length(obs_comments)-1)
		if (obs_comments == "_") {
			obs_comments = ""
		}

		# get function return type
		current_field = args_start + 1
		returntype = $current_field

		# get function name and set up to get arguments
		current_field++
		funcname = $current_field
		argalias = funcname "_args"
		if (substr(argalias, 1, 4) == "sys_") {
			argalias = substr(argalias, 5)
		}
		current_field++ # bump past function name

		if ($current_field != "(")
			parserr($current_field, "(")
		current_field++

		if (current_field == args_end) {
			if ($current_field != "void")
				parserr($current_field, "argument definition")
			return
		}

		# extract argument types and names
		while (current_field <= args_end) {
			argc++
			argtype[argc]=""
			ext_argtype[argc]=""
			oldf=""
			while (current_field < args_end && $(current_field + 1) != ",") {
				if (argtype[argc] != "" && oldf != "*") {
					argtype[argc] = argtype[argc] " ";
				}
				argtype[argc] = argtype[argc] $current_field;
				ext_argtype[argc] = argtype[argc];
				oldf = $current_field;
				current_field++
			}
			if (argtype[argc] == "")
				parserr($current_field, "argument definition")
			argname[argc] = $current_field;
			current_field += 2;			# skip name, and any comma
		}
		if (argc > 8) {
			printf "%s: line %d: too many arguments!\n", infile, NR
			exit 1
		}
		if (argc != 0)
			argssize = "AC(" argalias ")"
	}

	{
		auditev = $2;
	}

	{
		add_sysent_entry = 1
		add_sysnames_entry = 1
		add_sysheader_entry = 1
		add_sysproto_entry = 1


		if ($3 != "ALL") {
			files_keyword_OK = 0
			add_sysent_entry = 0
			add_sysnames_entry = 0
			add_sysheader_entry = 0
			add_sysproto_entry = 0
			
			if (match($3, "[T]") != 0) {
				add_sysent_entry = 1
				files_keyword_OK = 1
			}
			if (match($3, "[N]") != 0) {
				add_sysnames_entry = 1
				files_keyword_OK = 1
			}
			if (match($3, "[H]") != 0) {
				add_sysheader_entry = 1
				files_keyword_OK = 1
			}
			if (match($3, "[P]") != 0) {
				add_sysproto_entry = 1
				files_keyword_OK = 1
			}
			
			if (files_keyword_OK == 0) {
				printf "%s: line %d: unrecognized keyword %s\n", infile, NR, $2
				exit 1
			}
		}
		
		
		parseline()
		
		# output function argument structures to sysproto.h and build the
		# name of the appropriate argument mungers
		munge32 = "NULL"
		size32 = 0

		if ((funcname != "nosys" && funcname != "enosys") || (syscall_num == 0 && funcname == "nosys")) {
			printf("\t/* %s */\n\tcase %d: {\n", funcname, syscall_num) > systraceargstempfile
			printf("\t/* %s */\n\tcase %d:\n", funcname, syscall_num) > systraceargdesctempfile
			printf("\t/* %s */\n\tcase %d:\n", funcname, syscall_num) > systracerettempfile
			if (argc > 0) {
				printf("\t\tswitch(ndx) {\n") > systraceargdesctempfile
				printf("\t\tstruct %s *p = params;\n", argalias) > systraceargstempfile
				for (i = 1; i <= argc; i++) {
					arg = argtype[i]
					sub("__restrict$", "", arg)
					if (index(arg, "*") > 0)
						printf("\t\tcase %d:\n\t\t\tp = \"userland %s\";\n\t\t\tbreak;\n", i - 1, arg) > systraceargdesctempfile
					else
						printf("\t\tcase %d:\n\t\t\tp = \"%s\";\n\t\t\tbreak;\n", i - 1, arg) > systraceargdesctempfile
					if (index(arg, "*") > 0 || arg == "caddr_t" || arg == "caddr_ut")
                        printf("\t\tuarg[%d] = (uint64_t) p->%s; /* %s */\n", \
							i - 1, \
							argname[i], arg) > systraceargstempfile
					else if (substr(arg, 1, 1) == "u" || arg == "size_t" || arg == "size_ut")
                        printf("\t\tuarg[%d] = (uint64_t) p->%s; /* %s */\n", \
							i - 1, \
							argname[i], arg) > systraceargstempfile
					else
                        printf("\t\tiarg[%d] = (int64_t) p->%s; /* %s */\n", \
							i - 1, \
							argname[i], arg) > systraceargstempfile
				}
				printf("\t\tdefault:\n\t\t\tbreak;\n\t\t};\n") > systraceargdesctempfile

			}
			printf("\t\tbreak;\n\t}\n", argc) > systraceargstempfile
			printf("\t\tif (ndx == 0 || ndx == 1)\n") > systracerettempfile
			printf("\t\t\tp = \"%s\";\n", returntype) > systracerettempfile
			printf("\t\tbreak;\n") > systracerettempfile
			printf("\t\tbreak;\n") > systraceargdesctempfile
			if (argc != 0) {
				if (add_sysproto_entry == 1) {
					printf("struct %s {\n", argalias) > sysarg
				}
				munge32 = "munge_"
				for (i = 1; i <= argc; i++) {
					# Build name of argument munger.
					# We account for all sys call argument types here.
					# This is where you add any new types.  With LP64 support
					# each argument consumes 64-bits.  
					# see .../xnu/bsd/dev/munge.c for munge argument types.
					if (argtype[i] == "long") {
						ext_argtype[i] = "user_long_t";
						munge32 = munge32 "s"
						size32 += 4
					}
					else if (argtype[i] == "u_long") {
						ext_argtype[i] = "user_ulong_t";
						munge32 = munge32 "w"
						size32 += 4
					}
					else if (argtype[i] == "size_t") {
						ext_argtype[i] = "user_size_t";
						munge32 = munge32 "w"
						size32 += 4
					}
					else if (argtype[i] == "size_ut") {
						ext_argtype[i] = "user_size_ut";
						munge32 = munge32 "w"
						size32 += 4
					}
					else if (argtype[i] == "ssize_t") {
						ext_argtype[i] = "user_ssize_t";
						munge32 = munge32 "s"
						size32 += 4
					}
					else if (argtype[i] == "user_ssize_t" || argtype[i] == "user_long_t") {
						munge32 = munge32 "s"
						size32 += 4
					}
					else if (argtype[i] == "user_addr_t" || argtype[i] == "user_size_t" ||
						argtype[i] == "user_ulong_t") {
						munge32 = munge32 "w"
						size32 += 4
					}
					else if (argtype[i] == "caddr_t" ||
						  argtype[i] == "semun_t" || argtype[i] == "uuid_t" ||
							match(argtype[i], "[\*]") != 0) {
						ext_argtype[i] = "user_addr_t";
						munge32 = munge32 "w"
						size32 += 4
					}
				  else if (argtype[i] == "caddr_ut") {
						ext_argtype[i] = "user_addr_ut";
						munge32 = munge32 "w"
						size32 += 4
				  }
					else if (argtype[i] == "int" || argtype[i] == "u_int" ||
							 argtype[i] == "uid_t" || argtype[i] == "pid_t" ||
							 argtype[i] == "id_t" || argtype[i] == "idtype_t" ||
							 argtype[i] == "socklen_t" || argtype[i] == "uint32_t" || argtype[i] == "int32_t" ||
							 argtype[i] == "sigset_t" || argtype[i] == "gid_t" || argtype[i] == "unsigned int" ||
							 argtype[i] == "mode_t" || argtype[i] == "key_t" ||
							 argtype[i] == "mach_port_name_t" || argtype[i] == "au_asid_t" ||
							 argtype[i] == "sae_associd_t" || argtype[i] == "sae_connid_t") {
						munge32 = munge32 "w"
						size32 += 4
					}
					else if (argtype[i] == "off_t" || argtype[i] == "int64_t" || argtype[i] == "uint64_t") {
						munge32 = munge32 "l"
						size32 += 8
					}
					else {
						printf "%s: line %d: invalid type \"%s\" \n", 
							infile, NR, argtype[i]
						printf "You need to add \"%s\" into the type checking code. \n", 
							 argtype[i]
						exit 1
					}
					if (add_sysproto_entry == 1) {
						printf("\tchar %s_l_[PADL_(%s)]; " \
							"%s %s; char %s_r_[PADR_(%s)];\n",
							argname[i], ext_argtype[i],
							ext_argtype[i], argname[i],
							argname[i], ext_argtype[i]) > sysarg
					}
				}
				if (add_sysproto_entry == 1) {
					printf("};\n") > sysarg
				}
			}
			else if (add_sysproto_entry == 1) { 
				printf("struct %s {\n\tint32_t dummy;\n};\n", argalias) > sysarg
			}
		}
		
		# output to init_sysent.c
		tempname = funcname
		if (add_sysent_entry == 0) {
			argssize = "0"
			munge32 = "NULL"
			munge_ret = "_SYSCALL_RET_NONE"
			if (tempname != "enosys") {
				tempname = "nosys"
			}
		}
		else {
			# figure out which return value type to munge
			if (returntype == "user_addr_t") {
				munge_ret = "_SYSCALL_RET_ADDR_T"
			}
			else if (returntype == "user_ssize_t") {
				munge_ret = "_SYSCALL_RET_SSIZE_T"
			}
			else if (returntype == "user_size_t") {
				munge_ret = "_SYSCALL_RET_SIZE_T"
			}
			else if (returntype == "int") {
				munge_ret = "_SYSCALL_RET_INT_T"
			}
			else if (returntype == "u_int" || returntype == "mach_port_name_t") {
				munge_ret = "_SYSCALL_RET_UINT_T"
			}
			else if (returntype == "uint32_t") {
				munge_ret = "_SYSCALL_RET_UINT_T"
			}
			else if (returntype == "uint64_t") {
				munge_ret = "_SYSCALL_RET_UINT64_T"
			}
			else if (returntype == "off_t") {
				munge_ret = "_SYSCALL_RET_OFF_T"
			}
			else if (returntype == "void") {
				munge_ret = "_SYSCALL_RET_NONE"
			}
			else {
				printf "%s: line %d: invalid return type \"%s\" \n", 
					infile, NR, returntype
				printf "You need to add \"%s\" into the return type checking code. \n", 
					 returntype
				exit 1
			}
		}

		printf("#if CONFIG_REQUIRES_U32_MUNGING\n") > sysent
		printf("\t{ \(sy_call_t *\)(void (*)(void))%s, %s, %s, %s, %s},",
				tempname, munge32, munge_ret, argssize, size32) > sysent
		linesize = length(tempname) + length(munge32) + \
			length(munge_ret) + length(argssize) + length(size32) + 28
		align_comment(linesize, 88, sysent)
		printf("/* %d = %s%s*/\n", syscall_num, funcname, additional_comments) > sysent
		printf("#else\n") > sysent
		printf("\t{ \(sy_call_t *\)(void (*)(void))%s, %s, %s, %s},\n",
				tempname, munge_ret, argssize, size32) > sysent
		printf("#endif\n") > sysent
		
		# output to syscalls.c
		if (add_sysnames_entry == 1) {
			tempname = funcname
			if (substr(tempname, 1, 4) == "sys_") {
				tempname = substr(tempname, 5)
			}
			if (funcname == "nosys" || funcname == "enosys") {
				if (syscall_num == 0)
					tempname = "syscall"
				else
					tempname = "#" syscall_num
			}
			printf("\t\"%s\", ", tempname) > syscallnamestempfile
			linesize = length(tempname) + 8
			align_comment(linesize, 25, syscallnamestempfile)
			if (substr(tempname,1,1) == "#") {
				printf("/* %d =%s*/\n", syscall_num, additional_comments) > syscallnamestempfile
			}
			else {
				printf("/* %d = %s%s*/\n", syscall_num, tempname, additional_comments) > syscallnamestempfile
			}
		}

		# output to syscalls.h
		if (add_sysheader_entry == 1) {
			tempname = funcname
			if (substr(tempname, 1, 4) == "sys_") {
				tempname = substr(tempname, 5)
			}
			if (syscall_num == 0) {
				tempname = "syscall"
			}
			if (tempname != "nosys" && tempname != "enosys") {
				printf("#define\t%s%s", syscallprefix, tempname) > syshdrtempfile
				linesize = length(syscallprefix) + length(tempname) + 12
				align_comment(linesize, 30, syshdrtempfile)
				printf("%d\n", syscall_num) > syshdrtempfile
			}
			else if (skip_for_header == 0) {
				printf("\t\t\t/* %d %s*/\n", syscall_num, additional_comments) > syshdrtempfile
			}
		}
		
		# output function prototypes to sysproto.h
		if (add_sysproto_entry == 1) {
			if (funcname =="exit") {
				printf("void %s(struct proc *, struct %s *, int32_t *);\n", 
						funcname, argalias) > sysprotoend
			}
			else if ((funcname != "nosys" && funcname != "enosys") || (syscall_num == 0 && funcname == "nosys")) {
				printf("int %s(struct proc *, struct %s *, %s *);\n", 
						funcname, argalias, returntype) > sysprotoend
			}
		}

		# output to audit_kevents.c
		printf("\t%s,\t\t", auditev) > audittempfile
		printf("/* %d = %s%s*/\n", syscall_num, tempname, additional_comments) > audittempfile 

		tempname = funcname
		if (skip_for_header == 0) {
			if (tempname == "nosys" || tempname == "enosys") {
				if (obs_comments == "") {
					printf("0x40c%04x\tBSC_#%d%s\n", (syscall_num*4), syscall_num, obs_comments) > tracecodetempfile 
				} else {
					printf("0x40c%04x\tBSC%s\n", (syscall_num*4), obs_comments) > tracecodetempfile 
				}
			} else {	
				sub(/^_+/, "", tempname)
				printf("0x40c%04x\tBSC_%s\n", (syscall_num*4), tempname) > tracecodetempfile 
			}
		}

		syscall_num++
		next
	}

	END {
		printf "#define AC(name) (sizeof(struct name) / sizeof(syscall_arg_t))\n" > sysinc
		printf "\n" > sysinc

		printf("\n__END_DECLS\n") > sysprotoend
		printf("#undef PAD_\n") > sysprotoend
		printf("#undef PADL_\n") > sysprotoend
		printf("#undef PADR_\n") > sysprotoend
		printf "\n#endif /* __APPLE_API_PRIVATE */\n" > sysprotoend
		printf "#endif /* KERNEL */\n" > sysprotoend
		printf("\n#endif /* !%s */\n", sysproto_h) > sysprotoend

		printf("};\n") > sysent
		printf("const unsigned int nsysent = sizeof(sysent) / sizeof(sysent[0]);\n") > sysent

		printf("};\n") > syscallnamestempfile
		printf("#define\t%sMAXSYSCALL\t%d\n", syscallprefix, syscall_num) \
		    > syshdrtempfile
		printf("#define\t%sinvalid\t%d\n", syscallprefix, 63) \
		    > syshdrtempfile
		printf("\n#endif /* __APPLE_API_PRIVATE */\n") > syshdrtempfile
		printf("#endif /* !%s */\n", syscall_h) > syshdrtempfile
		printf("};\n\n") > audittempfile
		printf("#endif /* AUDIT */\n") > audittempfile

		printf "\tdefault:\n\t\tbreak;\n\t};\n}\n" > systraceargstempfile
		printf "\tdefault:\n\t\tbreak;\n\t};\n\tif (p != NULL)\n\t\tstrlcpy(desc, p, descsz);\n}\n" > systraceargdesctempfile
		printf "\tdefault:\n\t\tbreak;\n\t};\n\tif (p != NULL)\n\t\tstrlcpy(desc, p, descsz);\n}\n" > systracerettempfile
	} '

if [ $output_syscalltablefile -eq 1 ]; then
    cat $syslegal > $syscalltablefile
    cat $sysinc $sysent >> $syscalltablefile
fi

if [ $output_syscallnamesfile -eq 1 ]; then
    cat $syslegal $syscallnamestempfile > $syscallnamesfile
fi

if [ $output_sysprotofile -eq 1 ]; then
    cat $syslegal $sysarg $sysprotoend > $sysprotofile
fi

if [ $output_syshdrfile -eq 1 ]; then
    cat $syslegal $syshdrtempfile > $syshdrfile
fi

if [ $output_auditevfile -eq 1 ]; then
    cat $syslegal $audittempfile > $auditevfile
fi

if [ $output_systrace -eq 1 ]; then
	cat $systraceargstempfile > $systraceargsfile
	cat $systraceargdesctempfile >> $systraceargsfile
	cat $systracerettempfile >> $systraceargsfile
fi

if [ $output_tracecodes -eq 1 ]; then
	if [ $use_stdout -eq 1 ]; then
		cat $tracecodetempfile
	else
		cat $tracecodetempfile > $tracecodename
	fi
fi

