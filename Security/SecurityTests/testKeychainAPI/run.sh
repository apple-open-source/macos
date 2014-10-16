#!/bin/sh

HOME=/tmp/kc$$
export HOME
mkdir -p $HOME/Library/Preferences

echo Running with HOME=$HOME

${LOCAL_BUILD_DIR}/testKeychainAPI.app/Contents/MacOS/testKeychainAPI -vnall
${LOCAL_BUILD_DIR}/testKeychainAPI.app/Contents/MacOS/testKeychainAPI -vsall

rm -rf /tmp/kc$$
