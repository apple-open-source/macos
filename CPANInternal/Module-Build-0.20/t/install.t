use strict;

use Test; 
BEGIN { plan tests => 17 }
use Module::Build;
use File::Spec;
use File::Path;
use Config;

ok(1);
require File::Spec->catfile('t', 'common.pl');

my $start_dir = Module::Build->cwd;

# Would be nice to just have a 'base_dir' parameter for M::B->new()
my $goto = File::Spec->catdir( $start_dir, 't', 'Sample' );
chdir $goto or die "can't chdir to $goto: $!";


my $build = new Module::Build( module_name => 'Sample',
			       script_files => [ 'script' ],
			       requires => { 'File::Spec' => 0 },
			       license => 'perl' );
ok $build;


my $destdir = File::Spec->catdir($start_dir, 't', 'install_test');
$build->add_to_cleanup($destdir);

{
  eval {$build->dispatch('install', destdir => $destdir)};
  ok $@, '';
  
  my $libdir = strip_volume( $build->install_destination('lib') );
  my $install_to = File::Spec->catfile($destdir, $libdir, 'Sample.pm');
  print "Should have installed module as $install_to\n";
  ok -e $install_to;
}

{
  eval {$build->dispatch('install', installdirs => 'core', destdir => $destdir)};
  ok $@, '';
  my $libdir = strip_volume( $Config{installprivlib} );
  my $install_to = File::Spec->catfile($destdir, $libdir, 'Sample.pm');
  print "Should have installed module as $install_to\n";
  ok -e $install_to;
}

{
  my $libdir = File::Spec->catdir(File::Spec->rootdir, 'foo', 'bar');
  eval {$build->dispatch('install', install_path => {lib => $libdir}, destdir => $destdir)};
  ok $@, '';
  my $install_to = File::Spec->catfile($destdir, $libdir, 'Sample.pm');
  print "Should have installed module as $install_to\n";
  ok -e $install_to;
}

{
  my $libdir = File::Spec->catdir(File::Spec->rootdir, 'foo', 'base');
  eval {$build->dispatch('install', install_base => $libdir, destdir => $destdir)};
  ok $@, '';
  my $install_to = File::Spec->catfile($destdir, $libdir, 'lib', 'Sample.pm');
  print "Should have installed module as $install_to\n";
  ok -e $install_to;  
}

eval {$build->dispatch('realclean')};
ok $@, '';

{
  # Try again by running the script rather than with programmatic interface
  my $libdir = File::Spec->catdir('', 'foo', 'lib');
  eval {$build->run_perl_script('Build.PL', [], ['--install_path', "lib=$libdir"])};
  ok $@, '';
  
  eval {$build->run_perl_script('Build', [], ['install', '--destdir', $destdir])};
  ok $@, '';
  my $install_to = File::Spec->catfile($destdir, $libdir, 'Sample.pm');
  print "# Should have installed module as $install_to\n";
  ok -e $install_to;

  my $basedir = File::Spec->catdir('', 'bar');
  eval {$build->run_perl_script('Build', [], ['install', '--destdir', $destdir,
					      '--install_base', $basedir])};
  ok $@, '';
  
  my $relpath = $build->install_base_relative('lib');
  $install_to = File::Spec->catfile($destdir, $basedir, $relpath, 'Sample.pm');
  print "# Should have installed module as $install_to\n";
  ok -e $install_to;
  
  eval {$build->dispatch('realclean')};
  ok $@, '';
}

sub strip_volume {
  my $dir = shift;
  (undef, $dir) = File::Spec->splitpath( $dir, 1 );
  return $dir;
}

