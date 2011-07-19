#!/bin/sh
set -e
set -x

install -g "$INSTALL_GROUP" \
	-o "$INSTALL_OWNER" \
	-m "$INSTALL_MODE_FLAG",a+x \
	"$SRCROOT"/src/vimtutor \
	"$DSTROOT"/usr/bin

DESTDIR="$DSTROOT"/usr/share/vim
RUNTIMEDIR="$DESTDIR"/vim73

mkdir -p "$RUNTIMEDIR"
tar -cf - -C "$SRCROOT"/runtime \
	--exclude spell \
	--exclude rgb.txt \
	--exclude macmap.vim \
	--exclude makemenu.vim \
	--exclude \*.1 \
	--exclude \*.man \
	--exclude Makefile \
	--exclude \*.info \
	--exclude \*.dict \
	--exclude \*.png \
	--exclude icons \
	--exclude \*.awk \
	--exclude termcap \
	--exclude \*.gif \
	--exclude \*.xpm \
	--exclude doctags.c \
	--exclude vimlogo.\* \
	. | tar -xof - -C "$RUNTIMEDIR"

mkdir -p "$RUNTIMEDIR"/spell

for f in \
	spell/cleanadd.vim \
	spell/en.ascii.spl \
	spell/en.ascii.sug \
	spell/en.latin1.spl \
	spell/en.latin1.sug \
	spell/en.utf-8.spl \
	spell/en.utf-8.sug \
	spell/he.vim \
	spell/yi.vim \
	macros/maze/Makefile \
	macros/hanoi/click.me.info \
	macros/hanoi/poster.info \
	macros/hanoi.info \
	macros/life/click.me.info \
	macros/maze/maze_5.78.info \
	macros/maze/poster.info \
	macros/maze/README.txt.info \
	macros/maze.info \
	macros/README.txt.info \
	macros/urm/README.txt.info \
	macros/urm.info \
	tutor/README.txt.info \
	tutor/tutor.info \
	tools/mve.awk \
	tools/ccfilter.1 \
	tools/shtags.1 \
	; do
	cp "$SRCROOT"/runtime/"$f" "$RUNTIMEDIR"/"$f"
done


cp "$SRCROOT"/local/vimrc "$DESTDIR"/vimrc

chmod -R "$ALTERNATE_MODE" "$DESTDIR"
chown -R "$ALTERNATE_OWNER:$ALTERNATE_GROUP" "$DESTDIR"
chmod a-x,u+w "$RUNTIMEDIR"/tutor/README.txt.info
