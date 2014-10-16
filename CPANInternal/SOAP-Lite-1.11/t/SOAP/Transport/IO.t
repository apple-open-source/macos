use strict;
use Test::More qw(no_plan);

use_ok qw(SOAP::Transport::IO);

my $server;
ok $server = SOAP::Transport::IO::Server->new(), 'new()';

SKIP: {
    eval "require IO::Scalar"
        or skip "cannot test Scalar IO without IO::Scalar", 1;
    my $input = q{};
    my $input_handle = IO::Scalar->new(\$input);
    
    ok $server->in($input_handle);
    
    eval { $server->handle() };
    ok !$@;

}

is $server, $server->new(), '$server->new() is $server';

my $name = __FILE__;
$name =~s{ t $}{xml}x;
ok $server = SOAP::Transport::IO::Server->in($name), 'in($filename)';
eval { $server->handle() };
ok !$@;

ok $server->in(undef), 'in(undef)';
ok $server->out(undef), 'in(undef)';
# TODO: add some content to IO.xml and run it through a SOAP server...