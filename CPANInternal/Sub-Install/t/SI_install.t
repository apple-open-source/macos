use Sub::Install;
Sub::Install::install_installers('UNIVERSAL');

# This test, from here on out, is the verbatim "install.t" test from
# Sub::Installer 0.0.2

use Test::More 'no_plan';
use warnings;

# Install a sub in a package...

my $sub_ref = main->install_sub({ ok1 => \&ok });

is ref $sub_ref, 'CODE'                  => 'install returns code ref';

is_deeply \&ok, $sub_ref                 => 'install returns correct code ref';

ok1(1                                    => 'installed sub runs');


# Install the same sub in the same package...

$SIG{__WARN__} = sub { ok 1 => 'warned as expected' if $_[0] =~ /redefined/ };


$sub_ref = main->install_sub({ ok1 => \&is });

is ref $sub_ref, 'CODE'                  => 'install2 returns code ref';

is_deeply \&is, $sub_ref                 => 'install2 returns correct code ref';

ok1(1,1                                  => 'installed sub reruns');

# Install in another package...

$sub_ref = Other->install_sub({ ok2 => \&ok });

is ref $sub_ref, 'CODE'                  => 'install2 returns code ref';

is_deeply \&ok, $sub_ref                 => 'install2 returns correct code ref';

ok1(1,1                                  => 'installed sub reruns');

package Other;

ok2(1                                    => 'remotely installed sub runs');
