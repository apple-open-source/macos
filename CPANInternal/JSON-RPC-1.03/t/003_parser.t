use strict;
use Test::More;
use Plack::Request;
use JSON;

use_ok "JSON::RPC::Parser";
use_ok "JSON::RPC::Procedure";

subtest 'basic' => sub {
    my $req = Plack::Request->new( {
        QUERY_STRING   => 'method=sum&params=[1,2,3]&id=1',
        REQUEST_METHOD => "GET",
    } );
    my $parser = JSON::RPC::Parser->new(
        coder => JSON->new,
    );

    my $procedures = $parser->construct_from_req( $req );
    ok $procedures, "procedures is defined";
    is @$procedures, 1, "should be 1 procedure";
    my $procedure = $procedures->[0];
    ok $procedure, "procedure is defined";
    isa_ok $procedure, "JSON::RPC::Procedure";
    is $procedure->id, 1, "id matches";
    is $procedure->method, "sum", "method matches";
    is_deeply $procedure->params, [ 1, 2, 3 ], "parameters match";
};

done_testing;
