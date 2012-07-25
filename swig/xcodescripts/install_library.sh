#!/bin/sh
#
# Install SWIG library files
#
# Based on the "install-lib" target in the original Makefile.in
#

SWIG_LIB="/usr/local/share/swig/1.3.40"

liblanguages="gcj typemaps tcl perl5 python guile java mzscheme ruby php ocaml octave \
	pike chicken csharp modula3 allegrocl clisp lua cffi uffi r"

libmodules="std"

mkdir -p ${DSTROOT}${SWIG_LIB}
for file in ${SRCROOT}/Lib/*.i ${SRCROOT}/Lib/*.swg; do
	i=`basename $file`
	echo "Installing $i"
	install -m 644 $file ${DSTROOT}${SWIG_LIB}/$i
done

for lang in ${liblanguages} ${libmodules}; do
	echo "Installing language specific files for $lang"
	mkdir -p ${DSTROOT}${SWIG_LIB}/$lang
	doti="`cd ${SRCROOT}/Lib/$lang && ls *.i 2>/dev/null || echo ''`";
	dotswg="`cd ${SRCROOT}/Lib/$lang && ls *.swg 2>/dev/null || echo ''`";
	if [ -f ${SRCROOT}/Lib/$lang/extra-install.list ]; then
		extra="`sed '/^#/d' ${SRCROOT}/Lib/$lang/extra-install.list`";
	else
		extra=''
	fi
	for file in $doti $dotswg $extra; do
		echo "Installing ${DSTROOT}${SWIG_LIB}/$lang/$file"
		install -m 644 ${SRCROOT}/Lib/$lang/$file ${DSTROOT}${SWIG_LIB}/$lang/$file
	done
done
