use strict;

use Module::Build;
use File::Spec;

unless ( -e File::Spec->catfile( qw( lib DateTime TimeZone America Chicago.pm ) ) )
{
    warn <<'EOF';

It looks like you haven't run tools/parse_olson to generate the time
zone modules yet.

This script needs the Olson database files to run, which can be found
at ftp://elsie.nci.nih.gov/pub/ in a file called tzdata*.tar.gz.

Once you've run this script you will be able to test and install this
distribution.

EOF

    exit;
}

Module::Build->new( module_name => 'DateTime::TimeZone',
                    author      => 'Dave Rolsky <autarch@urth.org>',
                    license     => 'perl',
                    requires    => { 'Params::Validate' => 0.72,
                                     'Class::Singleton' => 1.03,
                                     'Pod::Man'    => 1.14,
                                   },
                    build_requires => { 'Module::Build' => 0 },
		    sign        => 1,
                    create_makefile_pl => 'passthrough',
                  )->create_build_script;
