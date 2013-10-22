package main;
use strict; use warnings;
use SOAP::Lite;
use SOAP::Transport::HTTP;

my $soap = SOAP::Transport::HTTP::CGI->new(
    dispatch_to => 'main'
);

$soap->handle();

sub test {
    my ($self, $envelope) = @_;
    return SOAP::Data->name('testResult')->value('Überall')->type('string');

}

1;