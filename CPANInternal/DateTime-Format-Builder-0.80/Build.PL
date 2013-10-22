use strict;

use Module::Build;

Module::Build->new
    ( module_name => 'DateTime::Format::Builder',
      author      => 'Dave Rolsky <autarch@urth.org>',
      license     => 'perl',
      requires    => { 'Class::Factory::Util'       => 1.6,
                       'DateTime'                   => 0.12,
                       'DateTime::Format::Strptime' => 1.04,
                       'Params::Validate'           => 0.72,
                       'Task::Weaken'               => 0,
                     },
      build_requires => { 'Module::Build' => 0 },
      sign        => 1,
      create_makefile_pl => 'traditional',
      create_readme => 1,
    )->create_build_script;
