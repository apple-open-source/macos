#!/bin/sh

TARGET="$DERIVED_SRC/KeySchema.cpp"
mkdir -p "$DERIVED_SRC"
/usr/bin/m4 "${PROJECT_DIR}/OSX/libsecurity_cdsa_utilities/lib/KeySchema.m4" > "$TARGET.new"
cmp -s "$TARGET.new" "$TARGET" || mv "$TARGET.new" "$TARGET"
TARGET="$DERIVED_SRC/Schema.cpp"
/usr/bin/m4 "${PROJECT_DIR}/OSX/libsecurity_cdsa_utilities/lib/Schema.m4" > "$TARGET.new"
cmp -s "$TARGET.new" "$TARGET" || mv "$TARGET.new" "$TARGET"
