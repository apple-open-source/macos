use strict;
use warnings;

use Config;
use ExtUtils::MakeMaker;
use File::Basename qw(dirname);
use Getopt::Long;

eval "use ExtUtils::MakeMaker::Coverage";
$@ or print "Adding testcover target\n";

use vars qw($opt_default $opt_libpath $opt_static);

GetOptions(
    "default", \$opt_default,
    "lib=s",   \$opt_libpath,
    "static",  \$opt_static,
);

$opt_default ||= $ENV{CRYPT_SSLEAY_DEFAULT};

# FIND POSSIBLE SSL INSTALLATIONS

my @POSSIBLE_SSL_DIRS;
if ($opt_libpath) {
    # explicit from command-line
    @POSSIBLE_SSL_DIRS = ($opt_libpath);
    $opt_default       = 1;
}
elsif ($^O eq 'MSWin32') {
    @POSSIBLE_SSL_DIRS = 'c:\\openssl';
}
elsif ($^O eq 'VMS') {
    @POSSIBLE_SSL_DIRS = '/ssl$root';
}
else {
    # Unix and the rest
    @POSSIBLE_SSL_DIRS = qw(
        /local
        /local/ssl
        /opt/ssl
        /usr
        /usr/local
        /usr/local/ssl
        /usr/local/openssl
    );
}

my @CANDIDATE = do {
    my %seen;
    grep {!$seen{$_->{check}}++}
        map {Candidate($_)}
            @POSSIBLE_SSL_DIRS
};

if (@CANDIDATE == 0) {
    $opt_default = 0;
    my $bar = '=' x 72;
    print <<"INFO";
$bar
No installed SSL libraries found in any of the following places.
INFO
    print "    $_\n" for @POSSIBLE_SSL_DIRS;
    print <<"INFO";
You will have to either specify a directory location at the following
prompt, or rerun the Makefile.PL program and use the --lib switch
to specify the path. If the path in question is considered standard
on your platform, please consider filing a bug report in order to
have it taken into account in a subsequent version of Crypt::SSLeay.

INFO

    if (-f '/etc/debian_version') {
        print <<"DEBIAN_INFO";

This host looks like it is running Debian. Crypt::SSLeay needs to
be compiled with C headers that the libssl-dev package makes
available. Please install that package before trying to build this
module. (You can always deinstall the package afterwards, once
Crypt::SSLeay has been built).
DEBIAN_INFO
    }
}

my $SSL_DIR;
my $pkg_config;

if (@CANDIDATE == 1) {
    $pkg_config = $CANDIDATE[0];
    if ($opt_default) {
        $SSL_DIR = $pkg_config->{dir};
    }
    else {
        print <<"INFO";
=======================================================
Only one $pkg_config->{type} installation found at $pkg_config->{dir}
Consider running 'perl Makefile.PL --default' the next
time Crypt::SSLeay is upgraded to select this directory
automatically thereby avoiding the following prompt.
=======================================================
INFO
    }
}
elsif (@CANDIDATE > 1) {
    print "Found multiple possibilities for OpenSSL\n";
    for my $c (@CANDIDATE) {
        print "  $c->{dir} ($c->{type} $c->{ver})\n";
    }
}

if (not $SSL_DIR) {
    my %cand;
    for my $c (@CANDIDATE) {
        $cand{$c->{dir}} = {%$c};
    }

    $SSL_DIR = prompt "Which SSL install path do you want to use?",
        $CANDIDATE[0]->{dir};

    if (exists $cand{$SSL_DIR}) {
        # we've already determined that this directory is usable
        $pkg_config = $cand{$SSL_DIR};
    }
    else {
        # unknown directory, better check it out
        $pkg_config = Candidate($SSL_DIR);
        if (not $pkg_config) {
            die <<"INFO";
$SSL_DIR does not appear to be an SSL library installation, since
the required header files were not found. The build cannot proceed.
INFO
        }
    }
}

# note: $SSL_DIR is now sane at this point

my (@INC_FLAGS, @LIB_FLAGS);

if ($^O eq 'VMS') {
    push @INC_FLAGS, "-I$pkg_config->{inc}";
    push @LIB_FLAGS, qw(-L/SYS$SHARE -lSSL$LIBSSL_SHR32 -lSSL$LIBCRYPTO_SHR32);
}
elsif ($^O eq 'MSWin32') {
    # external tools probably expect \ and not / for path separators
    $SSL_DIR =~ tr{/}{\\};

    # default to drive C: if no drive or relative path
    $SSL_DIR = "c:$SSL_DIR" if $SSL_DIR !~ m{\A(?:[a-z]:|\.\.[\\/])}i;

    my $inc = $pkg_config->{inc};
    $inc =~ tr{/}{\\};
    $inc = "c:$inc" if $inc !~ m{\A(?:[a-z]:|\.\.[\\/])}i;
    push @INC_FLAGS, "-I$inc";
    push @INC_FLAGS, "-I$SSL_DIR\\inc32" if -d "$SSL_DIR/inc32";

    my $vanilla = $Config{cc} eq 'gcc' ? 1 : 0;
    $vanilla and print "Assuming Vanilla/Strawberry Perl installation\n";
    
    if ($vanilla and -d "$SSL_DIR\\lib\\MinGW") {
        push @LIB_FLAGS, "-L$SSL_DIR\\lib\\MinGW";
    }
    elsif(-d "$SSL_DIR/lib") {
        push @LIB_FLAGS, "-L$SSL_DIR\\lib";
    }
    else {
        my $dir = $opt_static ? "$SSL_DIR\\out32" : "$SSL_DIR\\out32dll";
        if (-d $dir) {
            push @LIB_FLAGS, "-L$dir";
        }
        else {
            # Allow developers to point at OpenSSL source...
            push @LIB_FLAGS, "-L$SSL_DIR";
        }
    }

    push @LIB_FLAGS, qw(-lssleay32 -llibeay32);
    push @LIB_FLAGS, qw(-lRSAglue -lrsaref) if $pkg_config->{type} ne 'OpenSSL';
}
else {
    push @INC_FLAGS, "-I$pkg_config->{inc}";

    push @LIB_FLAGS, "-L$pkg_config->{lib}", qw(-lssl -lcrypto -lgcc);
    push @LIB_FLAGS, qw(-lRSAglue -lrsaref) if $pkg_config->{type} ne 'OpenSSL';

    # ccc on alpha support
    if ($^O eq 'linux'
        and `uname -m` =~ /alpha/
        and !(system("nm $SSL_DIR/lib/libssl.a|grep -q 'U _Ots'")>>8)
    ) {
        push @LIB_FLAGS, '-lots'; 
    }

    # this fix was suggested for building on RedHat 9
    push @INC_FLAGS, '-I/usr/kerberos/include' if -d '/usr/kerberos/include';
}

# write include file that determines ssl support
# we need to include crypto.h for SSLeay so the version gets picked up in SSLeay.xs

open(INCLUDE, ">crypt_ssleay_version.h") || die("can't open crypt_ssleay_version.h for writing: $!");
print INCLUDE <<"INFO";
#include "$pkg_config->{prefix}ssl.h"
#include "$pkg_config->{prefix}crypto.h"
#include "$pkg_config->{prefix}err.h"
#include "$pkg_config->{prefix}rand.h"
#include "$pkg_config->{prefix}pkcs12.h"

INFO

if ($] < 5.005) {
    print "adding PL_sv_undef symbol for this ancient perl installation";
    print INCLUDE <<"INFO";
/* defining PL_sv_undef for very old perls ($]) */
#ifndef PL_sv_undef
#define PL_sv_undef sv_undef
#endif

INFO
}

if ($pkg_config->{type} eq 'OpenSSL') {
    # OPENSSL_free defined in OpenSSL 0.9.6 and higher
    if ($pkg_config->{ver} =~ /\b0\.9\.[2-5]/) {
        print INCLUDE "#define CRYPT_SSLEAY_free free\n";
    }
    else {
        print INCLUDE "#define CRYPT_SSLEAY_free OPENSSL_free\n";
    }
}
else {
    print INCLUDE "#define CRYPT_SSLEAY_free free\n";
}

close INCLUDE or die "Cannot close crypt_ssleay_version.h for output: $!\n";

print <<"INFO";

BUILD INFORMATION
================================================
ssl library: $pkg_config->{type} $pkg_config->{ver} in $SSL_DIR
ssl header:  $pkg_config->{prefix}ssl.h
libraries:   @LIB_FLAGS
include dir: @INC_FLAGS
================================================
INFO

my @license =
    do {
        my $version = $ExtUtils::MakeMaker::VERSION;
        $version =~ tr/_//d;
        $version} > 6.30
    ? qw(LICENSE perl)
    : ();

WriteMakefile(
    NAME          => 'Crypt::SSLeay',
    AUTHOR        => 'David Landgren',
    ABSTRACT_FROM => 'SSLeay.pm',
    VERSION_FROM  => 'SSLeay.pm',
    LIBS          => ["@LIB_FLAGS"],
    INC           => "@INC_FLAGS",
    NEEDS_LINKING => 1,
    clean         => {
        FILES => 'crypt_ssleay_version.h test.config',
    },
    @license,
);

if (open OUT, '> test.config') {
    print OUT <<"INFO";
ssl $pkg_config->{type} $pkg_config->{ver} in $SSL_DIR
lib @LIB_FLAGS
inc @INC_FLAGS
cc $Config{cc}
INFO

    print <<"INFO";
The test suite can attempt to connect to public servers
to ensure that the code is working properly. If you are
behind a strict firewall or have no network connectivity,
these tests may fail (through no fault of the code).

INFO
    my $network_tests = prompt
        "Do you want to run the live tests (y/N) ?",
        'N';
    print OUT "network_tests ", ($network_tests =~ /y/i) ? 1 : 0, "\n";
    close OUT;
}

## HELPERS

sub Candidate {
    my $dir = shift;
    return unless -d $dir;

    my $inc_dir;
    my $version_file;
    for (
         "$dir/inc32/openssl/opensslv.h", # old win32 builds
         "$dir/crypto/opensslv.h", # cygwin32 builds
         "$dir/include/openssl/opensslv.h", 
         "$dir/../include/openssl/opensslv.h", # Solaris
         "$dir/include/opensslv.h", 
         "$dir/include/crypto.h"
    ) {
        if (-e $_) {
            $version_file = $_;
            last;
        } 
    }
    return unless defined $version_file;
    my $fingerprint = join(':', (stat $version_file)[0,1]);

    my $open_ssl = ($version_file =~ /openssl/i) ? 1 : 0;
    $inc_dir = dirname($version_file);
    return unless -e "$inc_dir/ssl.h";

    my $prefix;
    if ($^O eq 'MSWin32') {
        $inc_dir =~ s{[\\/]openssl\z}{};
        $prefix  = 'openssl/';
    }
    elsif (index($inc_dir, '/../') > -1) {
        # OpenSSL include directory is in a sibling directory
        $inc_dir =~ s{\/openssl\z}{};
        $prefix  = 'openssl/';
    }
    else {
        $prefix = ($inc_dir =~ /\bopenssl/i) ? 'openssl/' : '';
    }

    open(VERSION_FILE, $version_file) or return;
    my $version_match = $open_ssl ? "OPENSSL_VERSION_NUMBER" : "SSLEAY_VERSION_NUMBER";
    my $version;
    my $type;
    while (<VERSION_FILE>) {
        if (/^#define\s+$version_match\s+0x0+(\d\d\d)/) {
            $version = $1;
            $version =~ s/(\d)0(\d)/$1$2/;            
            $type = ($version > 92) ? "OpenSSL" : "SSLeay";
            $version = join('.', split(//, "0$version"));
            last;
        }
    }
    close(VERSION_FILE);

    # Silly test to look for the library files
    my $found_lib;
    my $libd;
    my $subdir = $opt_static ? 'out32' : 'out32dll';
    if (-d "$dir/$subdir") {
        $libd = [$subdir];
    }
    elsif ($^O eq 'MSWin32' and $Config{cc} eq 'gcc') {
        $libd = ['lib/MinGW'];
    }
    else {
        # second attempt is for Solaris, like the include directory, the
        # library directory may be in a sibling directory
        $libd = ['lib', '../lib'];
    }

    SCAN:
    for my $d (@$libd) {
        my $lib_dir = "$dir/$d";
        if (opendir(LIBDIR, $lib_dir)) {
            while (defined($_ = readdir(LIBDIR))) {
                if (/\A(?:lib(?:crypto|eay32|ssl)|ssleay32)/) {
                    $found_lib = $lib_dir;
                    last SCAN;
                }
            }
            closedir(LIBDIR);
        }
    }

    if (!$found_lib) {
        my @tried = join( ',' => map {"$dir/$_"} @$libd);
        print "Did not locate expected SSL library files in @tried\n";
    }

    return {
        dir    => $dir,
        inc    => $inc_dir,
        lib    => $found_lib,
        ver    => $version,
        type   => $type,
        prefix => $prefix,
        check  => $fingerprint,
    };
}

