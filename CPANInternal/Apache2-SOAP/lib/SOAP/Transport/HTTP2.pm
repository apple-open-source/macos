# ======================================================================
#
# Copyright (C) 2000-2004 Paul Kulchenko (paulclinger@yahoo.com)
# SOAP::Lite is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.
#
# $Id: HTTP2.pm,v 1.1.1.1 2007/10/26 20:44:35 mleasure Exp $
#
# ======================================================================

package SOAP::Transport::HTTP2;

use strict;
use vars qw($VERSION @ISA);
#$VERSION = sprintf("%d.%s", map {s/_//g; $_} q$Name:  $ =~ /-(\d+)_([\d_]+)/);
$VERSION = 0.72;

use SOAP::Lite;
use SOAP::Transport::HTTP;

@ISA = qw(SOAP::Transport::HTTP);

# ======================================================================

package SOAP::Transport::HTTP2::Client;

use vars qw(@ISA);
@ISA = qw(SOAP::Transport::HTTP::Client);

# ======================================================================

package SOAP::Transport::HTTP2::Server;

use vars qw(@ISA);
@ISA = qw(SOAP::Transport::HTTP::Server);

# ======================================================================

package SOAP::Transport::HTTP2::CGI;

use vars qw(@ISA);
@ISA = qw(SOAP::Transport::HTTP::CGI);

# ======================================================================

package SOAP::Transport::HTTP2::Daemon;

use vars qw(@ISA);
@ISA = qw(SOAP::Transport::HTTP::Daemon);

# ======================================================================

package SOAP::Transport::HTTP2::Apache;

use vars qw(@ISA);
@ISA = qw(SOAP::Transport::HTTP::Server);

sub DESTROY { SOAP::Trace::objects('()') }

sub new {
  my $self = shift;
  unless (ref $self) {
    my $class = ref($self) || $self;
    $self = $class->SUPER::new(@_);
    SOAP::Trace::objects('()');
  }
 MOD_PERL: {
    ( (exists $ENV{MOD_PERL_API_VERSION}) &&
      ($ENV{MOD_PERL_API_VERSION} == 2) ) and do {
      require Apache2::RequestRec;
      require Apache2::RequestUtil;
      require Apache2::RequestIO;
      require Apache2::Const;
      require APR::Table;
      Apache2::Const->import(-compile => 'OK');
      $self->{'MOD_PERL_VERSION'} = 2;
      last MOD_PERL;
    };
    (eval { require Apache;} ) and do {
       require Apache::Constants;
       Apache::Constants->import('OK');
       $self->{'MOD_PERL_VERSION'} = 1;
       last MOD_PERL;
     };
    die "Unsupported version of mod_perl";
  }
  return $self;
}

sub handler { 
  my $self = shift->new; 
  my $r = shift;
  unless ($r) {
    $r = ($self->{'MOD_PERL_VERSION'} == 1) ?
      Apache->request : Apache2::RequestUtil->request();
  }

  my $cl = ($self->{'MOD_PERL_VERSION'} == 1) ?
    $r->header_in('Content-length') : $r->headers_in->{'Content-length'};
  $self->request(HTTP::Request->new(
    $r->method() => $r->uri,
    HTTP::Headers->new($r->headers_in),
    do { my ($c,$buf); while ($r->read($buf,$cl)) { $c.=$buf; } $c; }
  ));
  $self->SUPER::handle;

  # we will specify status manually for Apache, because
  # if we do it as it has to be done, returning SERVER_ERROR,
  # Apache will modify our content_type to 'text/html; ....'
  # which is not what we want.
  # will emulate normal response, but with custom status code 
  # which could also be 500.
  if ($self->{'MOD_PERL_VERSION'} == 1 ) {
    $self->response->headers->scan(sub { $r->header_out(@_) });
    $r->send_http_header(join '; ', $self->response->content_type);
    $r->print($self->response->content);
    return &Apache::Constants::OK;
  }
  else {
    $self->response->headers->scan(sub {
                                     my %h = @_;
                                     for (keys %h) {
                                       $r->headers_out->{$_} = $h{$_};
                                     }
                                   });
    $r->content_type(join '; ', $self->response->content_type);
    $r->print($self->response->content);
    return &Apache2::Const::OK;
  }
}

sub configure {
  my $self = shift->new;
  my $config = shift->dir_config;
  foreach (%$config) {
    $config->{$_} =~ /=>/
      ? $self->$_({split /\s*(?:=>|,)\s*/, $config->{$_}})
      : ref $self->$_() ? () # hm, nothing can be done here
                        : $self->$_(split /\s+|\s*,\s*/, $config->{$_})
      if $self->can($_);
  }
  $self;
}

{ sub handle; *handle = \&handler } # just create alias

# ======================================================================
#
# Copyright (C) 2001 Single Source oy (marko.asplund@kronodoc.fi)
# a FastCGI transport class for SOAP::Lite.
#
# ======================================================================

package SOAP::Transport::HTTP2::FCGI;

use vars qw(@ISA);
@ISA = qw(SOAP::Transport::HTTP::FCGI);

# ======================================================================

1;
