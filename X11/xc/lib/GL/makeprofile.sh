#!/bin/sh
# $XFree86: xc/lib/GL/makeprofile.sh,v 1.3 2000/08/28 02:43:11 tsi Exp $

libname=$1

if [ ! -f ${libname} ] ; then
    echo "no file ${libname}" 
    srcdir=${LIBGL_MODULES_DIR}
    driver=$1
    libname=${srcdir}/lib_${driver}_dri_p.a
    echo "trying ${libname}" 
fi

if [ ! -f ${libname} ] ; then 
   echo "no file ${libname}"
   echo "please specify full path to lib_(driver)_dri_p.a"
   exit 1 
fi

ld -o glxsyms -noinhibit-exec --whole-archive -Ttext=`cat glx_lowpc` ${libname} 2> /dev/null || { echo "couldn't build relocated object" ; exit 1 }

gprof glxsyms < gmon.out > profile || { echo "gprof failed" ; exit 1 }

