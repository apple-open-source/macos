#!/bin/sh

VERBOSE=$1

EXITSTATUS=0

# for i in headerDoc2HTML.pl gatherHeaderDoc.pl headerwalk.pl Modules/HeaderDoc/*.pm ; do COMMENTS="$(grep -c '^[ 	]*#[ 	]*\/\*!' "$i")"; SUBS="$(grep -c '^[ 	]*sub[ 	]' "$i")"; echo "$i: $COMMENTS / $SUBS"; done
echo "Checking function coverage:"
echo

for i in headerDoc2HTML.pl gatherHeaderDoc.pl headerwalk.pl headerdoc_tp/tp_webkit_tools/filtermacros.pl Modules/HeaderDoc/*.pm ; do
    if [ -f "$i" ] ; then
	COMMENTS="$(grep -B 1 '^[ 	]*sub[ 	]' "$i" | grep -v '^[ 	]*sub[ 	]' | grep -v '^--' | grep -c '[ 	]*#[^#]*\*\/')"
	SUBS="$(grep -c '^[ 	]*sub[ 	]' "$i")";

	if [ $SUBS -eq $COMMENTS ] ; then
		printf "$i: \033[32m$COMMENTS / $SUBS\033[39m\n"
	else
		printf "$i: \033[31m$COMMENTS / $SUBS\033[39m\n"
		EXITSTATUS=1

		if [ "$VERBOSE" = "-v" ] ; then
			OIFS="$IFS"
			IFS="
"

			echo
			echo "Missing docs:"
			LIST="$(grep -B 1 '^[ 	]*sub[ 	]' "$i")";
			ISCOMMENTED=0
			for LINE in $LIST ; do
				if [ "$(echo $LINE | grep '[         ]*#[^#]*\*\/')" != "" ] ; then
					ISCOMMENTED=1
				elif [ "$(echo $LINE | grep '^--$')" != "" ] ; then
					ISCOMMENTED=0
				else
					if [ $ISCOMMENTED = 0 ] ; then
						echo "    $LINE"
					fi
				fi
			done
			IFS="$OIFS"
			echo
		fi
	fi
    fi
done

scanforkeys()
{
TOKEN="$1"
FILE="$2"

PERLPROG='$/=undef;
my $x = <STDIN>;
my @arr = split(/\$'"$TOKEN"'/, $x);
my %keys = ();
for my $bit (@arr) {
	if ($bit =~ /^->{/s) {
		my $key = $bit;
		$key =~ s/^->{//s;
		$key =~ s/}.*$//sg;
		$key =~ s/\s*//s;
		if ($key !~ /^\$/) {
			$keys{$key} = $key;
		}
	}
}
for my $key (keys %keys) {
	print $key."\n";
}
'

if [ "$FILE" = "" ] ; then
	grep -v '^[[:space:]]*#'  Modules/HeaderDoc/*.pm | perl -e "$PERLPROG"
else 
	grep -v '^[[:space:]]*#' "$FILE" | perl -e "$PERLPROG"
fi

}

scanformissingkeys()
{
TOKEN="$1"
DEFFILE="$2"
TITLE="$3"
INFILE="$4"

# echo "INFILE $INFILE"
# echo "DEFFILE $DEFFILE"

IFS=" "

KEYS=""
for SCANKEY in $TOKEN ; do
	# echo "SCANNING FOR $SCANKEY"
	KEYS="$KEYS
$(scanforkeys "$SCANKEY" "$INFILE")"
done
# echo "KEYS: $KEYS"

IFS="
"

FIRST=1
for key in $KEYS ; do
    if [ "$key" != "" ] ; then
	if [ "$(grep -E "\@var[[:blank:]][[:blank:]]*$key([[:space:]]|$)" "$DEFFILE")" = "" ] ; then
		if [ $FIRST = 1 ] ; then
			FIRST=0
			echo
			echo "Missing $TITLE:"
			echo
		fi
		echo "     $key";
	else
		PASS="$(expr "$PASS" '+' "1")"
	fi
	TOTAL="$(expr "$TOTAL" '+' "1")"
    fi
done
}

scanforallmissingkeys()
{
    local ALLKEYS_SCRIPT;
    local KEYS;
    local DOCKEYS_SCRIPT;
    local DOCUMENTED_KEYS;
    local EXCEPTIONS="$1";

    ALLKEYS_SCRIPT="         "'use strict;'
    ALLKEYS_SCRIPT="$ALLKEYS_SCRIPT "'$/ = undef;'
    ALLKEYS_SCRIPT="$ALLKEYS_SCRIPT "'my $x = <STDIN>;'
    ALLKEYS_SCRIPT="$ALLKEYS_SCRIPT "'my @parts = split(/->{/, $x);'
    ALLKEYS_SCRIPT="$ALLKEYS_SCRIPT "'foreach my $part (@parts) {'
    ALLKEYS_SCRIPT="$ALLKEYS_SCRIPT "'    if ($part =~ /^(\w+)}/) {'
    ALLKEYS_SCRIPT="$ALLKEYS_SCRIPT "'        print "$1 ";'
    ALLKEYS_SCRIPT="$ALLKEYS_SCRIPT "'    }'
    ALLKEYS_SCRIPT="$ALLKEYS_SCRIPT "'}'

    for i in headerDoc2HTML.pl gatherHeaderDoc.pl headerwalk.pl Modules/HeaderDoc/*.pm ; do
	KEYS="$KEYS $(cat "$i" | grep -v '^[[:space:]]*\#' | perl -e "$ALLKEYS_SCRIPT")"
    done

    DOCKEYS_SCRIPT="         "'use strict;'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'$/ = undef;'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'my $x = <STDIN>;'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'my @parts = split(/[\n\r]\s*\#([^\n\r]*)\*\//s, $x);'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'my $first = 1;'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'foreach my $part (@parts) {'
    # DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    print STDERR "PART: $part\n";'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    my $found = 0;'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    if ((!$first) && ($part =~ /^\s*(?:my)?\s*[$@%]\s*(\w+(?:\:\:\w+)*)/s)) {'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'        print "$1 ";'
    # DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'        print STDERR "$1: YES\n";'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'        $found = 1;'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    }'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    my @subparts = split(/^\s*\#.*\/\*\!/, $part);'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    my $comment = $subparts[1];'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    my @tags = split(/\@var/, $comment);'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    my $firstsub = 1;'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    foreach my $tag (@tags) {'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'        if ($firstsub) {'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'            $firstsub = 0;'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'        } else {'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'            $found = 1;'
    # DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'            print STDERR "PROCESSMULTISTART\nCONTEXT: $part\nDATA: $tag: MULTISTART\n";'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'            $tag =~ s/^\W*//s;'
    # DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'            print STDERR "$tag: MULTIMID\n";'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'            $tag =~ s/\s.*$//s;'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'            print "$tag ";'
    # DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'            print STDERR "$tag: MULTI\n";'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'        }'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    }'
    # DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    if (!$found) {'
    # DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'        print STDERR "$part: NO\n";'
    # DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    }'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    if ($first) {'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'        $first = 0;'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'    }'
    DOCKEYS_SCRIPT="$DOCKEYS_SCRIPT "'}'

    for i in headerDoc2HTML.pl gatherHeaderDoc.pl headerwalk.pl Modules/HeaderDoc/*.pm ; do
	DOCUMENTED_KEYS="$DOCUMENTED_KEYS $(cat "$i" | perl -e "$DOCKEYS_SCRIPT")"
    done

	# echo "KEYS: $KEYS"
	# echo "DOCKEYS: $DOCUMENTED_KEYS"

    CMPKEYS_SCRIPT="                "'my $code = $ARGV[0];'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'my $doc = $ARGV[1];'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'my @codebits = split(/\s+/, $code);'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'my @docbits = split(/\s+/, $doc);'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'my %documented = ();'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'foreach my $docbit (@docbits) {'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'    if ($docbit) {'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'        $documented{$docbit} = 1;'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'    }'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'}'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'foreach my $codebit (@codebits) {'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'    if ($codebit) {'
    # CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'        print STDERR "$codebit: ";'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'        if (!$documented{$codebit}) {'
    # CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'            print STDERR "UNDOCUMENTED\n";'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'            print "$codebit\n";'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'            $documented{$codebit} = 1;' # Show only once.
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'        } else {'
    # CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'            print STDERR "DOCUMENTED\n";'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'        }'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'    }'
    CMPKEYS_SCRIPT="$CMPKEYS_SCRIPT "'}'

    UNDOCUMENTED_KEYS="$(perl -e "$CMPKEYS_SCRIPT" "$KEYS" "$DOCUMENTED_KEYS $EXCEPTIONS")"

    if [ "$UNDOCUMENTED_KEYS" != "" ] ; then
	echo
	echo "All undocumented keys: "
	echo
	echo "$UNDOCUMENTED_KEYS"
	echo
    fi

    local KEYCOUNT="$(echo "$KEYS" | tr ' ' '\n' | grep -c '.')"
    local MISSINGCOUNT="$(echo "$UNDOCUMENTED_KEYS" | tr ' ' '\n' | grep -c '.')"

    TOTAL="$(expr "$TOTAL" '+' "$KEYCOUNT")"
    PASS="$(expr "$PASS" '+' "$KEYCOUNT" '-' "$MISSINGCOUNT")"
}

scanformissingglobals()
{
	if [ ! -d "Documentation/hdapi" ] ; then
		./generateAPIDocs.sh > /dev/null 2>&1
	fi

	CODE=""
	CODE="$CODE"' $/ = undef;'
	CODE="$CODE"' my $temp = <STDIN>;'
	CODE="$CODE"' while ($temp =~ s/^.*?[\$\%\@](\w+(?:\:\:\w+)+)//s) {'
	CODE="$CODE"'     print "$1\n";'
	CODE="$CODE"' }'

	DATA="$(
		for i in headerDoc2HTML.pl gatherHeaderDoc.pl headerwalk.pl Modules/HeaderDoc/*.pm ; do
			sed 's/#.*$//' "$i" | grep -v '^[[:space:]]*use' | grep -v '^[[:space:]]*require' | perl -e "$CODE" | grep '^HeaderDoc::'

		done | sort -u
	)"

	IFS="
"
	FIRST=1
	for i in $DATA ; do
		TOKEN="$(echo "$i" | sed 's/^.*:://g')"
		CLASS="$(echo "$i" | sed 's/^\(.*\)::[^:]*$/\1/')"
		if [ "$CLASS" != "" ] ; then
			CLASS="$CLASS/"
		fi
		if [ "$(grep -r '\/\/apple_ref\/perl\/data\/'"$CLASS$TOKEN" "Documentation/hdapi")" = "" ] ; then
			if [ "$(grep -r '\/\/apple_ref\/perl\/cl\/'"$i" "Documentation/hdapi")" = "" ] ; then
				if [ $FIRST = 1 ] ; then
					FIRST=0
					echo; echo "Undocumented globals:"; echo
				fi
				echo $i
			fi
		else
			PASS="$(expr "$PASS" '+' "1")"
		fi
		TOTAL="$(expr "$TOTAL" '+' "1")"
		# echo "DATA: $DATA"
	done

	if [ $FIRST != 1 ] ; then
		echo
		echo "Note: After documenting missing globals, you must re-run"
		echo "      generateAPIDocs.sh before they wwill be detected"
		echo "      properly by this tool."
	fi
}

echo
printf "Scanning for missing parser state keys:"

PASS=0
TOTAL=0

scanformissingkeys "parserState" "Modules/HeaderDoc/ParserState.pm" "parser state keys" ""
scanformissingkeys "self clone" "Modules/HeaderDoc/ParserState.pm" "more parser state keys" "Modules/HeaderDoc/ParserState.pm"

if [ $PASS -eq $TOTAL ] ; then
	printf " \033[32m$PASS / $TOTAL\033[39m\n"
else
	printf " \033[31m$PASS / $TOTAL\033[39m\n"
	EXITSTATUS=1
fi
echo

PASS=0
TOTAL=0

printf "Scanning for missing parse tree keys:"

PASS=0
TOTAL=0

scanformissingkeys "treeCur treeTop cpptreecur cpptreetop parseTree subparsetop newnode" "Modules/HeaderDoc/ParseTree.pm" "parse tree keys" ""

scanformissingkeys "self clone" "Modules/HeaderDoc/ParseTree.pm" "more parse tree keys" "Modules/HeaderDoc/ParseTree.pm"

if [ $PASS -eq $TOTAL ] ; then
	printf " \033[32m$PASS / $TOTAL\033[39m\n"
else
	printf " \033[31m$PASS / $TOTAL\033[39m\n"
	EXITSTATUS=1
fi
echo


printf "Scanning for missing miscellaneous keys:"

PASS=0
TOTAL=0

scanforallmissingkeys "tag synthesized"

if [ $PASS -eq $TOTAL ] ; then
	printf " \033[32m$PASS / $TOTAL\033[39m\n"
else
	printf " \033[31m$PASS / $TOTAL\033[39m\n"
	EXITSTATUS=1
fi
echo

printf "Scanning for missing globals:"

PASS=0
TOTAL=0

scanformissingglobals

if [ $PASS -eq $TOTAL ] ; then
	printf " \033[32m$PASS / $TOTAL\033[39m\n"
else
	printf " \033[31m$PASS / $TOTAL\033[39m\n"
	EXITSTATUS=1
fi
echo

exit $EXITSTATUS
