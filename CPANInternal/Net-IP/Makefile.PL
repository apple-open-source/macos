use ExtUtils::MakeMaker;
use Config;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

# RIPE NCC common configuration
my %PARAM = ();

if ($Config{vendorprefix} eq '/usr/local/ncc') {
    
    warn "\nConfiguring for RIPE NCC Perl installation...\n\n";
    
    $PARAM{'INSTALLDIRS'}	= 'vendor';
    $PARAM{'INSTALLSCRIPT'}	= '$(INSTALLVENDORBIN)'; # Hack for Perl prior 5.8.1
    $PARAM{'macro'}		= { 'INSTALLPREFIX' => '$(VENDORPREFIX)' };
}

$PARAM{'dist'} = {
	'COMPRESS'  => 'gzip', 
        'SUFFIX'    => '.gz',
        'CI'        => 'cvs ci',
        'RCS_LABEL' => 'cvs tag -c -F $(NAME_SYM)-$(VERSION_SYM)',
};

$PARAM{'ABSTRACT_FROM'}	= 'IP.pm';
$PARAM{'AUTHOR'}	= 'Manuel Valente <manuel.valente@gmail.com>';

$PARAM{'clean'} = {FILES => "Net-IP.spec"};

WriteMakefile(
    'NAME'		=> 'Net::IP',
    'VERSION_FROM'	=> 'IP.pm',
    'PREREQ_PM'		=> {},
    'EXE_FILES'         => [qw(ipcount iptab)],
    'PL_FILES'		=> { 'Net-IP.spec.PL'	=>	'Net-IP.spec' },
    %PARAM
);
