#!/usr/bin/perl

use strict;
use File::Listing;
use LWP::Simple;

my $ok = 1;

# some sample URLs
my @urls = (
	    "http://www.apache.org/~jon/scarab/nightly/",
	    "http://www.apache.org/dist/apr/?C=N&O=D",
	    "http://xml.apache.org/dist/batik/",
	    "http://perl.apache.org/rpm/distrib/",
	    "http://stein.cshl.org/WWW/software/",
	    "http://www.cpan.org/modules/by-module/",
	   );
print "1.." . scalar(@urls) . "\n";

for my $url (@urls) {
    print "# $url\n";
    my @listing = parse_dir(get($url),undef,"apache");
    print "not " if @listing == 0;
    print "ok " . $ok++ . "\n";
    #require Data::Dumper; print Data::Dumper->Dump(["Listing for $url", \@listing],[]);
}
