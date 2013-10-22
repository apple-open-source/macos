package JSON::RPC::Parser;
use strict;
use JSON::RPC::Procedure;
use Carp ();
use Plack::Request;
use Class::Accessor::Lite
    new => 1,
    rw => [ qw(
        coder
    ) ]
;

sub construct_procedure {
    my $self = shift;
    JSON::RPC::Procedure->new( @_ );
}

sub construct_from_req {
    my ($self, $req) = @_;

    my $method = $req->method;
    my $proc;
    if ($method eq 'POST') {
        $proc = $self->construct_from_post_req( $req );
    } elsif ($method eq 'GET') {
        $proc = $self->construct_from_get_req( $req );
    } else {
        Carp::croak( "Invalid method: $method" );
    }

    return $proc;
}

sub construct_from_post_req {
    my ($self, $req) = @_;

    my $request = eval { $self->coder->decode( $req->content ) };
    if ($@) {
        Carp::croak( "JSON parse error: $@" );
    }

    my $ref = ref $request;
    if ($ref ne 'ARRAY') {
        $request = [ $request ];
    }

    my @procs;
    foreach my $req ( @$request ) {
        Carp::croak( "Invalid parameter") unless ref $req eq 'HASH';
        push @procs, $self->construct_procedure(
            method => $req->{method},
            id     => $req->{id},
            params => $req->{params},
        );
    }
    return \@procs;
}

sub construct_from_get_req {
    my ($self, $req) = @_;

    my $params = $req->query_parameters;
    my $decoded_params;
    if ($params->{params}) {
        $decoded_params = eval { $self->coder->decode( $params->{params} ) };
    }
    return [
        $self->construct_procedure(
            method => $params->{method},
            id     => $params->{id},
            params => $decoded_params
        )
    ];
}

1;

__END__

=head1 NAME

JSON::RPC::Parser - Parse JSON RPC Requests from Plack::Request

=head1 SYNOPSIS

    use JSON::RPC::Parser;

    my $parser = JSON::RPC::Parser->new(
        coder => JSON->new
    );
    my $procedure = $parser->construct_from_req( $request );

=head1 DESCRIPTION

Constructs a L<JSON::RPC::Procedure> object from a Plack::Request object

=cut
