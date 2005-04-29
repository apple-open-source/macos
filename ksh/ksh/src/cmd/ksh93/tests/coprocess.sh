####################################################################
#                                                                  #
#             This software is part of the ast package             #
#                Copyright (c) 1982-2004 AT&T Corp.                #
#        and it may only be used by you under license from         #
#                       AT&T Corp. ("AT&T")                        #
#         A copy of the Source Code Agreement is available         #
#                at the AT&T Internet web site URL                 #
#                                                                  #
#       http://www.research.att.com/sw/license/ast-open.html       #
#                                                                  #
#    If you have copied or used this software without agreeing     #
#        to the terms of the license you are infringing on         #
#           the license and copyright and are violating            #
#               AT&T's intellectual property rights.               #
#                                                                  #
#            Information and Software Systems Research             #
#                        AT&T Labs Research                        #
#                         Florham Park NJ                          #
#                                                                  #
#                David Korn <dgk@research.att.com>                 #
#                                                                  #
####################################################################
# test the behavior of co-processes
function err_exit
{
	print -u2 -n "\t"
	print -u2 -r ${Command}[$1]: "${@:2}"
	let Errors+=1
}
alias err_exit='err_exit $LINENO'

Command=$0
integer Errors=0

function ping # id
{
	integer x=0
	while ((x < 5))
	do	read -r
		print -r "$1 $REPLY"
	done
}

ping three |&
exec 3>&p
ping four |&
exec 4>&p
ping pipe |&

integer count
for i in three four pipe four pipe four three pipe pipe three pipe
do	case $i in
	three)	to=-u3;;
	four)	to=-u4;;
	pipe)	to=-p;;
	esac
	count=count+1
	print  $to $i $count
done

while	((count > 0))
do	count=count-1
	read -p
#	print -r - "$REPLY"
	set -- $REPLY
	if	[[ $1 != $2 ]]
	then	err_exit "$1 does not match 2"
	fi
	case $1 in
	three);;
	four) ;;
	pipe) ;;
	*)	err_exit "unknown message +|$REPLY|+"
	esac
done

file=/tmp/regress$$
trap "rm -f $file" EXIT
cat > $file  <<\!
/bin/cat |&
!
chmod +x $file
$file 2> /dev/null  || err_exit "parent coprocess prevents script coprocess"
exec 5<&p 6>&p
exec 5<&- 6>&-
${SHELL-ksh} |&
print -p  $'print hello | cat\nprint Done'
read -t 5 -p
read -t 5 -p
if	[[ $REPLY != Done ]]
then	err_exit	"${SHELL-ksh} coprocess not working"
fi
exec 5<&p 6>&p
exec 5<&- 6>&-
count=0
{
echo line1 | grep 'line2'
echo line2 | grep 'line1'
} |&
SECONDS=0
while
   read -p -t 10 line
do
   ((count = count + 1))
   echo "Line $count: $line"
done
if	(( SECONDS > 8 ))
then	err_exit 'read -p hanging'
fi
( sleep 3 |& sleep 1 && kill $!; sleep 3 |& sleep 1 && kill $! ) || 
	err_exit "coprocess cleanup not working correctly"
unset line
(
	integer n=0
	while read  line
	do	echo $line  |&
		if	cat  <&p 
		then	((n++))
			wait $!
		fi
	done > /dev/null 2>&1 <<-  !
		line1
		line2
		line3
		line4
		line5
		line6
		line7
	!
	(( n==7 ))  && print ok
)  | read -t 10 line
if	[[ $line != ok ]]
then	err_exit 'coprocess timing bug'
fi
(
	/bin/cat |&
	exec 6>&p
	print -u6 ok
	exec 6>&-
	sleep 1
	kill $! 2> /dev/null 
) && err_exit 'coprocess with subshell would hang'
for sig in IOT ABRT
do	if	( trap - $sig ) 2> /dev/null
	then	if	[[ $(	
				cat |&
				pid=$!
				trap "print TRAP" $sig
				(
					sleep 2
					kill -$sig $$
					sleep 2
					kill -$sig $$
					kill $pid
				) 2> /dev/null &
				read -p
			) != $'TRAP\nTRAP' ]]
		then	err_exit 'traps when reading from coprocess not working'
		fi
		break
	fi
done
exit $((Errors))
