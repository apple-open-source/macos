#!/usr/bin/perl -w                                         # -*- perl -*-

use strict;
use warnings;

use 5.006;
use lib qw( ./lib );
use Config;
use File::Spec::Functions qw( catfile );
use Template;
use ExtUtils::MakeMaker;
use Cwd;

select STDERR;
$| = 1;
select STDOUT;

use vars qw( $TT_VERSION $TT_PREFIX
             $TT_XS_ENABLE $TT_XS_DEFAULT
             $TT_QUIET $TT_ACCEPT $TT_YES );

# check O/S to set sensible defaults

my ($WIN32, $FLAVOUR, $PREFIX, $IMAGES, $MAKE);
if ($^O eq 'MSWin32') {  # any others also?
    $WIN32   = 1;
    $FLAVOUR = 'Win32';
    $PREFIX  = 'C:/Program Files/Template Toolkit 2';
    $IMAGES  = '/tt2/images';
}
else {
    $WIN32   = 0;
    $FLAVOUR = 'Unix';
    $PREFIX  = '/usr/local/tt2';
    $IMAGES  = '/tt2/images';
}
$MAKE=$Config{'make'};


# read command line args putting TT_* into $ttconfig and
# everything else (regular Makefile.PL args, e.g. PREFIX)
# goes into $config

my (%config, %ttconfig);
while ($_ = shift) {
    my ($k, $v) = split(/=/, $_, 2);
    if ($k =~ /^TT/) {
        $ttconfig{ $k } = $v || 0;
    }
    else {
        $config{ $k } = $v || 0;
    }
};


# print help if they asked for it

if (exists $ttconfig{ TT_HELP }) {
    print <<EOF;
The following options can be specified as command line
arguments to 'perl Makefile.PL'.  e.g.

  perl Makefile.PL TT_XS_DEFAULT=y TT_ACCEPT=y

  TT_XS_ENABLE   Enable XS Stash         (y)
  TT_XS_DEFAULT  Use XS Stash by default (y)
  TT_QUIET       no messages             (n)
  TT_ACCEPT      accept defaults         (n)

By default, the Makefile.PL runs in interactive mode,
prompting for confirmation of the various configuration
options.  Setting the TT_ACCEPT option causes the default
value (possibly modified by other command line options)
to be accepted.  The TT_QUIET option can also be set to
suppress the prompt messages.

EOF
    exit(0);
}

# these global package variables are the main flags used
# in this script, here defaulted to sensible values

$TT_VERSION       = $Template::VERSION;
$TT_XS_ENABLE     = 'y';
$TT_XS_DEFAULT    = 'y';
$TT_QUIET         = 'n';
$TT_ACCEPT        = 'n';

my $DEFAULTS_FILE   = '.defaults.cfg';
my $DEFAULTS = '';

if (-f $DEFAULTS_FILE) {
    require $DEFAULTS_FILE;
    $DEFAULTS = " read from '$DEFAULTS_FILE'";
}

$TT_XS_ENABLE     = $ttconfig{ TT_XS_ENABLE  } if defined $ttconfig{ TT_XS_ENABLE  };
$TT_XS_DEFAULT    = $ttconfig{ TT_XS_DEFAULT } if defined $ttconfig{ TT_XS_DEFAULT };
$TT_QUIET         = $ttconfig{ TT_QUIET      } if defined $ttconfig{ TT_QUIET      };

if (defined $ttconfig{ TT_ACCEPT }) {
    $TT_ACCEPT = $ttconfig{ TT_ACCEPT };
}
else {
    # standard behaviour for MakeMaker to indicate accept all defaults
    $TT_ACCEPT = $ENV{PERL_MM_USE_DEFAULT} ? 'y' : 'n';
}

foreach ($TT_XS_ENABLE, $TT_XS_DEFAULT ) {
    $_ = 'n' if ! $_;
}
$TT_ACCEPT = 0 if $TT_ACCEPT eq 'n';
$TT_QUIET  = 0 if $TT_QUIET eq 'n';
$TT_QUIET  = 0 unless $TT_ACCEPT;

# define version numbers of required modules
my $TT_APPCONFIG_VERSION = '1.56';
my $TT_FILE_SPEC_VERSION = '0.8';
my $TT_FILE_TEMP_VERSION = '0.12';


#========================================================================

welcome_message();
version_check();
mandatory_modules();
optional_stash_xs();
write_defaults();

print "\n";


#------------------------------------------------------------------------
# build options and write Makefile
#------------------------------------------------------------------------

package main;

my %opts = (
    %config,
    'NAME'             => 'Template',
    'DISTNAME'     => 'Template-Toolkit',
    'VERSION_FROM' => 'lib/Template.pm',
    'EXE_FILES'    => [ 'bin/tpage', 'bin/ttree' ],
    'PMLIBDIRS'    => [ 'lib' ],
    'DIR'          => [ ],
    'PREREQ_PM'    => {
        'AppConfig'    => $TT_APPCONFIG_VERSION,
        'File::Spec'   => $TT_FILE_SPEC_VERSION,
        'File::Temp'   => $TT_FILE_TEMP_VERSION,
        'Scalar::Util' => 0,
    },
    'dist'         => {
        'COMPRESS' => 'gzip',
        'SUFFIX'   => 'gz',
    },
    'test'         => {
        'TESTS'    => join(' ', map { glob } qw( t/*.t t/vmethods/*.t )),
    },
    'clean'        => {
        'FILES'        => join(' ', qw( docs/ttree.cfg
                                        examples/ttree.cfg
                                        t/dbi_test.cfg
                                        t/test/src/baz.ttc
                                        t/test/src/complex.org
                                        t/test/src/complex.ttc
                                        t/test/src/evalperl.ttc
                                        t/test/src/foo.ttc )),
    },
);

push @{ $opts{'DIR'} }, 'xs' if $TT_XS_ENABLE;

# Handle dev versions in our check
my $mmv = $ExtUtils::MakeMaker::VERSION;
$mmv =~ s/\_.+//;

if ($mmv >= 5.43) {
    $opts{ AUTHOR   } = 'Andy Wardley <abw@wardley.org>';
    $opts{ ABSTRACT } = 'comprehensive template processing system',
}

if ($ExtUtils::MakeMaker::VERSION ge '6.30_00') {
    $opts{'LICENSE' } = 'perl';
}

WriteMakefile( %opts );

    print <<EOF;

Configuration complete.  You should now run '$MAKE', '$MAKE test' and
then '$MAKE install'.   See the README file for further information.
EOF


#========================================================================



#------------------------------------------------------------------------
# welcome_message()
#
# Print opening banner.
#------------------------------------------------------------------------

sub welcome_message {
    print(<<EOF);

                    Template Toolkit Version $TT_VERSION
                    =============================

Using $FLAVOUR defaults$DEFAULTS.
Run 'perl Makefile.PL TT_HELP' for a summary of options.
EOF
    print "Messages suppressed (TT_QUIET).  " if $TT_QUIET;
    print "Accepting defaults automatically (TT_ACCEPT)." if $TT_ACCEPT;
}



#------------------------------------------------------------------------
# version_check()
#
# Check for pre-version 2.00 installation and issue warning
#------------------------------------------------------------------------

sub version_check {
    eval "use Template";
    unless ($@ or $Template::VERSION =~ /^2/) {
        warn(<<EOF) unless $TT_QUIET;

IMPORTANT NOTE:

    You have version $Template::VERSION of the Template Toolkit installed.

    There are some minor incompatabilities between version 1 and 2
    of the Template Toolkit which you should be aware of.  Installing
    this version will overwrite your version $Template::VERSION files
    unless you take measures to install one or the other version in a
    different location (i.e. perl Makefile.PL PREFIX=/other/path).

    Please consult the README and Changes file for further details.
    Most of the changes are in the more obscure features and
    directives so hopefully you will find the upgrade process fairly
    painless.  If you're feeling brave, then answer 'y', otherwise 'n'.

EOF
        exit unless ttprompt("Do you want to continue?", 'y') =~ /y/i;
    }
}


#------------------------------------------------------------------------
# mandatory_modules()
#
# Detect mandatory module
#------------------------------------------------------------------------

sub mandatory_modules {
    eval "use AppConfig";
    if ($@ or $AppConfig::VERSION < $TT_APPCONFIG_VERSION) {
        warn(<<EOF);

The Template Toolkit requires that the AppConfig module (version $TT_APPCONFIG_VERSION
or later) first be installed.  This is used by
the 'ttree' program for reading command line options and configuration
files.  It is available from CPAN:

    http://www.cpan.org/authors/Andy_Wardley/

EOF
    }

    eval "use File::Spec";
    if ($@ or $File::Spec::VERSION < $TT_FILE_SPEC_VERSION) {
        warn(<<EOF);

The Template Toolkit requires that the File::Spec module (version $TT_FILE_SPEC_VERSION
or later) first be installed.  This is used by the File plugin.  It is
available from CPAN:

    http://search.cpan.org/search?dist=File-Spec

EOF
    }

    eval "use File::Temp";
    if ($@ or $File::Temp::VERSION < $TT_FILE_TEMP_VERSION) {
        warn(<<EOF);

The Template Toolkit requires that the File::Temp module (version $TT_FILE_TEMP_VERSION
or later) first be installed.  This is used by the Template::Document
class for storing compiled templates.  It is available from CPAN:

    http://search.cpan.org/search?dist=File-Temp

EOF
    }
}


#------------------------------------------------------------------------
# optional_stash_xs()
#
# Prompt for installation and default use of XS Stash.
#------------------------------------------------------------------------

sub optional_stash_xs {
#    return if $TT_ACCEPT && (! $TT_XS_ENABLE || $TT_XS_ENABLE eq 'n');

    message(<<EOF);


Template::Stash::XS
-------------------

The Template::Stash module is a core part of the Template Toolkit,
implementing the magic for accessing data using the dot notation.

There is a high speed version, Template::Stash::XS, written in C.
This makes the Template Toolkit run about twice as fast as when using
the regular Template::Stash written in Perl.  If you've got a C
compiler on your system then you can elect to have the XS Stash built.
You can also specify that you want to use the XS Stash by default.

Note that as of version 2.15 the XS Stash now supports access to tied
hashes and arrays.

See 'perldoc Template::Config' for further details.

EOF

    $TT_XS_ENABLE = (ttprompt('Do you want to build the XS Stash module?',
                              $TT_XS_ENABLE) =~ /^y/i);

    if ($TT_XS_ENABLE) {
        $TT_XS_DEFAULT =
            (ttprompt('Do you want to use the XS Stash by default?',
                      $TT_XS_DEFAULT) =~ /^y/i);
    }
    else {
        # If the XS stash is disabled, we cannot use it as the default stash.
        $TT_XS_DEFAULT = 0;
    }

    # Actually, we would have to fix 'Config.pm' only if the XS stash is
    # disabled. But this way, we are sure the correct module is used.
        fix_file(catfile('lib','Template','Config.pm'),
                 '$STASH',
                 $TT_XS_DEFAULT ? 'Template::Stash::XS' : 'Template::Stash');
}





#--------------------------------------------------------------------
# write_defaults()
#
# write configuration defaults to file
#--------------------------------------------------------------------

sub write_defaults {
    open(FP, "> $DEFAULTS_FILE") || die "$DEFAULTS_FILE: $!\n";
    my ( $ttxs_enable, $ttxs_default )
            = map { $_ ? 'y' : 'n' }
            ( $TT_XS_ENABLE, $TT_XS_DEFAULT );
    print FP <<EOF;
\$TT_XS_ENABLE     = '$ttxs_enable';
\$TT_XS_DEFAULT    = '$ttxs_default';
\$TT_ACCEPT        = '$TT_ACCEPT';
\$TT_QUIET         = '$TT_QUIET';
1;
EOF
    close(FP);
}




#------------------------------------------------------------------------
# fix_file($file, $find, $fix)
#
# Fixes a variable definition in a file.  e.g.
# fix_file('templates/splash/config', 'images', '/tt2/splash')
#------------------------------------------------------------------------

sub fix_file {
    my ($file, $find, $fix) = @_;
    local *FP;
    local $/ = undef;

    $find = quotemeta($find);

    open(FP, "< $file") || die "$file: $!\n";
    my $text = <FP>;
    close(FP);

    ($text =~ s/^(\s*${find}\s*=\s*)'.*?'/$1'$fix'/m)
        || die "$find not found in $file\n";

    open(FP, "> $file") || die "$file: $!\n";
    print FP $text;
    close(FP);
}


#------------------------------------------------------------------------
# find_program($path, $prog)
#
# Find a program, $prog, by traversing the given directory path, $path.
# Returns full path if the program is found.
#
# Written by Craig Barratt, Richard Tietjen add fixes for Win32.
#
# abw changed name from studly caps findProgram() to find_program() :-)
#------------------------------------------------------------------------

sub find_program {
    my($path, $prog) = @_;
#     my $sep = $WIN32 ? qr/;/ : qr/:/;
#     foreach my $dir ( split($sep, $path) ) {
    foreach my $dir ( split($Config{path_sep}, $path) ) {
        my $file = File::Spec->catfile($dir, $prog);
        if ( !$WIN32 ) {
            return $file if ( -x $file );
        } else {
            # Windows executables end in .xxx, exe precedes .bat and .cmd
            foreach my $dx ( qw/exe bat cmd/ ) {
                return "$file.$dx" if ( -x "$file.$dx" );
            }
        }
    }
}


#------------------------------------------------------------------------
# message($text)
#
# Print message unless quiet mode.
#------------------------------------------------------------------------

sub message {
    return if $TT_QUIET;
    print @_;
}


#------------------------------------------------------------------------
# ttprompt($message, $default)
#------------------------------------------------------------------------

sub ttprompt {
    my ($msg, $def)=@_;
    my $ISA_TTY = -t STDIN && (-t STDOUT || !(-f STDOUT || -c STDOUT)) ; # Pipe?
    my $dispdef = defined $def ? "[$def] " : " ";
    $def = defined $def ? $def : "";
    my $ans = '';
    local $|=1;
    print "$msg $dispdef" unless $TT_QUIET;
    if ($TT_ACCEPT || ! $ISA_TTY) {
        print "$def\n" unless $TT_QUIET;
    }
    else {
        chomp($ans = <STDIN>);
    }
    return ($ans ne '') ? $ans : $def;
}


#------------------------------------------------------------------------
# yep($text)
#------------------------------------------------------------------------

sub yep {
    return if $TT_QUIET;
    print '[X] ', shift, "\n";
}


#------------------------------------------------------------------------
# nope($text)
#------------------------------------------------------------------------
sub nope {
    return if $TT_QUIET;
    print '[ ] ', shift, "\n";
}
