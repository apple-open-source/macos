# -*- perl -*-
#
# $Id: Makefile.PL,v 1.2 1999/08/12 14:28:53 joe Exp $
#

use ExtUtils::MakeMaker;

my %opts = (
    'NAME'	=> 'Net::Daemon',
    'VERSION_FROM' => 'lib/Net/Daemon.pm', # finds $VERSION
    'dist' => { 'DIST_DEFAULT' => q[all tardist],
	        'COMPRESS' => q[gzip -9vf],
	        'SUFFIX' => q[.gz]
	    },
    'realclean' => { 'FILES' => 't/config ndtest.prt' }
);

if ($ExtUtils::MakeMaker::VERSION >= 5.43) {
    $opts{ABSTRACT_FROM} = 'lib/Net/Daemon.pm';
    $opts{AUTHOR} = 'Jochen Wiedmann (joe@ispsoft.de)';
}

WriteMakefile(%opts);


package MY;

sub libscan {
    my ($self, $path) = @_;
    ($path =~ /\~$/) ? undef : $path;
}

sub postamble {
    qq{

pm_to_blib: README

README: lib/Net/Daemon.pm
\tpod2text lib/Net/Daemon.pm >README

}
}
