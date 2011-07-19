#!/bin/sh

checkonly=no

if [ "X$1" = "X--check" ] ; then
    checkonly=yes

    pid=$2
    name=$3
else
    name=$1
    pid=$2
fi


ec=0

rm -f leaks-log > /dev/null

if [ "$(uname -s)" = "Darwin" ] ; then
    echo "leaks check on $name ($pid)"
    leaks $pid > leaks-log 2>&1 || \
        { echo "leaks failed: $?"; cat leaks-log; exit 1; }

    grep -e "Process .*: 0 leaks for 0 total leaked bytes" leaks-log > /dev/null || \
	{ echo "Potentional memory leak in $name" ; ec=1; }

    if grep -e '1 leak for' leaks-log > /dev/null && grep -e "Leak.*environ.*__CF_USER_TEXT_ENCODING" leaks-log > /dev/null ; then
	echo "just running into rdar://problem/8764394"
	ec=0
    fi

    [ "$ec" != "0" ] && { echo ""; cat leaks-log ; }

    #[ "$ec" != "0" ] && { malloc_history $pid -all_by_size > l; }
    #[ "$ec" != "0" ] && { env PS1=": leaks-debugger !!!! ; " bash ; }

    [ "$ec" = "0" ] && rm leaks-log

fi

if [ "$checkonly" = no ] ; then
    kill $pid
    sleep 3
    kill -9 $pid 2> /dev/null
    sleep 3
fi

exit $ec
