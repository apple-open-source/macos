# This -*- perl -*- script makes the Makefile

#--- Distribution section ---

$NAME     = 'Net';
$DISTNAME = "libnet";
$VERSION  = "1.19";

BEGIN { require 5.002 }

use ExtUtils::MakeMaker;
use ExtUtils::Manifest qw(maniread);

#--- Installation check

sub chk_version
{
 my($pkg,$wanted,$msg) = @_;

 local($|) = 1;
 print "Checking for $pkg...";

 eval { my $p; ($p = $pkg . ".pm") =~ s#::#/#g; require $p; };

 my $vstr = ${"${pkg}::VERSION"} ? "found v" . ${"${pkg}::VERSION"}
				 : "not found";
 my $vnum = ${"${pkg}::VERSION"} || 0;

 print $vnum >= $wanted ? "ok\n" : " " . $vstr . "\n";

 $vnum >= $wanted;
}

sub MY::post_initialize
{
 my ($self) = @_;

 #--- Create Net::Config

 my $config_pm = "Net/libnet.cfg";
 my $libnet_cfg = "libnet.cfg";

 # Use %INC and ExtUtils::MakeMaker to determine how this machine
 # maps package names to path names

 foreach (keys %INC) {
  last if ($config_pm = $_) =~ s/^ExtUtils(.)MakeMaker.pm/Net${1}libnet.cfg/;
 }

 system(($^O eq 'VMS' ? 'mcr ': ()),$^X, 'Configure')
	unless -f $libnet_cfg;

 $self->{PM}->{$libnet_cfg} = $self->catfile('$(INST_LIBDIR)',$config_pm);

 "";
}

#--- Check for Socket

chk_version(Socket => '1.30') or
    warn  "\n"
	. "*** For Net::Cmd to work you require version 1.30, or later, of\n"
	. "    Socket.pm from CPAN/modules/by-module/Socket/Socket-x.x.tar.gz\n\n";

chk_version(IO::Socket => '1.05') or
    warn  "\n"
	. "*** For Net::Cmd to work you require version 1.05, or later, of\n"
	. "    IO/Socket.pm from CPAN/modules/by-module/IO/IO-x.x.tar.gz\n\n";

if ($^O eq 'os390')
{
  chk_version(Convert::EBCDIC => '0.06') or
   warn  "\n"
   . "*** For Net::Cmd to work on $^O version 0.06, or later, of\n"
   . "    Convert::EBCDIC is required, which can be found at"
   . "    CPAN/modules/by-module/Convert/Convert-EBCDIC-x.x.tar.gz\n\n";
}

#--- Write the Makefile

my @ppd;

if ($] >= 5.00503) {
  @ppd = (
    AUTHOR    => 'Graham Barr <gbarr@pobox.com>',
    ABSTRACT  => 'Collection of Network protocol modules',
  );
}

WriteMakefile(
  INSTALLDIRS => ($] >= 5.008 ? 'perl' : 'site'),
  VERSION     => $VERSION,
  DISTNAME    => $DISTNAME,
  NAME        => $NAME,
  'realclean' => {FILES => $config_pm},
  PREREQ_PM   => {
		Socket     => 1.3,
		IO::Socket => 1.05
	      },
  @ppd,
);

sub MY::postamble {
                                                                                                    
<<EOS;
                                                                                                    
distdir : run_cpansign
                                                                                                    
run_cpansign :
	cpansign -s
                                                                                                    
EOS
                                                                                                    
}

