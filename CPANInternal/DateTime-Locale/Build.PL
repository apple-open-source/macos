use 5.006;

use strict;
use warnings;

use Module::Build;

Module::Build->new(
    module_name => 'DateTime::Locale',
    dist_author => 'Dave Rolsky <autarch@urth.org>',
    license     => 'perl',
    requires    => {
        'List::MoreUtils'  => 0,
        'Params::Validate' => '0.91',
        perl               => 5.006,
    },
    build_requires     => { 'Module::Build' => 0 },
    sign               => 1,
    create_makefile_pl => 'traditional',
    meta_merge         => {
        resources => {
            homepage => 'http://datetime.perl.org/',
            bugtracker =>
                'http://rt.cpan.org/NoAuth/Bugs.html?Dist=DateTime-Locale',
            repository => 'http://hg.urth.org/hg/DateTime-Locale',
        },
    },
)->create_build_script;

check_conflicts();

# coped from Moose Makefile.PL
sub check_conflicts {
    my %conflicts = (
        'DateTime::Format::Strptime' => '1.1000',
    );

    my $found = 0;
    for my $mod ( sort keys %conflicts ) {
        eval "require $mod";
        next if $@;

        my $installed = $mod->VERSION();
        if ( $installed le $conflicts{$mod} ) {

            print <<"EOF";

***
    This version of DateTime::Locale conflicts with the version of
    $mod ($installed) you have installed.

    You will need to upgrade $mod after installing this version of
    DateTime::Locale.
***

EOF

            $found = 1;
        }
    }

    return unless $found;

    # More or less copied from Module::Build
    return if $ENV{PERL_MM_USE_DEFAULT};
    return unless -t STDIN && ( -t STDOUT || !( -f STDOUT || -c STDOUT ) );

    sleep 4;
}
