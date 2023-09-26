#!/bin/sh
set -ex

DESTDIR="$DSTROOT"/usr/share/i18n

install -d -o root -g wheel -m 0755 "$DESTDIR"

rsync -av --no-owner --no-group --exclude 'README' "$SRCROOT"/i18n/ "$DESTDIR"
