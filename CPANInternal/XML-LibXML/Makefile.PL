# -------------------------------------------------------------------------- #
# $Id: Makefile.PL,v 1.1.1.2 2007/10/10 23:04:13 ahuda Exp $
# -------------------------------------------------------------------------- #
# Makefile.PL for XML::LibXML.
# This file is required to generate a localized Makefile
# -------------------------------------------------------------------------- #

use ExtUtils::MakeMaker;
use Config;
use Symbol;
use File::Spec;

$|=0;
my %config;

# -------------------------------------------------------------------------- #
# -------------------------------------------------------------------------- #
# common information go to the top, so they are easier to find
# -------------------------------------------------------------------------- #
my %INFOS = (
    'NAME'	=> 'XML::LibXML',
    'VERSION_FROM' => 'LibXML.pm', # finds $VERSION
    'AUTHOR'    => 'Christian Glahn',
    'ABSTRACT'  => 'Interface to Gnome libxml2 xml parsing and DOM library',
    'PREREQ_PM' => { 'XML::SAX' => '0.11',
                     'XML::NamespaceSupport' => '1.07',
                     'XML::LibXML::Common' => '0.13',
                   },
    'OBJECT'    => '$(O_FILES)', # add the DOM extensions to libxml2
            );
# -------------------------------------------------------------------------- #
# -------------------------------------------------------------------------- #

# -------------------------------------------------------------------------- #
# libxml2 valid versions

# -------------------------------------------------------------------------- #

# -------------------------------------------------------------------------- #
# read extra configurations from the commandline
my %params;
@params{qw(DEBUG DEFINE EXTRALIBDIR GDOME INC LIBS SKIP_SAX_INSTALL XMLPREFIX)}=();

@ARGV = grep {
  my ($key, $val) = split(/=/, $_, 2);
  if (exists $params{$key}) {
    $config{$key} = $val; 0
  } else { 1 }
} @ARGV;

$extralibdir = $config{EXTRALIBDIR};
delete $config{EXTRALIBDIR};

# -------------------------------------------------------------------------- #

# -------------------------------------------------------------------------- #
# switch Debugging messages on
my $DEBUG = delete $config{DEBUG};

if ( $config{DEBUG} and $is_Win32 ) {
    warn "win32 compile\n";
}
# -------------------------------------------------------------------------- #

# -------------------------------------------------------------------------- #
# enable perls UTF8 support if available
if ( $] >= 5.006 ) {
    warn "enable native perl UTF8\n";
    $config{DEFINE} .= " -DHAVE_UTF8";
}
# -------------------------------------------------------------------------- #

# -------------------------------------------------------------------------- #
# get the libxml2 configuration
#
# For each release we already know which libxml2 versions work with the given
# module. All we need is to keep track of bad versions.
# If a user wants to build XML::LibXML with a newer version, there will be
# a warning, that errors are possible.
#
# We keep track of the valid versions by keeping a blacklist of intervals
# of working and not working versions where Ma.Mi.Pt <= X.Y.Z is of the same
# state.
#
# NOTE: All versions, the tests pass will be marked as working.
#

$skipsaxinstall = $ENV{SKIP_SAX_INSTALL} || $config{SKIP_SAX_INSTALL};
delete $config{SKIP_SAX_INSTALL};

unless ( $is_Win32 ) { # cannot get config in W32
    my @blacklist = (
		     # format X,Y,Z,is_ok, X,Y,Z is version,
                     # is_ok applies also to *preceding* versions
                     [2,4,22,0],
                     [2,4,25,0], # broken XPath
                     [2,4,28,0], # unsupported, may work fine with earlier XML::LibXML versions
                     [2,4,29,0], # broken
                     [2,4,30,0], # broken
                     [2,5,0,0], # unsupported
                     [2,5,1,0], # all pre 2.5.4 version have broken attr output
                     [2,5,5,0], # tests pass, but known as broken
                     [2,5,11,0], # will partially work
                     [2,6,0,0], # unsupported
                     [2,6,4,0], # schema error
                     [2,6,5,0], # broken xincludes
		     [2,6,15,0],
                   # [2,6,16,1], # first version to pass all tests
                     [2,6,18,1], # up to 2.6.18 all ok
                     [2,6,19,0], # broken c14n
                     [2,6,20,0], # broken schemas
                     [2,6,24,1], # all tests pass
                     [2,6,25,0], # broken XPath
                     [2,6,30,1], # tested, works ok
                    );
    my $xml2cfg = "xml2-config";
    my $libprefix = $ENV{XMLPREFIX} || $config{XMLPREFIX};

    delete $config{XMLPREFIX}; # delete if exists, otherwise MakeMaker gets confused

    if ( defined $libprefix ) {
        $xml2cfg = $libprefix . '/bin/' . $xml2cfg;
    }

    # if a user defined INC and LIBS on the command line we must not 
    # override them
    if ( not defined $config{LIBS} and not defined $config{INC} ) {
        print "running xml2-config...";
        eval {
            try_libconfig( $xml2cfg, \%config, @blacklist );
        };

        if ( $@ ) {
            if ( $@ =~ /^VERSION/ ) {
                die "The installed version of libxml2 not compatible with XML::LibXML.\n\n".
                    "You may continue at your own risk, but:\n\n".
                    "  - don't expect XML::LibXML to build or work correctly!\n".
                    "  - don't report errors!\n".
                    "  - don't send patches!\n\n".
                    "Check the README file for more information on versions\n".
                    "that are tested with XML::LibXML\n\n"; 
            }
            if ( $@ =~ /^UNTESTED (\S*)/ ) {
	      warn "Note: libxml2 $1 was not tested with this XML::LibXML version.\n"
#                 warn <<UNTESTED;
# WARNING!
# The installed version of libxml2 was not tested with this version of XML::LibXML.

#     XML::LibXML may fail building or some tests may not pass.
#     Expect strange errors and unstable scripts.

#     Check the README file for more informations
# END OF WARNING
# UNTESTED
            }

            if ( not defined $config{LIBS} and not defined $config{INC} ) {
                warn "\nusing fallback values for LIBS and INC\n";
                $config{LIBS} = '-L/usr/local/lib -L/usr/lib -lxml2 -lm';
                $config{INC} = '-I/usr/local/include -I/usr/include';
                print <<OPT;
options:
  LIBS='$config{LIBS}'
  INC='$config{INC}'
If this is wrong, Re-run as:
  \$ $^X Makefile.PL LIBS='-L/path/to/lib' INC='-I/path/to/include'

OPT
            }
        }
    }
}
# -------------------------------------------------------------------------- #

# -------------------------------------------------------------------------- #
# GDOME Support
#
# GDOME Support has to get explicitly activated by setting GDOME=1 as a config param.
#
unless ( $is_Win32 ) { # cannot get config in W32
    if ( $config{GDOME} ) {
        my $ver;
        my $state = undef; # there are three possible states:
                           # 1     : works
                           # 0     : works not
                           # undef : not yet tested
        my @blacklist = (
                         [0,7,2,0],
                         [0,7,3,1],
                        );
        print <<GDOME;

GDOME Support (experimental):
   XML::LibXML can parse into XML::GDOME DOMs if libgdome is installed.
   This feature is optional and is not required for using XML::LibXML.

GDOME

        print "running gdome-config...";

        eval {
            test_libconfig( "gdome-config", \%config, @blacklist );
            print "NOTE: You will need to install XML::GDOME to use this feature\n";
        };

        if ( $@ ) {
            if ( $@ =~ /^VERSION/ ) {
                warn "The installed libgdome version is not supported\n";
            }
            elsif ( $@ =~ /^UNTESTED/ ) {
                warn "The installed libgdome version was not yet tested with XML::LibXML.\n";
                print "NOTE: You will need to install XML::GDOME to use this feature\n";
            }
        }
    }
}
# -------------------------------------------------------------------------- #


# -------------------------------------------------------------------------- #
# fix the ld flags
# -------------------------------------------------------------------------- #
if ($config{LIBS} !~ /\-lxml2\b/) {
    # in this case we are not able to run xml2-config. therefore we need to
    # expand the libz as well.
    $config{LIBS} .= $is_Win32 ? ' -llibxml2 -lzlib' : ' -lxml2 -lz';
}

if ($config{LIBS} !~ /\-lz\b/) {
    # note if libxml2 has not -lz within its cflags, we should not use
    # it! We should trust libxml2 and assume libz is not available on the
    # current system (this is ofcourse not true with win32 systems.
    # $config{LIBS} .= $is_Win32 ? ' -lzlib' :' -lz';
    if ( $config{DEBUG} ) {
        warn "zlib was not configured\n";
        warn "set zlib\n" if $is_Win32;
    }
    $config{LIBS} .= $is_Win32 ? ' -lzlib' :'';
}

if ($config{LIBS} !~ /\-lm\b/) {
    # math support is important, but is not available separately in W32
    $config{LIBS} .= $is_Win32 ? '' :' -lm';
}
# -------------------------------------------------------------------------- #

# -------------------------------------------------------------------------- #
# MacOS X Compiler switches have to go here
#
# if we run on MacOSX, we should check if 10.2 is running and if so,
# if the Build Target is set correctly. Otherwise we have to set it by
# hand

# -------------------------------------------------------------------------- #

# -------------------------------------------------------------------------- #
# test if the libraries are really installed!
unless (have_library("xml2") or have_library("libxml2")) {
    die <<DEATH;
libxml2 not found
Try setting LIBS and INC values on the command line
Or get libxml2 from 
  http://xmlsoft.org/
If you install via RPMs, make sure you also install the -devel
RPMs, as this is where the headers (.h files) are.
DEATH
}
# -------------------------------------------------------------------------- #

# -------------------------------------------------------------------------- #
# NOTE This should be removed for a release, we should test here, if we are
# building from a CVS version
if ( -d './CVS' ) {
    print <<EOT;
If you are building XML::LibXML from CVS, you may wish to run

  \$ make docs

before doing anything else. This will re-build the documentation
from the XML file in examples/libxml.xml. This is *not* necessary
if you are building from a CPAN distribution.
EOT
}
# -------------------------------------------------------------------------- #

# -------------------------------------------------------------------------- #
# _NOW_ write the Makefile
WriteMakefile(
              %INFOS,
              %config,
);
# -------------------------------------------------------------------------- #


# -------------------------------------------------------------------------- #
# helper functions to build the Makefile
sub MY::manifypods {
    package MY;
    my $str = shift->SUPER::manifypods(@_);
#    warn $str;
#    $str =~ s/^manifypods : pure_all (.*)$/manifypods : pure_all docs $1/m;
    $str .= <<EOF;

docs : pure_all
\t\@$^X -Iblib/arch -Iblib/lib example/xmllibxmldocs.pl docs/libxml.dbk lib/XML/LibXML/
\t\@$^X -pi.old -e 's/a/a/' Makefile.PL
\t\@echo "==> YOU MUST NOW RE-RUN $^X Makefile.PL <=="
\t\@false

EOF
    return $str;
}

sub MY::install {
   package MY;
   my $script = shift->SUPER::install(@_);
   unless ( $::skipsaxinstall ) {
     $script =~ s/install :: (.*)$/install :: $1 install_sax_driver/m;
     $script .= <<"INSTALL";

install_sax_driver :
\t\@\$(PERL) -I\$(INSTALLSITELIB) -MXML::SAX -e "XML::SAX->add_parser(q(XML::LibXML::SAX::Parser))->save_parsers()"
\t\@\$(PERL) -I\$(INSTALLSITELIB) -MXML::SAX -e "XML::SAX->add_parser(q(XML::LibXML::SAX))->save_parsers()"

INSTALL
   } else {
     warn "Note: 'make install' will skip XML::LibXML::SAX registration with XML::SAX!\n";
   }
   return $script;
}

sub MY::test {
   package MY;
   my $script = shift->SUPER::test(@_);
   if ( $::extralibdir ) {
      $script =~ s/(\$\(TEST_VERBOSE\),)/$1 \'$::extralibdir\',/m;
   }
   return $script;
}

#	echo perl -pi~ -e '$$_=q($(version))."\n" if /#\ VERSION TEMPLATE/ ' $(TO_INST_PM)
sub MY::postamble {
  return <<'MAKE_FRAG';

# used to update version numbers in all modules
version:
	@version=`grep '\# VERSION TEMPLATE' $(VERSION_FROM)`; \
	echo "New version line: $$version"; \
	perl -pi~ -e '$$_=q('"$$version"')."\n" if /#\ VERSION TEMPLATE/ ' $(TO_INST_PM)

MAKE_FRAG
}


# -------------------------------------------------------------------------- #

# -------------------------------------------------------------------------- #
# Functions
#  - these should really be in MakeMaker... But &shrug;
# -------------------------------------------------------------------------- #

use Config;
use Cwd;
use Symbol;
use File::Spec;

use vars qw/$DEVNULL $is_Win32/;

BEGIN {
    $is_Win32 = ($^O =~ /Win32/);
    if ($is_Win32) {
        $DEVNULL = 'DEVNULL';
    }
    else {
        $DEVNULL = eval { File::Spec->devnull };
        if ($@) { $DEVNULL = '/dev/null' }
    }
}

sub rm_f {
    my @files = @_;
    my @realfiles;
    foreach (@files) {
        push @realfiles, glob($_);
    }
    if (@realfiles) {
        chmod(0777, @realfiles);
        unlink(@realfiles);
    }
}

sub rm_fr {
    my @files = @_;
    my @realfiles;
    foreach (@files) {
        push @realfiles, glob($_);
    }
    foreach my $file (@realfiles) {
        if (-d $file) {
            # warn("$file is a directory\n");
            rm_fr("$file/*");
            rm_fr("$file/.exists");
            rmdir($file) || die "Couldn't remove $file: $!";
        }
        else {
            # warn("removing $file\n");
            chmod(0777, $file);
            unlink($file);
        }
    }
}

sub xsystem {
    my $command = shift;
    if ($DEBUG) {
        print $command, "\n";
        if (system($command) != 0) {
            die "system call to '$command' failed";
        }
        return 1;
    }
    open(OLDOUT, ">&STDOUT");
    open(OLDERR, ">&STDERR");
    open(STDOUT, ">$DEVNULL");
    open(STDERR, ">$DEVNULL");
    my $retval = system($command);
    open(STDOUT, ">&OLDOUT");
    open(STDERR, ">&OLDERR");
    if ($retval != 0) {
        die "system call to '$command' failed";
    }
    return 1;
}

sub backtick {
    my $command = shift;
    if ($DEBUG) {
        print $command, "\n";
        my $results = `$command`;
        chomp $results;
        if ($? != 0) {
            die "backticks call to '$command' failed";
        }
        return $results;
    }
    open(OLDOUT, ">&STDOUT");
    open(OLDERR, ">&STDERR");
    open(STDOUT, ">$DEVNULL");
    open(STDERR, ">$DEVNULL");
    my $results = `$command`;
    my $retval = $?;
    open(STDOUT, ">&OLDOUT");
    open(STDERR, ">&OLDERR");
    if ($retval != 0) {
        die "backticks call to '$command' failed";
    }
    chomp $results;
    return $results;
}

sub try_link0 {
    my ($src, $opt) = @_;
    my $cfile = gensym();
    # local $config{LIBS};
    # $config{LIBS} .= $opt;
    unless (mkdir(".testlink", 0777)) {
        rm_fr(".testlink");
        mkdir(".testlink", 0777) || die "Cannot create .testlink dir: $!";
    }
    chdir(".testlink");
    open($cfile, ">Conftest.xs") || die "Cannot write to file Conftest.xs: $!";
print $cfile <<EOT;
#ifdef __cplusplus
extern "C" {
#endif
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#ifdef __cplusplus
}
#endif

EOT
    print $cfile $src;
    print $cfile <<EOT;

MODULE = Conftest          PACKAGE = Conftest

PROTOTYPES: DISABLE

EOT
    close($cfile);
    open($cfile, ">Conftest.pm") || die "Cannot write to file Conftest.pm: $!";
    print $cfile <<'EOT';
package Conftest;
$VERSION = 1.0;
require DynaLoader;
@ISA = ('DynaLoader');
bootstrap Conftest $VERSION;
1;
EOT
    close($cfile);
    open($cfile, ">Makefile.PL") || die "Cannot write to file Makefile.PL: $!";
    print $cfile <<'EOT';
use ExtUtils::MakeMaker;
my %config;
while($_ = shift @ARGV) {
    my ($k, $v) = split /=/, $_, 2;
    warn("$k = $v\n");
    $config{$k} = $v;
}
WriteMakefile(NAME => "Conftest", VERSION_FROM => "Conftest.pm", %config);
EOT
    close($cfile);
    open($cfile, ">test.pl") || die "Cannot write to file test.pl: $!";
    print $cfile <<EOT;
use Test; BEGIN { plan tests => 1; } END { ok(\$loaded) }
use Conftest; \$loaded++;
EOT
    close($cfile);
    my $quote = $is_Win32 ? '"' : "'";
    xsystem("$^X Makefile.PL " . join(' ', map { "${quote}$_=$config{$_}${quote}" } keys %config));

    # I am not shure if OTHERLDFLAGS is really required - at least the
    # libraries to include do not belong here!
    # I would assume a user to set OTHERLDFLAGS in the %config if they are
    # really required. if done so, we don't have to pass them here ...
    xsystem("$Config{make} test ${quote}OTHERLDFLAGS=${opt}${quote}");
} # end try_link0

sub try_link {
    my $start_dir = cwd();
    my $result = eval {
        try_link0(@_);
    };
    warn $@ if $DEBUG && $@;
    chdir($start_dir);
    rm_fr(".testlink");
    return $result;
}

sub have_library {
    my ($lib, $func) = (@_, "blank");
    printf("checking for %s() in -l%s... ", $func, $lib) if $func ne "blank";
    printf("looking for -l%s... ", $lib) if $func eq "blank";

    my $result;
    if ($func) {
        my $libs = $is_Win32 ? " $lib.lib  " : "-l$lib";
#        my $libs = "-l$lib";
        if ($is_Win32) {
            $result = try_link(<<"SRC", undef);
#include <windows.h>
#include <winsock.h>
blank() { return 0; }
int t() { ${func}(); return 0; }
SRC
            unless ($result) {
                $result = try_link(<<"SRC", undef);
#include <windows.h>
#include <winsock.h>
blank() { return 0; }
int t() { void ((*p)()); p = (void ((*)()))${func}; return 0; }
SRC
            }
        }
        else {
            $result = try_link(<<"SRC", undef);
blank() { return 0; }
int t() { ${func}(); return 0; }
SRC
        }
    }

    unless ($result) {
        print "no\n";
        return 0;
    }

    if ($func ne "main") {
        $config{DEFINE} .= uc(" -Dhave_$func");
    }

    print "yes\n";
    return 1;
}

# -------------------------------------------------------------------------- #
# try_libconfig class a generic config file and requests --version, --libs and
# --cflags
sub try_libconfig {
    my $cfgscript = shift;
    my $config = shift;
    my @bl = @_;

    my $state = undef; # there are three possible states:
                       # 1     : works
                       # 0     : works not
                       # undef : not yet tested

    my $ver = backtick("$cfgscript --version");
    if ( defined $ver ) {
        my ( $major, $minor, $point) = $ver =~ /(\d+).(\d+)\.(\d+)/g;
        foreach ( @bl ) {
            $state = $_->[3];
            last if $major <  $_->[0];
            next if $major >  $_->[0];
            last if $minor <  $_->[1];
            next if $minor >  $_->[1];
            last if $point <= $_->[2];
            $state = undef;
        }
        if ( defined $state and $state == 0 ) {
            print "failed\n";
            die "VERSION $ver\n";
        }

        $config->{LIBS} = backtick("$cfgscript --libs");
        $config->{INC}  = backtick("$cfgscript --cflags");

        unless ( defined $state ) {
            print "untested\n";
            die "UNTESTED $ver\n";
        }

        print "ok ($ver)\n";
    }
    else {
        print "failed\n";
        die "FAILED\n"; # strange error
    }
}
# -------------------------------------------------------------------------- #
