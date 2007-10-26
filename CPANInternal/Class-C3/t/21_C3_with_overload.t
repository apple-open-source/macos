#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 8;

BEGIN {
    use_ok('Class::C3');
}

{
    package BaseTest;
    use strict;
    use warnings;
    use Class::C3;
    
    package OverloadingTest;
    use strict;
    use warnings;
    use Class::C3;
    use base 'BaseTest';        
    use overload '""' => sub { ref(shift) . " stringified" },
                 fallback => 1;
    
    sub new { bless {} => shift }    
    
    package InheritingFromOverloadedTest;
    use strict;
    use warnings;
    use base 'OverloadingTest';
    use Class::C3;
}

Class::C3::initialize();

my $x = InheritingFromOverloadedTest->new();
isa_ok($x, 'InheritingFromOverloadedTest');

my $y = OverloadingTest->new();
isa_ok($y, 'OverloadingTest');

is("$x", 'InheritingFromOverloadedTest stringified', '... got the right value when stringifing');
is("$y", 'OverloadingTest stringified', '... got the right value when stringifing');

ok(($y eq 'OverloadingTest stringified'), '... eq was handled correctly');

my $result;
eval { 
    $result = $x eq 'InheritingFromOverloadedTest stringified' 
};
ok(!$@, '... this should not throw an exception');
ok($result, '... and we should get the true value');

#use Data::Dumper;
#diag Dumper { Class::C3::_dump_MRO_table }
