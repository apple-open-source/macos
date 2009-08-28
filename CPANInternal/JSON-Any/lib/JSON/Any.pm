##############################################################################
# JSON::Any
# v1.16
# Copyright (c) 2007 Chris Thompson
##############################################################################

package JSON::Any;

use warnings;
use strict;
use Carp;

=head1 NAME

JSON::Any - Wrapper Class for the various JSON classes.

=head1 VERSION

Version 1.16

=cut

our $VERSION = '1.16';

our $UTF8;

my ( %conf, $handler, $encoder, $decoder );
use constant HANDLER => 0;
use constant ENCODER => 1;
use constant DECODER => 2;
use constant UTF8    => 3;

BEGIN {
    %conf = (
        json => {
            encoder       => 'encode_json',
            decoder       => 'decode_json',
            create_object => sub {
                require utf8;
                utf8->import();
                JSON->import( '-support_by_pp', '-no_export' );
                my ( $self, $conf ) = @_;
                my @params = qw(
                  ascii
                  latin1
                  utf8
                  pretty
                  indent
                  space_before
                  space_after
                  relaxed
                  canonical
                  allow_nonref
                  allow_blessed
                  convert_blessed
                  filter_json_object
                  shrink
                  max_depth
                  max_size
                  loose
                  allow_bignum
                  allow_barekey
                  allow_singlequote
                  escape_slash
                  indent_length
                  sort_by
                );
                local $conf->{utf8} = !$conf->{utf8};    # it means the opposite
                my $obj = $handler->new;

                for my $mutator (@params) {
                    next unless exists $conf->{$mutator};
                    $obj = $obj->$mutator( $conf->{$mutator} );
                }

                $self->[ENCODER] = 'encode';
                $self->[DECODER] = 'decode';
                $self->[HANDLER] = $obj;
            },
        },
        json_dwiw => {
            encoder       => 'to_json',
            decoder       => 'from_json',
            create_object => sub {
                my ( $self, $conf ) = @_;
                my @params = qw(bare_keys);
                croak "JSON::DWIW does not support utf8" if $conf->{utf8};
                $self->[ENCODER] = 'to_json';
                $self->[DECODER] = 'from_json';
                $self->[HANDLER] =
                  $handler->new( { map { $_ => $conf->{$_} } @params } );
            },
        },
        json_xs_1 => {
            encoder       => 'to_json',
            decoder       => 'from_json',
            create_object => sub {
                my ( $self, $conf ) = @_;

                my @params = qw(
                  ascii
                  utf8
                  pretty
                  indent
                  space_before
                  space_after
                  canonical
                  allow_nonref
                  shrink
                  max_depth
                );

                my $obj = $handler->new;
                for my $mutator (@params) {
                    next unless exists $conf->{$mutator};
                    $obj = $obj->$mutator( $conf->{$mutator} );
                }
                $self->[ENCODER] = 'encode';
                $self->[DECODER] = 'decode';
                $self->[HANDLER] = $obj;
            },
        },
        json_xs_2 => {
            encoder       => 'encode_json',
            decoder       => 'decode_json',
            create_object => sub {
                require utf8;
                utf8->import();
                my ( $self, $conf ) = @_;

                my @params = qw(
                  ascii
                  latin1
                  utf8
                  pretty
                  indent
                  space_before
                  space_after
                  relaxed
                  canonical
                  allow_nonref
                  allow_blessed
                  convert_blessed
                  filter_json_object
                  shrink
                  max_depth
                  max_size
                );

                local $conf->{utf8} = !$conf->{utf8};    # it means the opposite

                my $obj = $handler->new;
                for my $mutator (@params) {
                    next unless exists $conf->{$mutator};
                    $obj = $obj->$mutator( $conf->{$mutator} );
                }
                $self->[ENCODER] = 'encode';
                $self->[DECODER] = 'decode';
                $self->[HANDLER] = $obj;
            },
        },
        json_syck => {
            encoder       => 'Dump',
            decoder       => 'Load',
            create_object => sub {
                my ( $self, $conf ) = @_;
                croak "JSON::Syck does not support utf8" if $conf->{utf8};
                $self->[ENCODER] = sub { Dump(@_) };
                $self->[DECODER] = sub { Load(@_) };
                $self->[HANDLER] = 'JSON::Syck';
              }
        },
    );

    # JSON::PC claims it has the same API as JSON
    $conf{json_pc} = $conf{json};
}

sub _make_key {
    my $handler = shift;
    ( my $key = lc($handler) ) =~ s/::/_/g;
    if ( 'json_xs' eq $key ) {
        no strict 'refs';
        $key .= "_" . ( split /\./, ${"$handler\::VERSION"} )[0];
    }
    return $key;
}

sub import {
    my $class = shift;
    my @order = @_;

    ( $handler, $encoder, $decoder ) = ();

    @order = split /\s/, $ENV{JSON_ANY_ORDER}
      if !@order and $ENV{JSON_ANY_ORDER};
    @order = qw(XS JSON DWIW Syck) unless @order;

    foreach my $testmod (@order) {
        $testmod = "JSON::$testmod" unless $testmod eq "JSON";
        eval "require $testmod";
        unless ($@) {
            $handler = $testmod;
            my $key = _make_key($handler);
            $encoder = $conf{$key}->{encoder};
            $decoder = $conf{$key}->{decoder};
            last;
        }
    }

    croak "Couldn't find a JSON Package."   unless $handler;
    croak "Couldn't find a decoder method." unless $decoder;
    croak "Couldn't find a encoder method." unless $encoder;
}

=head1 SYNOPSIS

This module will provide a coherent API to bring together the various JSON
modules currently on CPAN. This module will allow you to code to any JSON API
and have it work regardless of which JSON module is actually installed.

	use JSON::Any;

	my $j = JSON::Any->new;

	$json = $j->objToJson({foo=>'bar', baz=>'quux'});
	$obj = $j->jsonToObj($json);

or

	$json = $j->encode({foo=>'bar', baz=>'quux'});
	$obj = $j->decode($json);

or

	$json = $j->Dump({foo=>'bar', baz=>'quux'});
	$obj = $j->Load($json);

or

	$json = $j->to_json({foo=>'bar', baz=>'quux'});
	$obj = $j->from_json($json);

or without creating an object:

	$json = JSON::Any->objToJson({foo=>'bar', baz=>'quux'});
	$obj = JSON::Any->jsonToObj($json);

On load, JSON::Any will find a valid JSON module in your @INC by looking 
for them in this order:

	JSON::XS 
	JSON 
	JSON::DWIW 
	JSON::Syck

And loading the first one it finds.

You may change the order by specifying it on the C<use JSON::Any> line:

	use JSON::Any qw(DWIW Syck XS JSON);

Specifying an order that is missing one of the modules will prevent that module from being used:

	use JSON::Any qw(DWIW XS JSON);

This will check in that order, and will never attempt to load JSON::Syck. This can also be set via
the $ENV{JSON_ANY_ORDER} environment variable.

WARNING: If you call JSON::Any with an empty list

    use JSON::Any ();
    
It will skip the JSON package detection routines and will die loudly that it couldn't find a package.

=head1 FUNCTIONS

=over

=item C<new>

Will take any of the parameters for the underlying system and pass them through. 
However these values don't map between JSON modules, so, from a portability
standpoint this is really only helpful for those paramters that happen
to have the same name. This will be addressed in a future release.

The one parameter that is universally supported (to the extent that is
supported by the underlying JSON modules) is C<utf8>. When this parameter is
enabled all resulting JSON will be marked as unicode, and all unicode strings
in the input data structure will be preserved as such.

The actual output will vary, for example L<JSON> will encode and decode unicode
chars (the resulting JSON is not unicode) wheras L<JSON::XS> will emit unicode
JSON.

=back

=cut

sub new {
    my $class = shift;
    my $self  = bless [], $class;
    my $key   = _make_key($handler);
    if ( my $creator = $conf{$key}->{create_object} ) {
        my @config = @_;
        if ( $ENV{JSON_ANY_CONFIG} ) {
            push @config, map { split /=/, $_ } split /,\s*/,
              $ENV{JSON_ANY_CONFIG};
        }
        $creator->( $self, my $conf = {@config} );
        $self->[UTF8] = $conf->{utf8};
    }
    return $self;
}

=over

=item C<handlerType>

Takes no arguments, returns a string indicating which JSON Module is in use.

=back

=cut

sub handlerType {
    my $class = shift;
    $handler;
}

=over

=item C<handler>

Takes no arguments, if called on an object returns the internal JSON::* 
object in use.  Otherwise returns the JSON::* package we are using for 
class methods.

=back

=cut

sub handler {
    my $self = shift;
    if ( ref $self ) {
        return $self->[HANDLER];
    }
    return $handler;
}

=over

=item C<objToJson>

Takes a single argument, a hashref to be converted into JSON.
It returns the JSON text in a scalar.

=back

=cut

sub objToJson {
    my $self = shift;
    my $obj  = shift;
    croak 'must provide object to convert' unless defined $obj;

    my $json;

    if ( ref $self ) {
        my $method;
        unless ( ref $self->[ENCODER] ) {
            croak "No $handler Object created!" unless exists $self->[HANDLER];
            $method = $self->[HANDLER]->can( $self->[ENCODER] );
            croak "$handler can't execute $self->[ENCODER]" unless $method;
        }
        else {
            $method = $self->[ENCODER];
        }
        $json = $self->[HANDLER]->$method($obj);
    }
    else {
        $json = $handler->can($encoder)->($obj);
    }

    utf8::decode($json)
      if ( ref $self ? $self->[UTF8] : $UTF8 )
      and !utf8::is_utf8($json)
      and utf8::valid($json);
    return $json;
}

=over

=item C<to_json>

=item C<Dump>

=item C<encode>

Aliases for objToJson, can be used interchangeably, regardless of the 
underlying JSON module.

=back

=cut

*to_json = \&objToJson;
*Dump    = \&objToJson;
*encode  = \&objToJson;

=over

=item C<jsonToObj>

Takes a single argument, a string of JSON text to be converted
back into a hashref.

=back

=cut

sub jsonToObj {
    my $self = shift;
    my $obj  = shift;
    croak 'must provide json to convert' unless defined $obj;

    if ( ref $self ) {
        my $method;
        unless ( ref $self->[DECODER] ) {
            croak "No $handler Object created!" unless exists $self->[HANDLER];
            $method = $self->[HANDLER]->can( $self->[DECODER] );
            croak "$handler can't execute $self->[DECODER]" unless $method;
        }
        else {
            $method = $self->[DECODER];
        }
        return $self->[HANDLER]->$method($obj);
    }
    $handler->can($decoder)->($obj);
}

=over

=item C<from_json>

=item C<Load>

=item C<decode>

Aliases for jsonToObj, can be used interchangeably, regardless of the 
underlying JSON module.

=back

=cut

*from_json = \&jsonToObj;
*Load      = \&jsonToObj;
*decode    = \&jsonToObj;

=head1 AUTHOR

Chris Thompson, C<< <cthom at cpan.org> >>
Chris Prather, C<< <perigrin at cpan.org> >>

=head1 BUGS

Please report any bugs or feature requests to
C<bug-json-any at rt.cpan.org>, or through the web interface at
L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=JSON-Any>.
I will be notified, and then you'll automatically be notified of progress on
your bug as I make changes.

=head1 ACKNOWLEDGEMENTS

This module came about after discussions on irc.perl.org about the fact 
that there were now six separate JSON perl modules with different interfaces.

In the spirit of Class::Any, JSON::Any was created with the considerable 
help of Matt 'mst' Trout.

Simon Wistow graciously supplied a patch for backwards compat with JSON::XS 
versions previous to 2.01

San Dimas High School Football Rules!

=head1 COPYRIGHT & LICENSE

Copyright 2007 Chris Thompson, some rights reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=cut

1;    # End of JSON::Any
