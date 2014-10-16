use strict;
use warnings;
use Test::More tests => 1;
use Data::UUID qw(NameSpace_DNS);

my $generator = new Data::UUID;
open(my $bad_fh,"<","/a/failing/path/that/does/not/exist/but/sets/dollarbang");

    eval { 
        ok($generator->create_from_name_str( NameSpace_DNS, '1.2.3.4' ), "\$! didn't leak!");;
    };

if (my $msg = $@) {
    ok(undef, "create_from_name_str failed: $msg");
}
