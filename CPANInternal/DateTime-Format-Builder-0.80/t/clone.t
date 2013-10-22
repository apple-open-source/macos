use strict;

use Test::More tests => 5;

use DateTime::Format::Builder;


my $clone_it = sub {
    my $obj = shift;
    my $method = shift;
    my $clone = $obj->$method();
    isa_ok( $clone => 'DateTime::Format::Builder' );
    my $p1 = $obj->get_parser();
    my $p2 = $clone->get_parser();
    is( $p1 => $p2, "Parser cloned");
};


my $obj = DateTime::Format::Builder->new();
isa_ok( $obj => 'DateTime::Format::Builder' );

for my $method (qw( new clone ))
{
    $clone_it->( $obj, $method );
}
