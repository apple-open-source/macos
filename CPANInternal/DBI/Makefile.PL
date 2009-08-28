# -*- perl -*-
#
# $Id: Makefile.PL 10473 2008-01-04 15:41:49Z timbo $
#
#  Copyright (c) 1994-2006  Tim Bunce  Ireland
#
# See COPYRIGHT section in DBI.pm for usage and distribution rights.

BEGIN {
    require 5.006_00;
    unshift @INC, "lib";
}

use ExtUtils::MakeMaker 5.16, qw(WriteMakefile $Verbose prompt);
use Getopt::Long;
use Config;
use File::Find;
use File::Spec;
use strict;

use DBI::DBD;

$| = 1;
$^W = 1;
my $os = $^O;
my $osvers = $Config{osvers};
$osvers =~ s/^\s*(\d+\.\d+).*/$1/;  # drop sub-sub-version: 2.5.1 -> 2.5
my $ext_pl = $^O eq 'VMS' ? '.pl' : '';
my $is_developer = (-d ".svn" && -f "MANIFEST.SKIP");

$::opt_v = 0;
$::opt_thread = 1;	# thread if we can, use "-nothread" to disable
$::opt_g = 0;
$::opt_g = 1 if $is_developer && $ENV{LOGNAME} && $ENV{LOGNAME} eq 'timbo'; # it's me! (probably)

GetOptions(qw(v! g! thread!))
	or die "Invalid arguments\n";

$::opt_g &&= '-g';  # convert to actual string

if ($] < 5.008000) {
	warn qq{\a
**************************************************************************
  Perl versions below 5.6.1 are no longer supported by the DBI.
  Perl versions 5.6.x may fail during installation with a complaint
  about the use of =head3 in the pod documentation.
**************************************************************************
};
    prompt("Press return to continue...");
}


if (($ENV{LANG}||'') =~ m/utf-?8/i) {
    print "\n";
    print "*** Your LANG environment variable is set to '$ENV{LANG}'\a\n";
    print "*** This is known to cause problems for some perl installations.\n";
    print "*** If you get test failures, please try again with LANG unset.\n";
    print "*** Please also email dbi-dev\@perl.org and include the output of 'perl -V'\n";
    print "\a\n";
    sleep 4;
}

if ($Config{useithreads}) {
    print "\n";
    print "*** You are using a perl configured with threading enabled.\a\n";
    print "*** You should be aware that using multiple threads is\n";
    print "*** not recommended for production environments.\n";
    print "\n";
    sleep 4;
    $::opt_thread = 1;
}
else {

    if ($Config{archname} =~ /thread/ && $::opt_thread) {
	# oh dear... tell it like it is:
	print "\n";
	print "DBI versions from 1.29 onwards no longer support the old style\n";
	print "of perl threading (now known as '5005 threads'). It is badly flawed\n";
	print "and could never be safe to use in production environments.\n\n";
	print "If you are using multiple threads you are *strongly* encouraged to\n";
	print "upgrade to perl 5.8.2 or later.\n";
	print "If you are not using multiple threads you are *strongly* encouraged to\n";
	print "upgrade to at least 5.6.1 (preferably perl 5.8.2 or later.)\n";
	print "or at the very least rebuild this version with threading disabled.\n";
	print "If you have stick with your current build of perl...\n";
	print "then you also have to stick with DBI 1.28 for safety.\n";
	print "Or if *desparate* you may be able to build this DBI using 'perl Makefile.PL -nothread' but\n";
	print "but but, that will have *no* logic to handle threads because the logic\n";
	print "that was there for 5005 threads has now been removed! You have been warned.\n";
	die "*** ABORTED.\n";
    }

    $::opt_thread = 0;
}


my @missing;

eval "use RPC::PlServer 0.2001 ();";
if ($@) {
    push @missing, 'RPC::PlServer';
    print "\a",<<'MSG';
*** Note:
    The optional PlRPC-modules (RPC::PlServer etc) are not installed.
    If you want to use the DBD::Proxy driver and DBI::ProxyServer
    modules, then you'll need to install the RPC::PlServer, RPC::PlClient,
    Storable and Net::Daemon modules. The CPAN Bundle::DBI may help you.
    You can install them any time after installing the DBI.
    You do *not* need these modules for typical DBI usage.

MSG
    sleep 1;
}

if (@missing) {
    print <<'MSG';
Optional modules are available from any CPAN mirror, in particular
    http://search.cpan.org/
    http://www.perl.com/CPAN/modules/by-module
    http://www.perl.org/CPAN/modules/by-module
    ftp://ftp.funet.fi/pub/languages/perl/CPAN/modules/by-module

MSG
    sleep 1;
}

 
my %opts = (
    NAME   => 'DBI',
    AUTHOR => 'Tim Bunce (dbi-users@perl.org)',
    VERSION_FROM  => 'DBI.pm',
    ABSTRACT_FROM => 'DBI.pm',
    PREREQ_PM => {
        "Test::Simple" => 0.40,
        "Storable" => 1,
        "File::Spec" => 0,
        "Scalar::Util" => 0,
    },
    EXE_FILES => [ "dbiproxy$ext_pl", "dbiprof$ext_pl", "dbilogstrip$ext_pl" ],
    DIR => [ ],
    dynamic_lib => { OTHERLDFLAGS => "$::opt_g" },
    clean => { FILES=> "\$(DISTVNAME) Perl.xsi t/zv*_*.t dbi__null_test_tmp*"
                ." dbiproxy$ext_pl dbiprof$ext_pl dbilogstrip$ext_pl dbiproxy.*log dbitrace.log dbi.prof ndtest.prt" },
    dist  => {
	DIST_DEFAULT=> 'clean distcheck disttest tardist',
	PREOP => '$(MAKE) -f Makefile.old distdir',
	COMPRESS => 'gzip -v9', SUFFIX => 'gz',
    },
);
$opts{CAPI} = 'TRUE' if $Config{archname} =~ /-object\b/i;

if (my $gccversion = $Config{gccversion}) {	# ask gcc to be more pedantic
    warn "WARNING: Your GNU C $gccversion compiler is very old. Please upgrade it and rebuild perl.\n"
	if $gccversion =~ m/^\D*(1|2\.[1-8])/;
    print "Your perl was compiled with gcc (version $Config{gccversion}), okay.\n";
    $gccversion =~ s/[^\d\.]//g; # just a number please
    $opts{DEFINE} .= ' -W -Wall -Wpointer-arith -Wbad-function-cast';
    $opts{DEFINE} .= ' -Wno-comment -Wno-sign-compare -Wno-cast-qual';
    $opts{DEFINE} .= ' -Wmissing-noreturn -Wno-unused-parameter' if $gccversion ge "3.0";
    if ($is_developer && $::opt_g) {
        $opts{DEFINE} .= ' -DPERL_GCC_PEDANTIC -ansi -pedantic' if $gccversion ge "3.0";
        $opts{DEFINE} .= ' -Wdisabled-optimization -Wformat'    if $gccversion ge "3.0";
        $opts{DEFINE} .= ' -Wmissing-prototypes';
    }
}

$opts{DEFINE} .= ' -DDBI_NO_THREADS' unless $::opt_thread;

# HP-UX 9 cannot link a non-PIC object file into a shared library.
# Since the # .a libs that Oracle supplies contain non-PIC object
# files, we sadly have to build static on HP-UX 9 :(
if ($os eq 'hpux' and $osvers < 10) {
    $opts{LINKTYPE} = 'static';
    print "Warning: Forced to build static not dynamic on $os $osvers.\a\n";
    print "** Note: DBI will be built *into* a NEW perl binary. You MUST use that new perl.\n";
    print "         See README and Makefile.PL for more information.\a\n";
}

if ($os eq 'MSWin32'  &&  $Config{libs} =~ /\bPerlCRT.lib\b/
    &&  -f "$Config{archlib}/CORE/PerlCRT.lib") {
    # ActiveState Perl needs this; should better be done in MakeMaker, but
    # as a temporary workaround it seems ok.
    $opts{LIBS} = "-L$Config{archlib}/CORE";
}

# Set aside some values for post_initialize() in package MY
my ( $cfg_privlibexp, $cfg_archlibexp, $cfg_sitelibexp, $cfg_sitearchexp,
     $cfg_man3direxp ) =
     @Config{qw( privlibexp archlibexp sitelibexp sitearchexp man3direxp ) };
for ( $cfg_privlibexp, $cfg_archlibexp, $cfg_sitelibexp, $cfg_sitearchexp,
     $cfg_man3direxp ) {
     $_ = '' unless defined $_;
}


$Verbose = $::opt_v;
WriteMakefile(
    dbd_edit_mm_attribs(\%opts, {
	create_pp_tests => 1,
    })
);
# WriteMakefile call is last thing executed
# so return value is propagated


# =====================================================================

package MY;

sub postamble {
warn <<EOT;

    I see you're using perl $] on $Config::Config{archname}, okay.
    Remember to actually *read* the README file!
    Use  'make' to build the software (dmake or nmake on Windows).
    Then 'make test' to execute self tests.
    Then 'make install' to install the DBI and then delete this working
    directory before unpacking and building any DBD::* drivers.

EOT
warn <<EOT if $os eq 'MSWin32';
    Windows users need to use the correct make command.
    That may be nmake or dmake depending on which Perl you are using.
    If using the Win32 ActiveState build then it is recommended that you
    use the ppm utility to fetch and install a prebuilt DBI instead.

EOT
    return "";
}

sub libscan {
    my($self, $path) = @_;
    ($path =~ /\~$|\B\.svn\b/) ? undef : $path;
}

sub const_cccmd {
    my $self = shift;
    local($_) = $self->SUPER::const_cccmd(@_);
    # If perl Makefile.PL *-g* then switch on debugging
    if ($::opt_g) {
	s/\s-O\d?\b//;      # delete optimise option
	s/\s-/ -g -/;       # add -g option
    }
    $_;
}


sub post_initialize {
    my($self) = shift;

    if ($cfg_privlibexp ne $cfg_sitelibexp) {
	# this block could probably be removed now
	my %old;
	File::Find::find( sub {
	    local $_ = $File::Find::name;
	    s:\\:/:g if $os eq 'MSWin32';
	    $File::Find::prune = 1, return
		if -d $_ && ( $_ eq $cfg_sitelibexp ||
			      $_ eq $cfg_sitearchexp ||
			      $_ eq $cfg_man3direxp );
	    ++$old{$_} if m:\bDB(I|D$):; # DBI files, but just DBD dirs
	}, $cfg_privlibexp, $cfg_archlibexp );
	if ( %old ) {
	warn "
Warning: By default new modules are installed into your 'site_lib'
         directories. Since site_lib directories come after the normal library
         directories you must delete old DBI files and directories from your
         'privlib' and 'archlib' directories and their auto subdirectories.

Reinstall DBI and your DBD::* drivers after deleting the old directories.

Here's a list of probable old files and directories:

 " . join( "\n ", ( sort keys %old ), "\n" );
	}
    }

    # install files that DBD's may need
    File::Find::find( sub {

	# may be '.' or '[]' depending on File::Find version
	$_ = '.' if $^O eq 'VMS' && $_ eq File::Spec->curdir;

	$File::Find::prune = 1, return if -d $_ && '.' ne $_;
	$self->{PM}->{$_} = File::Spec->catfile($self->{INST_ARCHAUTODIR}, $_)
	    if '.h' eq substr( $_, -2 ) || '.xst' eq substr( $_, -4 );
        }, '.' );

    return '';
}


sub post_constants {
    my($self) = shift;

    # ensure that Driver.xst and related code gets tested
    my $xst = main::dbd_postamble();
    $xst =~ s/\$\(BASEEXT\)/Perl/g;
    $xst .= '
dbixs_rev.h: DBIXS.h Driver_xst.h dbipport.h dbivport.h dbixs_rev.pl
	$(PERL) dbixs_rev.pl

DBI.c: Perl$(OBJ_EXT)

# make Changes file available as installed pod docs "perldoc DBI::Changes"
# make Roadmap.pod file available as installed pod docs "perldoc DBI::Roadmap"
inst_libdbi = ' . File::Spec->catdir($self->{INST_LIB}, 'DBI') . '
changes_pm = '  . File::Spec->catfile($self->{INST_LIB}, 'DBI', 'Changes.pm') . '
roadmap_pm = '  . File::Spec->catfile($self->{INST_LIB}, 'DBI', 'Roadmap.pm') . '
'.q{

config :: $(changes_pm) $(roadmap_pm)
	$(NOECHO) $(NOOP)

$(changes_pm): Changes
	$(MKPATH) $(inst_libdbi)
	$(RM_F) $(changes_pm)
	$(CP) Changes $(changes_pm)

$(roadmap_pm): Roadmap.pod
	$(MKPATH) $(inst_libdbi)
	$(RM_F) $(roadmap_pm)
	$(CP) Roadmap.pod $(roadmap_pm)

faq:
	: checkin any local changes not already checked in before overwriting
	svn commit --message "dbi.tiddlyspot.com FAQ update" dbi.tiddlyspot.com.html
	wget --ignore-length --output-document=dbi.tiddlyspot.com.html --timestamping http://dbi.tiddlyspot.com/download
	svn commit --message "dbi.tiddlyspot.com FAQ update" dbi.tiddlyspot.com.html

checkkeywords:
	$(RM_RF) blib
	find . -type f \( -name .svn -prune -o -name \*.pm -o -name \*.PL -o -name \*.pl \) \
            -exec bash -c '[ -z "$$(svn pg svn:keywords {})" ] && echo svn propset svn:keywords \"Id Revision\" {}' \;

checkpod:
	$(RM_RF) blib
	find . -type f \( -name .svn -prune -o -name \*.pm -o -name \*.PL -o -name \*.pl \) \
            -exec podchecker {} \; 2>&1 | grep -v 'pod syntax OK'
};

    return $xst;
}

# end.
