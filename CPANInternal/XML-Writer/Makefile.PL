#!/usr/bin/perl -w

use strict;

use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'    => 'XML::Writer',
    'VERSION' => '0.600',

    # A manually-created META.yml has all the other metadata;
    #  we don't want it overwritten
    NO_META => 1
);
