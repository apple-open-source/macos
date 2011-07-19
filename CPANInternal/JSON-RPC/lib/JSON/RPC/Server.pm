##############################################################################
# JSONRPC version 1.1
# http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html
##############################################################################

use strict;
use JSON ();
use Carp ();

use HTTP::Request ();
use HTTP::Response ();


##############################################################################

package JSON::RPC::Server;

my $JSONRPC_Procedure_Able;

BEGIN {
    if ($] >= 5.006) {
        require  JSON::RPC::Procedure;
        $JSONRPC_Procedure_Able = 1;
    }
}


$JSON::RPC::Server::VERSION = '0.92';


BEGIN {
    for my $method (qw/request path_info json version error_message max_length charset content_type
                        error_response_header return_die_message/)
    {
        eval qq|
            sub $method {
                \$_[0]->{$method} = \$_[1] if defined \$_[1];
                \$_[0]->{$method};
            }
        |;
    }
}


sub create_json_coder {
    JSON->new->utf8; # assumes UTF8
}


sub new {
    my $class = shift;

    bless {
        max_length    => 1024 * 100,
        charset       => 'UTF-8',
        content_type  => 'application/json',
        json          => $class->create_json_coder,
        loaded_module => { name  => {}, order => [], },
        @_,
    }, $class;
}


*dispatch_to = *dispatch; # Alias


sub dispatch {
    my ($self, @arg) = @_;

    if (@arg == 0){
        Carp::carp "Run test mode...";
    }
    elsif (@arg > 1) {
        for my $pkg (@arg) {
            $self->_load_module($pkg);
        }
    }
    else {
        if (ref $arg[0] eq 'ARRAY') {
            for my $pkg (@{$arg[0]}) {
                $self->_load_module($pkg);
            }
        }
        elsif (ref $arg[0] eq 'HASH') { # Lazy loading
            for my $path (keys %{$arg[0]}) {
                my $pkg = $arg[0]->{$path};
                $self->{dispatch_path}->{$path} = $pkg;
            }
        }
        elsif (ref $arg[0]) {
            Carp::croak 'Invalid dispatch value.';
        }
        else { # Single module
            $self->_load_module($arg[0]);
        }
    }

    $self;
}


sub handle {
    my ($self) = @_;
    my ($obj, $res, $jsondata);

    if ($self->request->method eq 'POST') {
        $jsondata = $self->retrieve_json_from_post();
    }
    elsif ($self->request->method eq 'GET') {
        $jsondata = $self->retrieve_json_from_get();
    }

    if ( $jsondata ) {
        $obj = eval q| $self->json->decode($jsondata) |;
        if ($@) {
            $self->raise_error(code => 201, message => "Can't parse JSON data.");
        }
    }
    else { # may have error_response_header at retroeve_json_from_post / get
        unless ($self->error_response_header) {
            $self->error_response_header($self->response_header(403, 'No data.'));
        }
    }

    if ($obj) {
        $res = $self->_handle($obj);
        unless ($self->error_response_header) {
            return $self->response( $self->response_header(200, $res) );
        }
    }

    $self->response( $self->error_response_header );
}


sub retrieve_json_from_post {  }    # must be implemented in subclass


sub retrieve_json_from_get {  }     # must be implemented in subclass


sub response {  }                   # must be implemented in subclass



sub raise_error {
    my ($self, %opt) = @_;
    my $status_code = $opt{status_code} || 200;

    if (exists $opt{version} and $opt{version} ne '1.1') {
        $self->version(0);
    }
    else {
        $self->version(1.1);
    }

    my $res = $self->_error($opt{id}, $opt{code}, $opt{message});

    $self->error_response_header($self->response_header($status_code, $res));

    return;
}


sub response_header {
    my ($self, $code, $result) = @_;
    my $h = HTTP::Headers->new;
    $h->header('Content-Type' => $self->content_type . '; charset=' . $self->charset);
    HTTP::Response->new($code => undef, $h, $result);
}


sub _handle {
    my ($self, $obj) = @_;

    $obj->{version} ? $self->version(1.1) : $self->version(0);

    my $method = $obj->{method};

    if (!defined $method) {
        return $self->_error($obj->{id}, 300, "method is nothing.");
    }
    elsif ($method =~ /[^-._a-zA-Z0-9]/) {
        return $self->_error($obj->{id}, 301, "method is invalid.");
    }

    my $procedure = $self->_find_procedure($method);

    unless ($procedure) {
        return $self->_error($obj->{id}, 302, "No such a method : '$method'.");
    }

    my $params;

    unless ($obj->{version}) {
        unless ($obj->{params} and ref($obj->{params}) eq 'ARRAY') {
            return $self->_error($obj->{id}, 400, "Invalid params for JSONRPC 1.0.");
        }
    }

    unless ($params = $self->_argument_type_check($procedure->{argument_type}, $obj->{params})) {
        return $self->_error($obj->{id}, 401, $self->error_message);
    }

    my $result;

    if ($obj->{version}) {
        $result = ref $params ? eval q| $procedure->{code}->($self, $params) |
                              : eval q| $procedure->{code}->($self) |
                              ;
    }
    else {
        my @params;
        if(ref($params) eq 'ARRAY') {
            @params = @$params;
        }
        else {
            $params[0] = $params;
        }
        $result = eval q| $procedure->{code}->($self, @params) |;
    }


    if ($self->error_response_header) {
        return;
    }
    elsif ($@) {
        return $self->_error($obj->{id}, 500, ($self->return_die_message ? $@ : 'Procedure error.'));
    }

    if (!$obj->{version} and !defined $obj->{id}) { # notification
        return '';
    }

    my $return_obj = {result => $result};

    if ($obj->{version}) {
        $return_obj->{version} = '1.1';
    }
    else {
        $return_obj->{error} = undef;
        $return_obj->{id}    = $obj->{id};
    }

    return $self->json->encode($return_obj);
}


sub _find_procedure {
    my ($self, $method) = @_;
    my $found;
    my $classname;
    my $system_call;

    if ($method =~ /^system\.(\w+)$/) {
        $system_call = 1;
        $method = $1;
    }
    elsif ($method =~ /\./) {
        my @p = split/\./, $method;
        $method = pop @p;
        $classname=  join('::', @p);
    }

    if ($self->{dispatch_path}) {
        my $path = $self->{path_info};

        if (my $pkg = $self->{dispatch_path}->{$path}) {

            return if ( $classname and $pkg ne $classname );
            return if ( $JSONRPC_Procedure_Able and JSON::RPC::Procedure->can( $method ) );

            $self->_load_module($pkg);

            if ($system_call) { $pkg .= '::system' }

            return $self->_method_is_ebable($pkg, $method, $system_call);
        }
    }
    else {
        for my $pkg (@{$self->{loaded_module}->{order}}) {

            next if ( $classname and $pkg ne $classname );
            next if ( $JSONRPC_Procedure_Able and JSON::RPC::Procedure->can( $method ) );

            if ($system_call) { $pkg .= '::system' }

            if ( my $ret = $self->_method_is_ebable($pkg, $method, $system_call) ) {
                return $ret;
            }
        }
    }

    return;
}


sub _method_is_ebable {
    my ($self, $pkg, $method, $system_call) = @_;

    my $allowable_procedure = $pkg->can('allowable_procedure');
    my $code;

    if ( $allowable_procedure ) {
        if ( exists $allowable_procedure->()->{ $method } ) {
            $code = $allowable_procedure->()->{ $method };
        }
        else {
            return;
        }
    }

    if ( $code or ( $code = $pkg->can($method) ) ) {
        return {code =>  $code} if ($system_call or !$JSONRPC_Procedure_Able);

        if ( my $procedure = JSON::RPC::Procedure::check($pkg, $code) ) {
            return if ($procedure->{return_type} and $procedure->{return_type} eq 'Private');
            $procedure->{code} = $code;
            return $procedure;
        }
    }

    if ($system_call) { # if not found, default system.foobar
        if ( my $code = 'JSON::RPC::Server::system'->can($method) ) {
            return {code => $code};
        }
    }

    return;
}


sub _argument_type_check {
    my ($self, $type, $params) = @_;

    unless (defined $type) {
        return defined $params ? $params : 1;
    }

    my $regulated;

    if (ref $params eq 'ARRAY') {
        if (@{$type->{position}} != @$params) {
            $self->error_message("Number of params is mismatch.");
            return;
        }

        if (my $hash = $type->{names}) {
            my $i = 0;
            for my $name (keys %$hash) {
                $regulated->{$name} = $params->[$i++];
            }
        }

    }
    elsif (ref $params eq 'HASH') {
        if (@{$type->{position}} != keys %$params) {
            $self->error_message("Number of params is mismatch.");
            return;
        }

        if (my $hash = $type->{names}) {
            my $i = 0;
            for my $name (keys %$params) {
                if ($name =~ /^\d+$/) {
                    my $realname = $type->{position}[$name];
                    $regulated->{$realname} = $params->{$name};
                }
                else {
                    $regulated->{$name} = $params->{$name};
                }
            }
        }

    }
    elsif (!defined $params) {
        if (@{$type->{position}} != 0) {
            $self->error_message("Number of params is mismatch.");
            return;
        }
        return 1;
    }
    else {
            $self->error_message("the params member is any other type except JSON Object or Array.");
            return;
    }

    return $regulated ? $regulated : $params;
}


sub _load_module {
    my ($self, $pkg) = @_;

    eval qq| require $pkg |;

    if ($@) {
        Carp::croak $@;
    }

    $self->{loaded_module}->{name}->{$pkg} = $pkg;
    push @{ $self->{loaded_module}->{order} }, $pkg;
}


# Error Handling

sub _error {
    my ($self, $id, $code, $message) = @_;

    if ($self->can('translate_error_message')) {
        $message = $self->translate_error_message($code, $message);
    }

    my $error_obj = {
        name    => 'JSONRPCError',
        code    => $code,
        message => $message,
    };

    my $obj;

    if ($self->version) {
        $obj = {
            version => "1.1",
            error   => $error_obj,
        };
        $obj->{id} = $id if (defined $id);
    }
    else {
        return '' if (!defined $id);
        $obj = {
            result => undef,
            error  => $message,
            id     => $id,
        };
    }

    return $self->json->encode($obj);
}


##############################################################################

package JSON::RPC::Server::system;

sub describe {
    {
        sdversion => "1.0",
        name      => __PACKAGE__,
        summary   => 'Default system description',
    }
}


1;
__END__

=pod


=head1 NAME

JSON::RPC::Server - Perl implementation of JSON-RPC sever

=head1 SYNOPSIS


 # CGI version
 use JSON::RPC::Server::CGI;
 
 my $server = JSON::RPC::Server::CGI->new;

 $server->dispatch_to('MyApp')->handle();
 
 
 
 # Apache version
 # In apache conf
 
 PerlRequire /your/path/start.pl
 PerlModule MyApp
 
 <Location /jsonrpc/API>
      SetHandler perl-script
      PerlResponseHandler JSON::RPC::Server::Apache
      PerlSetVar dispatch "MyApp"
      PerlSetVar return_die_message 0
 </Location>
 
 
 
 # Daemon version
 use JSON::RPC::Server::Daemon;
 
 JSON::RPC::Server::Daemon->new(LocalPort => 8080);
                          ->dispatch({'/jsonrpc/API' => 'MyApp'})
                          ->handle();
 
 
 
 # FastCGI version
 use JSON::RPC::Server::FastCGI;
 
 my $server = JSON::RPC::Server::FastCGI->new;
 
    $server->dispatch_to('MyApp')->handle();



=head1 DESCRIPTION

Gets a client request.

Parses its JSON data.

Passes the server object and the object decoded from the JSON data to your procedure (method).

Takes your returned value (scalar or arrayref or hashref).

Sends a response.

Well, you write your procedure code only.


=head1 METHODS

=over

=item new

Creates new JSON::RPC::Server object.


=item dispatch($package)

=item dispatch([$package1, $package1, ...])

=item dispatch({$path => $package, ...})

Sets your procedure module using package name list or arrayref or hashref.
Hashref version is used for path_info access.





=item dispatch_to

An alias to C<dispatch>.


=item handle

Runs server object and returns a response.


=item raise_error(%hash)

 return $server->raise_error(
    code => 501,
    message => "This is error in my procedure."
 );

Sets an error.
An error code number in your procedure is an integer between 501 and 899.


=item json

Setter/Getter to json encoder/decoder object.
The default value is L<JSON> object in the below way:

 JSON->new->utf8

In your procedure, changes its behaviour.

 $server->json->utf8(0);

The JSON coder creating method is  C<create_json_coder>.


=item version

Setter/Getter to JSON-RPC protocol version used by a client.
If version is 1.1, returns 1.1. Otherwise returns 0.


=item charset

Setter/Getter to cahrset.
Default is 'UTF-8'.


=item content_type

Setter/Getter to content type.
Default is 'application/json'.


=item return_die_message

When your program dies in your procedure,
sends a return object with errror message 'Procedure error' by default.

If this option is set, uses C<die> message.


 sub your_procedure {
     my ($s) = @_;
    $s->return_die_message(1);
    die "This is test.";
 }



=item retrieve_json_from_post

It is used by JSON::RPC::Server subclass.


=item retrieve_json_from_get

In the protocol v1.1, 'GET' request method is also allowable.

It is used by JSON::RPC::Server subclass.

=item response

It is used by JSON::RPC::Server subclass.

=item request

Returns L<HTTP::Request> object.

=item path_info

Returns PATH_INFO.

=item max_length

Returns max content-length to your application.


=item translate_error_message

Implemented in your subclass.
Three arguments (server object, error code and error message) are passed.
It must return a message.

 sub translate_error_message {
     my ($s, $code, $message) = @_;
     return $translation_jp_message{$code};
 }


=item create_json_coder

(Class method)
Returns a JSON de/encoder in C<new>.
You can override it to use your favorite JSON de/encode.


=back


=head1 RESERVED PROCEDURE

When a client call a procedure (method) name 'system.foobar',
JSON::RPC::Server look up MyApp::system::foobar.

L<http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html#ProcedureCall>

L<http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html#ServiceDescription>

There is JSON::RPC::Server::system::describe for default response of 'system.describe'.


=head1 SEE ALSO

L<JSON>

L<http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html>

L<http://json-rpc.org/wiki/specification>

=head1 AUTHOR

Makamaka Hannyaharamitu, E<lt>makamaka[at]cpan.orgE<gt>


=head1 COPYRIGHT AND LICENSE

Copyright 2007-2008 by Makamaka Hannyaharamitu

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut


