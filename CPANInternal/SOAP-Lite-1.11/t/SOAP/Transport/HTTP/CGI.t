package SOAP::Transport::CGI_TEST::Client;

use strict; use warnings;
use IPC::Open2;
use File::Basename qw(dirname);

# make a send_receive which performs an open2, much like a web server
# connects to STDIN and STDOUT...
sub send_receive {
    my ($self, %parameters) = @_;
    my ($context, $envelope, $endpoint, $action, $encoding, $parts) =
        @parameters{qw(context envelope endpoint action encoding parts)};

    # safety measure: Die if we hang
    $SIG{ALRM} = sub { die "did not return" };
    alarm(3);

    my $perl = $^X;

    my $dir = dirname(__FILE__);
    my $cmd = "$dir/CGI/test_server.pl";

    $ENV{'CONTENT_LENGTH'} = length($envelope);
    $ENV{'REQUEST_METHOD'} = 'POST';
    my($child_out, $child_in);
    my $pid = open2($child_out, $child_in, $perl, '-Mblib', $cmd);
    die "Cannot open $cmd: $!" if not ($pid);

    print $child_in $envelope;
    print $child_in "\n";
    close $child_in;
    my @result = <$child_out>;

    close $child_out;

    alarm(0);

    return $result[-1];
}

package main;
no strict;
use Test::More qw(no_plan);
use SOAP::Lite; # +trace;
my $soap = SOAP::Lite->new()->proxy('http://');
no warnings qw(redefine once);

# make override send_receive in CGI client
*SOAP::Transport::HTTP::Client::send_receive =
    \&SOAP::Transport::CGI_TEST::Client::send_receive;

my $som = $soap->call('test');
my $result = $som->result;


if ($] >= 5.008) {
    ok utf8::is_utf8($result), 'return utf8 string';
    {
        is $result, 'Überall', 'utf8 content: ' . $result;
    }
}
else {
    eval { use bytes;
        is length $result, 8, "length of >$result< is 8 due to wide character";

    }
}
