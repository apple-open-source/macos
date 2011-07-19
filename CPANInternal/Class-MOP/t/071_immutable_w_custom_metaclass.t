use strict;
use warnings;

use FindBin;
use File::Spec::Functions;

use Test::More;
use Test::Exception;
use Scalar::Util;

use Class::MOP;

use lib catdir( $FindBin::Bin, 'lib' );

{

    package Foo;

    use strict;
    use warnings;
    use metaclass;

    __PACKAGE__->meta->make_immutable;

    package Bar;

    use strict;
    use warnings;
    use metaclass;

    __PACKAGE__->meta->make_immutable;

    package Baz;

    use strict;
    use warnings;
    use metaclass 'MyMetaClass';

    sub mymetaclass_attributes {
        shift->meta->mymetaclass_attributes;
    }

    ::lives_ok{ Baz->meta->superclasses('Bar') }
        '... we survive the metaclass incompatibility test';
}

{
    my $meta = Baz->meta;
    ok( $meta->is_mutable, '... Baz is mutable' );
    is(
        Scalar::Util::blessed( Foo->meta ),
        Scalar::Util::blessed( Bar->meta ),
        'Foo and Bar immutable metaclasses match'
    );
    is( Scalar::Util::blessed($meta), 'MyMetaClass',
        'Baz->meta blessed as MyMetaClass' );
    ok( Baz->can('mymetaclass_attributes'),
        '... Baz can do method before immutable' );
    ok( $meta->can('mymetaclass_attributes'),
        '... meta can do method before immutable' );
    lives_ok { $meta->make_immutable } "Baz is now immutable";
    ok( $meta->is_immutable, '... Baz is immutable' );
    isa_ok( $meta, 'MyMetaClass', 'Baz->meta' );
    ok( Baz->can('mymetaclass_attributes'),
        '... Baz can do method after imutable' );
    ok( $meta->can('mymetaclass_attributes'),
        '... meta can do method after immutable' );
    isnt( Scalar::Util::blessed( Baz->meta ),
        Scalar::Util::blessed( Bar->meta ),
        'Baz and Bar immutable metaclasses are different' );
    lives_ok { $meta->make_mutable } "Baz is now mutable";
    ok( $meta->is_mutable, '... Baz is mutable again' );
}

done_testing;
