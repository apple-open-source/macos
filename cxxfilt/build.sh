#!/bin/sh -x

ACTION="$1"
TEMP_DIR="$2"
SRCROOT="$3"
SYMROOT="$4"
DSTROOT="$5"
CFLAGS="$6"

export CFLAGS  # to configure

if [ "$ACTION" = clean ]; then
  rm -rf "$TEMP_DIR"
  rm -rf "$SYMROOT"/usr
elif [ "$ACTION" = build -o "$ACTION" = "" -o "$ACTION" = install ] ; then
  if [ \! -f "$TEMP_DIR"/CONFIGURED_OK ] ; then
    rm -rf "$TEMP_DIR"
    mkdir -p "$TEMP_DIR" || exit 1
    cd "$TEMP_DIR" || exit 1
    # The --target is because bfd doesn't actually work on powerpc-darwin;
    # libiberty doesn't care about the target, but c++filt does, in one
    # very small way: it cares whether or not the target prepends an
    # underscore.  It turns out that this target does prepend an underscore.
    FAKE_TARGET=mn10300-elf
    # The --without-target-subdir is to work around what appears to be a bug.
    # The --disable-nls is because Darwin doesn't seem to have msgfmt.
    "$SRCROOT/src/configure" --prefix=/usr/local --target=$FAKE_TARGET \
	--without-target-subdir --enable-install-libiberty --disable-nls \
	|| exit 1
    touch "$TEMP_DIR"/CONFIGURED_OK || exit 1
  fi
  cd "$TEMP_DIR" || exit 1
  make -j 2 all || exit 1
  make DESTDIR="$SYMROOT" install-libiberty install-binutils || exit 1

  # Move the files to where we want them.
  cd "$SYMROOT"/usr || exit 1
  mkdir bin || exit 1
  mv local/bin/${FAKE_TARGET}-c++filt bin/c++filt  || exit 1
  mkdir -p share/man/man1 || exit 1
  mv local/man/man1/${FAKE_TARGET}-c++filt.1 share/man/man1/c++filt.1 \
      || exit 1
  rm -r local/bin local/man local/info local/${FAKE_TARGET} || exit 1
elif [ "$ACTION" = installhdrs ] ; then
  # headers get installed at the same time as the library
  :
else
  echo "unknown action $ACTION"
  exit 1
fi

if [ "$ACTION" = install ] ; then
  cd "$SYMROOT"/usr || exit 1
  mkdir -p "$DSTROOT"/usr/bin "$DSTROOT"/usr/share/man/man1 || exit 1
  strip bin/c++filt -o "$DSTROOT"/usr/bin/c++filt || exit 1
  cp -p share/man/man1/c++filt.1 "$DSTROOT"/usr/share/man/man1/c++filt.1 \
      || exit 1
  find local -type f -print | cpio -pdm "$DSTROOT"/usr || exit 1
fi

exit 0