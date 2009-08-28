#! /bin/sh
# This script will build resolver library
#

TARGET=resolver
echo
echo "=================================================="
echo $TARGET
echo "=================================================="

PROJECT_ROOT=../netinfo.build/root
TARGET_DYNAMIC_LIB=libresolv.9.dylib
TARGET_LIB_LINK=libresolv.dylib
TARGET_STATIC_LIB=libresolv.a
TARGET_VERSION=A
TARGET_DIR=usr/lib
COMPAT_VERS=1.0.0
CURR_VERS=2.0.0
TARGET_MAN_DIR=/usr/share/man/man5

BLD=$TARGET.build
DERIVED_SRC_DIR=$BLD/derived_src
OBJECT_DIR=$BLD/objects

DST_DIR=$PROJECT_ROOT/$TARGET_DIR
PUBLIC_HEADERS_DIR=$PROJECT_ROOT/usr/include
PRIVATE_HEADERS_DIR=$PROJECT_ROOT/usr/local/include
PROJECT_HEADERS_DIR=../netinfo.build/ProjectHeaders
MAN_DIR=${PROJECT_ROOT}${TARGET_MAN_DIR}

CFLAGS="-g -dynamic -fno-common -Wall -D_REENTRANT"
INCLUDE="-I. -I$PUBLIC_HEADERS_DIR"
LIBS=
nm /usr/lib/libSystem.B.dylib | grep -q pselect
if [ $? = 1 ]; then
  CFLAGS="$CFLAGS -DNEED_PSELECT"
fi

CLEAN=0
BUILD=1

while [ $# != 0 ]; do
  if [ ${1}x = cleanx ]; then
    CLEAN=1
    BUILD=0
  fi

  if [ ${1}x = freshx ]; then
    CLEAN=1
    BUILD=1
  fi

  shift
done

if [ $CLEAN = 1 ]; then
  echo "Cleaning $TARGET"
  rm -rf $BLD
  rm -rf .gdb_history
fi

if [ $BUILD = 0 ]; then
  echo "Done"
  exit 0
fi

if [ ! -d $BLD ]; then
  mkdir $BLD
fi
if [ ! -d $DERIVED_SRC_DIR ]; then
  mkdir $DERIVED_SRC_DIR
  ln -s ../.. $DERIVED_SRC_DIR/NetInfo
fi
if [ ! -d $OBJECT_DIR ]; then
  mkdir $OBJECT_DIR
fi

if [ ! -d $DST_DIR ]; then
  if [ -f /bin/mkdirs ]; then
    mkdirs $DST_DIR
  else
    mkdir -p $DST_DIR
  fi
fi

if [ ! -d $PUBLIC_HEADERS_DIR ]; then
  if [ -f /bin/mkdirs ]; then
    mkdirs $PUBLIC_HEADERS_DIR
    mkdirs $PUBLIC_HEADERS_DIR/arpa
  else
    mkdir -p $PUBLIC_HEADERS_DIR
    mkdir -p $PUBLIC_HEADERS_DIR/arpa
  fi
fi

if [ ! -d $PRIVATE_HEADERS_DIR ]; then
  if [ -f /bin/mkdirs ]; then
    mkdirs $PRIVATE_HEADERS_DIR
  else
    mkdir -p $PRIVATE_HEADERS_DIR
  fi
fi

if [ ! -d $PROJECT_HEADERS_DIR ]; then
  if [ -f /bin/mkdirs ]; then
    mkdirs $PROJECT_HEADERS_DIR
  else
    mkdir -p $PROJECT_HEADERS_DIR
  fi
fi

if [ ! -f $PUBLIC_HEADERS_DIR/dns.h ]; then
  echo "cp dns.h $PUBLIC_HEADERS_DIR"
  cp dns.h $PUBLIC_HEADERS_DIR
fi

if [ ! -f $PUBLIC_HEADERS_DIR/dns_util.h ]; then
  echo "cp dns_util.h $PUBLIC_HEADERS_DIR"
  cp dns_util.h $PUBLIC_HEADERS_DIR
fi

if [ ! -f $PRIVATE_HEADERS_DIR/dns_private.h ]; then
  echo "cp dns_private.h $PRIVATE_HEADERS_DIR"
  cp dns_private.h $PRIVATE_HEADERS_DIR
fi

if [ ! -f $PROJECT_HEADERS_DIR/dns_private.h ]; then
  echo "cp dns_private.h $PROJECT_HEADERS_DIR"
  cp dns_private.h $PROJECT_HEADERS_DIR
fi

if [ ! -f $PUBLIC_HEADERS_DIR/resolv.h ]; then
  echo "cp resolv.h $PUBLIC_HEADERS_DIR"
  cp resolv.h $PUBLIC_HEADERS_DIR
fi

if [ ! -f $PUBLIC_HEADERS_DIR/nameser.h ]; then
  echo "cp nameser.h $PUBLIC_HEADERS_DIR"
  cp nameser.h $PUBLIC_HEADERS_DIR
  echo "ln -s ../nameser.h $PUBLIC_HEADERS_DIR/arpa/nameser.h"
  ln -s ../nameser.h $PUBLIC_HEADERS_DIR/arpa/nameser.h
fi

if [ ! -d $MAN_DIR ]; then
  if [ -f /bin/mkdirs ]; then
    mkdirs $MAN_DIR
  else
    mkdir -p $MAN_DIR
  fi
fi

if [ ! -f $MAN_DIR/resolver.5 ]; then
  echo "cp resolver.5 $MAN_DIR"
  cp resolver.5 $MAN_DIR
fi

MAKE_TARGET=0
if [ ! -f $DST_DIR/$TARGET_DYNAMIC_LIB ]; then
  MAKE_TARGET=1
fi

for c in *.[mc]
do
  OBJ=`echo $c | sed 's/..$/.o/'`
  SRC=$c
  DST=$OBJECT_DIR/$OBJ

  MAKEIT=1
  if [ -f $DST ]; then
    RECENT=`/bin/ls -1t $DST $SRC | head -1`
    if [ $RECENT = $DST ]; then
      MAKEIT=0
    fi
  fi

  if [ $MAKEIT = 1 ]; then
    MAKE_TARGET=1
    echo cc -c $CFLAGS $INCLUDE -o $DST $SRC
    cc -c $CFLAGS $INCLUDE -o $DST $SRC
  fi
done

if [ $MAKE_TARGET = 1 ]; then
	echo libtool -dynamic -install_name /${TARGET_DIR}/${TARGET_DYNAMIC_LIB} -compatibility_version $COMPAT_VERS -current_version $CURR_VERS -arch_only ppc -o $DST_DIR/${TARGET_DYNAMIC_LIB} $OBJECT_DIR/*.o -lcc_dynamic -framework System
	libtool -dynamic -install_name /${TARGET_DIR}/${TARGET_DYNAMIC_LIB} -compatibility_version $COMPAT_VERS -current_version $CURR_VERS -arch_only ppc -o $DST_DIR/${TARGET_DYNAMIC_LIB} $OBJECT_DIR/*.o -lcc_dynamic -ldnsinfo -framework System
	echo "ln -s $TARGET_DYNAMIC_LIB $DST_DIR/${TARGET_LIB_LINK}"
	ln -s $TARGET_DYNAMIC_LIB $DST_DIR/${TARGET_LIB_LINK}
fi

echo "Finished building $TARGET"
exit 0
