# Don't want to collide with the B:: modules
package My_B;

# This is our good driver class

use strict;

use A ();
use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '0.01';
	@ISA     = 'A';
}

sub dummy { 1 }

1;
