#!/usr/bin/perl

use Module::Build;
my $perl_only;

my $t = Module::Build->new(
    module_name     => 'version',
    get_options     => {
       'perl_only' => { store => \$perl_only },
    },
);

my %build_arguments = (
    dist_name       => 'version',
    dist_version_from => 'lib/version.pm',
    license         => 'perl',
    requires        => {
	perl => '> 5.005, !=5.9.1, !=5.9.2',
    },
    dynamic_config  => 1,
);

mkdir $t->config_dir();

if ( $perl_only ) { #or not $t->have_c_compiler() ) {
    $build_arguments{module_name} = 'version::vpp';
    if ( $] > 5.008001 ) {
	$build_arguments{requires} = {
	    'Scalar::Util' => 1.08,
	};
    }
    $build_arguments{pm_files} = {
	'./lib/version.pm' => './lib/version.pm',
	'./vperl/vpp.pm' => 'lib/version/vpp.pm',
    };
}
else {
    $build_arguments{c_source} = './vutil';
    $build_arguments{module_name} = 'version::vxs';
    $build_arguments{pm_files} = {
	'./lib/version.pm' => './lib/version.pm',
	'./vutil/vxs.pm' => 'lib/version/vxs.pm'
    };
    $build_arguments{xs_files} = {
	'./vutil/vxs.xs' => 'lib/version/vxs.xs'
    };
    $build_arguments{add_to_cleanup} = 
    	['lib/version/vxs.*'];
}

my $class = Module::Build->subclass
(
    class => 'version::Builder',
    code => q{
    sub ACTION_dist{
        my $self = shift;
    	$self->do_system('svk log -x | gnuify-changelog.pl > Changes');
	$self->SUPER::ACTION_dist();
	}
    },
);

my $m = $class->new(%build_arguments);

$m->create_build_script;
