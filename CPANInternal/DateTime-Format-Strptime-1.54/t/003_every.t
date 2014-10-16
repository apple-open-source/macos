# t/002_basic.t - check module dates in various formats

use Test::More 0.88;
use DateTime::Format::Strptime;

{
    my $object = DateTime::Format::Strptime->new(
        pattern    => '%D',
        time_zone  => 'Australia/Melbourne',
        locale     => 'en_AU',
        diagnostic => 0,
    );

    my $epoch = DateTime->new(
        year      => 2003, month  => 11, day    => 5,
        hour      => 23,   minute => 34, second => 45,
        time_zone => 'Australia/Melbourne'
    )->epoch;

    my @tests = (

        # Compound Patterns
        [ '%T', '23:34:45',    '24-hour Time' ],
        [ '%r', '11:34:45 PM', '12-hour Time' ],
        [ '%R', '23:34',       'Simple 24-hour Time' ],
        [ '%D', '11/30/03',    'American Style Date' ],
        [ '%F', '2003-11-30',  'ISO Style Date' ],

        [
            '%a %b %B %C %d %e %h %H %I %j %k %l %m %M %n %N %O %p %P %S %U %u %w %W %y %Y %s %G %g %z %Z %%Y %%',
            "Wed Nov November 20 05  5 Nov 23 11 309 23 11 11 34 \n 123456789 Australia/Melbourne PM pm 45 44 3 3 44 03 2003 $epoch 2003 03 +1100 EST %Y %",
            "Every token at once"
        ],

        [ '%{year}', '2003', 'Extended strftime %{} matching' ],

    );

    foreach (@tests) {
        my ( $pattern, $data, $name ) = @$_;
        $name ||= $pattern;

        #print "-- $pattern ($data) --\n";
        $object->pattern($pattern);

        #print "\n" . $object->pattern . "\n" . $object->{parser};
        #print $object->parse_datetime( $data )->strftime("%Y-%m-%d %H:%M:%S\n");
        #print $object->parse_datetime( $data )->strftime("Got: $pattern\n");
        is( $object->format_datetime( $object->parse_datetime($data) ), $data,
            $name );
    }
}

{
    my $object = DateTime::Format::Strptime->new(
        pattern    => '%D',
        time_zone  => 'Australia/Melbourne',
        locale     => 'en_AU',
        diagnostic => 0,
    );

    my $epoch = DateTime->new(
        year      => 2003, month  => 11, day    => 5,
        hour      => 23,   minute => 34, second => 45,
        time_zone => 'Australia/Melbourne'
    )->epoch;

    my @tests = (

        # Compound Patterns
        [ '%T', '23:34:45',    '24-hour Time' ],
        [ '%r', '11:34:45 PM', '12-hour Time' ],
        [ '%R', '23:34',       'Simple 24-hour Time' ],
        [ '%D', '11/30/03',    'American Style Date' ],
        [ '%F', '2003-11-30',  'ISO Style Date' ],

        [
            '%a %b %B %C %d %e %h %H %I %j %k %l %m %M %n %N %p %P %S %U %u %w %W %y %Y %s %G %g %z %Z %%',
            "Wed Nov November 20 05  5 Nov 23 11 309 23 11 11 34 \n 123456789 PM pm 45 44 3 3 44 03 2003 $epoch 2003 03 +1100 EST %",
            "Every token at once"
        ],

        [ '%{year}', '2003', 'Extended strftime %{} matching' ],

    );

    foreach (@tests) {
        my ( $pattern, $data, $name ) = @$_;
        $name ||= $pattern;
        $object->pattern($pattern);
        is( $object->format_datetime( $object->parse_datetime($data) ), $data,
            $name );
    }
}

done_testing();
