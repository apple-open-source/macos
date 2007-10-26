use strict;

use Test; 
BEGIN { plan tests => 9 }
use Module::Build;
ok 1;

my $start_dir = Module::Build->cwd;

my $goto = File::Spec->catdir( $start_dir, 't', 'Sample' );
chdir $goto or die "can't chdir to $goto: $!";

# Here we make sure actions are only called once per dispatch()
my $build = Module::Build->subclass
  (
   code => "sub ACTION_loop { die 'recursed' if \$::x++; shift->depends_on('loop'); }"
  )->new( module_name => 'Sample' );
ok $build;

$build->dispatch('loop');
ok $::x, 1;

$build->dispatch('realclean');

{
  # Make sure globbing works in filenames
  $build->test_files('*t*');
  my $files = $build->test_files;
  ok  grep {$_ eq 'script'} @$files;
  ok  grep {$_ eq 'test.pl'} @$files;
  ok !grep {$_ eq 'Build.PL'} @$files;

  # Make sure order is preserved
  $build->test_files('foo', 'bar');
  $files = $build->test_files;
  ok @$files, 2;
  ok $files->[0], 'foo';
  ok $files->[1], 'bar';
}
