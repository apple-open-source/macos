#!/bin/sh
#
# $Xorg: find-rtns.sh,v 1.3 2000/08/17 19:55:19 cpqbld Exp $
#
# find-routines - convert leak tracer stack traces into file/lineno traces
#                 modified to work with the an unmodified version of
#                 gdb-4.18
#
# Usage: find-routines <program-name> {leak-tracing-output-files}
#
# $XFree86: xc/util/memleak/find-rtns.sh,v 1.4 2002/07/01 02:26:08 tsi Exp $
#

TMP1=find-routine.tmp1
TMP=find-routine.tmp
trap "rm -f $TMP $TMP1" 0
OBJ=$1
shift
echo 'set width 500' > $TMP1
# To load shared libs set breakpoint and run
echo 'break main' >> $TMP1
echo 'r' >> $TMP1
for i in `grep '\(return stack:\)\|\(allocated at\)' $* | 
	tr ' ' '\012' | 
	grep 0x | sort -u`; 
    do
	echo 'x/i '$i >> $TMP1
	echo 'i line * '$i >> $TMP1
done

cat $TMP1 | gdb $OBJ \
    | awk '\
           /^\(gdb\) \(?g?d?b?\)? ?0x[[:xdigit:]]*.*:.*/ \
    {a = gensub(/^\(gdb\) \(?g?d?b?\)? ?(0x[[:xdigit:]]*).*:.*/,"\\1","G");\
     b = gensub(/^\(gdb\) \(?g?d?b?\)? ?(0x[[:xdigit:]]*.*):.*/,"\\1","G");\
     printf("s;%s;%s",a,b); next; } \
           /.*No line.*/ \
    {printf(";\n",a);next} \
           /.*Line [[:digit:]]+.*/ \
    {a = gensub(/.*(Line [[:digit:]]+ of .*) starts.*/,"\\1","G"); \
     printf(" at %s;\n", a); next}'>> $TMP

awk '/return stack/	{ printf ("return stack\n");
			  for (i = 3; i <= NF; i++)
				printf ("\troutine %s\n", $i); }
     /allocated at/     { printf ("allocated at\n");
			  for (i = 3; i <= NF; i++)
			        printf ("\t\troutine %s\n", $i); }
     /^[A-Z]/		{ print }' $* |
	sed -f $TMP
