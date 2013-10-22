use Test::More 'no_plan';

use Class::Std;

use strict;
use warnings;

my @warnings;
local $SIG{__WARN__} = sub { push @warnings, @_; warn @_; };


UNIVERSAL::can(undef, 'any');
ok(! @warnings, 'overwritten UNIVERSAL::can throws no warnings');

