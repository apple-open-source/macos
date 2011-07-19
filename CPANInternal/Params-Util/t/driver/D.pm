package D;

# This is our broken driver class

use strict;

use A ();
use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '0.01';
	@ISA     = 'A';
}

sub dummy { 1 }

0;
