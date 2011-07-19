use 5.00503;
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'		=> 'JSON::RPC',
    'VERSION_FROM'	=> 'lib/JSON/RPC.pm', # finds $VERSION
    'PREREQ_PM'		=> {
              Test::More        => 0,
              CGI               => 2.92,
              HTTP::Request     => 0,
              HTTP::Response    => 0,
              JSON              => 2.0,
              LWP::UserAgent    => 2.001,
    }, # e.g., Module::Name => 1.1
    ($] >= 5.005 ?    ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM => 'lib/JSON/RPC.pm', # retrieve abstract from module
       AUTHOR     => 'Makamaka Hannyaharamitu, E<lt>makamaka[at]cpan.orgE<gt>') : ()),
);
