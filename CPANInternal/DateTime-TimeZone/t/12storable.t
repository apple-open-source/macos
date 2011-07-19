use strict;
use warnings;

use File::Spec;
use Test::More;

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN { require 'check_datetime_version.pl' }

use DateTime::TimeZone;
use DateTime::TimeZone::OffsetOnly;
use Storable;

plan tests => 11;


{
    my $tz1 = DateTime::TimeZone->new( name => 'America/Chicago' );
    my $frozen = Storable::nfreeze($tz1);

    ok( length $frozen < 2000,
        'the serialized tz object should not be immense' );

    test_thaw_and_clone( $tz1 );
}

{
    for my $obj ( DateTime::TimeZone::OffsetOnly->new( offset => '+0100' ),
                  DateTime::TimeZone::Floating->new(),
                  DateTime::TimeZone::UTC->new(),
                )
    {
        test_thaw_and_clone($obj);
    }
}

sub test_thaw_and_clone
{
    my $tz1 = shift;
    my $name = $tz1->name;

    my $tz2 = Storable::thaw( Storable::nfreeze($tz1) );

    my $class = ref $tz1;
    is( $tz2->name, $name, "thaw frozen $class" );

    if ( exists $tz1->{spans} )
    {
        is( $tz1->{spans}, $tz2->{spans}, "spans remain shared for $class after freeze/thaw");
    }

    my $tz3 = Storable::dclone($tz1);
    is( $tz3->name, $name, "dclone $class" );

    if ( exists $tz1->{spans} )
    {
        is( $tz1->{spans}, $tz3->{spans}, "spans remain shared for $class after dclone");
    }
}
