# $Id: 00-version.t 685 2007-10-10 13:54:34Z olaf $ -*-perl-*-

use Test::More;
use File::Spec;
use File::Find;
use ExtUtils::MakeMaker;
use strict;

my @files;
my $blib = File::Spec->catfile(qw(blib lib));
	
find( sub { push(@files, $File::Find::name) if /\.pm$/}, $blib);

plan skip_all => 'No versions from git checkouts' if -e '.git';


my $can = eval { MM->can('parse_version') };

if (!$@ and $can) {
	plan tests => (2* scalar @files) - 1;
} else {
	plan skip_all => ' Not sure how to parse versions.';
}

foreach my $file (@files) {
	my $version = MM->parse_version($file);
	diag("$file\t=>\t$version") if $ENV{'NET_DNS_DEBUG'};
	isnt("$file: $version", "$file: undef", "$file has a version");
	next if $file =~ /Net\/DNS.pm$/;
	ok ($version>290,"$file: version has reasonable value");
}



