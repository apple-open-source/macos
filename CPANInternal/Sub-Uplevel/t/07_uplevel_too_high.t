use lib qw(t/lib);
use strict;
use Test::More tests => 5;

BEGIN { use_ok('Sub::Uplevel'); }

sub show_caller {
    return scalar caller;
}

sub wrap_show_caller {
    my $uplevel = shift;
    return uplevel $uplevel, \&show_caller;
}

my $warning = '';
local $SIG{__WARN__} = sub { $warning = shift };

my $caller = wrap_show_caller(1);
is($caller, 'main', "wrapper returned correct caller");
is( $warning, '', "don't warn if ordinary uplevel" );

$warning = '';
$caller = wrap_show_caller(2);
my $file = __FILE__;
is($caller, undef, "wrapper returned correct caller");
like( $warning, qr/uplevel 2 is more than the caller stack/, "warn if too much uplevel" );
