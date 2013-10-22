package JSON::RPC::Dispatch;
use strict;
use JSON::RPC::Constants qw(:all);
use JSON::RPC::Parser;
use JSON::RPC::Procedure;
use Router::Simple;
use Scalar::Util;
use Try::Tiny;

use Class::Accessor::Lite
    rw => [ qw(
        coder
        handlers
        parser
        prefix
        router
    ) ]
;

sub new {
    my ($class, @args) = @_;
    my $self = bless {
        handlers => {},
        @args,
    }, $class;
    if (! $self->{coder}) {
        require JSON;
        $self->{coder} = JSON->new->utf8;
    }
    if (! $self->{parser}) {
        $self->{parser} = JSON::RPC::Parser->new( coder => $self->coder )
    }
    if (! $self->{router}) {
        $self->{router} = Router::Simple->new;
    }
    return $self;
}

sub guess_handler_class {
    my ($self, $klass) = @_;

    my $prefix = $self->prefix || '';
    return "$prefix\::$klass";
}

sub construct_handler {
    my ($self, $klass) = @_;

    my $handler = $self->handlers->{ $klass };
    if (! $handler) {
        eval "require $klass";
        die if $@;
        $handler = $klass->new();
        $self->handlers->{$klass} = $handler;
    }
    return $handler;
}

sub get_handler {
    my ($self, $klass) = @_;

    if ( Scalar::Util::blessed( $klass )){
        if (JSONRPC_DEBUG > 1) {
            warn "Handler is already object : $klass";
        }
        return $klass;
    }

    if ($klass !~ s/^\+//) {
        $klass = $self->guess_handler_class( $klass );
    }

    my $handler = $self->construct_handler( $klass );
    if (JSONRPC_DEBUG > 1) {
        warn "$klass -> $handler";
    }
    return $handler;
}

sub handle_psgi {
    my ($self, $req, @args) = @_;

    if ( ! Scalar::Util::blessed($req) ) {
        # assume it's a PSGI hash
        require Plack::Request;
        $req = Plack::Request->new($req);
    }

    my @response;
    my $procedures;
    try {
        $procedures = $self->parser->construct_from_req( $req );
        if (@$procedures <= 0) {
            push @response, {
                error => {
                    code => RPC_INVALID_REQUEST,
                    message => "Could not find any procedures"
                }
            };
        }
    } catch {
        my $e = $_;
        if (JSONRPC_DEBUG) {
            warn "error while creating jsonrpc request: $e";
        }
        if ($e =~ /Invalid parameter/) {
            push @response, {
                error => {
                    code => RPC_INVALID_PARAMS,
                    message => "Invalid parameters",
                }
            };
        } elsif ( $e =~ /parse error/ ) {
            push @response, {
                error => {
                    code => RPC_PARSE_ERROR,
                    message => "Failed to parse json",
                }
            };
        } else {
            push @response, {
                error => {
                    code => RPC_INVALID_REQUEST,
                    message => $e
                }
            }
        }
    };

    my $router = $self->router;
    foreach my $procedure (@$procedures) {
        if ( ! $procedure->{method} ) {
            my $message = "Procedure name not given";
            if (JSONRPC_DEBUG) {
                warn $message;
            }
            push @response, {
                error => {
                    code => RPC_METHOD_NOT_FOUND,
                    message => $message,
                }
            };
            next;
        }

        my $matched = $router->match( $procedure->{method} );
        if (! $matched) {
            my $message = "Procedure '$procedure->{method}' not found";
            if (JSONRPC_DEBUG) {
                warn $message;
            }
            push @response, {
                error => {
                    code => RPC_METHOD_NOT_FOUND,
                    message => $message,
                }
            };
            next;
        }

        my $action = $matched->{action};
        try {
            my ($ip, $ua);
            if (JSONRPC_DEBUG > 1) {
                warn "Procedure '$procedure->{method}' maps to action $action";
                $ip = $req->address || 'N/A';
                $ua = $req->user_agent || 'N/A';
            }
            my $params = $procedure->params;
            my $handler = $self->get_handler( $matched->{handler} );

            my $code = $handler->can( $action );
            if (! $code) {
                if ( JSONRPC_DEBUG ) {
                    warn "[INFO] handler $handler does not implement method $action!.";
                }
                die "Internal Error";
            }
            my $result = $code->( $handler, $procedure->params, $procedure, @args );
            if (JSONRPC_DEBUG) {
                warn "[INFO] action=$action "
                    . "params=["
                    . (ref $params ? $self->{coder}->encode($params) : $params)
                    . "] ret="
                    . (ref $result ? $self->{coder}->encode($result) : $result)
                    . " IP=$ip UA=$ua";
            }

            push @response, {
                jsonrpc => '2.0',
                result  => $result,
                id      => $procedure->id,
            };
        } catch {
            my $e = $_;
            if (JSONRPC_DEBUG) {
                warn "Error while executing $action: $e";
            }
            my $error = {code => RPC_INTERNAL_ERROR} ;
            if (ref $e eq "HASH") {
               $error->{message} = $e->{message},
               $error->{data}    = $e->{data},
            } else {
               $error->{message} = $e,
            }
            push @response, {
                jsonrpc => '2.0',
                id      => $procedure->id,
                error   => $error,
            };
        };
    }
    my $res = $req->new_response(200);
    $res->content_type( 'application/json; charset=utf8' );
    $res->body(
        $self->coder->encode( @$procedures > 1 ? \@response : $response[0] )
    );

    return $res->finalize;
}

no Try::Tiny;

1;

__END__

=head1 NAME

JSON::RPC::Dispatch - Dispatch JSON RPC Requests To Handlers

=head1 SYNOPSIS

    use JSON::RPC::Dispatch;

    my $router = Router::Simple->new; # or use Router::Simple::Declare
    $router->connect( method_name => {
        handler => $class_name_or_instance,
        action  => $method_name_to_invoke
    );

    my $dispatch = JSON::RPC::Dispatch->new(
        router => $router
    );

    sub psgi_app {
        $dispatch->handle_psgi( $env );
    }

=head1 DESCRIPTION

See docs in L<JSON::RPC> for details

=cut
