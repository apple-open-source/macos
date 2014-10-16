#!/usr/bin/perl

use strict;
use warnings;

use ExtUtils::MakeMaker;

my %prereqs = (

    # Core package
    "Test::More"       => 0,
    "XML::Parser"      => "2.23",
    "MIME::Base64"     => 0,
    "URI"              => 0,
    "Scalar::Util"     => 0,
    "Task::Weaken"     => 0,         # to be sure than weaken works
    "constant"         => 0,         # don't force people to upgrade
    "Class::Inspector" => 0,

    # Client TCP support
    "IO::SessionData"  => 1.03,
    "IO::Socket::SSL"  => 0,

    # Compression support for HTTP
    "Compress::Zlib"   => 0,

    # Client HTTP support
    "LWP::UserAgent"   => 0,

    # Client HTTPS support
    "Crypt::SSLeay" => 0,

    # "Standalone HTTP server"
    # ,"SOAP::Transport::HTTP::Daemon",{
    # "HTTP::Daemon" => 0,

    # "Apache/mod_perl server"
    # "Apache" => 0,

    # "FastCGI server"
    # "FCGI" => 0,

    # "POP3 server"
    # "Net::POP3" => 0,
    # "MIME::Parser" => 0,

    # "MQ transport support"
    # "SOAP::Transport::MQ" => 0.712,

    # "JABBER transport support"
    # "SOAP::Transport::JABBER" => 0.712,

    # MIME messages
    # "MIME::Parser" => 0,

    # DIME messages
    # "IO::Scalar" => "2.105",
    # "DIME::Tools" => 0.03,
    # "Data::UUID" => "0.11",

    #"IO::File"         => 0,


    # "MIME interoperability w/ Axis"
    # "MIME::Parser" => "6.106",
);

ExtUtils::MakeMaker::WriteMakefile(
    'NAME' => 'SOAP::Lite',
    'AUTHOR' =>
'Pavel Kulchenko, Randy J Ray, Byrne Reese, Martin Kutter (MKUTTER@cpan.org), Fred Moyer',
    'LICENSE'       => 'perl',
    'VERSION_FROM'  => 'lib/SOAP/Lite.pm',
    'ABSTRACT_FROM' => 'lib/SOAP/Lite.pm',
    'PREREQ_PM'     => \%prereqs,
    'EXE_FILES' => [ 'bin/SOAPsh.pl', 'bin/stubmaker.pl' ],
    (
        $ExtUtils::MakeMaker::VERSION ge '6.48'
        ? (
            MIN_PERL_VERSION => 5.006,
            META_MERGE       => {
                resources => {
                    repository => 'https://github.com/redhotpenguin/soaplite',
                },
                keywords => [ 'SOAP', 'SOAP client', 'SOAP server' ],
                no_index => {
                    directory => [ qw( examples t ) ],
                },
            },
          )
        : ()
    ),
);

1;
