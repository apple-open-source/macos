package F;

# This is a driver with a faked ->isa

use strict;

use vars qw{$VERSION};
BEGIN {
	$VERSION = '0.01';
}

sub isa {
	my $class = shift;
	my $parent = shift;
	if ( defined $parent and ! ref $parent and $parent eq 'A' ) {
		return !!1;
	} else {
		return !1;
	}
}

sub dummy { 1 }

1;
