##############################################################################
package JSON::RPC::Server::Daemon;

use strict;
use JSON::RPC::Server; # for old Perl 5.005
use base qw(JSON::RPC::Server);

$JSON::RPC::Server::Daemon::VERSION = '0.03';

use Data::Dumper;

sub new {
    my $class = shift;
    my $self  = $class->SUPER::new();
    my $pkg;

    if(  grep { $_ =~ /^SSL_/ } @_ ){
        $self->{_daemon_pkg} = $pkg = 'HTTP::Daemon::SSL';
    }
    else{
        $self->{_daemon_pkg} = $pkg = 'HTTP::Daemon';
    }
    eval qq| require $pkg; |;
    if($@){ die $@ }

    $self->{_daemon} ||= $pkg->new(@_) or die;

    return $self;
}


sub handle {
    my $self = shift;
    my %opt  = @_;
    my $d    = $self->{_daemon} ||= $self->{_daemon_pkg}->new(@_) or die;

    while (my $c = $d->accept) {
        $self->{con} = $c;
        while (my $r = $c->get_request) {
            $self->request($r);
            $self->path_info($r->url->path);
            $self->SUPER::handle();
            last;
        }
        $c->close;
    }

}


sub retrieve_json_from_post {
    return $_[0]->request->content;
}


sub retrieve_json_from_get {
}


sub response {
    my ($self, $response) = @_;
    $self->{con}->send_response($response);
}

1;
__END__


=head1 NAME

JSON::RPC::Server::Daemon - JSON-RPC sever for daemon

=head1 SYNOPSIS

 # Daemon version
 #--------------------------
 # In your daemon server script
 use JSON::RPC::Server::Daemon;
 
 JSON::RPC::Server::Daemon->new(LocalPort => 8080);
                          ->dispatch({'/jsonrpc/API' => 'MyApp'})
                          ->handle();
 
 #--------------------------
 # In your application class
 package MyApp;
 
 use base qw(JSON::RPC::Procedure); # Perl 5.6 or more than
 
 sub echo : Public {    # new version style. called by clients
     # first argument is JSON::RPC::Server object.
     return $_[1];
 }
 
 sub sum : Public(a:num, b:num) { # sets value into object member a, b.
     my ($s, $obj) = @_;
     # return a scalar value or a hashref or an arryaref.
     return $obj->{a} + $obj->{b};
 }
 
 sub a_private_method : Private {
     # ... can't be called by client
 }
 
 sub sum_old_style {  # old version style. taken as Public
     my ($s, @arg) = @_;
    return $arg[0] + $arg[1];
 }

=head1 DESCRIPTION

This module is for http daemon servers using L<HTTP::Daemon> or L<HTTP::Daemon::SSL>.

=head1 METHODS

They are inherited from the L<JSON::RPC::Server> methods basically.
The below methods are implemented in JSON::RPC::Server::Daemon.

=over

=item new

Creates new JSON::RPC::Server::Daemon object.
Arguments are passed to L<HTTP::Daemon> or L<HTTP::Daemon::SSL>.

=item handle

Runs server object and returns a response.

=item retrieve_json_from_post

retrieves a JSON request from the body in POST method.

=item retrieve_json_from_get

In the protocol v1.1, 'GET' request method is also allowable.
it retrieves a JSON request from the query string in GET method.

=item response

returns a response JSON data to a client.

=back

=head1 SEE ALSO

L<HTTP::Daemon>,

L<HTTP::Daemon::SSL>,

L<JSON::RPC::Server>,

L<JSON::RPC::Procedure>,

L<JSON>,

L<http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html>,

L<http://json-rpc.org/wiki/specification>,

=head1 AUTHOR

Makamaka Hannyaharamitu, E<lt>makamaka[at]cpan.orgE<gt>


=head1 COPYRIGHT AND LICENSE

Copyright 2007-2008 by Makamaka Hannyaharamitu

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut


