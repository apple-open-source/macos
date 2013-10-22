use strict;
use Test::More;
use Plack::Test;
use HTTP::Request;
use JSON;

BEGIN {
    use_ok "JSON::RPC::Dispatch";
    use_ok "JSON::RPC::Constants", ':all';
    use_ok "JSON::RPC::Test";
    use_ok "t::JSON::RPC::Test::Handler::Sum";
}

subtest 'defaults' => sub {
    my $dispatch = JSON::RPC::Dispatch->new();
    if (ok $dispatch->coder) {
        isa_ok $dispatch->coder, 'JSON';
    }

    if (ok $dispatch->router) {
        isa_ok $dispatch->router, "Router::Simple";
    }

    if (ok $dispatch->parser) {
        isa_ok $dispatch->parser, "JSON::RPC::Parser";
    }
};

subtest 'normal dispatch' => sub {
    my $coder = JSON->new;
    my $router = Router::Simple->new;
    $router->connect( blowup => {
        handler => "Sum",
        action  => "blowup",
    } );
    $router->connect( 'sum' => {
        handler => 'Sum',
        action => 'sum',
    } );
    $router->connect( tidy_error => {
        handler => "Sum",
        action  => "tidy_error",
    } );

    $router->connect( 'sum_obj' => {
        handler => t::JSON::RPC::Test::Handler::Sum->new,
        action => 'sum',
    } );

    my $dispatch = JSON::RPC::Dispatch->new(
        coder  => $coder,
        parser => JSON::RPC::Parser->new( coder => $coder ),
        prefix => 't::JSON::RPC::Test::Handler',
        router => $router,
    );
    ok $dispatch, "dispatch ok";


    my $request_get = sub {
        my $cb = shift;

        my ($req, $res, $json);
        my $uri = URI->new( "http://localhost" );

        # no such method...
        $uri->query_form(
            method => 'not_found'
        );
        $req = HTTP::Request->new( GET => $uri );
        $res = $cb->( $req );
        if (! ok $res->is_success, "response is success") {
            diag $res->as_string;
        }

        $json = $coder->decode( $res->decoded_content );
        if ( ! ok $json->{error}, "I should have gotten an error" ) {
            diag explain $json;
        }

        if (! is $json->{error}->{code}, JSON::RPC::Constants::RPC_METHOD_NOT_FOUND(), "code is RPC_METHOD_NOT_FOUND" ) {
            diag explain $json;
        }


        my @params = ( 1, 2, 3, 4, 5 );
        foreach my $method ( qw(sum sum_obj) ){
            $uri->query_form(
                method => $method,
                params => $coder->encode(\@params)
            );

            $req = HTTP::Request->new( GET => $uri );
            $res = $cb->( $req );
            if (! ok $res->is_success, "response is success") {
                diag $res->as_string;
            }

            $json = $coder->decode( $res->decoded_content );
            if (! ok ! $json->{error}, "no errors") {
                diag explain $json;
            }

            my $sum = 0;
            foreach my $p (@params) {
                $sum += $p;
            }
            is $json->{result}, $sum, "sum matches";
        }


        my $id = time();
        $uri->query_form(
            jsonrpc => '2.0',
            id     => $id,
            method => 'blowup',
            params => "fuga",
        );
        $req = HTTP::Request->new( GET => $uri );
        $res = $cb->( $req );

        if (! ok $res->is_success, "response is success") {
            diag $res->as_string;
        }

        $json = $coder->decode( $res->decoded_content );
        is $json->{jsonrpc}, '2.0';
        is $json->{id}, $id;
        ok $json->{error};
    };


    my $request_post = sub {
        my $cb = shift;

        my ($req, $res, $post_content, $json);

        my $headers = HTTP::Headers->new( Content_Type => 'application/json',);
        my $uri = URI->new( "http://localhost" );

        $post_content = $coder->encode( { method => 'not_found' } );

        # no such method...
        $req = HTTP::Request->new( POST => $uri, $headers, $post_content);
        $res = $cb->($req);

        if (! ok $res->is_success, "response is success") {
            diag $res->as_string;
        }

        $json = $coder->decode( $res->decoded_content );
        if ( ! ok $json->{error}, "I should have gotten an error" ) {
            diag explain $json;
        }

        if (! is $json->{error}->{code}, JSON::RPC::Constants::RPC_METHOD_NOT_FOUND(), "code is RPC_METHOD_NOT_FOUND" ) {
            diag explain $json;
        }


        my @params = ( 1, 2, 3, 4, 5 );
        foreach my $method ( qw(sum sum_obj) ){
            $post_content = $coder->encode(
                {
                    method => $method,
                    params => \@params,
                },
            );

            $req = HTTP::Request->new( POST => $uri, $headers, $post_content );
            $res = $cb->( $req );
            if (! ok $res->is_success, "response is success") {
                diag $res->as_string;
            }

            $json = $coder->decode( $res->decoded_content );
            if (! ok ! $json->{error}, "no errors") {
                diag explain $json;
            }

            my $sum = 0;
            foreach my $p (@params) {
                $sum += $p;
            }
            is $json->{result}, $sum, "sum matches";
        }


        my $id = time();
        $post_content = $coder->encode(
            {
                jsonrpc => '2.0',
                id     => $id,
                method => 'blowup',
                params => "fuga",
            },
        );
        $req = HTTP::Request->new( POST => $uri, $headers, $post_content );
        $res = $cb->( $req );

        if (! ok $res->is_success, "response is success") {
            diag $res->as_string;
        }

        $json = $coder->decode( $res->decoded_content );
        is $json->{jsonrpc}, '2.0';
        is $json->{id}, $id;
        ok $json->{error};

    };

    # XXX I want to test both Plack::Request and raw env, but test_rpc
    # makes it kinda hard... oh well, it's not /that/ much of a problem
    test_rpc $dispatch, sub {
        my $cb = shift;
        subtest 'JSONRPC via GET' => sub { $request_get->($cb) };
        subtest 'JSONRPC via POST' => sub { $request_post->($cb) };
        subtest 'JSONRPC Error' => sub { 
            my ($post_content, $req, $res, $json);
            my $headers = HTTP::Headers->new( Content_Type => 'application/json',);
            my $uri = URI->new( "http://localhost" );

            $post_content = $coder->encode( [ method => "hoge"] );
            $req = HTTP::Request->new( POST => $uri, $headers, $post_content );
            $res = $cb->($req);
            $json = $coder->decode( $res->decoded_content );
            if (! is $json->{error}->{code}, RPC_INVALID_PARAMS ){
                diag explain $json;
            }

            $post_content = "{ [[ broken json }";
            $req = HTTP::Request->new( POST => $uri, $headers, $post_content );
            $res = $cb->($req);
            $json = $coder->decode( $res->decoded_content );
            if (! is $json->{error}->{code}, RPC_PARSE_ERROR ) {
                diag explain $json;
            }

            $post_content = "[]";
            $req = HTTP::Request->new( POST => $uri, $headers, $post_content );
            $res = $cb->($req);
            $json = $coder->decode( $res->decoded_content );
            if (! is $json->{error}->{code}, RPC_INVALID_REQUEST ){
                diag explain $json;
            }

            # invalid method 'PUT'
            $req = HTTP::Request->new( PUT => $uri );
            $res = $cb->($req);
            $json = $coder->decode( $res->decoded_content );
            if (! is $json->{error}->{code}, RPC_INVALID_REQUEST ){
                diag explain $json;
            }


            my $id = time();
            $post_content = $coder->encode(
                {
                    jsonrpc => '2.0',
                    id     => $id,
                    method => 'tidy_error',
                    params => "foo",
                }
            );
            $req = HTTP::Request->new( POST => $uri, $headers, $post_content);
            $res = $cb->($req);
            $json = $coder->decode( $res->decoded_content );
            if (! is $json->{error}->{code}, RPC_INTERNAL_ERROR) {
                diag explain $json;
            }
            is $json->{error}->{message}, 'short description of the error';
            is $json->{error}->{data}, 'additional information about the error';
        };
    };
};

done_testing;
