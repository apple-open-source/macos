# we need to comment this out or PAUSE might index it
# pack age DateTime::Format::Apache;

use DateTime::Format::Builder (
    parsers => {
        parse_datetime => {
            strptime => '%e/%b/%Y:%H:%M:%S %z',

            #     params => [qw( day month year hour minute second time_zone )],
            #     regex => qr{ ^
            # 	(\d+)/(\w{3})/(\d{4})
            # 	:
            # 	(\d\d):(\d\d):(\d\d)
            # 	\s
            # 	([+-]\d{4})
            # 	$ }x,
            #     postprocess => sub {
            # 	my %args = @_;
            # 	$args{parsed}{month} = month_to_num( $args{parsed}{month} );
            # 	1;
            #     },
        },
    },
);

sub month_to_num {
    my $wanted = shift;
    my %months;
    my $lang = DateTime::Language->new( language => 'en' );
    my $i;
    $months{$_} = ++$i for @{ $lang->month_abbreviations };
    return $months{$wanted};
}

sub format_datetime {
    my ( $self, $dt ) = @_;
    return $dt->strftime("%e/%b/%Y:%H:%M:%S %z");
}

package main;

my $parser = DateTime::Format::Apache->new();

my @dates = ( '27/Feb/2003:19:45:11 -0400', '27/Apr/2003:19:45:11 -0400' );

for my $date (@dates) {
    my $dt
        = $parser->parse_datetime($date)->set_time_zone('Australia/Sydney');
    print "$date => ", $dt->datetime, " => ", $parser->format_datetime($dt),
        "\n";
}
