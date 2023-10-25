#!/bin/sh

rm -rf i18n/csmapper
rm -rf i18n/esdb

mkdir -p i18n/csmapper
mkdir -p i18n/esdb

xcrun bmake -C csmapper -j4 all
(find csmapper '(' -name '*.646' -or -name '*.mps' ')' -print0; \
    find csmapper -maxdepth 1 \
    '(' -type f -name 'charset.*' -or -name 'mapper.*' ')' \
    -and -not -name '*.src' -print0) | \
    xargs -0 tar -cvf - | (cd i18n; tar -xvf -)

xcrun bmake -C esdb -j4 all
(find esdb -name '*.esdb' -print0; \
    find esdb -maxdepth 1 -type f -name 'esdb.*' -print0) | \
    xargs -0 tar -cvf - | (cd i18n; tar -xvf -)

