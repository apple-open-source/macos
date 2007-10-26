#!/usr/bin/perl -T

#------------------------------------------------------------------------------
# This is amavis.pl, a simple demonstrational program functionally much like
# the amavis.c helper program, but talks the new AM.PDP protocol with the
# amavisd daemon. See README.protocol for the description of AM.PDP protocol.
# Usage:
#   amavis.pl sender recip1 recip2 ...  < message.txt
# To be placed in amavisd.conf:
#   $protocol='AM.PDP';  $unix_socketname='/var/amavis/amavisd.sock';
#
#
# Author: Mark Martinec <mark.martinec@ijs.si>
# Copyright (C) 2004  Mark Martinec,  All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of the author, nor the name of the "Jozef Stefan"
#   Institute, nor the names of contributors may be used to endorse or
#   promote products derived from this software without specific prior
#   written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVERREADME.protocol CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#(the license above is the new BSD license, and pertains to this program only)
#
# Patches and problem reports are welcome.
# The latest version of this program is available at:
#   http://www.ijs.si/software/amavisd/
#------------------------------------------------------------------------------

use strict;
use re 'taint';
use IO::Socket;
use Digest::MD5;
use Time::HiRes ();

use vars qw($VERSION);  $VERSION = 1.000;
use vars qw($socketname);

# $socketname = '127.0.0.1:9998';
  $socketname = '/var/amavis/amavisd.sock';

sub sanitize_str {
  my($str, $keep_eol) = @_;
  my(%map) = ("\r" => '\\r', "\n" => '\\n', "\f" => '\\f', "\t" => '\\t',
              "\b" => '\\b', "\e" => '\\e', "\\" => '\\\\');
  if ($keep_eol) {
    $str =~ s/([^\012\040-\133\135-\176])/  # and \240-\376 ?
              exists($map{$1}) ? $map{$1} :
                     sprintf(ord($1)>255 ? '\\x{%04x}' : '\\%03o', ord($1))/eg;
  } else {
    $str =~ s/([^\040-\133\135-\176])/      # and \240-\376 ?
              exists($map{$1}) ? $map{$1} :
                     sprintf(ord($1)>255 ? '\\x{%04x}' : '\\%03o', ord($1))/eg;
  }
  $str;
}

sub do_log($$) {
  my($level, $errmsg) = @_;
  print STDERR sanitize_str($errmsg),"\n";
}

sub proto_decode($) {
  my($str) = @_;
  $str =~ s/%([0-9a-fA-F]{2})/pack("C",hex($1))/eg;
  $str;
}

sub proto_encode($@) {
  my($attribute_name,@strings) = @_; local($1);
  $attribute_name =~    # encode all but alfanumerics, '_' and '-'
    s/([^0-9a-zA-Z_-])/sprintf("%%%02x",ord($1))/eg;
  for (@strings) {      # encode % and nonprintables
    s/([^\041-\044\046-\176])/sprintf("%%%02x",ord($1))/eg;
  }
  $attribute_name . '=' . join(' ',@strings);
}

sub ask_amavisd($$) {
  my($sock,$query_ref) = @_;
  my(@encoded_query) =
    map { /^([^=]+)=(.*)\z/s; proto_encode($1,$2) } @$query_ref;
  do_log(0,'> '.$_)  for @encoded_query;
  $sock->print( map { $_."\015\012" } (@encoded_query,'') )
    or die "Can't write response to socket: $!";
  $sock->flush or die "Can't flush on socket: $!";
  my(%attr);
  local($/) = "\015\012";    # set line terminator to CRLF
  # must not use \r and \n, which may not be \015 and \012 on certain platforms
  do_log(0,"waiting for response");
  while(<$sock>) {
    last  if /^\015\012\z/;  # end of response
    if (/^ ([^=\000\012]*?) (=|:[ \t]*) ([^\012]*?) \015\012 \z/xsi) {
      my($attr_name) = proto_decode($1);
      my($attr_val)  = proto_decode($3);
      if (!exists $attr{$attr_name}) { $attr{$attr_name} = [] }
      push(@{$attr{$attr_name}}, $attr_val);
    }
  }
  if (!defined($_) && $! != 0) { die "read from socket failed: $!" }
  \%attr;
}

# Main program starts here

  die "Usage:  amavis.pl sender recip1 recip2 ...  < message.txt\n" if !@ARGV;
  my($sock);
  my($is_inet) = $socketname=~m{^/} ? 0 : 1; # simpleminded: unix vs. inet sock
  if ($is_inet) {   # inet socket
    $sock = IO::Socket::INET->new($socketname)
      or die "Can't connect to INET socket $socketname: $!";
  } else {          # unix socket
    $sock = IO::Socket::UNIX->new(Type => SOCK_STREAM)
      or die "Can't create UNIX socket: $!";
    $sock->connect( pack_sockaddr_un($socketname) )
      or die "Can't connect to UNIX socket $socketname: $!";
  }

  # generate some semi-unique directory name; not good enough for production
  my($ctx) = Digest::MD5->new; # 128 bits (32 hex digits)
  $ctx->add(sprintf("%s %.9f %s", $$, Time::HiRes::time, join(',',@ARGV)));
  my($id) = substr($ctx->b64digest,0,16);  $id =~ tr{+/}{-.};

  my($tempdir) = "/var/amavis/amavis-milter-$id";
  my($fname) = "$tempdir/email.txt";
  mkdir($tempdir,0750) or die "Can't create directory $tempdir: $!";

  # copy message from stdin to a file email.txt in the temporary directory
  open(F,">$fname") or die "Can't create file $fname: $!";
  while (<STDIN>) { print F $_  or die "Can't write to $fname: $!" }
  close(F) or die "Can't close $fname: $!";

  my(@query) = (
    'request=AM.PDP',
    "mail_file=$fname",
    "tempdir=$tempdir",
    'tempdir_removed_by=server',
    'sender='.shift @ARGV,
    map {"recipient=$_"} @ARGV,
#   'protocol_name=ESMTP',
#   'helo_name=b.example.com',
#   'client_address=10.2.3.4',
  );
  my($attr_ref) = ask_amavisd($sock,\@query);
  for my $attr_name (keys %$attr_ref) {
    for my $attr_val (@{$attr_ref->{$attr_name}})
      { do_log(0,"< $attr_name=$attr_val") }
  }
  # may do another query here if needed ...
  $sock->close or die "Can't close socket: $!";
  close(STDIN) or die "Can't close STDIN: $!";
  my($exit_code) = shift @{$attr_ref->{'exit_code'}};
  $exit_code = 0  if $exit_code==99;  # same thing in this case, both is ok
  exit 0+$exit_code;
