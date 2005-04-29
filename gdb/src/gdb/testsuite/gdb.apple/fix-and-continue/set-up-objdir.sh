#! /bin/sh

# Create the object directory.
# Copy the sources into the objdir (because we'll be moving around/rebuilding
# the source files)
# Substitute the correct values into the Makefile if necessary.

if [ $# -ne 2 ]
then
  echo ERROR: Usage: $0 source-directory-name object-directory-name
  exit 1
fi

if [ ! -d "$1" ]
then
  echo ERROR: source directory \"$1\" does not exist.
  exit 1
fi

if [ ! -d "$2" ]
then
  if mkdir -p "$2"
  then
    :
  else
    echo ERROR: Unable to create object directory \"$2\".
    exit 1
  fi
fi

srcdir=`cd "$1";pwd`
objdir=`cd "$2";pwd`

cd "$objdir"

rm -f *.c *.o a.out Makefile*

(cd "$srcdir"; /usr/bin/tar -c -f - . ) | /usr/bin/tar xBpf - >/dev/null 2>&1

if [ ! -f "Makefile-in" -a ! -f "Makefile" ]
then
  echo ERROR: No makefile found!
  exit 1
fi

if [ -f Makefile-in -a ! -f Makefile ]
then
  cat Makefile-in | sed -e "s|@SRCDIR@|$srcdir|g" \
                        -e "s|@OBJDIR@|$objdir|g" > Makefile
fi

rm -f *.expdontrun

exit 0
