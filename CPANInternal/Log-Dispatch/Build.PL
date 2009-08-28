use strict;
use warnings;

use Module::Build;

my $build =
    Module::Build->new( module_name => 'Log::Dispatch',
                        license     => 'perl',
                        requires    => { 'Params::Validate' => 0.15,
                                       },
                        build_requires => { 'File::Temp'    => 0,
                                            'Module::Build' => 0,
                                            'Test::More'    => 0,
                                          },
                        sign               => 1,
                        create_makefile_pl => 'traditional',
                      );

$build->create_build_script();
