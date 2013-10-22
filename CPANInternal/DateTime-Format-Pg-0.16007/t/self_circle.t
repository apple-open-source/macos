use strict;
use Test::More tests => 4;
use DateTime::Format::Pg 0.02;

my @ts = (
    "2007-03-10 06:00:00+01",
    "2007-03-10 06:00:00-0100",
);

foreach my $ts (@ts) {
    my $dt = DateTime::Format::Pg->parse_datetime($ts);
    my $dt_formated = DateTime::Format::Pg->format_datetime($dt);

    # Pg will truncate timezone like +0100 to +01
    $ts =~ s/([+\-]\d{2})$/${1}00/;
    is($dt_formated, $ts, "format ok");
    ok(DateTime::Format::Pg->parse_datetime($dt_formated));

}
