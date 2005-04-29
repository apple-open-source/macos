#!/bin/sh

# install message catalogs - very primitive

lgs=$1
locdir=$2
M=

# if no locale then do nothing
if [ "$locdir" = "" ]; then
  exit 0
fi

if [ "$lgs" = "??" ]; then
  M=mess.*.cat
else
  for i in $lgs
  do
    if [ -f mess.$i.cat ]; then
      M="$M mess.$i.cat"
    else
      echo "==== No mess.$i.cat found. ===="
    fi
  done
fi

for j in $M; do
  if [ -f $j ]; then
    i=`echo $j | sed -e 's/mess.//; s/.cat//'`
    dest=`echo $locdir | sed -e "s/%N/man/; s/%L/$i/"`
    dest=${PREFIX}$dest
    echo "mkdir -p `dirname $dest`"
    mkdir -p `dirname $dest`;
    echo "install -c -m 644 $j $dest"
    install -c -m 644 $j $dest
  fi
done

