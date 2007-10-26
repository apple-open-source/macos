use strict;

use Module::Build;


Module::Build->new( module_name => 'DateTime::Locale',
                    author      => 'Dave Rolsky <autarch@urth.org>',
                    license     => 'perl',
                    requires    => { 'Params::Validate' => 0,
                                   },
		    sign        => 1,
                    create_makefile_pl => 'traditional',
                  )->create_build_script;
