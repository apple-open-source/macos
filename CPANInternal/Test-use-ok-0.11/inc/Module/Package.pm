#line 1
##
# name:      Module::Package
# abstract:  Postmodern Perl Module Packaging
# author:    Ingy döt Net <ingy@cpan.org>
# license:   perl
# copyright: 2011
# see:
# - Module::Package::Plugin
# - Module::Install::Package
# - Module::Package::Tutorial

package Module::Package;
use 5.005;
use strict;

BEGIN {
    $Module::Package::VERSION = '0.26';
    $inc::Module::Package::VERSION ||= $Module::Package::VERSION;
    @inc::Module::Package::ISA = __PACKAGE__;
}

sub import {
    my $class = shift;
    $INC{'inc/Module/Install.pm'} = __FILE__;
    unshift @INC, 'inc' unless $INC[0] eq 'inc';
    eval "use Module::Install 1.01 (); 1" or $class->error($@);

    package main;
    Module::Install->import();
    eval {
        module_package_internals_version_check($Module::Package::VERSION);
        module_package_internals_init(@_);
    };
    if ($@) {
        $Module::Package::ERROR = $@;
        die $@;
    }
}

# XXX Remove this when things are stable.
sub error {
    my ($class, $error) = @_;
    if (-e 'inc' and not -e 'inc/.author') {
        require Data::Dumper;
        $Data::Dumper::Sortkeys = 1;
        my $dump1 = Data::Dumper::Dumper(\%INC);
        my $dump2 = Data::Dumper::Dumper(\@INC);
        die <<"...";
This should not have happened. Hopefully this dump will explain the problem:

inc::Module::Package: $inc::Module::Package::VERSION
Module::Package: $Module::Package::VERSION
inc::Module::Install: $inc::Module::Install::VERSION
Module::Install: $Module::Install::VERSION

Error: $error

%INC:
$dump1
\@INC:
$dump2
...
    }
    else {
        die $error;
    }
}

1;

