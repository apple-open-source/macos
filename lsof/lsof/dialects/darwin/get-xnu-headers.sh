#!/bin/sh
#
# get-xnu-headers.sh -- get specified XNU kernel header files
#
# Checks for the availability of XNU (kernel) header files which are
# needed by "lsof".  If the header files are not currently available
# then we will check them out from the Darwin CVS repository.
#
# Note: You must be a registered Darwin user in order to access
#       the Darwin sources.
#
#       See http://www.opensource.apple.com/tools/cvs/docs.html
#
# Usage: dir file1 file2 ...
#
# Where: dir	the Darwin frameworks path -- e.g., /System/Library/Frameworks
#
#	 file1	first header file to get
#	 file2	second header file to get
#
# Exit:
#
#	Return code: 0 = all header files are available
#		     1 = not all header files are available
#
#	STDOUT: "" if all header files are available
#		"<error message>" if not all header files are available

# Pre-defined constants

L=`pwd`/dialects/darwin/include		# local header file path
S=`basename $0 .sh`			# script's base name

# Other variables

BR=""					# open source branch
DUN=""					# Darwin registered user name
HS=0					# missing header file status
UH=0					# if 1, some missing header files are
					# also unavailable via CVS

# Check argument count.  There must be at least two arguments.

if test $# -lt 2	# {
then
  echo "insufficient arguments: $#"
  exit 1
fi	# }

# Save frameworks path.

F=$1
shift

# See if all header files are available in the frameworks tree.

for h in $*	# {
do
  if test ! -f ${F}/System.framework/PrivateHeaders/${h}	# {
  then
    HS=1
  fi	# }
done	# }
if test $HS -eq 0	# {
then
  
  # All header files are available. Return success.

  exit 0
fi	# }

# Not all header files are available in the framworks tree.  Use cvs to
# get them.

# First get the registered Darwin user name.

trap 'echo TRAP; exit 1' 1 2 3 15
cat << .CAT_MARK

-----------------------------------------------------------------

It's necessary to check out some XNU kernel header files from the
open source CVS repository.  See this URL for more information:

   http://www.opensource.apple.com/tools/cvs/docs.html

You must first specify your registered Darwin user name in order
to access the open source repository.  The companion Darwin user
name password must be stored in ~/.cvspass.

You must also specify the open source branch from which the headers
should be obtained.
.CAT_MARK
END=0
while test $END = 0	# {
do
  echo ""
  echo -n "What is your registered Darwin user name? "
  read DUN EXCESS
  if test "X$DUN" = "X"	# {
  then
    echo ""
    echo "Please enter a non-empty name."
  else
    END=1
  fi	# }
done	# }

# Warn if there's no ~/.cvspass file.

if test ! -f ${HOME}/.cvspass	# {
then
  cat << .CAT_MARK

!!!WARNING!!!   !!!WARNING!!!   !!!WARNING!!!   !!!WARNING!!!   !!!WARNING!!!

There is no ~/.cvspass, so a CVS checkout will fail the authentication
test.

Hint: don't continue; exit, set CVSROOT, and do a cvs login.

!!!WARNING!!!   !!!WARNING!!!   !!!WARNING!!!   !!!WARNING!!!   !!!WARNING!!!
.CAT_MARK

  END=0
  while test $END -eq 0	# {
  do
    echo ""
    echo -n "Continue (y|n) [n]? "
    read ANS EXCESS
    if test "X$ANS" = "Xn" -o "X$ANS" = "XN" -o "X$ANS" = "X"	# {
    then
      exit 1
    else
      if test "X$ANS" = "Xy" -o "X$ANS" = "XY"	# {
      then
	END=1
      else
	echo ""
	echo "Please answer y or n."
      fi	# }
    fi	# }
  done	# }
fi	#}

# Get the branch.

cat << .CAT_MARK

--------------------------------------------------------------------

Now you must specify the open source branch of the CVS repository
from which the header files will be checked out.  These are some
likely open source branches:

    Darwin 1.2.1: xnu-3-1
    Darwin 1.3.1: xnu-4-2
    Darwin 1.4.1: xnu-9-1
    Mac OS X 10.0   (4K78): Apple-123-5
    Mac OS X 10.0.4 (4Q12): Apple-124-13
    Mac OS X 10.1   (5G64): Apple-201
    Mac OS X 10.1.1 (5M28): Apple-201-5
    Mac OS X 10.1.2 (5P48): Apple-201-14

Please specify a branch to use.  (It needn't be one of the suggested
ones.)

.CAT_MARK
END=0
while test $END = 0	# {
do
  echo ""
  echo -n "What branch? "
  read BR EXCESS
  if test "X$BR" = "X"	# {
  then
    echo ""
    echo "Please enter a non-empty branch."
  else
    END=1
  fi	# }
done	# }

# Now fetch the missing header files, if possible.

R=${ALT_XNU_CVSROOT:-":pserver:${DUN}@anoncvs.opensource.apple.com:/cvs/Darwin"}
for h in $*	# {
do
  LP=${L}/${h}
  D=`dirname $LP`
  mkdir -p $D
  if test -f $LP	# {
  then
    LPT=${LP}.old
    rm -f $LPT
    mv $LP $LPT
  else
    LPT=""
  fi	# }
  echo "Checking out: $h"
  cvs -l -d ${R} checkout -p -r ${BR} System/xnu/bsd/${h} > $LP 2> /dev/null
  ERR=$?
  if test $ERR -ne 0 -o ! -f $LP -o ! -s $LP	# {
  then
    rm -f $LP
    if test $ERR -ne 0	# {
    then
	echo "WARNING: CVS checkout failed."
    fi	# }
    if test "X$LPT" != "X"	# {
    then
      rm -f $LP
      mv $LPT $LP
      echo "WARNING: using old $LP"
    else
      echo "ERROR: unavailable: $h"
      UH=1
    fi	# }
  fi	# }
done	# }
exit $UH
