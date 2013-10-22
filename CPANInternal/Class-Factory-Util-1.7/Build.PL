use strict;
use Module::Build;

Module::Build->new
    ( module_name        => 'Class::Factory::Util',
      license            => 'perl',
      sign               => 1,
      create_readme      => 1,
      create_makefile_pl => 'passthrough',
    )->create_build_script();
