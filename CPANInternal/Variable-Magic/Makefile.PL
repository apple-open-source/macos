use 5.008;

use strict;
use warnings;
use ExtUtils::MakeMaker;

BEGIN {
 eval { require Config };
 die 'OS unsupported' if $@;
 Config->import(qw/%Config/);
}

my @DEFINES;

my $pl = $Config{perl_patchlevel};
print "Checking perl patchlevel... ";
if (defined $pl && length $pl) {
 $pl = int $pl;
 push @DEFINES, '-DVMG_PERL_PATCHLEVEL=' . $pl;
 print $pl, "\n";
} else {
 $pl = undef;
 print "none\n";
}

my $is_as = 0;
print "Checking if this is ActiveState Perl 5.8.8 build 822 or higher... ";
if ($^V eq v5.8.8) {
 eval {
  require ActivePerl;
  1;
 } and do {
  if (defined &ActivePerl::BUILD) {
   my $build = int ActivePerl::BUILD();
   if ($build >= 822) {
    $is_as = 1;
    push @DEFINES, '-DVMG_COMPAT_ARRAY_PUSH_NOLEN=1';
   }
  }
 }
}
print $is_as ? "yes\n" : "no\n";

my $is_5110rel = 0;
print "Checking if this is a released perl 5.11.0 or higher... ";
if ($^V ge v5.11.0 and not defined $pl) {
 my $describe = $Config{git_describe};
 # An empty 'describe' is fine
 if (defined $describe and $describe !~ /^GitLive-/) {
  $is_5110rel = 1;
  push @DEFINES, '-DVMG_COMPAT_ARRAY_PUSH_NOLEN=0';
 }
}
print $is_5110rel ? "yes\n" : "no\n";

# Threads, Windows and 5.8.x don't seem to be best friends
if ($^O eq 'MSWin32' && $^V lt v5.9.0) {
 push @DEFINES, '-DVMG_MULTIPLICITY=0';
}

# Fork emulation got "fixed" in 5.10.1
if ($^O eq 'MSWin32' && $^V lt v5.10.1) {
 push @DEFINES, '-DVMG_FORKSAFE=0';
}

@DEFINES = (DEFINE => join ' ', @DEFINES) if @DEFINES;

my $dist = 'Variable-Magic';

(my $name = $dist) =~ s{-}{::}g;

(my $file = $dist) =~ s{-}{/}g;
$file = "lib/$file.pm";

my %PREREQ_PM = (
 'Carp'     => 0,
 'Exporter' => 0,
 'XSLoader' => 0,
 'base'     => 0,
);

my %META = (
 configure_requires => {
  'Config'              => 0,
  'ExtUtils::MakeMaker' => 0,
 },
 build_requires => {
  'Carp'                => 0,
  'Config'              => 0,
  'ExtUtils::MakeMaker' => 0,
  'Test::More'          => 0,
  %PREREQ_PM,
 },
 dynamic_config => 1,
 resources => {
  bugtracker => "http://rt.cpan.org/NoAuth/ReportBug.html?Queue=$dist",
  homepage   => "http://search.cpan.org/dist/$dist/",
  license    => 'http://dev.perl.org/licenses/',
  repository => "http://git.profvince.com/?p=perl%2Fmodules%2F$dist.git",
 },
);

WriteMakefile(
 NAME             => $name,
 AUTHOR           => 'Vincent Pit <perl@profvince.com>',
 LICENSE          => 'perl',
 VERSION_FROM     => $file,
 ABSTRACT_FROM    => $file,
 PL_FILES         => {},
 @DEFINES,
 PREREQ_PM        => \%PREREQ_PM,
 MIN_PERL_VERSION => 5.008,
 META_MERGE       => \%META,
 dist             => {
  PREOP    => "pod2text $file > \$(DISTVNAME)/README",
  COMPRESS => 'gzip -9f', SUFFIX => 'gz'
 },
 clean            => {
  FILES => "$dist-* *.gcov *.gcda *.gcno cover_db Debian_CPANTS.txt"
 },
);
