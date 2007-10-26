#! /bin/bash

# Load the latest Samba source into the samba subdirectory.

error()
{
    echo "$@"
    exit 1
}

checkout_upstream()
{
    rev="$1"
    svn export --quiet --force --revision "$rev" $SAMBA $SVNTMP
}

query_latest_revision()
{
    svn info $SAMBA | \
	awk '/Revision/{print $2}'
}

check_svn_url()
{
    svn ls $SVNURL/samba > /dev/null 2>&1 
}

cleanup()
{
    rm -rf $SVNTMP
}

usage()
{
    error "Usage: update-samba.sh [DIR]"
}

BRANCH=SAMBA_3_0_25
SAMBA="svn://svnanon.samba.org/samba/branches/$BRANCH"
SVNLOAD=$(dirname $0)/svn_load_dirs.pl
SVNURL=$(svn info | awk '/URL/ {print $2}')

if [ ! -d samba ] ; then
    error expecting Samba code to be in ./samba/
fi

echo checking where to import source
check_svn_url || error "expected $SVNURL/samba"


if [ "$1" ] ; then
    # Pull in tree from given directory
    [[ -d "$1" ]] || usage
    SVNTMP="$1"
else
    # Pull in latest SVN
    REVISION="$(query_latest_revision)"
    SVNTMP="$BRANCH-svn-revision-$REVISION"
    echo downloading revision $REVISION
    checkout_upstream $REVISION || error "failed to checkout samba source"
fi

#trap cleanup 0 1 2 3 15

echo importing source from $SVNTMP
$SVNLOAD -v  -wc $(pwd)/samba $SVNURL samba  $SVNTMP

