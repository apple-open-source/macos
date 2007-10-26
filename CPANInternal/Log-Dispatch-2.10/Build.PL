use strict;

use Module::Build;

my $build =
    Module::Build->new( module_name => 'Log::Dispatch',
                        license     => 'perl',
                        requires    => { 'Params::Validate' => 0.15,
                                         'Test::More'       => 0,
                                       },
                        sign        => 1,
                        create_makefile_pl => 'passthrough',
                      );

$build->add_to_cleanup('write_mode.tst');
$build->create_build_script;
