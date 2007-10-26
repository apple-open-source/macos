# $Id: self.t,v 1.4 2003/06/24 07:16:28 koschei Exp $
use lib 'inc';
use blib;
use strict;
use Test::More tests => 4;
use vars qw( $class );

BEGIN {
    $class = 'DateTime::Format::Builder';
    use_ok $class;
}

{
    my $sample = 'SampleClassWithSelf';
    $class->create_class(
	class	 => $sample,
	parsers    => {
	    parse_datetime => [    
	    [
		preprocess => sub {
		    my %p = @_;
		    my $self = $p{self};
		    $p{parsed}->{time_zone} = $self->{global}
			if $self->{global};
		    return $p{input};
		},
	    ],
	    {
		params => [ qw( year month day hour minute second ) ],
		regex  => qr/^(\d\d\d\d)(\d\d)(\d\d)T(\d\d)(\d\d)(\d\d)$/,
		preprocess =>  sub {
		    my %p = @_;
		    my $self = $p{self};
		    $p{parsed}->{time_zone} = $self->{pre}
			if $self->{pre}; 
		    return $p{input};
		},
		postprocess => sub {
		    my %p = @_;
		    my $self = $p{self};
		    $p{parsed}->{time_zone} = $self->{post}
			if $self->{post}; 
		    return 1;
		},
	    },
	    ],
	}
    );

    my %tests = (
	global => 'Africa/Cairo',
	pre	=> 'Europe/London',
	post	=> 'Australia/Sydney',
    );

    while ( my ($callback, $value) = each %tests )
    {
	my $parser = $sample->new();
	$parser->{$callback} = $value;
	my $dt = $parser->parse_datetime( "20030716T163245" );
	is( $dt->time_zone->name, $value );
    }
}
