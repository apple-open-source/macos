#!/bin/sh

RESOLVELINKS="./xmlman/resolveLinks"

if [ ! -x "$RESOLVELINKS" ] ; then
	RESOLVELINKS="/usr/bin/resolveLinks"
fi

rm -rf Documentation/hdapi
./headerDoc2HTML.pl -H -O -j -Q -n -p -o Documentation/hdapi Documentation/HeaderDoc.hdoc
if [ -d headerdoc_tp ] ; then
	./headerDoc2HTML.pl -H -O -j -Q -n -p -o Documentation/hdapi headerwalk.pl headerDoc2HTML.pl gatherHeaderDoc.pl headerdoc_tp/*.pl headerdoc_tp/tp_webkit_tools/filtermacros.pl Modules/HeaderDoc/*.pm xmlman/*.c xmlman/*.h
else
	./headerDoc2HTML.pl -H -O -j -Q -n -p -o Documentation/hdapi headerwalk.pl headerDoc2HTML.pl gatherHeaderDoc.pl Modules/HeaderDoc/*.pm xmlman/*.c xmlman/*.h
fi

./gatherHeaderDoc.pl -N Documentation/hdapi index.html

$RESOLVELINKS -s Documentation/reference_additions.xref Documentation/hdapi

if [ -x ./breadcrumbtree.pl ] ; then
	if which perl5.8.9 > /dev/null ; then
		perl5.8.9 ./breadcrumbtree.pl Documentation/hdapi
	else 
		./breadcrumbtree.pl Documentation/hdapi
	fi
fi

