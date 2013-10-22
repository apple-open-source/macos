#line 1
##
# name:      Module::Install::Package
# abstract:  Module::Install support for Module::Package
# author:    Ingy döt Net <ingy@cpan.org>
# license:   perl
# copyright: 2011
# see:
# - Module::Package

# This module contains the Module::Package logic that must be available to
# both the Author and the End User. Author-only logic goes in a
# Module::Package::Plugin subclass.
package Module::Install::Package;
use strict;
use Module::Install::Base;
use vars qw'@ISA $VERSION';
@ISA = 'Module::Install::Base';
$VERSION = '0.26';

#-----------------------------------------------------------------------------#
# XXX BOOTBUGHACK
# This is here to try to get us out of Module-Package-0.11 cpantesters hell...
# Remove this when the situation has blown over.
sub pkg {
    *inc::Module::Package::VERSION = sub { $VERSION };
    my $self = shift;
    $self->module_package_internals_init($@);
}

#-----------------------------------------------------------------------------#
# We allow the author to specify key/value options after the plugin. These
# options need to be available both at author time and install time.
#-----------------------------------------------------------------------------#
# OO accessor for command line options:
sub package_options {
    @_>1?($_[0]->{package_options}=$_[1]):$_[0]->{package_options}}

my $default_options = {
    deps_list => 1,
    install_bin => 1,
    install_share => 1,
    manifest_skip => 1,
    requires_from => 1,
};

#-----------------------------------------------------------------------------#
# Module::Install plugin directives. Use long, ugly names to not pollute the
# Module::Install plugin namespace. These are only intended to be called from
# Module::Package.
#-----------------------------------------------------------------------------#

# Module::Package starts off life as a normal call to this Module::Install
# plugin directive:
my $module_install_plugin;
my $module_package_plugin;
# XXX ARGVHACK This @argv thing is a temporary fix for an ugly bug somewhere in the
# Wikitext module usage.
my @argv;
sub module_package_internals_init {
    my $self = $module_install_plugin = shift;
    my ($plugin_spec, %options) = @_;
    $self->package_options({%$default_options, %options});

    if ($module_install_plugin->is_admin) {
        $module_package_plugin = $self->_load_plugin($plugin_spec);
        $module_package_plugin->mi($module_install_plugin);
        $module_package_plugin->version_check($VERSION);
    }
    # NOTE - This is the point in time where the body of Makefile.PL runs...
    return;

    sub INIT {
        return unless $module_install_plugin;
        return if $Module::Package::ERROR;
        eval {
            if ($module_install_plugin->is_admin) {
                $module_package_plugin->initial();
                $module_package_plugin->main();
            }
            else {
                $module_install_plugin->_initial();
                $module_install_plugin->_main();
            }
        };
        if ($@) {
            $Module::Package::ERROR = $@;
            die $@;
        }
        @argv = @ARGV; # XXX ARGVHACK
    }

    # If this Module::Install plugin was used (by Module::Package) then wrap
    # up any loose ends. This will get called after Makefile.PL has completed.
    sub END {
        @ARGV = @argv; # XXX ARGVHACK
        return unless $module_install_plugin;
        return if $Module::Package::ERROR;
        $module_package_plugin
            ? do {
                $module_package_plugin->final;
                $module_package_plugin->replicate_module_package;
            }
            : $module_install_plugin->_final;
    }
}

# Module::Package, Module::Install::Package and Module::Package::Plugin
# must all have the same version. Seems wise.
sub module_package_internals_version_check {
    my ($self, $version) = @_;
    return if $version < 0.1800001;   # XXX BOOTBUGHACK!!
    die <<"..." unless $version == $VERSION;

Error! Something has gone awry:
    Module::Package version=$version is using 
    Module::Install::Package version=$VERSION
If you are the author of this module, try upgrading Module::Package.
Otherwise, please notify the author of this error.

...
}

# Find and load the author side plugin:
sub _load_plugin {
    my ($self, $spec) = @_;
    $spec ||= '';
    my $version = '';
    $Module::Package::plugin_version = 0;
    if ($spec =~ s/\s+(\S+)\s*//) {
        $version = $1;
        $Module::Package::plugin_version = $version;
    }
    my ($module, $plugin) =
        not($spec) ? ('Plugin', "Plugin::basic") :
        ($spec =~ /^\w(\w|::)*$/) ? ($spec, $spec) :
        ($spec =~ /^:(\w+)$/) ? ('Plugin', "Plugin::$1") :
        ($spec =~ /^(\S*\w):(\w+)$/) ? ($1, "$1::$2") :
        die "$spec is invalid";
    $module = "Module::Package::$module";
    $plugin = "Module::Package::$plugin";
    eval "use $module $version (); 1" or die $@;
    return $plugin->new();
}

#-----------------------------------------------------------------------------#
# These are the user side analogs to the author side plugin API calls.
# Prefix with '_' to not pollute Module::Install plugin space.
#-----------------------------------------------------------------------------#
sub _initial {
    my ($self) = @_;
}

sub _main {
    my ($self) = @_;
}

# NOTE These must match Module::Package::Plugin::final.
sub _final {
    my ($self) = @_;
    $self->_all_from;
    $self->_requires_from;
    $self->_install_bin;
    $self->_install_share;
    $self->_WriteAll;
}

#-----------------------------------------------------------------------------#
# This section is where all the useful code bits go. These bits are needed by
# both Author and User side runs.
#-----------------------------------------------------------------------------#

my $all_from = 0;
sub _all_from {
    my $self = shift;
    return if $all_from++;
    return if $self->name;
    my $file = shift || "$main::PM" or die "all_from has no file";
    $self->all_from($file);
}

my $requires_from = 0;
sub _requires_from {
    my $self = shift;
    return if $requires_from++;
    return unless $self->package_options->{requires_from};
    my $file = shift || "$main::PM" or die "requires_from has no file";
    $self->requires_from($main::PM)
}

my $install_bin = 0;
sub _install_bin {
    my $self = shift;
    return if $install_bin++;
    return unless $self->package_options->{install_bin};
    return unless -d 'bin';
    my @bin;
    File::Find::find(sub {
        return unless -f $_;
        push @bin, $File::Find::name;
    }, 'bin');
    $self->install_script($_) for @bin;
}

my $install_share = 0;
sub _install_share {
    my $self = shift;
    return if $install_share++;
    return unless $self->package_options->{install_share};
    return unless -d 'share';
    $self->install_share;
}

my $WriteAll = 0;
sub _WriteAll {
    my $self = shift;
    return if $WriteAll++;
    $self->WriteAll(@_);
}

#-----------------------------------------------------------------------------#
# Take a guess at the primary .pm and .pod files for 'all_from', and friends.
# Put them in global magical vars in the main:: namespace.
#-----------------------------------------------------------------------------#
package Module::Package::PM;
use overload '""' => sub {
    $_[0]->guess_pm unless @{$_[0]};
    return $_[0]->[0];
};
sub set { $_[0]->[0] = $_[1] }
sub guess_pm {
    my $pm = '';
    my $self = shift;
    if (-e 'META.yml') {
        open META, 'META.yml' or die "Can't open 'META.yml' for input:\n$!";
        my $meta = do { local $/; <META> };
        close META;
        $meta =~ /^module_name: (\S+)$/m
            or die "Can't get module_name from META.yml";
        $pm = $1;
        $pm =~ s!::!/!g;
        $pm = "lib/$pm.pm";
    }
    else {
        require File::Find;
        my @array = ();
        File::Find::find(sub {
            return unless /\.pm$/;
            my $name = $File::Find::name;
            my $num = ($name =~ s!/+!/!g);
            my $ary = $array[$num] ||= [];
            push @$ary, $name;
        }, 'lib');
        shift @array while @array and not defined $array[0];
        die "Can't guess main module" unless @array;
        (($pm) = sort @{$array[0]}) or
            die "Can't guess main module";
    }
    $self->set($pm);
}
$main::PM = bless [$main::PM ? ($main::PM) : ()], __PACKAGE__;

package Module::Package::POD;
use overload '""' => sub {
    return $_[0]->[0] if @{$_[0]};
    (my $pod = "$main::PM") =~ s/\.pm/.pod/ or die;
    return -e $pod ? $pod : '';
};
sub set { $_[0][0] = $_[1] }
$main::POD = bless [$main::POD ? ($main::POD) : ()], __PACKAGE__;

1;

