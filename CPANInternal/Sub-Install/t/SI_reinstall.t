use Sub::Install;
Sub::Install::install_installers('UNIVERSAL');

# This test, from here on out, is the verbatim "reinstall.t" test from
# Sub::Installer 0.0.2

use Test::More 'no_plan';
use warnings;

# Install a sub in a package...

my $sub_ref = main->reinstall_sub({ ok1 => \&ok });

is ref $sub_ref, 'CODE'                  => 'reinstall returns code ref';

is_deeply \&ok, $sub_ref                 => 'reinstall returns correct code ref';

ok1(1                                    => 'reinstalled sub runs');


# Install the same sub in the same package...

$SIG{__WARN__} = sub { ok 0 => "warned unexpected: @_" if $_[0] =~ /redefined/ };

$sub_ref = main->reinstall_sub({ ok1 => \&is });

is ref $sub_ref, 'CODE'                  => 'reinstall2 returns code ref';

is_deeply \&is, $sub_ref                 => 'reinstall2 returns correct code ref';

ok1(1,1                                  => 'reinstalled sub reruns');

# Install in another package...

$sub_ref = Other->reinstall_sub({ ok2 => \&ok });

is ref $sub_ref, 'CODE'                  => 'reinstall2 returns code ref';

is_deeply \&ok, $sub_ref                 => 'reinstall2 returns correct code ref';

ok1(1,1                                  => 'reinstalled sub reruns');

package Other;

ok2(1                                    => 'remotely reinstalled sub runs');
