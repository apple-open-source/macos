use strict;

# On some platforms (*ahem*, MacOS 5.6.1) "use lib qw(lib);" doesn't
# find the local "lib" directory, so we use File::Spec to do it properly.
use File::Spec;
use lib File::Spec->catdir('lib');

# We use Module::Build to test & install itself.
use Module::Build;

my $build = new Module::Build
  (
   module_name => 'Module::Build',   
   license => 'perl',
   requires => {
		'perl'           => '5.005_03',
		'Config'         => 0,
		'Data::Dumper'   => 0,
		'File::Basename' => 0,
		'File::Compare'  => 0,
		'File::Copy'     => 0,
		'File::Find'     => 0,
		'File::Path'     => 0,
		'File::Spec'     => 0,
		'ExtUtils::Install' => 0,
		'IO::File'       => 0,
		'Cwd'            => 0,
	       },
   recommends => {
		  'YAML' => 0.35,
		  'Archive::Tar' => '1.00',
		  'ExtUtils::Install' => 0.30,
		  'ExtUtils::ParseXS' => 2.02,
		  # 'Module::Signature' => 0.21,  # Our support isn't good enough yet
		 },
   build_requires => {
		      Test => 0,
		     },
  );

$build->create_build_script;
$build->notes(foo => 'bar');
