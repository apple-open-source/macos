#/bin/bash

RESULT=`grep SNACK_VERSION "$1"`
THIS_ONE=0
for i in $RESULT; do
   if test "$THIS_ONE" = "1" ; then
     #echo "THIS ONE is \'$i\'"
     LEN=`expr length "$i"`
     #echo "LEN is $LEN"
     END=`expr $LEN - 2`
     #echo "END is $END"
     VERSION=`expr substr "$i" 2 $END`
   fi
   if test "$i" = "SNACK_VERSION"; then
     THIS_ONE=1
   fi
done
#echo "final VERSION is '$VERSION'"
echo $VERSION

