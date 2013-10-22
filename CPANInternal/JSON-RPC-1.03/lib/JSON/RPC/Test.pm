package JSON::RPC::Test;
use strict;
use parent qw(Exporter);
our @EXPORT = qw(test_rpc);

sub test_rpc {
    if (ref $_[0] && @_ == 2) {
        @_ = (dispatch => $_[0], client => $_[1]);
    }

    my %args = @_;
    my $dispatch = delete $args{dispatch};
    $args{app} = sub {
        $dispatch->handle_psgi(@_);
    };

    @_ = %args;
    goto \&Plack::Test::test_psgi;
}

1;

=head1 NAME

JSON::RPC::Test - Simple Wrapper To Test Your JSON::RPC

=head1 SYNOPSIS

    use JSON::RPC::Test;

    test_rpc $dispatch, sub {
        ...
    };

    # or
    test_rpc
        dispatch => $dispatch,
        client   => sub {
            ...
        }
    ;

=cut
