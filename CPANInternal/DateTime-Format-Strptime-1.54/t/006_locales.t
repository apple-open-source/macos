#!perl -w

# t/002_basic.t - check module dates in various formats

use Test::More 0.88;

#use Test::More qw/no_plan/;
use DateTime::Format::Strptime;
use DateTime;

my @locales = qw/en ga pt de/;

#diag("\nChecking Day Names");
my $pattern = "%Y-%m-%d %A";
foreach my $locale (@locales) {
    foreach my $day ( 1 .. 7 ) {
        my $dt = DateTime->now( locale => $locale )->set( day => $day );
        my $input = $dt->strftime($pattern);
        eval {
            $strptime = DateTime::Format::Strptime->new(
                pattern  => $pattern,
                locale   => $locale,
                on_error => 'croak',
            );
        };
        ok( $@ eq '', "Constructor with Day Name" );

        my $parsed;
        eval { $parsed = $strptime->parse_datetime($input); } unless $@;
        diag("[$@]") if $@ ne '';
        ok( $@ eq '', "Parsed with Day Name" );

        is( $parsed->strftime($pattern), $input, "Matched with Day Name" );
    }

    #	diag( $locale );
}

#diag("\nChecking Month Names");
$pattern = "%Y-%m-%d %B";
foreach my $locale (@locales) {
    foreach my $month ( 1 .. 12 ) {
        my $dt = DateTime->now( locale => $locale )
            ->set( month => $month, day => 20 );
        my $input = $dt->strftime($pattern);
        eval {
            $strptime = DateTime::Format::Strptime->new(
                pattern  => $pattern,
                locale   => $locale,
                on_error => 'croak',
            );
        };
        ok( $@ eq '', "Constructor with Month Name" );

        my $parsed;
        eval { $parsed = $strptime->parse_datetime($input); } unless $@;
        diag("[$@]") if $@ ne '';
        ok( $@ eq '', "Parsed with Month Name" );

        is( $parsed->strftime($pattern), $input, "Matched with Month Name" );
    }

    #	diag( $locale );
}

#diag("\nChecking AM/PM tokens");
$pattern = "%Y-%m-%d %H:%M %p";
foreach my $locale (@locales) {
    foreach my $hour ( 11, 12 ) {
        my $dt = DateTime->now( locale => $locale )->set( hour => $hour );
        my $input = $dt->strftime($pattern);
        eval {
            $strptime = DateTime::Format::Strptime->new(
                pattern  => $pattern,
                locale   => $locale,
                on_error => 'croak',
            );
        };
        ok( $@ eq '', "Constructor with Meridian" );

        my $parsed;
        eval { $parsed = $strptime->parse_datetime($input); } unless $@;
        diag("[$@]") if $@ ne '';
        ok( $@ eq '', "Parsed with Meridian" );

        is( $parsed->strftime($pattern), $input, "Matched with Meridian" );
    }

    #	diag( $locale );
}

#diag("\nChecking format_datetime honors strptime's locale rather than the dt's");
{

    # Create a parser that has locale 'fr'
    my $dmy_format = new DateTime::Format::Strptime(
        pattern => '%d/%m/%Y',
        locale  => 'fr'
    );
    is( $dmy_format->locale, 'fr' );

    # So, therefore, will a $dt created using it.
    my $dt = $dmy_format->parse_datetime('03/08/2004');
    is( $dt->locale->id, 'fr' );

    # Now we create a new strptime for formatting, but in a different locale
    my $pt_format = new DateTime::Format::Strptime(
        pattern => '%B/%Y',
        locale  => 'pt'
    );
    is( $pt_format->locale, 'pt' );

    my $string = $pt_format->format_datetime($dt);

    # Make sure the format honored the locale in the strptime
    is( $string, "agosto/2004" );

    # Make sure the datetime, however, retained its own locale
    is( $dt->locale->id, 'fr' )
}

done_testing();
