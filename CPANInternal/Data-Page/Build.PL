use Module::Build;
use strict;

my $build = Module::Build->new(
  create_makefile_pl => 'traditional',
  license            => 'perl',
  module_name        => 'Data::Page',
  requires           => {
    'Test::More'                     => '0',
    'Class::Accessor::Chained::Fast' => '0',
    'Test::Exception'                => '0',
  },
);
$build->create_build_script;
