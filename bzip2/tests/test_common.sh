#!/bin/sh -eu

checkattrs() {
	local file key val xlist

	file=$1
	shift

	if [ $# -eq 0 ]; then
		# We shouldn't have had any xattrs...
		echo "$file: no attrs expected"

		xlist=$(xattr -l "$file")

		if [ ! -z "$xlist" ]; then
			1>&2 echo "$file: xattrs should be empty, but found:"
			1>&2 echo "$xlist"
			return 1
		fi

		return 0
	fi

	echo "$file: checking for $# attrs"
	while [ $# -ne 0 ]; do
		key=${1%%=*}
		val=${1##*=}
		shift

		echo "$file: checking for $key"
		contents=$(xattr -p "$key" "$file")
		if [ -z "$contents" ]; then
			1>&2 echo "$file: xattr $key is missing"
			return 1
		elif [ "$contents" != "$val" ]; then
			1>&2 echo "$file: xattr $key expected: $val, actual: $contents"
			return 1
		fi
	done

	return 0
}

checkfile() {
	local contents file key val xlist

	file=$1
	shift

	if ! grep -q 'Lorem Ipsum' "$file" ||
	    grep -qv 'Lorem Ipsum' "$file"; then
		1>&2 echo "[$file] Expected: Lorem Ipsum"
		1>&2 echo "Actual:"
		1>&2 cat "$file"
		return 1
	fi

	checkattrs "$file" "$@"
}

mkfile() {
	local file key val

	file=$1
	shift

	# We don't want to simply truncate the file if it's already there, in
	# case it has stale xattrs.

	rm -f "$file"
	echo 'Lorem Ipsum' > "$file"
	while [ $# -ne 0 ]; do
		key=${1%%=*}
		val=${1##*=}
		shift

		xattr -w "$key" "$val" "$file"
	done
}
