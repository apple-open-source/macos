#!/bin/sh

set -e

srcroot="$1"

ripperdir="$srcroot"/ext/ripper

update_mtimes() {
	local ref

	ref="$1"
	shift

	for upf in "$@"; do
		if [ ! -e "$upf" ]; then
			1>&2 echo "$upf does not exist"
			exit 1
		fi
	done

	1>&2 echo "Adjusting mtime[ref=$ref] of $*"
	touch -r "$ref" "$@"
}

# id.h's dependencies need to be on the same page.
update_mtimes "$srcroot"/id.h \
    "$srcroot"/tool/generic_erb.rb "$srcroot"/template/id.h.tmpl \
    "$srcroot"/defs/id.def

# Make ripper.y mtime consistent with its own depedendencies to avoid a chain in
# which ripper.y needs regenerated.
update_mtimes "$srcroot"/id.h \
    "$ripperdir"/tools/preproc.rb "$ripperdir"/tools/dsl.rb "$srcroot"/parse.y
update_mtimes "$srcroot"/parse.y \
    "$ripperdir"/ripper.y

# Ensure that our bison(1)-output files have the same mtime as their original
# source files to avoid regenerating them.  The distfiles we grab from the
# ruby project have already generated correct versions of the parsers to begin
# with.
for parsery in $(find "$srcroot" -name '*.y'); do
	parserc="${parsery%.y}.c"
	if [ -e "$parserc" ]; then
		update_mtimes $parsery $parserc
	fi
done
