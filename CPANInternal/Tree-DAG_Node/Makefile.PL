# This script writes the Makefile for installing this distribution.
#
# See "perldoc perlmodinstall" or "perldoc ExtUtils::MakeMaker" for
# info on how to control how the installation goes.

require 5.004;
use strict;
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME          => 'Tree::DAG_Node',
    ABSTRACT_FROM => 'lib/Tree/DAG_Node.pm',
    VERSION_FROM  => 'lib/Tree/DAG_Node.pm',
    AUTHOR        => 'David Hand, <cogent@cpan.org>',
    LICENSE       => 'perl',
    PREREQ_PM     => {
        Carp   => 0,
        strict => 0,
        vars   => 0,
    },
    dist         => {
        COMPRESS => 'gzip -6f',
        SUFFIX   => 'gz',
    },
);
