use strict;
use warnings;

use Test::More;

use Class::MOP;

{
    package MyMeta;
    use base 'Class::MOP::Class';
    sub initialize {
        my $class = shift;
        my ( $package, %options ) = @_;
        ::cmp_ok( $options{foo}, 'eq', 'this',
            'option passed to initialize() on create_anon_class()' );
        return $class->SUPER::initialize( @_ );
    }

}

my $anon = MyMeta->create_anon_class( foo => 'this' );
isa_ok( $anon, 'MyMeta' );

done_testing;
