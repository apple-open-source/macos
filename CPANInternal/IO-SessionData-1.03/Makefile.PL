#!/usr/bin/perl

use strict;
use warnings;

use ExtUtils::MakeMaker;

WriteMakefile(
    NAME          => 'IO::SessionData',
    VERSION_FROM  => 'lib/IO/SessionData.pm',
    PREREQ_PM     => {},
    ABSTRACT      => 'supporting module for SOAP::Lite',
    AUTHOR        => 'Fred Moyer <fred@redhotpenguin.com>'
);
