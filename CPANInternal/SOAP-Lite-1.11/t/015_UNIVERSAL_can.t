#!/usr/bin/perl -w
use Test;
BEGIN {
    if ($] < 5.008001) {
        print "# +autodispatch broken in 5.8.0\n";
        plan tests => 0;
        exit 0;
    }
}


package foo;

sub new {
  my $class = shift;
  my $self  = {count => 0,};
  bless $self, $class;
}

sub bar {
  my $self = shift;
  ++$self->{count};
}

sub proxy {
  my $self = shift;
  my $meth = shift;
  if ($self->can($meth)) {
    return $self->$meth;
  } else {
    return;
  }
}

# This code works if it's a regular class

package main;
use Test;
plan tests => 3;
my $f = new foo();

use SOAP::Lite +autodispatch =>
   uri => 'http://example.org/foo',
   proxy => 'http://example.org.cgi';


ok $f->bar() == 1;
if ($f->can("bar")) { 
    ok $f->bar == 2; 
}
if ($f->can("proxy")) { 
    ok $f->proxy("bar") == 3; 
}