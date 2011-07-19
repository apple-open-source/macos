use strict;
use warnings;
use Data::Dumper::Concise::Sugar;

use Data::Dumper::Concise ();

use Test::More qw(no_plan);

my $warned_string;

BEGIN {
   $SIG{'__WARN__'} = sub {
      $warned_string = $_[0]
   }
}

my @foo = Dwarn 'warn', 'friend';
is $warned_string,qq{"warn"\n"friend"\n}, 'Dwarn warns';

ok eq_array(\@foo, ['warn','friend']), 'Dwarn passes through correctly';

my $bar = DwarnS 'robot',2,3;
is $warned_string,qq{"robot"\n}, 'DwarnS warns';
is $bar, 'robot', 'DwarnS passes through correctly';
