#!/bin/bash

# Copyright (c) 2001-2006 The Trustees of Indiana University.  
#                         All rights reserved.
# Copyright (c) 2006-2007 Los Alamos National Security, LLC.  All rights
#                         reserved. 
#
# This file is part of the Open MPI software package.  For license
# information, see the LICENSE file in the top level directory of the
# Open MPI source distribution.
#
# Modified for Apple XBS support by Ben Byer 8/2006, 6/2007
#

########################################################################
#
# Configuration Options
#
########################################################################

#
# User-configurable stuff
#
OMPI_PREFIX="/usr"
OMPI_OPTIONS="--disable-mpi-f77 --without-cs-fs -enable-mca-no-build=ras-slurm,pls-slurm,gpr-null,sds-pipe,sds-slurm,pml-cm --mandir=/usr/share/man --sysconfdir=/usr/share NM=\"nm -p\""

#
# Not so modifiable stuff
#

OMPI_STARTDIR=`pwd`

echo "--> Configuration options:"
echo "    Prefix:         $OMPI_PREFIX"
echo "    Config Options: $OMPI_OPTIONS"

########################################################################
#
# Start actual code that does stuff
#
########################################################################

#
# Clean out the environment a bit
#
echo "--> Cleaning environment"
PATH=/bin:/sbin/:/usr/bin
LANGUAGE=C
LC_ALL=C
LC_MESSAGES=
LANG=
export PATH LANGUAGE LC_ALL LC_MESSAGES LANG
unset LD_LIBRARY_PATH CC CXX FC F77 OBJC

########################################################################
#
# Configure, Build, and Install Open MPI
#
########################################################################
#
# Copy source into right place
#
echo "--> Copying build tree"
cp -R openmpi $OBJROOT/

cd $OBJROOT

build_arch=`uname -p`"-apple-darwin"`uname -r`

real_install=1
for arch in $RC_ARCHS ; do
    builddir="$OBJROOT/build-$arch"
    mkdir "$builddir"

    case "$arch" in
        ppc)
            host_arch="powerpc-apple-darwin"`uname -r`
            ;;
        ppc64)
            # lie, but makes building on G4 easier
            host_arch="powerpc64-apple-darwin"`uname -r`
            ;;
        i386)
            host_arch="i386-apple-darwin"`uname -r`
            ;;
        x86_64)
            host_arch="x86_64-apple-darwin"`uname -r`
            ;;
    esac

    #
    # Run configure
    # 
    cd $builddir
    config="$SRCROOT/openmpi/configure CFLAGS=\"-arch $arch\" CXXFLAGS=\"-arch $arch\" OBJCFLAGS=\"-arch $arch\" --prefix=$OMPI_PREFIX $OMPI_OPTIONS --build=$build_arch --host=$host_arch"
    echo "--> Running configure: $config"
    eval $config

    if test $? != 0; then
        echo "*** Problem running configure - aborting!"
        exit 1
    fi

    #
    # Build
    #
    cmd="make -j 4 all"
    echo "--> Building: $cmd"
    eval $cmd

    if test $? != 0; then
        echo "*** Problem building - aborting!"
        exit 1
    fi

    #
    # Install into tmp place
    #
    if test $real_install -eq 1 ; then
        distdir="dist"
        real_install=0
    else
        distdir="dist-$arch"
    fi
    fulldistdir="$OBJROOT/$distdir"
    cmd="make DESTDIR=$fulldistdir install"
    echo "--> Installing:"
   eval $cmd

    if test $? != 0; then
        echo "*** Problem installing - aborting!"
        exit 1
    fi

    echo "Cleaning out source tree..."
    make distclean > /dev/null

    distdir=
    fulldistdir=
done


########################################################################
#
# Make the fat binary
#
########################################################################
print_arch_if() {
    case "$1" in
        ppc)
            echo "#ifdef __ppc__" >> mpi.h
            ;;
        ppc64)
            echo "#ifdef __ppc64__" >> mpi.h
            ;;
        i386)
            echo "#ifdef __i386__" >> mpi.h
            ;;
        x86_64)
            echo "#ifdef __x86_64__" >> mpi.h
            ;;
    esac
} 

# Set arch to the first arch in the list.  Go through the for loop,
# although we'll break out at the end of the first time through.  Look
# at the other arches that were built by using ls.

for arch in $RC_ARCHS ; do
    cd $OBJROOT
    other_archs=`ls -d dist-*`
    fulldistdir="$OBJROOT/dist"

    echo "--> Creating fat binares and libraries"
    for other_arch in $other_archs ; do
        cd "$fulldistdir"

        # <prefix>/bin
        files=`find ./${OMPI_PREFIX}/bin -type f -print`
        for file in $files ; do
            other_file="$OBJROOT/${other_arch}/$file"
            if test -r $other_file ; then
                lipo -create $file $other_file -output $file
            fi
        done

        # <prefix>/lib - ignore .la files
        files=`find ./${OMPI_PREFIX}/lib -type f -print | grep -v '\.la$'`
        for file in $files ; do
            other_file="$OBJROOT/${other_arch}/$file"
            if test -r $other_file ; then
                lipo -create $file $other_file -output $file
            else
                echo "Not lipoing missing file $other_file"
            fi
        done

    done

    cd $OBJROOT

    echo "--> Creating multi-architecture mpi.h"
    # mpi.h
    # get the top of mpi.h
    mpih_top=`grep -n '@OMPI_BEGIN_CONFIGURE_SECTION@' $OBJROOT/dist/${OMPI_PREFIX}/include/mpi.h | cut -f1 -d:`
    mpih_top=`echo "$mpih_top - 1" | bc`
    head -n $mpih_top $OBJROOT/dist/${OMPI_PREFIX}/include/mpi.h > mpih_top.txt

    # now the bottom of mpi.h
    mpih_bottom_top=`grep -n '@OMPI_END_CONFIGURE_SECTION@' $OBJROOT/dist/${OMPI_PREFIX}/include/mpi.h | cut -f1 -d:`
    mpih_bottom_bottom=`wc -l $OBJROOT/dist/${OMPI_PREFIX}/include/mpi.h | cut -f1 -d/`
    mpih_bottom=`echo "$mpih_bottom_bottom - $mpih_bottom_top" | bc`
    tail -n $mpih_bottom $OBJROOT/dist/${OMPI_PREFIX}/include/mpi.h > mpih_bottom.txt

    # now get our little section of fun
    mpih_top=`echo "$mpih_top + 1" | bc`
    mpih_fun_len=`echo "$mpih_bottom_top - $mpih_top + 1" | bc`
    head -n $mpih_bottom_top $OBJROOT/dist/${OMPI_PREFIX}/include/mpi.h | tail -n $mpih_fun_len > mpih_$arch.txt

    # start putting it back together
    rm -f mpi.h
    cat mpih_top.txt > mpi.h

    print_arch_if $arch
    cat mpih_$arch.txt >> mpi.h
    echo "#endif" >> mpi.h

    for other_arch_dir in $other_archs ; do
        other_arch=`echo $other_arch_dir | cut -f2 -d-`
        mpih_top=`grep -n '@OMPI_BEGIN_CONFIGURE_SECTION@' $OBJROOT/$other_arch_dir/${OMPI_PREFIX}/include/mpi.h | cut -f1 -d:`
        mpih_bottom_top=`grep -n '@OMPI_END_CONFIGURE_SECTION@' $OBJROOT/$other_arch_dir/${OMPI_PREFIX}/include/mpi.h | cut -f1 -d:`
        mpih_fun_len=`echo "$mpih_bottom_top - $mpih_top + 1" | bc`
        head -n $mpih_bottom_top $OBJROOT/$other_arch_dir/${OMPI_PREFIX}/include/mpi.h | tail -n $mpih_fun_len > mpih_$other_arch.txt

        print_arch_if $other_arch
        cat mpih_$other_arch.txt >> mpi.h
        echo "#endif" >> mpi.h
    done

    cat mpih_bottom.txt >> mpi.h
    mv mpi.h $OBJROOT/dist/${OMPI_PREFIX}/include/.
    rm mpih*
    break
done

mv $OBJROOT/dist/* $DSTROOT
