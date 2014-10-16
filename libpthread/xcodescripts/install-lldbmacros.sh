#!/bin/bash -e
# install the pthread lldbmacros into the module

mkdir -p $DWARF_DSYM_FOLDER_PATH/$DWARF_DSYM_FILE_NAME/Contents/Resources/Python || true
rsync -aq $SRCROOT/lldbmacros/* $DWARF_DSYM_FOLDER_PATH/$DWARF_DSYM_FILE_NAME/Contents/Resources/Python/
