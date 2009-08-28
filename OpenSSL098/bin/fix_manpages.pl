#!/usr/bin/perl

use strict;
use warnings;

sub rename_wrapper($$$)
{
	my ($dir, $fn, $ext) = @_;
	my $ofn = $dir.$fn.$ext;
	my $mfn = $dir.uc($fn).$ext;

	return unless -f $ofn;
	
	if( rename( $ofn, $mfn ) )
	{
		printf("renamed \"%s\" to \"%s\".\n", $ofn, $mfn);
	} else {
		warn("Could not rename \"$ofn\" to \"$mfn\": $!.\n");
	}
}

my $ext = ".pod";

my $cryptdir = "doc/crypto/";
my @cryptfiles = qw( sha hmac md5 mdc2 pem rc4 );

my $ssldir = "doc/ssl/";
my @sslfiles = qw( ssl );

for my $file ( @cryptfiles )
{
	&rename_wrapper( $cryptdir, $file, $ext );
}

for my $file ( @sslfiles )
{
	&rename_wrapper( $ssldir, $file, $ext );
}

exit;

