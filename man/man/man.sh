#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
#  Copyright (c) 2010 Gordon Tetlow
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
#  THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
#  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
#  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
#  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
#  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
#  SUCH DAMAGE.
#
# $FreeBSD$

# Usage: add_to_manpath path
# Adds a variable to manpath while ensuring we don't have duplicates.
# Returns true if we were able to add something. False otherwise.
add_to_manpath() {
	case "$manpath" in
	*:$1)	decho "  Skipping duplicate manpath entry $1" 2 ;;
	$1:*)	decho "  Skipping duplicate manpath entry $1" 2 ;;
	*:$1:*)	decho "  Skipping duplicate manpath entry $1" 2 ;;
	*)	if [ -d "$1" ]; then
			decho "  Adding $1 to manpath"
			manpath="$manpath:$1"
			return 0
		fi
		;;
	esac

	return 1
}

# Usage: build_manlocales
# Builds a correct MANLOCALES variable.
build_manlocales() {
	# If the user has set manlocales, who are we to argue.
	if [ -n "$MANLOCALES" ]; then
		return
	fi

	parse_configs

	# Trim leading colon
	MANLOCALES=${manlocales#:}

	decho "Available manual locales: $MANLOCALES"
}

# Usage: build_mansect
# Builds a correct MANSECT variable.
build_mansect() {
	# If the user has set mansect, who are we to argue.
	if [ -n "$MANSECT" ]; then
		return
	fi

	parse_configs

	# Trim leading colon
	MANSECT=${mansect#:}

	if [ -z "$MANSECT" ]; then
		MANSECT=$man_default_sections
	fi
	decho "Using manual sections: $MANSECT"
}

# Usage: build_manpath
# Builds a correct MANPATH variable.
build_manpath() {
	local IFS

	# If the user has set a manpath, who are we to argue.
	if [ -n "$MANPATH" ]; then
		case "$MANPATH" in
		*:) PREPEND_MANPATH=${MANPATH} ;;
		:*) APPEND_MANPATH=${MANPATH} ;;
		*::*)
			PREPEND_MANPATH=${MANPATH%%::*}
			APPEND_MANPATH=${MANPATH#*::}
			;;
		*) return ;;
		esac
	fi

	if [ -n "$PREPEND_MANPATH" ]; then
		IFS=:
		for path in $PREPEND_MANPATH; do
			add_to_manpath "$path"
		done
		unset IFS
	fi

	search_path
#ifdef __APPLE__
	xcode_path
#endif

	decho "Adding default manpath entries"
	IFS=:
	for path in $man_default_path; do
		add_to_manpath "$path"
	done
	unset IFS

	parse_configs

	if [ -n "$APPEND_MANPATH" ]; then
		IFS=:
		for path in $APPEND_MANPATH; do
			add_to_manpath "$path"
		done
		unset IFS
	fi
	# Trim leading colon
	MANPATH=${manpath#:}

	decho "Using manual path: $MANPATH"
}

# Usage: check_cat catglob
# Checks to see if a cat glob is available.
check_cat() {
	if exists "$1"; then
		use_cat=yes
		catpage=$found
		setup_cattool $catpage
		decho "    Found catpage $catpage"
		return 0
	else
		return 1
	fi
}

# Usage: check_man manglob catglob
# Given 2 globs, figures out if the manglob is available, if so, check to
# see if the catglob is also available and up to date.
check_man() {
	if exists "$1"; then
		# We have a match, check for a cat page
		manpage=$found
		setup_cattool $manpage
		decho "    Found manpage $manpage"

		if [ -n "${use_width}" ]; then
			# non-standard width
			unset use_cat
			decho "    Skipping catpage: non-standard page width"
#ifdef __APPLE__
		elif [ -z "$2" ]; then
			# nothing
			decho "    Skipping catpage"
#endif
		elif exists "$2" && is_newer $found $manpage; then
			# cat page found and is newer, use that
			use_cat=yes
			catpage=$found
			setup_cattool $catpage
			decho "    Using catpage $catpage"
		else
			# no cat page or is older
			unset use_cat
			decho "    Skipping catpage: not found or old"
		fi
		return 0
	fi

	return 1
}

# Usage: decho "string" [debuglevel]
# Echoes to stderr string prefaced with -- if high enough debuglevel.
decho() {
	if [ $debug -ge ${2:-1} ]; then
		echo "-- $1" >&2
	fi
}

# Usage: exists glob
# Returns true if glob resolves to a real file.
exists() {
	local IFS

	# Don't accidentally inherit callers IFS (breaks perl manpages)
	unset IFS

	# Use some globbing tricks in the shell to determine if a file
	# exists or not.
	set +f

	for file in "$1"*
	do
		if [ -r "$file" ]; then
			found="$file"
			set -f
			return 0
		fi
	done
	set -f

	return 1
}

# Usage: find_file path section subdir pagename
# Returns: true if something is matched and found.
# Search the given path/section combo for a given page.
find_file() {
	local manroot catroot mann man0 catn cat0

	manroot="$1/man$2"
	catroot="$1/cat$2"
	if [ -n "$3" ]; then
		manroot="$manroot/$3"
		catroot="$catroot/$3"
	fi

	if [ ! -d "$manroot" -a ! -d "$catroot" ]; then
		return 1
	fi
	decho "  Searching directory $manroot" 2

	mann="$manroot/$4.$2"
	man0="$manroot/$4.0"
	catn="$catroot/$4.$2"
	cat0="$catroot/$4.0"

	# This is the behavior as seen by the original man utility.
	# Let's not change that which doesn't seem broken.
	if check_man "$mann" "$catn"; then
		return 0
	elif check_man "$man0" "$cat0"; then
		return 0
	elif check_cat "$catn"; then
		return 0
	elif check_cat "$cat0"; then
		return 0
	fi

	return 1
}

# Usage: is_newer file1 file2
# Returns true if file1 is newer than file2 as calculated by mtime.
is_newer() {
	if ! [ "$1" -ot "$2" ]; then
		decho "    mtime: $1 not older than $2" 3
		return 0
	else
		decho "    mtime: $1 older than $2" 3
		return 1
	fi
}

# Usage: manpath_parse_args "$@"
# Parses commandline options for manpath.
manpath_parse_args() {
	local cmd_arg

	OPTIND=1
	while getopts 'Ldq' cmd_arg; do
		case "${cmd_arg}" in
		L)	Lflag=Lflag ;;
		d)	debug=$(( $debug + 1 )) ;;
		q)	qflag=qflag ;;
		*)	manpath_usage ;;
		esac
	done >&2
}

# Usage: manpath_usage
# Display usage for the manpath(1) utility.
manpath_usage() {
	echo 'usage: manpath [-Ldq]' >&2
	exit 1
}

# Usage: manpath_warnings
# Display some warnings to stderr.
manpath_warnings() {
	if [ -n "$Lflag" -a -n "$MANLOCALES" ]; then
		echo "(Warning: MANLOCALES environment variable set)" >&2
	fi
}

# Usage: man_check_for_so page path
# Returns: True if able to resolve the file, false if it ended in tears.
# Detects the presence of the .so directive and causes the file to be
# redirected to another source file.
man_check_for_so() {
	local IFS line tstr

	unset IFS
	if [ -n "$catpage" ]; then
		return 0
	fi

	# We need to loop to accommodate multiple .so directives.
	while true
	do
#ifdef __APPLE__
		line=$($cattool "$manpage" | head -1)
#else
#		line=$($cattool $manpage | head -1)
#endif
		case "$line" in
		.so*)	trim "${line#.so}"
			decho "$manpage includes $tstr"
			# Glob and check for the file.
#ifdef __APPLE__
			if ! check_man "$path/$tstr" "" &&
			   ! check_man "${manpage%/*}/$tstr" ""; then
#else
#			if ! check_man "$path/$tstr" ""; then
#endif
				decho "  Unable to find $tstr"
				return 1
			fi
			;;
		*)	break ;;
		esac
	done

	return 0
}

#ifdef __APPLE__
# Usage: man_display_page path
#else
# Usage: man_display_page
#endif
# Display either the manpage or catpage depending on the use_cat variable
man_display_page() {
#ifdef __APPLE__
	local IFS path pipeline testline
#else
#	local IFS pipeline testline
#endif
	# We are called with IFS set to colon. This causes really weird
	# things to happen for the variables that have spaces in them.
	unset IFS

#ifdef __APPLE__
	path=$1
#endif

	# If we are supposed to use a catpage and we aren't using troff(1)
	# just zcat the catpage and we are done.
	if [ -z "$tflag" -a -n "$use_cat" ]; then
		if [ -n "$wflag" ]; then
			echo "$catpage (source: $manpage)"
			ret=0
		else
			if [ $debug -gt 0 ]; then
#ifdef __APPLE__
				decho "Command: $cattool \"$catpage\" | $MANPAGER"
#else
#				decho "Command: $cattool $catpage | $MANPAGER"
#endif
				ret=0
			else
#ifdef __APPLE__
				eval "$cattool \"$catpage\" | $MANPAGER"
#else
#				eval "$cattool $catpage | $MANPAGER"
#endif
				ret=$?
			fi
		fi
		return
	fi

	# Okay, we are using the manpage, do we just need to output the
	# name of the manpage?
	if [ -n "$wflag" ]; then
		echo "$manpage"
		ret=0
		return
	fi

	if [ -n "$use_width" ]; then
		mandoc_args="-O width=${use_width}"
	fi
	testline="$MANDOC -Tlint -Wunsupp >/dev/null 2>&1"
	if [ -n "$tflag" ]; then
		pipeline="$MANDOC -Tps $mandoc_args"
	else
		pipeline="$MANDOC $mandoc_args | $MANPAGER"
	fi

#ifdef __APPLE__
	if ! eval "$cattool \"$manpage\" | $testline" ;then
#else
#	if ! eval "$cattool $manpage | $testline" ;then
#endif
#ifdef __APPLE__
		# We don't provide defaults for these, so if one of them is set
		# then we can likely go forth.
		if [ -n "$NROFF" -o -n "$TROFF" ]; then
			man_display_page_groff $path
			return
#else
#		if which -s groff; then
#			man_display_page_groff
#endif
		else
#ifdef __APPLE__
			echo "This manpage is not compatible with mandoc(1) and might display incorrectly." >&2

			# Leave sufficient time to note the above warning.
			sleep 1
#else
#			echo "This manpage needs groff(1) to be rendered" >&2
#			echo "First install groff(1): " >&2
#			echo "pkg install groff " >&2
#			ret=1
#endif
		fi
#ifndef __APPLE__
#		return
#endif
	fi

	if [ $debug -gt 0 ]; then
#ifdef __APPLE__
		decho "Command: cd \"$path\" && $cattool \"$manpage\" | $pipeline"
#else
#		decho "Command: $cattool $manpage | $pipeline"
#endif
		ret=0
	else
#ifdef __APPLE__
		(cd "$path" && eval "$cattool \"$manpage\" | $pipeline")
#else
#		eval "$cattool $manpage | $pipeline"
#endif
		ret=$?
	fi
}

#ifdef __APPLE__
# Usage: man_display_page_groff path
#else
# Usage: man_display_page_groff
#endif
# Display the manpage using groff
man_display_page_groff() {
#ifdef __APPLE__
	local IFS l nroff_dev path pipeline preproc_arg tool
#else
#	local EQN NROFF PIC TBL TROFF REFER VGRIND
#	local IFS l nroff_dev pipeline preproc_arg tool
#endif

#ifdef __APPLE__
	path=$1
#endif

#ifdef __APPLE__
	# rdar://problem/113400634 -  Naive path checking; all of the tools we
	# use need to be fully-specified in man.conf.
	local _toolpath
	for tool in EQN NROFF PIC TBL TROFF REFER VGRIND; do
		eval "_toolpath=\${${tool}}"
		if [ -n "$_toolpath" ]; then
			case "$_toolpath" in
			/*)
				;;
			*)
				1>&2 echo "${tool} must be an absolute path (currently: ${_toolpath})"
				ret=1
				return
				;;
			esac
		elif [ "$tool" != "VGRIND" ]; then
			1>&2 echo "$tool must be set to render groff manpages"
			ret=1
			return
		fi
	done
#endif

	# So, we really do need to parse the manpage. First, figure out the
	# device flag (-T) we have to pass to eqn(1) and groff(1). Then,
	# setup the pipeline of commands based on the user's request.

	# If the manpage is from a particular charset, we need to setup nroff
	# to properly output for the correct device.
	case "${manpage}" in
	*.${man_charset}/*)
		# I don't pretend to know this; I'm just copying from the
		# previous version of man(1).
		case "$man_charset" in
		KOI8-R)		nroff_dev="koi8-r" ;;
		ISO8859-1)	nroff_dev="latin1" ;;
		ISO8859-15)	nroff_dev="latin1" ;;
		UTF-8)		nroff_dev="utf8" ;;
		*)		nroff_dev="ascii" ;;
		esac

		NROFF="$NROFF -T$nroff_dev"
		EQN="$EQN -T$nroff_dev"

		# Iff the manpage is from the locale and not just the charset,
		# then we need to define the locale string.
		case "${manpage}" in
		*/${man_lang}_${man_country}.${man_charset}/*)
			NROFF="$NROFF -dlocale=$man_lang.$man_charset"
			;;
		*/${man_lang}.${man_charset}/*)
			NROFF="$NROFF -dlocale=$man_lang.$man_charset"
			;;
		esac

		# Allow language specific calls to override the default
		# set of utilities.
		l=$(echo $man_lang | tr [:lower:] [:upper:])
		for tool in EQN NROFF PIC TBL TROFF REFER VGRIND; do
			eval "$tool=\${${tool}_$l:-\$$tool}"
		done
		;;
	*)	NROFF="$NROFF -Tascii"
		EQN="$EQN -Tascii"
		;;
	esac

	if [ -z "$MANCOLOR" ]; then
		NROFF="$NROFF -P-c"
	fi

	if [ -n "${use_width}" ]; then
		NROFF="$NROFF -rLL=${use_width}n -rLT=${use_width}n"
	fi

	if [ -n "$MANROFFSEQ" ]; then
		set -- -$MANROFFSEQ
		OPTIND=1
		while getopts 'egprtv' preproc_arg; do
			case "${preproc_arg}" in
			e)	pipeline="$pipeline | $EQN" ;;
			g)	;; # Ignore for compatibility.
			p)	pipeline="$pipeline | $PIC" ;;
			r)	pipeline="$pipeline | $REFER" ;;
			t)	pipeline="$pipeline | $TBL" ;;
			v)	pipeline="$pipeline | $VGRIND" ;;
			*)	usage ;;
			esac
		done
		# Strip the leading " | " from the resulting pipeline.
		pipeline="${pipeline#" | "}"
	else
		pipeline="$TBL"
	fi

	if [ -n "$tflag" ]; then
		pipeline="$pipeline | $TROFF"
	else
		pipeline="$pipeline | $NROFF | $MANPAGER"
	fi

	if [ $debug -gt 0 ]; then
#ifdef __APPLE_
		decho "Command: cd $path && $cattool \"$manpage\" | $pipeline"
#else
#		decho "Command: $cattool $manpage | $pipeline"
#endif
		ret=0
	else
#ifdef __APPLE__
		(cd $path && eval "$cattool \"$manpage\" | $pipeline")
#else
#		eval "$cattool $manpage | $pipeline"
#endif
		ret=$?
	fi
}

# Usage: man_find_and_display page
# Search through the manpaths looking for the given page.
man_find_and_display() {
	local candidate candidate_manpage found_page locpath p path sect

	# Check to see if it's a file. But only if it has a '/' in
	# the filename.
	case "$1" in
	*/*)	if [ -f "$1" -a -r "$1" ]; then
			decho "Found a usable page, displaying that"
			unset use_cat
			manpage="$1"
#ifdef __APPLE__
			path=$(dirname "$manpage")
			setup_cattool "$manpage"
			if man_check_for_so "$manpage" "$path"; then
#else
#			setup_cattool $manpage
#			if man_check_for_so $manpage $(dirname $manpage); then
#endif
				found_page=yes
#ifdef __APPLE__
				manpage=$(realpath "$manpage")
				man_display_page "$path"
#else
#				man_display_page
#endif
			fi
			return
		fi
		;;
	esac

	IFS=:
	for sect in $MANSECT; do
		decho "Searching section $sect" 2
		for path in $MANPATH; do
			for locpath in $locpaths; do
				p=$path/$locpath
				p=${p%/.} # Rid ourselves of the trailing /.

				# Check if there is a MACHINE specific manpath.
#ifndef __APPLE__
#				if find_file $p $sect $MACHINE "$1"; then
#					if man_check_for_so $manpage $p; then
#						found_page=yes
#						man_display_page
#						if [ -n "$aflag" ]; then
#							continue 2
#						else
#							return
#						fi
#					fi
#				fi
#endif

				# Check if there is a MACHINE_ARCH
				# specific manpath.
#ifndef __APPLE__
#				if find_file $p $sect $MACHINE_ARCH "$1"; then
#					if man_check_for_so $manpage $p; then
#						found_page=yes
#						man_display_page
#						if [ -n "$aflag" ]; then
#							continue 2
#						else
#							return
#						fi
#					fi
#				fi
#endif

				# Check plain old manpath.
				if find_file $p $sect '' "$1"; then
					if man_check_for_so $manpage $p; then
#ifdef __APPLE__
						case "$manpage" in
						*.$man_preferred_section)
							candidate="$p"
							candidate_manpage=$manpage
							found_page=yes
							;;
						*)
							# Use first candidate if we
							# don't find one with
							# the "preferred"
							# section (if one was
							# specified in args)
							if [ -z "$candidate" ]; then
								candidate="$p"
								candidate_manpage=$manpage
								found_page=maybe
							fi
							;;
						esac
#else
#						found_page=yes
#						man_display_page
#endif
						if [ -n "$aflag" ]; then
#ifdef __APPLE__
							# If we're displaying
							# them all anyways,
							# just do it now.
							man_display_page $p
#endif
							continue 2
						else
#ifdef __APPLE__
							# Keep cycling until we
							# either hit the end or
							# have a definitive
							# match.
							if [ "$foundpage" != "yes" ]; then
								continue 2
							fi
							break 2
#else
#							return
#endif
						fi
					fi
				fi
			done
		done
	done
	unset IFS

	# Nothing? Well, we are done then.
	if [ -z "$found_page" ]; then
		echo "No manual entry for $1" >&2
		ret=1
		return
#ifdef __APPLE__
	elif [ -z "$aflag" ]; then
		# We deferred rendering in case we find a better candidate while
		# searching.
		manpage="$candidate_manpage"
		man_display_page "$candidate"
#endif
	fi
}

# Usage: man_parse_opts "$@"
# Parses commandline options for man.
man_parse_opts() {
	local cmd_arg

	OPTIND=1
#ifdef __APPLE__
	while getopts 'C:M:P:S:adfhkm:op:tw' cmd_arg; do
#else
#	while getopts 'M:P:S:adfhkm:op:tw' cmd_arg; do
#endif
		case "${cmd_arg}" in
#ifdef __APPLE__
		C)	config_global=$OPTARG ;;
#endif
		M)	MANPATH=$OPTARG ;;
#ifdef __APPLE__
		P)	MANPAGER=$OPTARG; WHATISPAGER=$OPTARG ;;
#else
#		P)	MANPAGER=$OPTARG ;;
#endif
		S)	MANSECT=$OPTARG ;;
		a)	aflag=aflag ;;
		d)	debug=$(( $debug + 1 )) ;;
		f)	fflag=fflag ;;
		h)	man_usage 0 ;;
		k)	kflag=kflag ;;
#ifdef __APPLE__
		m)	;;
#else
#		m)	mflag=$OPTARG ;;
#endif
		o)	oflag=oflag ;;
		p)	MANROFFSEQ=$OPTARG ;;
		t)	tflag=tflag ;;
		w)	wflag=wflag ;;
		*)	man_usage ;;
		esac
	done >&2

	shift $(( $OPTIND - 1 ))

	# Check the args for incompatible options.
	case "${fflag}${kflag}${tflag}${wflag}" in
	fflagkflag*)	echo "Incompatible options: -f and -k"; man_usage ;;
	fflag*tflag*)	echo "Incompatible options: -f and -t"; man_usage ;;
	fflag*wflag)	echo "Incompatible options: -f and -w"; man_usage ;;
	*kflagtflag*)	echo "Incompatible options: -k and -t"; man_usage ;;
	*kflag*wflag)	echo "Incompatible options: -k and -w"; man_usage ;;
	*tflagwflag)	echo "Incompatible options: -t and -w"; man_usage ;;
	esac

	# Short circuit for whatis(1) and apropos(1)
	if [ -n "$fflag" ]; then
		do_whatis "$@"
		exit
	fi

	if [ -n "$kflag" ]; then
		do_apropos "$@"
		exit
	fi
}

# Usage: man_setup
# Setup various trivial but essential variables.
man_setup() {
	# Setup machine and architecture variables.
#ifndef __APPLE__
#	if [ -n "$mflag" ]; then
#		MACHINE_ARCH=${mflag%%:*}
#		MACHINE=${mflag##*:}
#	fi
#	if [ -z "$MACHINE_ARCH" ]; then
#		MACHINE_ARCH=$($SYSCTL -n hw.machine_arch)
#	fi
#	if [ -z "$MACHINE" ]; then
#		MACHINE=$($SYSCTL -n hw.machine)
#	fi
#	decho "Using architecture: $MACHINE_ARCH:$MACHINE"
#endif

	setup_pager
	build_manpath
	build_mansect
	man_setup_locale
	man_setup_width
}

# Usage: man_setup_width
# Set up page width.
man_setup_width() {
	local sizes

	unset use_width
#ifdef __APPLE__
	if [ -z "$MANWIDTH" -a -t 0 -a -t 1 ]; then
		MANWIDTH=tty
	fi
#endif
	case "$MANWIDTH" in
	[0-9]*)
		if [ "$MANWIDTH" -gt 0 2>/dev/null ]; then
			use_width=$MANWIDTH
		fi
		;;
	[Tt][Tt][Yy])
		if { sizes=$($STTY size 0>&3 2>/dev/null); } 3>&1; then
			set -- $sizes
#ifdef __APPLE__
			# The upstream version uses an 80 column minimum here,
			# but there are smaller widths that are still reasonable
			# to consume manpage content on.  Furthermore, the GNU
			# version macOS has historically used didn't apply any
			# limitations here.  Follow suit, but keep upstream's
			# general rule of reducing by two columns -- this
			# provides for a margin that makes it a little easier to
			# read.
			if [ $2 -gt 2 ]; then
#else
#			if [ $2 -gt 80 ]; then
#endif
				use_width=$(($2-2))
			fi
		fi
		;;
	esac
	if [ -n "$use_width" ]; then
		decho "Using non-standard page width: ${use_width}"
	else
		decho 'Using standard page width'
	fi
}

# Usage: man_setup_locale
# Setup necessary locale variables.
man_setup_locale() {
	local lang_cc
	local locstr

	locpaths='.'
	man_charset='US-ASCII'

	# Setup locale information.
	if [ -n "$oflag" ]; then
		decho 'Using non-localized manpages'
	else
		# Use the locale tool to give us proper locale information
		eval $( $LOCALE )

		if [ -n "$LANG" ]; then
			locstr=$LANG
		else
			locstr=$LC_CTYPE
		fi

		case "$locstr" in
		C)		;;
		C.UTF-8)	;;
		POSIX)		;;
		[a-z][a-z]_[A-Z][A-Z]\.*)
				lang_cc="${locstr%.*}"
				man_lang="${locstr%_*}"
				man_country="${lang_cc#*_}"
				man_charset="${locstr#*.}"
				locpaths="$locstr"
				locpaths="$locpaths:$man_lang.$man_charset"
				if [ "$man_lang" != "en" ]; then
					locpaths="$locpaths:en.$man_charset"
				fi
				locpaths="$locpaths:."
				;;
		*)		echo 'Unknown locale, assuming C' >&2
				;;
		esac
	fi

	decho "Using locale paths: $locpaths"
}

# Usage: man_usage [exitcode]
# Display usage for the man utility.
man_usage() {
	echo 'Usage:'
	echo ' man [-adho] [-t | -w] [-M manpath] [-P pager] [-S mansect]'
	echo '     [-m arch[:machine]] [-p [eprtv]] [mansect] page [...]'
	echo ' man -f page [...] -- Emulates whatis(1)'
	echo ' man -k page [...] -- Emulates apropos(1)'

#ifdef __APPLE__
	# rdar://problem/86345977 - conformance failure, and returning failure
	# from here is a valid approach that other utilities take as well.
	exit 1
#else
	# When exit'ing with -h, it's not an error.
#	exit ${1:-1}
#endif
}

# Usage: parse_configs
# Reads the end-user adjustable config files.
parse_configs() {
	local IFS file files

	if [ -n "$parsed_configs" ]; then
		return
	fi

	unset IFS

	# Read the global config first in case the user wants
	# to override config_local.
	if [ -r "$config_global" ]; then
		parse_file "$config_global"
	fi

	# Glob the list of files to parse.
	set +f
	files=$(echo $config_local)
	set -f

	for file in $files; do
		if [ -r "$file" ]; then
			parse_file "$file"
		fi
	done

	parsed_configs='yes'
}

# Usage: parse_file file
# Reads the specified config files.
parse_file() {
	local file line tstr var

	file="$1"
	decho "Parsing config file: $file"
	while read line; do
		decho "  $line" 2
		case "$line" in
		\#*)		decho "    Comment" 3
				;;
		MANPATH*)	decho "    MANPATH" 3
				trim "${line#MANPATH}"
				add_to_manpath "$tstr"
				;;
		MANLOCALE*)	decho "    MANLOCALE" 3
				trim "${line#MANLOCALE}"
				manlocales="$manlocales:$tstr"
				;;
		MANCONFIG*)	decho "    MANCONFIG" 3
				trim "${line#MANCONFIG}"
				config_local="$tstr"
				;;
		MANSECT*)	decho "    MANSECT" 3
				trim "${line#MANSECT}"
				mansect="$mansect:$tstr"
				;;
#ifdef __APPLE__
		# Assume every other line describes a variable
		*[\ \	]*)	var="${line%%[\ \	]*}"
#else
#		# Set variables in the form of FOO_BAR
#		*_*[\ \	]*)	var="${line%%[\ \	]*}"
#endif
				trim "${line#$var}"
				eval "$var=\"$tstr\""
				decho "    Parsed $var" 3
				;;
		esac
	done < "$file"
}

#ifdef __APPLE__
xcode_path() {
	local mpath xmanpaths

	xmanpaths=$($XCSELECT --show-manpaths 2>/dev/null)
	if [ -n "$xmanpaths" ]; then
		for mpath in $xmanpaths; do
			add_to_manpath "$mpath"
		done
	fi
}
#endif

# Usage: search_path
# Traverse $PATH looking for manpaths.
search_path() {
	local IFS p path

	decho "Searching PATH for man directories"

	IFS=:
	for path in $PATH; do
		if add_to_manpath "$path/man"; then
			:
		elif add_to_manpath "$path/MAN"; then
			:
		else
			case "$path" in
			*/bin)	p="${path%/bin}/share/man"
				add_to_manpath "$p"
				p="${path%/bin}/man"
				add_to_manpath "$p"
				;;
			esac
		fi
	done
	unset IFS

	if [ -z "$manpath" ]; then
		decho '  Unable to find any manpaths, using default'
		manpath=$man_default_path
	fi
}

# Usage: search_whatis cmd [arglist]
# Do the heavy lifting for apropos/whatis
search_whatis() {
	local IFS bad cmd f good key keywords loc opt out path rval wlist
#ifdef __APPLE__
	local localwlist
#endif

	cmd="$1"
	shift

	whatis_parse_args "$@"

	build_manpath
	build_manlocales
	setup_pager

	if [ "$cmd" = "whatis" ]; then
		opt="-w"
	fi

	f='whatis'
#ifdef __APPLE__
	localwlist=''
#endif

	IFS=:
	for path in $MANPATH; do
		if [ \! -d "$path" ]; then
			decho "Skipping non-existent path: $path" 2
			continue
		fi

		if [ -f "$path/$f" -a -r "$path/$f" ]; then
			decho "Found whatis: $path/$f"
#ifdef __APPLE__
			wlist="$wlist%$path/$f"
		else
			decho "Using makewhatis.local: $path"
			localwlist="$localwlist%$path"
#else
#			wlist="$wlist $path/$f"
#endif
		fi

		for loc in $MANLOCALES; do
			if [ -f "$path/$loc/$f" -a -r "$path/$loc/$f" ]; then
				decho "Found whatis: $path/$loc/$f"
#ifdef __APPLE__
				wlist="$wlist%$path/$loc/$f"
#else
#				wlist="$wlist $path/$loc/$f"
#endif
			fi
		done
	done
	unset IFS

#ifdef __APPLE__
	if [ -z "$wlist" ] && [ -z "$localwlist" ]; then
#else
#	if [ -z "$wlist" ]; then
#endif
		echo "$cmd: no whatis databases in $MANPATH" >&2
		exit 1
	fi

#ifdef __APPLE__
	if [ -n "$MANSECT" ]; then
		sectfilter=""
		IFS=:
		for sect in $MANSECT; do
			sectfilter="${sectfilter}|${sect}"
		done
		unset IFS
		sectfilter="\\((${sectfilter#|})\\).*[[:space:]]-[[:space:]]"
	else
		sectfilter="."
	fi

	decho "${sectfilter}"

#endif

	rval=0
	for key in $keywords; do
#ifdef __APPLE__
		found=0

		# Apple systems frequently won't have any directories in manpath
		# with a whatis database.
		if [ -n "$wlist" ]; then
			wlist=${wlist#%}
			IFS=%
			out=$($GREP -Ehi $opt -- "$key" $wlist | \
			    $GREP -E "${sectfilter}" | $SED -e 's/\\n/\\\\n/g')
			unset IFS
			if [ -n "$out" ]; then
				good="$good\n$out"
				found=1
			fi
		fi
#else
#		out=$(grep -Ehi $opt -- "$key" $wlist)
#		if [ -n "$out" ]; then
#			good="$good\\n$out"
#		else
#			bad="$bad\\n$key: nothing appropriate"
#			rval=1
#		fi
#endif

#ifdef __APPLE__
		localwlist=${localwlist#%}
		IFS=%
		for path in $localwlist; do
			out=$(/usr/libexec/makewhatis.local "-o /dev/fd/1" \
			    "$path" | $GREP -Ehi $opt -- "$key" | \
			    $GREP -E "${sectfilter}" | $SED -e 's/\\n/\\\\n/g')
			if [ -n "$out" ]; then
				good="$good\\n$out"
				found=1
			fi
		done
		unset IFS

		if [ $found -eq 0 ]; then
			bad="$bad\\n$key: nothing appropriate"
			rval=1
		fi
#endif
	done

	# Strip leading carriage return.
	good=${good#\\n}
	bad=${bad#\\n}

	if [ -n "$good" ]; then
#ifdef __APPLE__
		printf "%b\n" "$good" | $WHATISPAGER
#else
#		echo -e "$good" | $MANPAGER
#endif
	fi

	if [ -n "$bad" ]; then
#ifdef __APPLE__
		printf "%b\n" "$bad" >&2
#else
#		echo -e "$bad" >&2
#endif
	fi

	exit $rval
}

# Usage: setup_cattool page
# Finds an appropriate decompressor based on extension
setup_cattool() {
	case "$1" in
	*.bz)	cattool='/usr/bin/bzcat' ;;
	*.bz2)	cattool='/usr/bin/bzcat' ;;
#ifdef __APPLE__
	*.gz)	cattool='/usr/bin/zcat -f' ;;
#else
#	*.gz)	cattool='/usr/bin/zcat' ;;
#	*.lzma)	cattool='/usr/bin/lzcat' ;;
#	*.xz)	cattool='/usr/bin/xzcat' ;;
#endif
	*)	cattool='/usr/bin/zcat -f' ;;
	esac
}

# Usage: setup_pager
# Correctly sets $MANPAGER
setup_pager() {
	# Setup pager.
	if [ -z "$MANPAGER" ]; then
		if [ -n "$MANCOLOR" ]; then
			MANPAGER="$LESS_CMD -sR"
		else
			if [ -n "$PAGER" ]; then
				MANPAGER="$PAGER"
			else
				MANPAGER="$LESS_CMD -s"
			fi
		fi
	fi
#ifdef __APPLE__
	# whatis/apropos have historically used more(1) on Apple systems,
	if [ -z "$WHATISPAGER" ]; then
		if [ -n "$MANCOLOR" ]; then
			WHATISPAGER="$MORE_CMD -ER"
		else
			if [ -n "$PAGER" ]; then
				WHATISPAGER="$PAGER"
			else
				WHATISPAGER="$MORE_CMD -E"
			fi
		fi
	fi
#endif
	decho "Using pager: $MANPAGER"
}

# Usage: trim string
# Trims whitespace from beginning and end of a variable
trim() {
	tstr=$1
	while true; do
		case "$tstr" in
		[\ \	]*)	tstr="${tstr##[\ \	]}" ;;
		*[\ \	])	tstr="${tstr%%[\ \	]}" ;;
		*)		break ;;
		esac
	done
}

# Usage: whatis_parse_args "$@"
# Parse commandline args for whatis and apropos.
whatis_parse_args() {
	local cmd_arg
	OPTIND=1
#ifdef __APPLE__
	while getopts 'ds:' cmd_arg; do
#else
#	while getopts 'd' cmd_arg; do
#endif
		case "${cmd_arg}" in
		d)	debug=$(( $debug + 1 )) ;;
#ifdef __APPLE__
		s)	MANSECT=$OPTARG ;;
#endif
		*)	whatis_usage ;;
		esac
	done >&2

	shift $(( $OPTIND - 1 ))

	keywords="$*"
}

# Usage: whatis_usage
# Display usage for the whatis/apropos utility.
whatis_usage() {
	echo "usage: $cmd [-d] [-s mansect] keyword [...]"
	exit 1
}



# Supported commands
do_apropos() {
#ifndef __APPLE__
#	[ $(stat -f %i /usr/bin/man) -ne $(stat -f %i /usr/bin/apropos) ] && \
#		exec apropos "$@"
#endif
	search_whatis apropos "$@"
}

do_man() {
	local IFS

	man_parse_opts "$@"
	man_setup

	shift $(( $OPTIND - 1 ))
	IFS=:
	for sect in $MANSECT; do
		if [ "$sect" = "$1" ]; then
			decho "Detected manual section as first arg: $1"
#ifdef __APPLE__
			man_preferred_section="$1"
#endif
			MANSECT="$1"
			shift
			break
		fi
	done
	unset IFS

#ifdef __APPLE__
	if [ $# -gt 1 ] && [ -z "$man_preferred_section" ]; then
		local base_section

		# See if we match an alternatively valid section; the previous
		# version of man will reportedly accept any single number
		# followed by up to 3 letters.  Naturally, if there's only one
		# argument then we skip this entirely as it won't be the
		# section.
		base_section=$(echo "$1" | $SED -nE \
		    -e 's/^([0-9])[[:alpha:]]{1,3}$/\1/p')

		if [ -n "$base_section" ]; then
			decho "Detected extended manual section as first arg: $1"

			man_preferred_section="$1"
			MANSECT="$1:$base_section"
			shift
		fi
	fi

	# Convoluted to simplify later handling of these path args; just
	# expanding $* into $pages won't DTRT, we'll break up args with internal
	# spaces.  Sidestep it entirely by instead shuffling the remainder of
	# our args over into a newline-delimited $pages, which we can then
	# process through with read.
	if [ $# -lt 1 ]; then
		return
	fi

	nl=$'\n'
	for i in $(seq 1 $#); do
		eval page=\"\${${i}}\"
		if [ "${i}" -gt 1 ]; then
			pages="${pages}${nl}"
		fi
		pages="${pages}${page}"
	done
#else
#	pages="$*"
#endif

	if [ -z "$pages" ]; then
		echo 'What manual page do you want?' >&2
		exit 1
	fi

#ifdef __APPLE__
	while read page; do
		if [ -z "${page}" ]; then
			continue
		fi
#else
#	for page in $pages; do
#endif
		decho "Searching for $page"
		man_find_and_display "$page"
#ifndef __APPLE__
#	done
#else
	done <<EOF
$pages
EOF
#endif

	exit ${ret:-0}
}

do_manpath() {
	manpath_parse_args "$@"
	if [ -z "$qflag" ]; then
		manpath_warnings
	fi
	if [ -n "$Lflag" ]; then
		build_manlocales
		echo $MANLOCALES
	else
		build_manpath
		echo $MANPATH
	fi
	exit 0
}

do_whatis() {
#ifndef __APPLE__
#	[ $(stat -f %i /usr/bin/man) -ne $(stat -f %i /usr/bin/whatis) ] && \
#		exec whatis "$@"
#endif
	search_whatis whatis "$@"
}

#ifndef __APPLE__
# Apple systems require absolute paths for the groff-suite specified in
# /etc/man.conf.

# User's PATH setting decides on the groff-suite to pick up.
#EQN=eqn
#NROFF='groff -S -P-h -Wall -mtty-char -man'
#PIC=pic
#REFER=refer
#TBL=tbl
#TROFF='groff -S -man'
#VGRIND=vgrind
#endif

#ifdef __APPLE__
# rdar://problem/113400634 - avoid PATH searches where possible
CAT=/bin/cat
FIND=/usr/bin/find
GREP=/usr/bin/grep
MANDOC=/usr/bin/mandoc
SED=/usr/bin/sed
XCSELECT=/usr/bin/xcode-select

# These ones are special because they use LESS and MORE environment variables to
# provide some default options.
LESS_CMD=/usr/bin/less
MORE_CMD=/usr/bin/more
#endif
LOCALE=/usr/bin/locale
STTY=/bin/stty
SYSCTL=/sbin/sysctl

debug=0
man_default_sections='1:8:2:3:3lua:n:4:5:6:7:9:l'
#ifdef __APPLE__
man_default_path='/usr/share/man:/usr/local/share/man'
# First argument that we'll eat if it's a valid section
man_preferred_section=''
#else
#man_default_path='/usr/share/man:/usr/share/openssl/man:/usr/local/share/man:/usr/local/man'
#endif
cattool='/usr/bin/zcat -f'

config_global='/etc/man.conf'

# This can be overridden via a setting in /etc/man.conf.
config_local='/usr/local/etc/man.d/*.conf'

# Set noglobbing for now. I don't want spurious globbing.
set -f

#ifdef __APPLE__
# man(1) is defined to exhibit default behavior for SIGINT/SIGQUIT; that is,
# the should terminate with the correct exit codes to reflect that they were
# terminated by their respective signals.
trap 'trap - INT; kill -INT $$' INT
trap 'trap - QUIT; kill -QUIT $$' QUIT
#endif

case "$0" in
*apropos)	do_apropos "$@" ;;
*manpath)	do_manpath "$@" ;;
*whatis)	do_whatis "$@" ;;
*)		do_man "$@" ;;
esac
