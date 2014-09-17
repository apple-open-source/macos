#!/bin/sh -ex

if [ -z "$OBJROOT" ]
then
	OBJROOT=.
fi

ARCH_CACHE="$OBJROOT/.test_arch_cache"

# Always test locally built products.
if [ -z "$TEST_NO_SOURCE_DYLD" ]
then
	export DYLD_LIBRARY_PATH="$BUILT_PRODUCTS_DIR"
	#export DYLD_PRINT_LIBRARIES=1
	export OPENSSL_X509_TEA_DISABLE=1
fi

make_arch_cache()
{
	if [ ! -f $ARCH_CACHE ]
	then
		for arch in $ARCHS
		do
			# Cannot do ppc since there is no /bin/sh for ppc
			if [ "$arch" != "`echo $arch | sed "s@ppc@@g"`" ]
			then
				continue
			fi

			valid="`lipo -detailed_info /usr/lib/dyld | grep -o $arch 2>/dev/null`"
			if [ "$valid" == "$arch" ]
			then
				echo " $arch" >> "$ARCH_CACHE"
			fi
		done
	fi
}

remove_arch_cache()
{
	rm -f "$ARCH_CACHE"
}

get_exec_arches()
{

	if [ -n "$1" ] && [ "$1" == "squash" ]
	then
		remove_arch_cache
	fi

	make_arch_cache
	cat $ARCH_CACHE
}

test_binary()
{
	if [ -n "$1" ]
	then
		for aa in `get_exec_arches`
		do
			arch -${aa} $@
		done
	fi
}

test_binary_in_dir()
{
	cd "$1"
	shift

	test_binary $@
}


