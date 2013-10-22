use strict;

use Test::More tests => 14;

use DateTime::Format::Builder;


# Test multiple parsers having the same length
{
    my $which;

    my @parsers = (
	{
	    length => 10,
	    params => [ qw( month year day ) ],
	    regex  => qr/^(\d\d)-(\d\d\d\d)-(\d\d)$/,
	    postprocess => sub { $which = 1 },
	},
	{
	    length => 10,
	    params => [ qw( year month day ) ],
	    regex  => qr/^(\d\d\d\d)-(\d\d)-(\d\d)$/,
	    postprocess => sub { $which = 2 },
	},
	{
	    length => 10,
	    params => [ qw( day month year ) ],
	    regex  => qr/^(\d\d)-(\d\d)-(\d\d\d\d)$/,
	    postprocess => sub { $which = 3 },
	},
    );

    my %data = (
	1 => "05-2003-10",
	2 => "2003-04-07",
	3 => "13-12-2006",
    );

    {
	my $parser = DateTime::Format::Builder->parser( @parsers );
	isa_ok( $parser => 'DateTime::Format::Builder' );

	for my $length (sort keys %data)
	{
	    my $date = $data{$length};
	    my $dt = $parser->parse_datetime( $date );
	    isa_ok $dt => 'DateTime';
	    is( $which, $length, "Used length parser $length" );
	}
    }
}

# Test single parser having multiple lengths
{
    my $which = 0;
    my @parsers = (
	{
	    length     => 4,
	    regex      => qr/bar/,
	    params     => [],
	    preprocess => sub { $which = 4 }
	},
	{
	    length     => 5,
	    regex      => qr/bar/,
	    params     => [],
	    preprocess => sub { $which = 5 }
	},
	{
	    length => [qw( 4 5 )],
	    regex  => qr/(-?\d\d\d\d)/,
	    params => [qw( year )],
	}
    );

    my $parser = DateTime::Format::Builder->parser( @parsers );
    isa_ok( $parser => 'DateTime::Format::Builder' );

    my %data = (
	4 => 2003,
	5 => -2003,
    );

    for my $length (sort keys %data)
    {
	my $year = $data{$length};
	my $dt = $parser->parse_datetime( $year );
	isa_ok( $dt => 'DateTime' );
	is( $length, $which, "Parser length $length for $year" );
	is( $dt->year, $year, "Year $year matches" );
    }


}
