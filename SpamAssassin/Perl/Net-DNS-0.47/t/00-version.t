# $Id: 00-version.t,v 1.1 2004/04/09 17:04:48 dasenbro Exp $

use Test::More;
use File::Spec;
use File::Find;
use ExtUtils::MakeMaker;
use strict;

my @files;
my $blib = File::Spec->catfile(qw(blib lib));
	
find( sub { push(@files, $File::Find::name) if /\.pm$/}, $blib);

my $can = eval { MM->can('parse_version') };

if (!$@ and $can) {
	plan tests => scalar @files;
} else {
	plan skip_all => ' Not sure how to parse versions.';
}

foreach my $file (@files) {
	my $version = MM->parse_version($file);
	diag("$file\t=>\t$version") if $ENV{'NET_DNS_DEBUG'};
	isnt("$file: $version", "$file: undef", "$file has a version");
}



