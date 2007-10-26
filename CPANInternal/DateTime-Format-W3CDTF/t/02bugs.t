#!/usr/bin/perl -w

# test bug 3766 
#	http://rt.cpan.org/NoAuth/Bug.html?id=3766
#   returns undef when you pass it a DateTime object 
#   whose timezone has been explicitly set.

# test bug 3771
#   http://rt.cpan.org/NoAuth/Bug.html?id=3771
#   format_datetime method a DateTime object whose timestamp is set
#   explicitly as "00:00:00", the method will not print a timestamp or an offset.

use strict;
use Test::More tests => 4;

use DateTime;
use DateTime::Format::W3CDTF;

my @dates = (
	{ date   => { year => 1977, month => 11, day => 11, hour => 1, minute => 12, time_zone => 'America/Los_Angeles' },
	  w3cdtf => '1977-11-11T01:12:00-08:00',
	  msg	 => 'formatter works with explicit timezone',
	},
	{ date   => { year => 1977, month => 4, day => 7, time_zone => 'America/Los_Angeles' },
	  w3cdtf => '1977-04-07T00:00:00-08:00',
	  msg	 => 'formatter works without timestamp',
	},
	{ date   => { year => 2003, month => 4, day => 7, hour => 2, time_zone => 'America/Los_Angeles' },
	  w3cdtf => '2003-04-07T02:00:00-07:00',
	  msg	 => 'formatter properly recognizing daylights saving'
	},
	{ date   => { year => 2003, month => 12, day => 25, hour => 0, minute => 00, second => 00, time_zone => 'America/Montreal' },
	  w3cdtf => '2003-12-25T00:00:00-05:00',
	  msg	 => 'formatter properly formats midnight'
	}
);
my $f = DateTime::Format::W3CDTF->new();

foreach my $d ( @dates ) {
	my $dt = DateTime->new( %{ $d->{date} } );
	is ( $f->format_datetime($dt), $d->{w3cdtf}, $d->{msg});
}