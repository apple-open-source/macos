XCOMM! /bin/sh
XCOMM
XCOMM $XFree86$
XCOMM
XCOMM Collects multiple outputs of x11perf.  Just feed it a list of files, each
XCOMM containing the output from an x11perf run, and this shell will extract the
XCOMM object/second information and show it in tabular form.  An 80-column line
XCOMM is big enough to compare 4 different servers.
XCOMM
XCOMM This script normally uses the results from $1 to extract the test label
XCOMM descriptions, so you can run x11perf on a subset of the test and then
XCOMM compare the results.  But note that x11perffill requires the labels file
XCOMM to be a superset of the x11perf results file.  If you run into an ugly
XCOMM situation in which none of the servers completes the desired tests 
XCOMM (quite possible on non-DEC servers :), you can use -l <filename> as $1 and
XCOMM $2 to force x11perfcomp to use the labels stored in file $2.  (You can run
XCOMM x11perf with the -labels option to generate such a file.)
XCOMM
XCOMM Mark Moraes, University of Toronto <moraes@csri.toronto.edu>
XCOMM Joel McCormack, DEC Western Research Lab <joel@decwrl.dec.com>
XCOMM

PATH=LIBPATH:.:$PATH
export PATH

set -e
tmp=${TMPDIR-/tmp}/rates.$$
trap "rm -rf $tmp" 0 1 2 15
mkdir $tmp || exit 1
mkdir $tmp/rates
ratio=
allfiles=
XCOMM Include relative rates in output?  Report only relative rates?
case $1 in
-r|-a)
	ratio=1
	shift;
	;;
-ro)
	ratio=2
	shift;
	;;
esac
XCOMM Get either the provided label file, or construct one from all the
XCOMM files given.
case $1 in
-l)	cp $2 $tmp/labels
	shift; shift
	;;
*)	for file in "$@"; do
		awk '$2 == "reps" || $2 == "trep" { print $0; next; }' $file |
 		sed 's/^.*: //' |
 		sed 's/ /_/g' |
 		awk 'NR > 1 	{ printf ("%s %s\n", prev, $0); } \
				{ prev = $0; }'
	done | tsort 2>/dev/null | sed 's/_/ /g' > $tmp/labels
	;;
esac
XCOMM Go through all files, and create a corresponding rate file for each
n=1
for i
do
XCOMM Get lines with average numbers, fill in any tests that may be missing
XCOMM then extract the rate field
	base=`basename $i`
	(echo "     $n  "
	 echo '--------'
	 awk '$2 == "reps" || $2 == "trep" { \
		line = $0; \
		next; \
	    } \
	    NF == 0 && line != "" { \
		print line; \
		line=""; \
		next; \
	    } \
	 ' $i > $tmp/$n.avg
	 fillblnk $tmp/$n.avg $tmp/labels |
	 sed 's/( *\([0-9]*\)/(\1/'   |
	 awk '$2 == "reps" || $2 == "trep" { \
	 					n = substr($6,2,length($6)-7); \
						printf "%8s\n", n; \
    	 				   }'
	) > $tmp/rates/$n
	echo "$n: $i"
	allfiles="$allfiles$tmp/rates/$n "
	n=`expr $n + 1`
done
case x$ratio in
x)
	ratio=/bin/cat
	;;
x1)
	ratio="perfboth $n"
	;;
*)
	ratio="perfratio $n"
	;;
esac
echo ''
(echo Operation; echo '---------'; cat $tmp/labels) |
paste $allfiles - | sed 's/	/  /g' | $ratio
rm -rf $tmp
