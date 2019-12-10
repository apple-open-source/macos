#!/bin/sh

cd `dirname $0`
cat /dev/null > verbose-outputs.txt
rm -rf DIFF/ NEW/

mkdir -p NEW
mkdir -p DIFF
passed=0
echo $passed >.passed
failed=0
echo $failed >.failed
cat /dev/null > failure-outputs.txt

runComplexTests()
{
  [ "$1" != "" ] && only=$1.sh
  for i in *.sh
  do
    case $i in TEST*.sh) continue;; esac
    [ "$only" != "" -a "$i" != "$only" ] && continue
    sh ./$i
  done
}

runSimpleTests()
{
  passed=`cat .passed`
  failed=`cat .failed`
  only=$1
  cat TESTLIST TESTLISTRADAR | while read name input output options
  do
    case $name in
      \#*) continue;;
      '') continue;;
    esac
    rm -f core
    [ "$only" != "" -a "$name" != "$only" ] && continue
    if ./TESTonce.sh $name $input $output "$options"
    then
      passed=`expr $passed + 1`
      echo $passed >.passed
    else
      failed=`expr $failed + 1`
      echo $failed >.failed
    fi
    [ "$only" != "" -a "$name" = "$only" ] && break
  done
  # I hate shells with their stupid, useless subshells.
  passed=`cat .passed`
  failed=`cat .failed`
}

echo $passed >.passed
echo $failed >.failed
if [ $# -eq 0 ]
then
  runComplexTests
  runSimpleTests
elif [ $# -eq 1 ]
then
  runComplexTests $1
  runSimpleTests $1
else
  echo "Usage: $0 [test_name]"
  exit 30
fi

# exit with number of failing tests.
echo '------------------------------------------------'
printf "%4u tests failed\n" $failed
printf "%4u tests passed\n" $passed
echo
echo
cat failure-outputs.txt
echo
echo
exit $failed
