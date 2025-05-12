#! /bin/sh

#
# testone.sh - execute a single testcase
#
# (c) 1998-2006 (W3C) MIT, ERCIM, Keio University
# See tidy.c for the copyright notice.
#
# <URL:http://tidy.sourceforge.net/>
#
# CVS Info:
#
#    $Author$
#    $Date$
#    $Revision$
#
# set -x

VERSION='$Id'

echo Testing $1

set +f

TESTNO=$1
EXPECTED=$2
TIDY=../bin/tidy
INFILES=./input/in_${TESTNO}.*ml
CFGFILE=./input/cfg_${TESTNO}.txt

EXPOUTFILE=./output/out_${TESTNO}.html
EXPMSGFILE=./output/msg_${TESTNO}.txt

TIDYFILE=./tmp/out_${TESTNO}.html
MSGFILE=./tmp/msg_${TESTNO}.txt

unset HTML_TIDY

shift
shift

# Remove any pre-exising test outputs
for INFIL in $MSGFILE $TIDYFILE
do
  if [ -f $INFIL ]
  then
    rm $INFIL
  fi
done

for INFILE in $INFILES
do
    if [ -r $INFILE ]
    then
      break
    fi
done

# If no test specific config file, use default.
if [ ! -f $CFGFILE ]
then
  CFGFILE=./input/cfg_default.txt
fi

# Make sure output directory exists.
if [ ! -d ./tmp ]
then
  mkdir ./tmp
fi

$TIDY -f $MSGFILE -config $CFGFILE "$@" --tidy-mark no -o $TIDYFILE $INFILE
STATUS=$?

if [ $STATUS -ne $EXPECTED ]
then
  echo "== $TESTNO failed (Status received: $STATUS vs expected: $EXPECTED)" 
  cat $MSGFILE
  exit 1
fi

if [ -f $EXPOUTFILE ]
then
  /usr/bin/diff -q $EXPOUTFILE $TIDYFILE 2> /dev/null
  STATUS=$?
  if [ $STATUS -ne 0 ]
  then
    echo "== $TESTNO failed ($TIDYFILE differs from $EXPOUTFILE)"
    /usr/bin/diff $EXPOUTFILE $TIDYFILE
    exit 1
  fi
fi

if [ -f $EXPMSGFILE -o -f $MSGFILE ]
then
  /usr/bin/diff -q $EXPMSGFILE $MSGFILE 2> /dev/null
  STATUS=$?
  if [ $STATUS -ne 0 ]
  then
    echo "== $TESTNO failed ($MSGFILE differs from $EXPMSGFILE)"
    /usr/bin/diff $EXPMSGFILE $MSGFILE
    exit 1
  fi
fi

exit 0

