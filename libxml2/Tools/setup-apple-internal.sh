#!/bin/bash

set -e
set -o pipefail

echo "Starting Apple Internal libXML2 setup..."

echo "Checking correct tools are installed..."
for cmd in brew glibtoolize aclocal autoheader automake autoupdate autoconf pkgconf; do
    if ! command -v "$cmd" &> /dev/null; then
        echo "❌ Error: $cmd is not installed. Please install the proper tools with `brew install autoconf autoconf-archive automake libtool pkg-config`."
        exit 1
    else
        echo "✅ $cmd is correctly installed."
    fi
done

# Grab our current directory (libxml2/Tools) and go to the ../libxml2 directory.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_DIR="$SCRIPT_DIR/../libxml2"

if [[ -d "$TARGET_DIR" ]]; then
    cd "$TARGET_DIR"
    echo "Changed directory to $(pwd)"
else
    echo "❌ Error: Directory $TARGET_DIR does not exist."
    exit 1
fi

# Track whether we already retried, so we don't get into an infinite recursion loop.
RETRY_ATTEMPTED=false

run_build_steps() {
    echo "Running glibtoolize..."
    glibtoolize --force
    echo "✅ Done running glibtoolize!"

    BREW_PKG_M4_PREFIX=$(brew --prefix pkgconf)
    PKG_M4_SOURCE="$BREW_PKG_M4_PREFIX/share/aclocal/pkg.m4"
    PKG_M4_DEST="./m4/"
    if [[ -f "$PKG_M4_SOURCE" ]]; then
        echo "Copying pkg.m4 to $PKG_M4_DEST..."
        cp "$PKG_M4_SOURCE" "$PKG_M4_DEST"
    else
        echo "❌ Error: pkg.m4 not found at $PKG_M4_SOURCE"
        exit 1
    fi
    echo "✅ Done copying pkg.m4 to $PKG_M4_DEST!"

    echo "Running aclocal..."
    aclocal -I m4
    echo "✅ Done running aclocal!"

    echo "Running autoheader..."
    autoheader
    echo "✅ Done running autoheader!"

    echo "Running automake..."
    automake --add-missing --force-missing
    echo "✅ Done running automake!"

    echo "Running autoupdate..."
    autoupdate
    echo "✅ Done running autoupdate!"

    echo "Running autoconf..."
    autoconf
    echo "✅ Done running autoconf!"

    export CC="xcrun -sdk macosx.internal cc -target arm64e-apple-macos"
    echo "✅ Compiler set to: $CC"

    echo "Running configure script..."
    if ./configure --prefix=/usr --without-iconv --with-icu --without-lzma --without-python --with-xptr-locs --with-zlib; then
        echo "✅ Done running configure script!"
    else
        if [[ "$RETRY_ATTEMPTED" == true ]]; then
            echo "❌ Configure failed again after retry. Exiting."
            exit 1
        fi

        echo "⚠️ Configure failed! Attempting to move extra files from '../Pregenerated Files/extra/' into './m4/' and retrying..."

        PREGEN_EXTRA_FILES="../Pregenerated Files/extra/"
        M4_DIR="./m4/"
        FILES_TO_COPY=(
                "ax_cxx_compile_stdcxx.m4"
                "ax_cxx_compile_stdcxx_11.m4"
        )

        if [[ -d "$M4_DIR" ]]; then
            for filename in "${FILES_TO_COPY[@]}"; do
                SRC="$PREGEN_EXTRA_FILES$filename"
                DEST="$M4_DIR$filename"
                if [[ -f "$SRC" ]]; then
                    cp "$SRC" "$DEST" || { echo "❌ Error: Failed to copy $SRC"; exit 1; }
                    echo "✅ Copied: $SRC → $DEST"
                else
                    echo "❌ Error: File not found: $SRC"
                    exit 1
                fi
            done
            echo "✅ Files copied successfully. Retrying configure..."

            # Set retry flag and retry configure
            RETRY_ATTEMPTED=true
            run_build_steps
        else
            echo "❌ Error: './m4/' directory not found."
            exit 1
        fi
    fi
}

# Start the first run
run_build_steps

echo "✅ Build setup completed successfully! You can now run make, make check, etc..."
