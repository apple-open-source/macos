#!/bin/env perl

BEGIN {
unless(grep /blib/, @INC) {
chdir 't' if -d 't';
unshift @INC, '../lib' if -d '../lib';
}
}

use strict;
use Test::More qw(no_plan);

my @modules = qw(SOAP::Lite
                 SOAP::Transport::HTTP
                 SOAP::Transport::MAILTO
                 SOAP::Transport::FTP
                 SOAP::Transport::TCP
                 SOAP::Transport::IO
                 SOAP::Transport::LOCAL
                 SOAP::Transport::POP3
                 XML::Parser::Lite
                 UDDI::Lite XMLRPC::Lite
                 XMLRPC::Transport::HTTP
                 XMLRPC::Transport::TCP
                 XMLRPC::Transport::POP3
                 SOAP::Packager
                 SOAP::Transport::MQ SOAP::Transport::JABBER
                );
foreach (@modules) {
    eval "require $_";

    if ($@ =~ /(Can\'t locate)|(XML::Parser::Lite requires)|(this is only version)|(load mod_perl)/) {
        SKIP: {
            skip("Module $_ does not exist or is breaking in an expected way", 1);
        }
    next;
    }

    is($@, '', "use $_") or warn $@;
}