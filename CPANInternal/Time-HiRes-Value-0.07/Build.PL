use strict;
use warnings;

use Module::Build;

my $build = Module::Build->new
  (
   module_name => 'Time::HiRes::Value',
   dist_version_from => 'lib/Time/HiRes/Value.pm',
   requires => {
                 'Time::HiRes' => 0
               },
   build_requires => {
                 'Test::More' => 0,
                 'Test::Exception' => 0,
               },
   license => 'perl',
   create_makefile_pl => 'traditional',
   create_license => 1,
   create_readme  => 1,
  );
  
$build->create_build_script;
