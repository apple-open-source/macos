#!/usr/bin/perl -w

use ExtUtils::MakeMaker;

my $name = 'UNIVERSAL::require';
my $version_from = "lib/$name.pm";
$version_from =~ s{::}{/}g;

WriteMakefile(
    NAME         =>     'UNIVERSAL::require',
    VERSION_FROM => $version_from,
    LICENSE      => 'perl',
    PREREQ_PM    => { Test::More => 0.47 },
);
