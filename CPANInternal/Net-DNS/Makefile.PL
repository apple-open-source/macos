
# $Id: Makefile.PL 686 2007-11-29 21:12:40Z olaf $ -*-perl-*-

use strict;
$^W = 1;

use ExtUtils::MakeMaker qw(WriteMakefile prompt);
use IO::Socket ();
use Config qw(%Config);
use Getopt::Long  qw(GetOptions);

sub DEBUG { 0; }

# An existing makefile can confuse the CC test.
unlink('Makefile');
# clean up the online testing flag file.
unlink("t/online.enabled"); 

warn <<AMEN if $] < 5.005;

This verion of Net::DNS has not been tested against perl v$]
The author is confident that this package will function 
properly with older perls such as yours, but bugs may lurk. 


AMEN

#
# Test to see if V6 is available.
#    
unless(    eval {require Socket6;} &&
	   # INET6 prior to 2.01 will not work; sorry.
	   eval {require IO::Socket::INET6; IO::Socket::INET6->VERSION("2.00");}	   ){
print <<V6WARNING;


The libraries needed to support IPv6 transport have not been found. 
You will need recent versions of the IO::Socket::INET6 and Socket6 
libraries (from CPAN). 

V6WARNING
}


#
# Get the command line args
#
my ($use_xs, $online_tests);

GetOptions(
	'xs!'           => \$use_xs,
	'pm'            => sub { 
		warn qq/\n\tWARNING: Use of "--pm" is deprecated.  Use "--noxs" instead.\n\n/;
		$use_xs = 0;
	},
	'online-tests!' => \$online_tests,
);



#
# Get our makefile started...
#  NB Net::IP is still needed for tests.
my %Makefile = (
	NAME          => 'Net::DNS',
	VERSION_FROM  => 'lib/Net/DNS.pm',
	PREREQ_PM     => {
		'Test::More'       => 0.18,
		'IO::Socket'       => 0,
		'MIME::Base64'     => 2.11,
		'Digest::MD5'      => 2.12,
		'Net::IP'          => 1.2,
		'Digest::HMAC_MD5' => 1.00,
	},
	XS            => {},
	C             => [],
	clean         => { FILES => 't/online.enabled compile.* DNS.o DNS.c DNS.bs' },


);



if ($^O eq 'MSWin32') {
	$Makefile{'PREREQ_PM'}->{'Win32::Registry'} = 0;
}


# Not relevant with netdnslib.
#if ($^O eq 'darwin' and $Config{'osvers'} =~ m/^[78]\./) {
#	$Makefile{'DEFINE'} = '-DBIND_8_COMPAT';
#}

if ($] >= 5.005) {
	$Makefile{'AUTHOR'}   = 'Olaf Kolkman <olaf@net-dns.org>';
	$Makefile{'ABSTRACT'} = 'Perl DNS resolver module';
}

#
# Check if we have a C compiler

unless (defined $use_xs) {
	if (test_cc()) {
		print "You have a working compiler.\n";
		$use_xs = 1;
		$Makefile{'MYEXTLIB'}    = 'netdns$(LIB_EXT)';
	} else {
		$use_xs = 0;
		print <<END;
	
I cannot determine if you have a C compiler. I will install the 
perl-only implementation.
	
You can force installation of the XS version with:

	perl Makefile.PL --xs
END
	$Makefile{'MYEXTLIB'}    = '',		

	}
}








if ($use_xs) {
	# turn the XS bits on.
        print "Activating XS Magic...\n" if DEBUG; 
	$Makefile{'LIBS'} = '-lresolv' if($^O =~ /solaris|sunos/i);
	$Makefile{'OBJECT'} = '$(O_FILES)';
	delete $Makefile{'XS'};
	delete $Makefile{'C'};
}

#
# Check if we have internet connection
# (I lifted this code from LWP... )
#
unless (defined $online_tests) {
	my $s = IO::Socket::INET->new(
		PeerAddr => "www.google.com:80",
		Timeout  => 10,
	);
	
	if ($s) {
		close($s);
	
		print <<EOT;
	
You appear to be directly connected to the Internet.  I have some tests
that try to query live nameservers.
	
EOT
	
		$online_tests = prompt("Do you want to enable these tests?", "y") =~ /^y/i ? 1 : 0;
	}
}

if ($online_tests) {
	print "Activating Online Tests...\n" if DEBUG; 
	open(ENABLED, ">t/online.enabled") || die "Can't touch ./t/online.enabled $!";
	close(ENABLED)                     || die "Can't touch ./t/online.enabled $!";
}

WriteMakefile(%Makefile);





sub test_cc {
	#
	# The perl/C checking voodoo is stolen from Graham Barr's
	# Scalar-List-Utils distribution.
	#
	print "Testing if you have a C compiler and the needed header files....\n";

	unless (open(F, ">compile.c")) {
	    warn "Cannot write compile.c, skipping test compilation and installing pure Perl version.\n";
		return;
	}

	print F <<'EOF';
#include "netdns.h"
int main() { return 0; }
EOF
	
	close(F) or return;

	my $ret = system("$Config{'cc'}  -c compile.c -o compile$Config{obj_ext}");
	
	foreach my $file (glob('compile*')) {
		unlink($file) || warn "Could not delete $file: $!\n";
	}

	return ($ret == 0);
}
		
package MY;

sub metafile {
	my $self    = shift;
	my $meta    = $self->SUPER::metafile_target(@_); 
	my ($extra) = $self->echo('license: perl', 'META.yml', 1);
	return "$meta\t$extra\n";
}


use Config qw(%Config);
sub postamble {
    my $content;

    $content.='
test_cover : pure_all
	cover -delete
	HARNESS_PERL_SWITCHES=-MDevel::Cover $(MAKE) test
	cover

netdns$(LIB_EXT): netdns$(OBJ_EXT)
	$(AR) '.($^O eq 'MSWin32' && $Config{'cc'} eq 'cl'?'/OUT:':'cr ').
	  'netdns$(LIB_EXT) netdns$(OBJ_EXT)

';
#    print "$content";
    return $content;

}



sub MY::libscan {
    my $path = $_[1];
    return '' if $path =~ /\B\.svn\b/;
    return $path;
}
