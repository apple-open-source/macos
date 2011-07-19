#!perl -T

use Class::Unload;
use lib 't/lib';

use Test::More tests => 6; 

require MyClass::Child;

can_ok( 'MyClass::Child', 'parent_method' );
can_ok( 'MyClass::Child', 'child_method' );

Class::Unload->unload('MyClass::Child');

for my $method ( qw/ parent_method child_method/ ) {
    eval { MyClass::Child->$method };
    like( $@, qr/Can't locate object method "$method" via package "MyClass::Child"/,
          "$method on unloaded class fails");
}

require MyClass::Child;

Class::Unload->unload('MyClass::Parent');

can_ok( 'MyClass::Child', 'child_method' );
eval { MyClass::Child->parent_method };
like( $@, qr/Can't locate object method "parent_method" via package "MyClass::Child"/,
      "method on unloaded parent class fails");

