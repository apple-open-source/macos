#!/bin/sh
#
# Each line of the text files in /etc/paths are directories that should be
# added to the current path.  We source /etc/paths/default first, so that
# the default paths (/usr/bin:/bin:/usr/sbin:/sbin) appear early in the path.
#

shopt -s extglob
function read_path_dir () {
DIR="$1"
NEWPATH="$2"
SEP=""
IFS=$'\n'
if [ -d "$DIR".d ]; then
	for f in "$DIR" "$DIR".d/* ; do
	  if [ -f "$f" ]; then
		for p in $(< "$f") ; do
			[[ "$NEWPATH" = *(*:)${p}*(:*) ]] && continue
			[ ! -z "$NEWPATH" ] && SEP=":"
			NEWPATH="${NEWPATH}${SEP}${p}"
		done
	  fi
	done
fi
echo $NEWPATH
}

P=`read_path_dir /etc/paths "$PATH"`
MP=`read_path_dir /etc/manpaths "$MANPATH"`

if [ "$1" == "-c" -o \( -z "$1" -a "${SHELL%csh}" != "$SHELL" \) ]; then
	echo setenv PATH \"$P\"\;
	echo setenv MANPATH \"$MP\"\;
	exit 0
elif [ "$1" == "-s" -o -z "$1" ]; then
	echo PATH=\"$P\"\; export PATH;
	echo MANPATH=\"$MP\"\; export MANPATH;
	exit 0
else
	echo "usage: path_helper [-c | -s]" 1>&2
	exit 1
fi
