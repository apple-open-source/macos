#!/bin/sh

version=$(sw_vers -buildVersion)
case $version in
   10[A-Z]*)
	arch='-arch i386 -arch x86_64 -arch ppc'
	projects="Heimdal"
        release="SnowLeopard"
	;;
   11[A-Z]*)
	arch='-arch i386 -arch x86_64'
	projects="HeimdalCompilers HeimdalMacOSX HeimdalOpenDirectory"
        release="Barolo"
	;;
   *) echo "unknown build $version";
esac

echo "Building $projects with $arch for $release"

rm -rf build

drop=$(git log -1 --pretty=format:%h)

roots=
for a in $projects ; do
	buildit . -project $a -target=$a \
	  -release $release -rootsDirectory $HOME/BuildRoots \
	  $arch -merge /  "$@"|| exit 1
	dst=$HOME/Roots/${a}-${version}-${drop}.cpio.gz
	rm $dst 2> /dev/null
	ditto -cz $HOME/BuildRoots/$a.roots/$a~dst $dst
	roots="$roots $dst"
done

echo "done"
echo "Roots: $roots"

scp $roots lha@public.apple.com:Public
