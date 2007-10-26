######################### We start with some black magic to print on failure.

use strict;
use Test;

print("1..0 # Skipped: no compiler found\n"), exit(0) unless have_compiler();
plan tests => 11;

use Config;
use Module::Build;
use File::Spec;
ok(1);

require File::Spec->catfile('t', 'common.pl');

######################### End of black magic.

# Pretend we're in the t/XSTest/ subdirectory
my $build_dir = File::Spec->catdir('t','XSTest');
chdir $build_dir or die "Can't change to $build_dir : $!";

my $m = new Module::Build
  (
   module_name => 'XSTest',
   dist_author => 'XSTest Author',
  );
ok(1);

eval {$m->dispatch('clean')};
ok $@, '';

eval {$m->dispatch('build')};
ok $@, '';

{
  # Try again in a subprocess 
  eval {$m->dispatch('clean')};
  ok $@, '';

  $m->create_build_script;
  ok -e 'Build';
  
  eval {$m->run_perl_script('Build')};
  ok $@, '';
}

# We can't be verbose in the sub-test, because Test::Harness will
# think that the output is for the top-level test.
eval {$m->dispatch('test')};
ok $@, '';

{
  $m->dispatch('ppd', args => {codebase => '/path/to/codebase-xs'});

  my $ppd = slurp('XSTest.ppd');

  my $perl_version = Module::Build::PPMMaker->_ppd_version($m->perl_version);

  # This test is quite a hack since with XML you don't really want to
  # do a strict string comparison, but absent an XML parser it's the
  # best we can do.
  ok $ppd, <<"EOF";
<SOFTPKG NAME="XSTest" VERSION="0,01,0,0">
    <TITLE>XSTest</TITLE>
    <ABSTRACT>Perl extension for blah blah blah</ABSTRACT>
    <AUTHOR>XSTest Author</AUTHOR>
    <IMPLEMENTATION>
        <PERLCORE VERSION="$perl_version" />
        <OS VALUE="$^O" />
        <ARCHITECTURE NAME="$Config{archname}" />
        <CODEBASE HREF="/path/to/codebase-xs" />
    </IMPLEMENTATION>
</SOFTPKG>
EOF
}

eval {$m->dispatch('realclean')};
ok $@, '';

# Make sure blib/ is gone after 'realclean'
ok not -e 'blib';

#################################################################
# Routines below were taken from ExtUtils::ParseXS

use Config;
sub have_compiler {
  my %things;
  foreach (qw(cc ld)) {
    return 0 unless $Config{$_};
    my $thing = (File::Spec->file_name_is_absolute($Config{cc}) ?
		 $Config{cc} :
		 find_in_path($Config{cc}));
    return 0 unless $thing;
    return 0 unless -x $thing;
  }
  return 1;
}

sub find_in_path {
  my $thing = shift;
  $thing = (Module::Build->split_like_shell($thing))[0]; # It may be something like 'ccache gcc'
  
  my @path = split $Config{path_sep}, $ENV{PATH};
  my @exe_ext = $^O eq 'MSWin32' ?
    split($Config{path_sep}, $ENV{PATHEXT} || '.com;.exe;.bat') :
    ('');
  foreach (@path) {
    my $fullpath = File::Spec->catfile($_, $thing);
    foreach my $ext ( @exe_ext ) {
      return "$fullpath$ext" if -e "$fullpath$ext";
    }
  }
  return;
}
