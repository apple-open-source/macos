## Build & Test instructions for Apple Internal LibXML2 fork on macOS

Useful Quip Doc for LibXML2/LibXSLT related topics: <https://quip-apple.com/6bkDAxzomUJH>

The files in this directory are generated programatically as part of the regular automake-based libxml2
build process. The manner in which they are generated is sufficiently complicated that for now we'll
stick with checking in the generated files and updating them by hand when needed.

`Pregenerated Files/include/config.h` and `Pregenerated Files/include/libxml/xmlversion.h`: Taken directly from a regular automake-based build of `libxml2`.
`Pregenerated Files/xml2-config`: Hand-modified based on generated xml2-config to include the SDKROOT in paths.

### Automatic Script for Setup (skips steps 1-3):
```
./Tools/setup-apple-internal.sh
```

### Steps to build project & rebuild generated files:

1. Install Apple Internal Homebrew  <https://sdp.apple.com/docs/brew/install/>
```
# Get the Liv CLI
/usr/bin/curl -Lg 'https://artifacts.apple.com/sdp/g/liv/liv-[RELEASE].macos' -o /tmp/liv && chmod +x /tmp/liv

# Run the Liv CLI
/tmp/liv brew install
```

2. Install autoconf, automake, libtool, pkg-config from Homebrew:
```
brew install autoconf autoconf-archive automake libtool pkg-config

# pkg-config installation may fail with this error:
# Error: Cannot install pkg-config because conflicting formulae are installed.
#   pkgconf: because both install `pkg.m4` file

# You want pkg-config (note the hyphen) and _not_ pkgconfig; these are two distinct formulae:
# pkg-config: https://formulae.brew.sh/formula/pkg-config
# pkgconfig:  https://formulae.brew.sh/formula/pkgconf

# If you get the above error, uninstall pkgconf, then reattempt the pkg-config installation
brew uninstall --force pkgconf
brew install pkg-config

# Verify the installation succeeded:
brew list
==> Formulae
autoconf            automake            m4
autoconf-archive    libtool             pkg-config
```

3. In the libxml2/libxml2 directory (since the Apple-internal `libxml2` repo has a `libxml2` subdirectory):

```
# Autotools setup
# Use Makefile.am, configure.ac, etc. to generate a Makefile and finally binaries
glibtoolize --force
cp /opt/homebrew/Cellar/pkgconf/2.3.0_1/share/aclocal/pkg.m4 ./m4/
aclocal -I m4
autoheader
automake --add-missing --force-missing
autoupdate
autoconf

export CC="xcrun -sdk macosx.internal cc -target arm64e-apple-macos"

./configure --prefix=/usr --without-iconv --with-icu --without-lzma --without-python --with-xptr-locs --with-zlib

# Build the project
make
```

NOTE: If you run into issues when running ./configure, that look something like this:
```
./configure: line 21534: syntax error near unexpected token `noext,'
./configure: line 21534: `AX_CXX_COMPILE_STDCXX_11(noext, mandatory)'
```
Please copy the two files in `Pregenerated Files/extra` into `libxml2/libxml2/m4`:
```
# Assuming you are in the top-level libxml2 directory
cp "./Pregenerated Files/extra/"* libxml2/m4/
```
Then rerun all the steps above.

4. [Optional] Run tests (compare output prior to patch as there is some spew):
```
make -j $(sysctl -n hw.ncpu)
make check
# To run tests with AddressSanitizer enabled, re-run configure with this environment variable:
  CC="xcrun -sdk macosx.internal cc -target arm64e-apple-macos -fsanitize=address"
```

5. Copy replacement files into place:
```
cp -p config.h "../Pregenerated Files/include/"
cp -p include/libxml/xmlversion.h "../Pregenerated Files/include/libxml/"
cp -p xml2-config "../Pregenerated Files/"
```

6. Update Makefile.fuzz from changes to libxml2/fuzz/Makefile.

7. Run git-add on changed files (including those in libxml2), and check them in.
```
cd ..
git add "Pregenerated Files/include/config.h" "Pregenerated Files/include/libxml/xmlversion.h" "Pregenerated Files/xml2-config"
```
8. Update libxml2.plist with libxml2 version, md5 hash, radars to upstream as needed.

9. Run git-commit to commit the updated files.

10. Clean up the files generated from Step 3.
```
git status --ignored
git clean --force -d -x
```
