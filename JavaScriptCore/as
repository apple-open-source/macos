#!/usr/bin/perl -w
use strict;

# this script is a wrapper around the assembler that suppresses
# all type-info .weak_definition lines so that all type-infos are not-weak

print "making all type-infos not-weak\n";

open OUT, "| /usr/bin/as '" . join("' '", @ARGV) . "'" or die "can't run as";
while (<STDIN>) { print OUT unless /\.weak_definition __ZTI/ }
close OUT;
