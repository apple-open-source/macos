##############################################################################
package JSON::RPC::Server::Apache2;

use strict;

use lib qw(/var/www/cgi-bin/json/);
use base qw(JSON::RPC::Server);

use Apache2::Const -compile => qw(OK HTTP_BAD_REQUEST SERVER_ERROR);

use APR::Table ();
use Apache2::RequestRec ();
use Apache2::RequestIO ();
use Apache2::RequestUtil ();


$JSON::RPC::Server::Apache::VERSION = '0.05';


sub handler {
    my($r) = @_;

    my $s = __PACKAGE__->new;

    $s->request($r);

    $s->{path_info} = $r->path_info;

    my @modules = $r->dir_config('dispatch') || $r->dir_config('dispatch_to');

    $s->return_die_message( $r->dir_config('return_die_message') );

    $s->dispatch([@modules]);

    $s->handle(@_);

    Apache2::Const::OK;
}


sub new {
    my $class = shift;
    return $class->SUPER::new();
}


sub retrieve_json_from_post {
    my $self = shift;
    my $r    = $self->request;
    my $len  = $r->headers_in()->get('Content-Length');

    return if($r->method ne 'POST');
    return if($len > $self->max_length);

    my ($buf, $content);

    while( $r->read($buf,$len) ){
        $content .= $buf;
    }

    $content;
}


sub retrieve_json_from_get {
    my $self = shift;
    my $r    = $self->request;
    my $args = $r->args;

    $args = '' if (!defined $args);

    $self->{path_info} = $r->path_info;

    my $params = {};

    $self->version(1.1);

    for my $pair (split/&/, $args) {
        my ($key, $value) = split/=/, $pair;
        if ( defined ( my $val = $params->{ $key } ) ) {
            if ( ref $val ) {
                push @{ $params->{ $key } }, $value;
            }
            else { # change a scalar into an arrayref
                $params->{ $key } = [];
                push @{ $params->{ $key } }, $val, $value;
            }
        }
        else {
            $params->{ $key } = $value;
        }
    }

    my $method = $r->path_info;

    $method =~ s{^.*/}{};
    $self->{path_info} =~ s{/?[^/]+$}{};

    $self->json->encode({
        version => '1.1',
        method  => $method,
        params  => $params,
    });
}


sub response {
    my ($self, $response) = @_;
    my $r = $self->request;

    $r->content_type($self->content_type);
    $r->print($response->content);

    return ($response->code == 200)
            ? Apache2::Const::OK : Apache2::Const::SERVER_ERROR;
}



1;
__END__


=pod


=head1 NAME

JSON::RPC::Server::Apache2 - JSON-RPC sever for mod_perl2

=head1 SYNOPSIS

 # In apache conf
 
 PerlRequire /your/path/start.pl
 PerlModule MyApp
 
 <Location /jsonrpc/API>
      SetHandler perl-script
      PerlResponseHandler JSON::RPC::Server::Apache
      PerlSetVar dispatch "MyApp"
      PerlSetVar return_die_message 0
 </Location>
 
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

Gets a client request.

Parses its JSON data.

Passes the server object and the object decoded from the JSON data to your procedure (method).

Takes your returned value (scalar or arrayref or hashref).

Sends a response.

Well, you write your procedure code only.


=head1 METHODS

They are inherited from the L<JSON::RPC::Server> methods basically.
The below methods are implemented in JSON::RPC::Server::Apache2.

=over

=item new

Creates new JSON::RPC::Server::Apache2 object.

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


