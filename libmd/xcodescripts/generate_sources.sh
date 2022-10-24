#!/bin/sh

stdvar()
{
	bitsz=$1
	variant=$2

	varupper=$(echo ${variant} | tr '[:lower:]' '[:upper:]')
	length=$(( ${bitsz} / 8 ))

	# Digest length
	echo "#define LENGTH ${length}"

	# The purpose of each following replacement::
	# - mdX -> ${variant}: which header to use
	# - MDX -> ${uppercase_variant}_: prefix for the End, Fd, FdChunk, File,
	#    FileChunk, and Data methods.
	# - ${uppercase_variant}__ -> ${uppercase_variant}_: fixes the name of
	#    the respective *_CTX types; they're MDX_CTX in the template and end
	#    up with a double underbar after the above replacement.
	sed -e "s/mdX/${variant}/g" -e "s/MDX/${varupper}_/g" \
	    -e "s/${varupper}__/${varupper}_/g" ${SRCROOT}/libmd/mdXhl.c
}

# The md4 and md5 variants are nonstandard because the digest length is
# naturally not indicated in the names. Neither MD4 nor MD5 include an underbar
# in their method prefixes, but it's not worth making stdvar() account for that
# kind of scenario at this time.
for variant in md4 md5; do
	varupper=$(echo ${variant} | tr '[:lower:]' '[:upper:]')
	dstfile=${BUILT_PRODUCTS_DIR}/${variant}hl.c
	echo "#define LENGTH 16" > ${dstfile}
	sed -e "s/mdX/${variant}/g" -e "s/MDX/${varupper}/g" ${SRCROOT}/libmd/mdXhl.c >> ${dstfile}
done

for variant in sha sha1 sha224 sha256 sha384 sha512; do
	dstfile=${BUILT_PRODUCTS_DIR}/${variant}hl.c
	bitsz=$(echo ${variant} | tr -d '[:alpha:]')
	case ${variant} in
		sha|sha1)
			# Deviation from the upstream build: we generate shahl.c
			# instead of sha0hl.c, but this is a fairly
			# insignificant difference.

			# variant=sha1 would typically use 'sha1.h' if it were a
			# "standard variant" like the others, but it actually
			# uses 'sha.h' along with sha0 (named `sha` here).
			stdvar 160 ${variant} | sed -e 's/sha1\.h/sha.h/' > ${dstfile}
			;;
		*)
			stdvar ${bitsz} ${variant} > ${dstfile}
			;;
	esac
done

