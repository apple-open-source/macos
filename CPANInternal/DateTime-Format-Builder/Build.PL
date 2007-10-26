use strict;

use Module::Build;

Module::Build->new( module_name => 'DateTime::Format::Builder',
                    author      => 'Dave Rolsky <autarch@urth.org>',
                    license     => 'perl',
                    requires    => { 'DateTime' => 0.12,
                                     'DateTime::Format::Strptime' => '1.04',
                                     'Params::Validate' => 0.72,
                                   },
		    sign        => 1,
                    create_makefile_pl => 'passthrough',
                  )->create_build_script;
