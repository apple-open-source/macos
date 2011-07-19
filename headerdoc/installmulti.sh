#!/bin/sh

DSTROOT="$1"

perl_libdir="$(perl -e 'require Config; print "$Config::Config{'privlib'}\n";')"

if [ -d /System/Library/Perl/Extras ] ; then
	perl_libdir="$(echo "$perl_libdir" | sed 's/Perl/Perl\/Extras/')"
fi

perl_libdir="$DSTROOT$perl_libdir"

echo "perl_libdir is $perl_libdir"

	if [ -f "/usr/local/versioner/perl/versions" ] ; then
		for dirname in `grep -v DEFAULT /usr/local/versioner/perl/versions` ; do
			dir="/System/Library/Perl/Extras/$dirname"

			if [ "$DSTROOT" != "" ] ; then
				dir="$DSTROOT$dir"
				mkdir -p $dir
			fi

			echo $dir
			if [ -d "$dir" ] ; then
				if [ "$dir" != "$perl_libdir" ] ; then
					echo "DIR: $dir"
					echo "PL: $perl_libdir"
					mkdir -p "$dir/HeaderDoc"
					# mkdir -p "$dir/HeaderDoc/bin"
					for name in $perl_libdir/HeaderDoc/*.pm Modules/HeaderDoc/Availability.list ; do
						if [ -f "$perl_libdir/HeaderDoc/$(basename "$name")" ] ; then
							ln -f "$perl_libdir/HeaderDoc/$(basename "$name")" "$dir/HeaderDoc/$(basename "$name")"
						fi
					done
					# for name in $perl_libdir/HeaderDoc/bin/* ; do
						# ln -f "$perl_libdir/HeaderDoc/bin/$(basename "$name")" "$dir/HeaderDoc/bin/$(basename "$name")"
					# done
				fi
			fi
		done
	fi

