#!/usr/bin/perl
use warnings;
use strict;
use Module::Build;

Module::Build->new(
    module_name => 'Class::Data::Accessor',
    license => 'perl',
    requires => {
        Carp => 0
    },
    create_makefile_pl => 'passthrough',
    create_readme => 1,
)->create_build_script;
