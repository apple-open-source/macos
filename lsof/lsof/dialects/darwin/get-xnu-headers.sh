#!/bin/sh
#
# get-xnu-headers.sh -- get specified XNU kernel header files
#
# Checks for the availability of XNU (kernel) header files which are
# needed by "lsof".  If the header files are not currently available
# then they will be extracted from an appropriate open source tarball,
# downloaded from ${DS}.
#
# Note: You must supply a registered Darwin user name and its password to
#       download the XNU header file tarball.
#
# Usage: file1 file2 ...
#
# Where: file1	first header file to get
#	 file2	second header file to get
#
# Exit:
#
#	Return code: 0 = all header files are available
#		     1 = not all header files are available
#
#	STDOUT: "" if all header files are available
#		"<error message>" if not all header files are available
#set -x	# for DEBUGging

# Pre-defined constants

DS="http://www.opensource.apple.com/darwinsource"
L=`pwd`/dialects/darwin			# local header file path
P="xnu/bsd"				# xnu tar path prefix
S=`basename $0 .sh`			# script's base name
T=xnu					# tar base name
TB=${T}.tar				# tarball name
TBGZ=${TB}.gz				# gzip'd tarball
TD=/tmp/${USER}.$$			# temporary directory

# Other variables

MH=""					# missing header file status
UH=0					# unavailable header file status

# Check argument count.  There must be at least one argument.

if test $# -lt 1	# {
then
  echo "insufficient arguments: $#"
  exit 1
fi	# }

# Record the header files that aren't already available in ${MH}.

for i in $*
do
  if test ! -r ${L}/${P}/$i
  then
    if test "X$MH" = "X"
    then
      MH=$i
    else
      MH="$MH $i"
    fi
  fi
done
if test "X$MH" = "X"
then

  # All header files are available, so exit cleanly.

  exit 0
fi

# Get the registered Darwin user name and password.

trap 'stty echo; echo TRAP; rm -rf $TD; exit 1' 1 2 3 15
cat << .CAT_MARK

---------------------------------------------------------------------

It's necessary to download a Darwin open source tarball and extract
some some XNU kernel header files from it.  The tarball will be
downloaded to $TD and gunzip'd there.  The resulting tar
file will be twenty to thirty megabytes in size, and the necessary
header files will be extracted from it to ${L}/${T}.

You must first specify your registered Darwin user name and password
in order to download the tarball.

You must then specify the open source branch from which the tarball
should be downloaded.

.CAT_MARK
END=0
while test $END = 0	# {
do
  echo -n "What is your registered Darwin user name? "
  read DUN EXCESS
  if test "X$DUN" = "X"	# {
  then
    echo ""
    echo "+=====================================+"
    echo "| Please enter a non-empty user name. |"
    echo "+=====================================+"
    echo ""
  else
    END=1
  fi	# }
done	# }
stty -echo
END=0
while test $END = 0	# {
do
  echo -n "What is your registered Darwin password? "
  read DPW EXCESS
  if test "X$DPW" = "X"	# {
  then
    echo ""
    echo "+====================================+"
    echo "| Please enter a non-empty password. |"
    echo "+====================================+"
    echo ""
  else
    END=1
  fi	# }
done	# }
stty echo

# Get the branch.

cat << .CAT_MARK


--------------------------------------------------------------------

Now you must specify the open source branch of the CVS repository
from which the header files will be checked out.  These are some
likely open source branches:

    Mac OS X 10.2.2 (kernel version 6.2): 10.2.2
    Mac OS X 10.2.4 (kernel version 6.4): 10.2.4
    Mac OS X 10.2.5 (kernel version 6.5): 10.2.5
    Mac OS X 10.2.6 (kernel version 6.6): 10.2.6

Please specify the last value of a row as the name of an open source
branch from which to fetch the tarball.

.CAT_MARK
END=0
while test $END = 0	# {
do
  echo -n "What branch? "
  read BR EXCESS
  if test "X$BR" = "X"	# {
  then
    echo ""
    echo "+==================================+"
    echo "| Please enter a non-empty branch. |"
    echo "+==================================+"
    echo ""
  else
    END=1
  fi	# }
done	# }

# Make a temporary directory to hold the tarball, download it and gunzip it.

rm -rf $TD
mkdir $TD
echo "Downloading tarball to ${TD}/$TBGZ ..."
(cd $TD;
curl --config - --remote-name ${DS}/tarballs/${BR}/apsl/$TBGZ << _EOF
silent
user ${DUN}:${DPW}
_EOF
)
if test ! -r ${TD}/$TBGZ -o ! -s ${TD}/$TBGZ
then
  echo "${S}: can't download ${TBGZ}."
  rm -rf $TD
  exit 1
fi
echo "Gunzip'ping ${TD}/$TBGZ ..."
gunzip ${TD}/$TBGZ
if test ! -r ${TD}/$TB -o ! -s ${TD}/$TB
then
  echo "${S}: gunzip of ${TD}/$TBGZ failed."
  ls -l ${TD}/$TBGZ
  ls -l ${TD}/$TB
  echo "Hint: examine ${TD}/${TBGZ}."
  echo "      It may have failed to download properly.  If you find it is not"
  echo "      in gzip format and contains HTML, your registered Darwin user"
  echo "      name or password may be incorrect, or the download tarball may"
  echo "      not exist."
  exit 1
fi
ls -l ${TD}/$TB

# Get the required header files from the tarball.

for i in $MH
do
  rm -f ${L}/${P}/$i
  echo -n "Extracting $i ... "
  (cd $L; tar xf ${TD}/$TB ${P}/$i > /dev/null 2>&1)
  if test $? -eq 0
  then
    echo "OK"
  else
    echo "UNAVAILABLE"
    UH=1
  fi
done
rm -rf $TD
exit $UH
