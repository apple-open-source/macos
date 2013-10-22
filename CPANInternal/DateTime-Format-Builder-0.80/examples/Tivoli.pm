# we need to comment this out or PAUSE might index it
# pack age DateTime::Format::Tivoli;

use DateTime::Format::Builder
(
    parsers => {
	parse_datetime => {
	    strptime => '%h %e %k:%M:%S %Y',
	},
    },
);

sub format_datetime
{
    my ($self, $dt) = @_;
    my $z = $dt->clone->set_time_zone( 'GMT' );
    return $z->strftime( '%h %e %k:%M:%S %Y' );
}

package main;

my $parser = DateTime::Format::Tivoli->new();

my @dates = ( 'Nov  5 22:49:45 2003', '27/Apr/2003:19:45:11 -0400' );

for my $date (@dates)
{
    my $dt = $parser->parse_datetime( $date )->set_time_zone( 'Australia/Sydney' );
    print "$date => ", $dt->datetime, " => ", $parser->format_datetime( $dt ), "\n";
}
