use strict;
use warnings;

use Test::More tests => 14;

use DateTime::Locale;

{

    package DateTime::Locale::en_GB_RIDAS;

    use base qw(DateTime::Locale::root);
}

{

    package DateTime::Locale::en_FOO_BAR;

    use base qw(DateTime::Locale::root);
}

{

    package DateTime::Locale::en_BAZ_BUZ;

    use base qw(DateTime::Locale::root);
}

{

    package DateTime::Locale::en_QUUX_QUAX;

    use base qw(DateTime::Locale::root);
}

{

    package DateTime::Locale::fr_FR_BZH2;
    @DateTime::Locale::fr_FR_BZH2::ISA = qw(DateTime::Locale::root);
    sub short_date_format {'test test2'}
}
{

    package Other::Locale::fr_FR_BZH;
    @Other::Locale::fr_FR_BZH::ISA = qw(DateTime::Locale::root);
    sub short_date_format {'test test2'}
}

DateTime::Locale->register(
    id           => 'en_GB_RIDAS',
    en_language  => 'English',
    en_territory => 'United Kingdom',
    en_variant   => 'Ridas Custom Locale',
);

{
    my $l = DateTime::Locale->load('en_GB_RIDAS');
    ok( $l, 'was able to load en_GB_RIDAS' );
    is( $l->variant, 'Ridas Custom Locale', 'variant is set properly' );
}

DateTime::Locale->register(
    {
        id           => 'en_FOO_BAR',
        en_language  => 'English',
        en_territory => 'United Kingdom',
        en_variant   => 'Foo Bar',
    }, {
        id           => 'en_BAZ_BUZ',
        en_language  => 'English',
        en_territory => 'United Kingdom',
        en_variant   => 'Baz Buz',
    },
);

{
    my $l = DateTime::Locale->load('en_FOO_BAR');
    ok( $l, 'was able to load en_FOO_BAR' );
    is( $l->variant, 'Foo Bar', 'variant is set properly' );

    $l = DateTime::Locale->load('en_BAZ_BUZ');
    ok( $l, 'was able to load en_BAZ_BUZ' );
    is( $l->variant, 'Baz Buz', 'variant is set properly' );
}

# backwards compatibility
DateTime::Locale->register(
    {
        id           => 'en_QUUX_QUAX',
        en_language  => 'English',
        en_territory => 'United Kingdom',
        en_variant   => 'Wacko',
    },
);

{
    my $l = DateTime::Locale->load('en_QUUX_QUAX');
    ok( $l, 'was able to load en_QUUX_QUAX' );
    is( $l->variant, 'Wacko', 'variant is set properly' );
}

# there was a bug with register if the class passed in had an explicit
# DateTime:: namespace
{
    DateTime::Locale->register(
        id           => 'fr_FR_BZH2',
        en_language  => 'French2',
        en_territory => 'French2',
        en_variant   => 'Britanny2',
        class        => 'DateTime::Locale::fr_FR_BZH2',
    );

    my $l = DateTime::Locale->load('fr_FR_BZH2');
    ok( $l, 'was able to load fr_FR_BZH2' );
    is( $l->short_date_format, 'test test2', "short date" );
    is( $l->name, 'French2 French2 Britanny2', 'name is also set' );
}

# with a custom namespace
{
    DateTime::Locale->register(
        id           => 'fr_FR_BZH',
        en_language  => 'French',
        en_territory => 'French',
        en_variant   => 'Britanny',
        class        => 'Other::Locale::fr_FR_BZH',
    );

    my $l = DateTime::Locale->load('fr_FR_BZH');
    ok( $l, 'was able to load fr_FR_BZH' );
    is( $l->short_date_format, 'test test2',             "short date" );
    is( $l->name,              'French French Britanny', 'name is also set' );
}
