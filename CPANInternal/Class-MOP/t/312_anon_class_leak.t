use strict;
use warnings;

use Class::MOP;
use Test::More;

BEGIN {
    eval "use Test::LeakTrace;";
    plan skip_all => "Test::LeakTrace is required for this test" if $@;
}

# 5.10.0 has a bug on weaken($hash_ref) which leaks an AV.
my $expected = ( $] == 5.010_000 ? 1 : 0 );

leaks_cmp_ok {
    Class::MOP::Class->create_anon_class();
}
'<=', $expected, 'create_anon_class()';

leaks_cmp_ok {
    Class::MOP::Class->create_anon_class( superclasses => [qw(Exporter)] );
}
'<=', $expected, 'create_anon_class(superclass => [...])';

done_testing;
