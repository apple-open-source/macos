use strict;
use warnings;
use utf8;

use File::Spec;
use Test::More;

use DateTime::Locale;

my @locale_ids   = sort DateTime::Locale->ids();
my %locale_names = map { $_ => 1 } DateTime::Locale->names;
my %locale_ids   = map { $_ => 1 } DateTime::Locale->ids;

plan tests =>
      5                               # starting
    + 1                               # one test for root locale
    + ( ( @locale_ids - 1 ) * 21 )    # tests for each other local
    + 56                              # check_root
    + 24                              # check_en
    + 66                              # check_en_GB
    + 23                              # check_en_US
    + 11                              # check_es_ES
    + 5                               # check_en_US_POSIX
    + 2                               # check_af
    + 9                               # check_DT_Lang
    ;

{
    ok( @locale_ids >= 240,     'Coverage looks complete' );
    ok( $locale_names{English}, "Locale name 'English' found" );
    ok( $locale_ids{ar_JO},     "Locale id 'ar_JO' found" );

    eval { DateTime::Locale->load('Does not exist') };
    like( $@, qr/invalid/i,
        'invalid locale name/id to load() causes an error' );

    # this type of locale id should work
    my $l = DateTime::Locale->load('en_US.LATIN-1');
    is( $l->id, 'en_US', 'id is en_US' );
}

# testing the basics for all ids
{
    for my $locale_id (@locale_ids) {
        my $locale = eval { DateTime::Locale->load($locale_id) };

        isa_ok( $locale, 'DateTime::Locale::Base' );

        next if $locale_id eq 'root';

        ok( $locale_ids{ $locale->id() },
            "'$locale_id':  Has a valid locale id" );

        ok( length $locale->name(), "'$locale_id':  Has a locale name" );
        ok(
            length $locale->native_name(),
            "'$locale_id':  Has a native locale name"
        );

        for my $test (
            {
                locale_method => 'month_format_wide',
                count         => 12,
            }, {
                locale_method => 'month_format_abbreviated',
                count         => 12,
            }, {
                locale_method => 'day_format_wide',
                count         => 7,
            }, {
                locale_method => 'day_format_abbreviated',
                count         => 7,
            }, {
                locale_method => 'quarter_format_wide',
                count         => 4,
            }, {
                locale_method => 'quarter_format_abbreviated',
                count         => 4,
            }, {
                locale_method => 'am_pm_abbreviated',
                count         => 2,
            }, {
                locale_method => 'era_wide',
                count         => 2,
            }, {
                locale_method => 'era_abbreviated',
                count         => 2,
            },
            ) {
            check_array( locale => $locale, %$test );
        }

        # We can't actually expect these to be unique.
        is( scalar @{ $locale->day_format_narrow() }, 7,
            'day_format_narrow() returns 7 items' );
        is( scalar @{ $locale->month_format_narrow() }, 12,
            'month_format_narrow() returns 12 items' );
        is( scalar @{ $locale->day_stand_alone_narrow() }, 7,
            'day_stand_alone_narrow() returns 7 items' );
        is( scalar @{ $locale->month_stand_alone_narrow() }, 12,
            'month_stand_alone_narrow() returns 12 items' );

        check_formats( $locale_id, $locale, 'date_formats', 'date_format' );
        check_formats( $locale_id, $locale, 'time_formats', 'time_format' );
    }
}

check_root();
check_en();
check_en_GB();
check_en_US();
check_es_ES();
check_en_US_POSIX();
check_af();
check_DT_Lang();

sub check_array {
    my %test = @_;

    my $locale_method = $test{locale_method};

    my %unique = map { $_ => 1 } @{ $test{locale}->$locale_method() };

    my $locale_id = $test{locale}->id();

TODO:
    {
        local $TODO
            = 'The ii locale does not have unique abbreviated days for some reason'
            if $test{locale}->id() =~ /^ii/
                && $locale_method eq 'day_format_abbreviated';

        is(
            keys %unique, $test{count},
            qq{'$locale_id': '$locale_method' contains $test{count} unique items}
        );
    }
}

sub check_formats {
    my ( $locale_id, $locale, $hash_func, $item_func ) = @_;

    my %unique = map { $_ => 1 } values %{ $locale->$hash_func() };

    ok( keys %unique >= 1,
        "'$locale_id': '$hash_func' contains at least 1 unique item" );

    foreach my $length (qw( full long medium short )) {
        my $method = $item_func . q{_} . $length;

        my $val = $locale->$method();

        if ( defined $val ) {
            delete $unique{$val};
        }
        else {
            Test::More::diag("$locale_id returned undef for $method()");
        }
    }

    is(
        keys %unique, 0,
        "'$locale_id':  Data returned by '$hash_func' and '$item_func patterns' matches"
    );
}

sub check_root {
    my $locale = DateTime::Locale->load('root');

    my %tests = (
        day_format_wide => [qw( 2 3 4 5 6 7 1 )],

        day_format_abbreviated => [qw( 2 3 4 5 6 7 1 )],

        day_format_narrow => [qw( 2 3 4 5 6 7 1 )],

        day_stand_alone_wide => [qw( 2 3 4 5 6 7 1 )],

        day_stand_alone_abbreviated => [qw( 2 3 4 5 6 7 1 )],

        day_stand_alone_narrow => [qw( 2 3 4 5 6 7 1 )],

        month_format_wide => [qw( 1 2 3 4 5 6 7 8 9 10 11 12 )],

        month_format_abbreviated => [qw( 1 2 3 4 5 6 7 8 9 10 11 12 )],

        month_format_narrow => [qw( 1 2 3 4 5 6 7 8 9 10 11 12 )],

        month_stand_alone_wide => [qw( 1 2 3 4 5 6 7 8 9 10 11 12 )],

        month_stand_alone_abbreviated => [qw( 1 2 3 4 5 6 7 8 9 10 11 12 )],

        month_stand_alone_narrow => [qw( 1 2 3 4 5 6 7 8 9 10 11 12 )],

        quarter_format_wide => [qw( Q1 Q2 Q3 Q4 )],

        quarter_format_abbreviated => [qw( Q1 Q2 Q3 Q4 )],

        quarter_format_narrow => [qw( 1 2 3 4 )],

        quarter_stand_alone_wide => [qw( Q1 Q2 Q3 Q4 )],

        quarter_stand_alone_abbreviated => [qw( Q1 Q2 Q3 Q4 )],

        quarter_stand_alone_narrow => [qw( 1 2 3 4 )],

        era_wide => [qw( BCE CE )],

        era_abbreviated => [qw( BCE CE )],

        era_narrow => [qw( BCE CE )],

        am_pm_abbreviated => [qw( AM PM )],

        datetime_format_full   => 'EEEE, y MMMM dd HH:mm:ss zzzz',
        datetime_format_long   => 'y MMMM d HH:mm:ss z',
        datetime_format_medium => 'y MMM d HH:mm:ss',
        datetime_format_short  => 'yyyy-MM-dd HH:mm',

        datetime_format_default => 'y MMM d HH:mm:ss',

        glibc_datetime_format => '%a %b %e %H:%M:%S %Y',
        glibc_date_format     => '%m/%d/%y',
        glibc_time_format     => '%H:%M:%S',

        first_day_of_week => 1,

        prefers_24_hour_time => 1,
    );

    test_data( $locale, %tests );

    my %formats = (
        d      => 'd',
        EEEd   => 'd EEE',
        hm     => 'h:mm a',
        Hm     => 'H:mm',
        hms    => 'h:mm:ss a',
        Hms    => 'H:mm:ss',
        M      => 'L',
        Md     => 'M-d',
        MEd    => 'E, M-d',
        MMM    => 'LLL',
        MMMd   => 'MMM d',
        MMMEd  => 'E MMM d',
        MMMMd  => 'MMMM d',
        MMMMEd => 'E MMMM d',
        ms     => 'mm:ss',
        y      => 'y',
        yM     => 'y-M',
        yMEd   => 'EEE, y-M-d',
        yMMM   => 'y MMM',
        yMMMEd => 'EEE, y MMM d',
        yMMMM  => 'y MMMM',
        yQ     => 'y Q',
        yQQQ   => 'y QQQ',
    );

    test_formats( $locale, %formats );
}

sub check_en {
    my $locale = DateTime::Locale->load('en');

    my %tests = (
        en_data(),

        name => 'English',
    );

    test_data( $locale, %tests );
}

sub check_en_GB {
    my $locale = DateTime::Locale->load('en_GB');

    my %tests = (
        en_data(),

        first_day_of_week => 7,

        name             => 'English United Kingdom',
        native_name      => 'English United Kingdom',
        language         => 'English',
        native_language  => 'English',
        territory        => 'United Kingdom',
        native_territory => 'United Kingdom',
        variant          => undef,
        native_variant   => undef,

        language_id  => 'en',
        territory_id => 'GB',
        variant_id   => undef,

        glibc_datetime_format => '%a %d %b %Y %T %Z',
        glibc_date_format     => '%d/%m/%y',
        glibc_time_format     => '%T',

        datetime_format_default => 'd MMM y HH:mm:ss',
    );

    test_data( $locale, %tests );

    my %formats = (
        Md       => 'd/M',
        MEd      => 'E, d/M',
        MMdd     => 'dd/MM',
        MMMEd    => 'E d MMM',
        MMMMd    => 'd MMMM',
        yMEd     => 'EEE, d/M/yyyy',
        yyMMM    => 'MMM yy',
        yyyyMM   => 'MM/yyyy',
        yyyyMMMM => 'MMMM y',

        # from en
        d      => 'd',
        EEEd   => 'd EEE',
        hm     => 'h:mm a',
        Hm     => 'H:mm',
        Hms    => 'H:mm:ss',
        M      => 'L',
        MMM    => 'LLL',
        MMMd   => 'MMM d',
        MMMMEd => 'E, MMMM d',
        ms     => 'mm:ss',
        y      => 'y',
        yM     => 'M/yyyy',
        yMMM   => 'MMM y',
        yMMMEd => 'EEE, MMM d, y',
        yMMMM  => 'MMMM y',
        yQ     => 'Q yyyy',
        yQQQ   => 'QQQ y',

        # from root
        hms => 'h:mm:ss a',
    );

    test_formats( $locale, %formats );
}

sub check_en_US {
    my $locale = DateTime::Locale->load('en_US');

    my %tests = (
        en_data(),

        first_day_of_week => 7,
    );

    test_data( $locale, %tests );
}

sub en_data {
    return (
        day_format_wide =>
            [qw( Monday Tuesday Wednesday Thursday Friday Saturday Sunday )],

        day_format_abbreviated => [qw( Mon Tue Wed Thu Fri Sat Sun )],

        day_format_narrow => [qw( M T W T F S S )],

        day_stand_alone_wide =>
            [qw( Monday Tuesday Wednesday Thursday Friday Saturday Sunday )],

        day_stand_alone_abbreviated => [qw( Mon Tue Wed Thu Fri Sat Sun )],

        day_stand_alone_narrow => [qw( M T W T F S S )],

        month_format_wide => [
            qw( January February March April May June
                July August September October November December )
        ],

        month_format_abbreviated =>
            [qw( Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec )],

        month_format_narrow => [qw( J F M A M J J A S O N D )],

        month_stand_alone_wide => [
            qw( January February March April May June
                July August September October November December )
        ],

        month_stand_alone_abbreviated =>
            [qw( Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec )],

        month_stand_alone_narrow => [qw( J F M A M J J A S O N D )],

        quarter_format_wide =>
            [ '1st quarter', '2nd quarter', '3rd quarter', '4th quarter' ],

        quarter_format_abbreviated => [qw( Q1 Q2 Q3 Q4 )],

        quarter_format_narrow => [qw( 1 2 3 4 )],

        quarter_stand_alone_wide =>
            [ '1st quarter', '2nd quarter', '3rd quarter', '4th quarter' ],

        quarter_stand_alone_abbreviated => [qw( Q1 Q2 Q3 Q4 )],

        quarter_stand_alone_narrow => [qw( 1 2 3 4 )],

        era_wide => [ 'Before Christ', 'Anno Domini' ],

        era_abbreviated => [qw( BC AD )],

        era_narrow => [qw( B A )],

        am_pm_abbreviated => [qw( AM PM )],

        first_day_of_week => 1,
    );
}

sub test_data {
    my $locale = shift;
    my %tests  = @_;

    for my $k ( sort keys %tests ) {
        my $desc = "$k for " . $locale->id();
        if ( ref $tests{$k} ) {
            is_deeply( $locale->$k(), $tests{$k}, $desc );
        }
        else {
            is( $locale->$k(), $tests{$k}, $desc );
        }
    }
}

sub test_formats {
    my $locale  = shift;
    my %formats = @_;

    for my $name ( keys %formats ) {
        is(
            $locale->format_for($name), $formats{$name},
            "Format for $name with " . $locale->id()
        );
    }

    is_deeply(
        [ $locale->available_formats() ],
        [ sort keys %formats ],
        "Available formats for " . $locale->id() . " match what is expected"
    );
}

sub check_es_ES {
    my $locale = DateTime::Locale->load('es_ES');

    is( $locale->name(),            'Spanish Spain',    'name()' );
    is( $locale->native_name(),     'español España', 'native_name()' );
    is( $locale->language(),        'Spanish',          'language()' );
    is( $locale->native_language(), 'español',         'native_language()' );
    is( $locale->territory(),       'Spain',            'territory()' );
    is( $locale->native_territory(), 'España', 'native_territory()' );
    is( $locale->variant(),          undef,     'variant()' );
    is( $locale->native_variant(),   undef,     'native_variant()' );

    is( $locale->language_id(),  'es',  'language_id()' );
    is( $locale->territory_id(), 'ES',  'territory_id()' );
    is( $locale->variant_id(),   undef, 'variant_id()' );
}

sub check_af {
    my $locale = DateTime::Locale->load('af');

    is_deeply(
        $locale->month_format_abbreviated(),
        [qw( Jan Feb Mar Apr Mei Jun Jul Aug Sep Okt Nov Des )],
        'month abbreviations for af use non-draft form'
    );

    is_deeply(
        $locale->month_format_narrow(),
        [ 1 .. 12 ],
        'month narrows for af use draft form because that is the only form available'
    );
}

sub check_en_US_POSIX {
    my $locale = DateTime::Locale->load('en_US_POSIX');

    is( $locale->variant(),        'Computer', 'variant()' );
    is( $locale->native_variant(), 'Computer', 'native_variant()' );

    is( $locale->language_id(),  'en',    'language_id()' );
    is( $locale->territory_id(), 'US',    'territory_id()' );
    is( $locale->variant_id(),   'POSIX', 'variant_id()' );
}

sub check_DT_Lang {
    foreach my $old (
        qw ( Austrian TigrinyaEthiopian TigrinyaEritrean
        Brazilian Portuguese
        Afar Sidama Tigre )
        ) {
        ok( DateTime::Locale->load($old),
            "backwards compatibility for $old" );
    }

    foreach my $old (qw ( Gedeo )) {
    SKIP:
        {
            skip
                'No CLDR XML data for some African languages included in DT::Language',
                1
                unless $locale_names{$old};

            ok( DateTime::Locale->load($old),
                "backwards compatibility for $old" );
        }
    }
}

