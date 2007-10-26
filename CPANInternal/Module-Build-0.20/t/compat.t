use Test;
use Module::Build;
use Module::Build::Compat;
use File::Spec;
use File::Path;
use Config;
require File::Spec->catfile('t', 'common.pl');

skip_test("Don't know how to invoke 'make'") unless $Config{make};
plan tests => 23;
ok(1);  # Loaded

my @make = $Config{make} eq 'nmake' ? ('nmake', '-nologo') : ($Config{make});

my $goto = File::Spec->catdir( Module::Build->cwd, 't', 'Sample' );
chdir $goto or die "can't chdir to $goto: $!";

my $build = Module::Build->new
  ( module_name => 'Sample',
    requires => { File::Spec => 0, File::Path => $File::Path::VERSION },
    build_requires => { Module::Build => 0 },
  );
ok $build;

$build->add_to_cleanup('Makefile.PL');

foreach my $type (qw(small passthrough traditional)) {
  Module::Build::Compat->create_makefile_pl($type, $build);
  my $result = $build->run_perl_script('Makefile.PL');
  ok $result;
  ok -e 'Makefile', 1, "Makefile exists";

  ok $build->do_system(@make);
  
  # Can't let 'test' STDOUT go to our STDOUT, or it'll confuse Test::Harness.
  my $success;
  my $output = stdout_of( sub {
			    $success = $build->do_system(@make, 'test');
			  } );
  ok $success;
  ok uc $output, qr{DONE\.|SUCCESS};
  
  ok $build->do_system(@make, 'realclean');
  $build->dispatch('realclean');
  ok not -e 'Makefile.PL';
}
