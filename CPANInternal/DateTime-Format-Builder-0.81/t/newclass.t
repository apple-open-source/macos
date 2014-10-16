use strict;
use warnings;

use Test::More;

use DateTime::Format::Builder;

my %common = (
    version => 4.00,
    parsers => {
        parse_datetime => {
            params => [qw( year month day hour minute second )],
            regex  => qr/^(\d\d\d\d)(\d\d)(\d\d)T(\d\d)(\d\d)(\d\d)$/,
        }
    },
);

# Does create_class() work properly?
{
    my $sample   = "20030716T163245";
    my $newclass = "DateTime::Format::ICal15";

    DateTime::Format::Builder->create_class(
        %common,
        class => $newclass,
    );

    my $parser = $newclass->new();
    cmp_ok( $newclass->VERSION, '==', '4.00', "Version matches" );

    {
        my $dt = $parser->parse_datetime($sample);
        isa_ok( $dt => "DateTime" );
        my %methods = qw(
            hour 16 minute 32 second 45
            year 2003 month 7 day 16
        );
        while ( my ( $method, $expected ) = each %methods ) {
            is(
                $dt->$method() => $expected,
                "\$dt->$method() == $expected"
            );
        }
    }

    # New with args
    {
        eval { $newclass->new( "with", "args" ) };
        ok( $@, "Should have errors" );
        like( $@, qr{ takes no parameters}, "Right error" );
    }

    # New from object
    {
        my $new = $parser->new();
        isa_ok( $new, $newclass, "New from object gives right class" );
    }
}

# New class, with given new
{
    my $newclass = "DateTime::Format::ICalTest";

    DateTime::Format::Builder->create_class(
        %common,
        class       => $newclass,
        constructor => sub { bless { "Foo" => "Bar" }, shift },
    );

    my $parser = $newclass->new();
    cmp_ok( $newclass->VERSION, '==', '4.00', "Version matches" );
    is( $parser->{"Foo"} => "Bar", "Used the right constructor" );
}

# New class, with undef new
{
    my $newclass = "DateTime::Format::ICalTestUndef";

    eval {
        DateTime::Format::Builder->create_class(
            %common,
            class       => $newclass,
            constructor => undef,
        );
    };
    ok( !$@, "Should be no errors with undef new" );
    ok( !( UNIVERSAL::can( $newclass, 'new' ) ), "Should be no constructor" );
}

done_testing();
