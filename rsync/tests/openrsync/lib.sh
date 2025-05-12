#! /bin/sh

set -u
set -e

: ${RSYNC_CLIENT=""}
: ${RSYNC_CLIENT_EXECUTOR=""}
: ${RSYNC_DEBUG=""}
: ${RSYNC_SERVER=""}
: ${RSYNC_FLAGS=""}
: ${RSYNC_SERVER_EXECUTOR=""}
: ${RSYNC_PREFIX_SRC=""}
: ${RSYNC_PREFIX_DEST=""}
: ${RSYNC_SETUP_SSHKEY=""}
: ${SUDO="sudo"}

mountlist="$PWD"/.~mounts

# Library of functions.
# Intended to be sourced by scripts (or interactive shells if you want).

asroot()
{
	if [ $(id -u) -eq 0 ]; then
		"$@"
	else
		$SUDO "$@"
	fi
}

check_id()
{
	local _id
	local _type

	_id=$1
	_type=$2

	case "$_type" in
	uid|gid)
		;;
	*)
		1>&2 echo "Unknown id type $_type"
		exit 1
		;;
	esac

	case "$(uname -s)" in
	Darwin)
		local _output

		case "$_type" in
		uid)
			_output=$(dscacheutil -q user -a uid "$_id")
			;;
		gid)
			_output=$(dscacheutil -q group -a gid "$_id")
			;;
		esac
		if [ -z "$_output" ]; then
			return 1
		else
			return 0
		fi
		;;
	*)
		local _db

		_db=""
		case "$_type" in
		uid)
			_db="passwd"
			;;
		gid)
			_db="group"
			;;
		esac

		getent "$_db" "$_id" >/dev/null
		return
		;;
	esac

	# Shouldn't happen, but they all exist.
	return 0
}

genfile_stdout_16m ()
{
    seq -f%015g 1048576
}
genfile_stdout_1m ()
{
    seq -f%015g 65536
}
genfile ()
{
    #touch "$1"
    genfile_stdout_1m > "$1"
}

lib_cleanup()
{
	rc=$?

	if [ -s "$mountlist" ]; then
		local mountpoint

		while read mountpoint; do
			asroot umount "$mountpoint" || true
		done < "$mountlist"

		:> "$mountlist"
	fi

	return "$rc"
}

mount_tmpfs()
{
	local mountpoint osname

	mountpoint="$1"
	osname="$(uname -s)"

	[ ! -d "$mountpoint" ] && mkdir -p "$mountpoint"

	case "${osname}" in
	Linux|*BSD)
		asroot mount -t tmpfs tmpfs "$mountpoint"
		;;
	Darwin)
		asroot mount_tmpfs "$mountpoint"
		;;
	*)
		1>&2 echo "Don't know how to mount tmpfs on ${osname}"
		exit 1
		;;
	esac

	echo "$mountpoint" >> "$mountlist"
}

# Shim for calling the configured rsync; we use this to manipulate src/dst file
# names in case we need to do ssh in order to make the requested configuration
# work at all (e.g., smb rsync with an openrsync backend).  All of our tests
# right now are written to assume a local server that the client forks.
rsync()
{
	local cmd="" argsep sshkey word

	set -- "$@"

	if [ -n "$RSYNC_CLIENT" ]; then
		if [ -n "$RSYNC_CLIENT_EXECUTOR" ]; then
			cmd="$RSYNC_CLIENT_EXECUTOR "
		fi

		cmd="$cmd$RSYNC_CLIENT"

		if [ -n "$RSYNC_SERVER" ]; then
			local path=""

			if [ -n "$RSYNC_SERVER_EXECUTOR" ]; then
				path="$RSYNC_SERVER_EXECUTOR "
			fi

			path="$path$RSYNC_SERVER"
			cmd="$cmd --rsync-path='$path'"
		fi

		if [ -n "$RSYNC_SSHKEY" ]; then
			# Var expansion
			eval "sshkey=\"$RSYNC_SSHKEY\""

			# Use the passphrase-less key we created in test setup,
			# and just accept the fingerprint as-is.
			cmd="$cmd --rsh='ssh -i $sshkey -o StrictHostKeyChecking=accept-new'"
		fi
	elif [ -n "$rsync" ]; then
		cmd="$rsync"
	else
		cmd="rsync"
	fi

	argsep=""
	while [ $# -gt 2 ]; do
		word="$1"

		cmd="$cmd '$word'"
		shift

		if [ "$word" == "--" ]; then
			argsep=yes
			break
		fi
	done

	if [ -z "$argsep" ]; then
		# The test may intentionally omit -- to avoid doing interop
		# testing, but it may still want a general sanity check on the
		# behavior so we'll just pass it through.  It still works to
		# test openrsync against a samba server, it's just that the
		# other way around won't fork/exec openrsync.
		cmd="$cmd $@"
	else
		# For interop testing, samba rsync does not support multiple sources via ssh
		# even if they're from the same host.
		if [ -n "$RSYNC_PREFIX_SRC" -a $# -gt 2 ]; then
		    case "$RSYNC_CLIENT" in
		    *samba*)
			echo "Skipping test incompatible with samba rsync"
		        exit 0
		        ;;
		    esac
		fi

		# Prefix any srcs
		while [ $# -gt 1 ]; do
			if [ -z "$RSYNC_PREFIX_SRC" ]; then
				cmd="$cmd $1"
			else
				case "$1" in
				/*)
					cmd="$cmd $RSYNC_PREFIX_SRC$1"
					;;
				*)
					cmd="$cmd $RSYNC_PREFIX_SRC$PWD/$1"
					;;
				esac
			fi
			shift
		done

		if [ -z "$RSYNC_PREFIX_DEST" ]; then
			cmd="$cmd $1"
		else
			case "$1" in
			/*)
				cmd="$cmd $RSYNC_PREFIX_DEST$1"
				;;
			*)
				cmd="$cmd $RSYNC_PREFIX_DEST$PWD/$1"
				;;
			esac
		fi
	fi

	eval "command $cmd"
}

# makes a directory path and optionally a file in it.
# if you want the last element to be a directory, add / at the end
mkdirfile ()
{
    case "$1" in
        '') error that cannot work;;
        */) mkdir -p "$1";;
        */*) mkdir -p "${1%/*}"; genfile "$1";;
        *) genfile "$1";;
    esac
}

mkdirsymlink ()
{
    (
        mkdir -p "$1"
        cd "$1"
        ln -sf "$2" "$3"
    )
}

# make a first interesting tree
generate_tree_1 ()
{
    mkdirfile foo/bar/baz/one.txt
    mkdirfile foo/bar/baz/one2.txt
    mkdirfile 'foo/bar/baz/  two.txt'
    mkdirfile 'foo/bar/baz/two  2.txt'
    mkdirfile 'foo/bar/baz/two3.txt  '
    mkdirsymlink foo/baz/ ../bar/baz/one.txt three.txt
    mkdirfile one/two/three/four.txt
    mkdirfile foo/five/one/two/five/blah.txt
    mkdirfile foo/one/two/five/blah.txt
}

# a frontend for find
# first argument is a dir to chdir to
findme ()
{
    local stat_fmt dirs

    OPTIND=1
    dirs=0
    stat_fmt="%Sp %Su %Sg %N"
    while getopts dt flag; do
         case "$flag" in
         d) dirs=1 ;;
         t) stat_fmt="${stat_fmt} %m" ;;
         esac
    done

    shift $((OPTIND - 1))

    if [ $# -lt 2 ] ; then
        echo usage: different 1>&2
        return 1
    fi

    (
        cd "$1" ; shift
	if [ ${dirs} -ne 0 ] ; then
            find "$@" -type d -exec stat -f "$stat_fmt" {} \; | sort
        else
            find "$@" ! -type d -exec stat -f "$stat_fmt %z" {} \; | sort
        fi
    )
}

xattr_capable ()
{

    if which lsextattr 1>/dev/null 2>&1; then
        echo "lsextattr"
        return 0
    elif which getfattr 1>/dev/null 2>&1; then
        echo "getfattr"
        return 0
    elif which xattr 1>/dev/null 2>&1; then
        echo "xattr"
        return 0
    fi

    return 1
}

xattr_set ()
{
    file=$1
    name=$2
    value=$3

    xattr_tool=$(xattr_capable)
    if [ $? -ne 0 ]; then
        # The caller should have checked that xattrs are supported.
        return 1
    fi

    case "$xattr_tool" in
        lsextattr)
            setextattr -h user "$name" "$value" "$file"
            ;;
        getfattr)
            setfattr -hn "$name" -v "$value" "$file"
            ;;
        xattr)
            xattr -ws "$name" "$value" "$file"
            ;;
    esac
}

xattr_dump ()
{
    file=$1

    xattr_tool=$(xattr_capable)
    if [ $? -ne 0 ]; then
        # This one will be called unconditionally, so we want to just succeed if
        # we don't have xattr support.
        return 0
    fi

    case "$xattr_tool" in
        lsextattr)
            lsextattr -hq user "$file"
            ;;
        getfattr)
            getfattr -hd "$file"
            ;;
        xattr)
            xattr -s "$file"
            ;;
    esac
}

# compare two trees.  This will later be modular to pick between:
# - diff
# - find . -print0 | sort --zero-terminated | xargs -0 tar fc foo.tar
# - mtree
compare_trees ()
{
    local need_time

    need_time="--"
    OPTIND=1
    while getopts t flag; do
         case "$flag" in
         t) need_time="-t" ;;
         esac
    done

    shift $((OPTIND - 1))
    if [ $# -ne 2 ] ; then
        echo usage: different 1>&2
        return 1
    fi

    # files_and_permissions
    findme "$need_time" "$1" . > find1
    findme "$need_time" "$2" . > find2
    diff -u find[12] 1>&2

    # dirs_and_permissions
    findme "-d" "$need_time" "$1" . > find1d
    findme "-d" "$need_time" "$2" . > find2d
    diff -u find[12]d 1>&2

    # file contents
    diff -ru "$1" "$2" 1>&2

    # check for any xattrs; note that this won't do the right thing if we have
    # files that both have xattrs and newlines in the filename.  This is a known
    # limitation that seems like an OK compromise.
    tmpf=$(mktemp -u rsync_xattr.XXXXXXXX)
    _IFS="$IFS"
    IFS=$'\n'
    for f in $(find -H "$1" -xattr); do
        xattr_dump "$f" > "$tmpf"
        if [ -s "$tmpf" ]; then
            # See if the second version of the file has matching xattrs
            mirrored=$(echo "$f" | sed -e "s/^$1/$2/")
            xattr_dump "$mirrored" > "$tmpf-2"
            1>&2 echo "xattr check: $1 and $2"
            diff -u "$tmpf" "$tmpf-2" 1>&2
            rm "$tmpf-2"
        fi
    done
    IFS="$_IFS"
    rm -f "$tmpf"
}
