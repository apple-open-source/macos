#!/bin/bash
# from https://stashweb.sd.apple.com/projects/CORESERVICES/repos/rapport/browse
set -euo pipefail

srcPath="$1"
dstPath="$2"

mkdir -p $(dirname "$dstPath")

xcrun -sdk ${SDK_NAME} \
  clang -x c -P -E -include ${SRCROOT}/RegressionTests/bats_utd_plist.h "$srcPath" | \
  sed -e 's/<string>_FALSE_<\/string>/<false\/>/' -e 's/<string>_TRUE_<\/string>/<true\/>/' > "$dstPath"
plutil -convert binary1 "$dstPath"

[ "$(whoami)" == "root" ] || exit 0
chown root:wheel "$dstPath"
