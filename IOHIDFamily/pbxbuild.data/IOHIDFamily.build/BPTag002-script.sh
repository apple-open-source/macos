#!/bin/sh
mkdir -p "$DERIVED_SOURCES_DIR/IOKit" && ln -sf "$SRCROOT/IOHIDFamily" "$DERIVED_SOURCES_DIR/IOKit/hid"
