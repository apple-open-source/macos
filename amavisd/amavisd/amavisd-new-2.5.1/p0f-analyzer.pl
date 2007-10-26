#!/usr/bin/perl -T

#------------------------------------------------------------------------------
# This is p0f-analyzer.pl, a program to continuously read log reports from p0f
# utility, keep results in cache for a couple of minutes, and answer queries
# over UDP from some program (like amavisd-new) about collected data.
#
# Author: Mark Martinec <mark.martinec@ijs.si>
# Copyright (C) 2006  Mark Martinec,  All Rights Reserved.
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
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
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
  use Socket;
  use vars qw($VERSION);
  $VERSION = '1.310';

# Example usage:
#   p0f -i bge0 -l 'tcp dst port 25' 2>&1 | p0f-analyzer.pl 2345
#
# In the p0f filter expression above specify an IP address of this host where
# your MTA is listening for incoming mail, in place of host.example.com above.
# Match the UDP port number (like 2345 above) with the port to which a client
# will be sending queries ($os_fingerprint_method in amavisd.conf).

  # argument should be a free UDP port where queries will be accepted on
  $ARGV[0] =~ /^[0-9]+\z/  or die <<'EOD';
Specify a valid UDP port as an argument.

Usage:
  p0f-analyzer.pl <udp-port>

Example usage:
  p0f -l 'tcp dst port 25' 2>&1 | p0f-analyzer.pl 2345
EOD

  my($port) = untaint($ARGV[0]);

# my($bind_addr) = '0.0.0.0';       # bind to all IPv4 interfaces
  my($bind_addr) = '127.0.0.1';     # bind just to a loopback interface

  my(@inet_acl) = qw( 127.0.0.1 );  # list of IP addresses from which queries
                                    # will be accepted, others are ignored
  my($retention_time) = 10*60;    # time to keep collected information in cache
  my($debug) = 0;                   # nonzero enables log messages to STDERR

  printf STDERR ("p0f-analyzer version %s starting.\n", $VERSION)  if $debug;
  printf STDERR ("listening on UDP port %s, allowed queries from: %s\n",
                 $port, join(", ",@inet_acl))  if $debug;
  socket(S, PF_INET, SOCK_DGRAM, getprotobyname('udp')) or die "socket: $!";

  my($packed_addr);
  $packed_addr = inet_aton($bind_addr)
    or die "inet_aton: bad IP address [$bind_addr]: $!";
  bind(S, sockaddr_in($port,$packed_addr))
    or die "binding to [$bind_addr] failed: $!";
  my($fn_sock) = fileno(S); my($fn_stdin) = fileno(STDIN);
  my($rin,$rout); $rin = '';
  vec($rin,$fn_sock,1) = 1; vec($rin,$fn_stdin,1) = 1;
  my(%src); my($cnt_since_cleanup) = 0;
  binmode(STDIN)  or die "Can't set STDIN binmode: $!";
  for (;;) {
    my($nfound,$timeleft) = select($rout=$rin, undef, undef, undef);
    my($now) = time;
    if (vec($rout,$fn_sock,1)) {
      my($port,$iaddr,$paddr,$clientaddr); my($inbuf);
      $paddr = recv(S,$inbuf,64,0);
      defined $paddr or die "recv: $!";
      ($port,$iaddr) = sockaddr_in($paddr)  if $paddr ne '';
      $clientaddr = inet_ntoa($iaddr)  if defined $iaddr;
      if (!defined($clientaddr)) {
        printf STDERR ("query from unknown client\n")  if $debug;
      } elsif (!grep {$_ eq $clientaddr} @inet_acl) {
        printf STDERR ("query from non-approved client: %s:%s\n",
                       $clientaddr,$port)  if $debug;
      } elsif ($port < 1024 || $port == 2049 || $port > 65536) {
        printf STDERR ("query from non-approved port: %s:%s\n",
                       $clientaddr,$port)  if $debug;
      } elsif ($inbuf !~ /^(\d+\.\d+\.\d+\.\d+) (.*)$/s) {
        printf STDERR ("invalid query syntax from %s:%s\n",
                       $clientaddr,$port)  if $debug;
      } else {
        my($src_ip,$nonce) = ($1,$2);
        if (length($nonce) > 1024) {
          printf STDERR ("invalid query from %s:%s, nonce too long: %d chrs\n",
                         $clientaddr,$port,length($nonce))  if $debug;
        } elsif ($nonce !~ /^([\040-\177].*)\z/s) {
          printf STDERR ("invalid query from %s:%s, forbidden char in nonce\n",
                         $clientaddr,$port)  if $debug;
        } else {
          printf STDERR ("query from  %s:%s: %s\n",
                         $clientaddr,$port,$inbuf)  if $debug;
          my($resp) = ''; my($timestamp);
          if (exists($src{$src_ip})) {
            for my $e (@{$src{$src_ip}}) {
              $timestamp = $e->{t};
              if ($resp eq '') { $resp = $e->{d} }
              elsif ($e->{d} eq $resp) {}
              else {  # keep the longest common string head
                my($j);  my($resp_l) = length($resp);
                for ($j=0; $j<$resp_l; $j++)
                  { last  if substr($e->{d},$j,1) ne substr($resp,$j,1) }
                if ($j < $resp_l) {
#                 printf STDERR ("TRUNCATED to %d: %s %s => /%s/\n",
#                                $j, $resp, $e->{d}, substr($resp,0,$j));
                  $resp = substr($resp,0,$j);
                }
              }
              last;
            }
          }
          $resp = $src_ip.' '.$nonce.' '.$resp;
          printf STDERR ("response to %s:%s: %s\n",
                         $clientaddr,$port,$resp)  if $debug;
          defined(send(S, $resp."\015\012", 0, $paddr)) or die "send: $!";
        }
      }
    }
    if (vec($rout,$fn_stdin,1)) {
      my($line);  my($nbytes) = sysread(STDIN,$line,1024);
      defined $nbytes or die "Read: $!";
      last if $nbytes == 0;  # eof
      chomp($line); $cnt_since_cleanup++;
      $line =~ /^(\d+\.\d+\.\d+\.\d+):(\d+)[ -]*(.*)
                  \ ->\ (\d+\.\d+\.\d+\.\d+):(\d+)\s*(.*)$/x or next;
      my($src_ip,$src_port,$src_t,$dst_ip,$dst_port,$src_d) =
        ($1,$2,$3,$4,$5,$6);
      my($descr) = "$src_t, $src_d";
      if (!exists($src{$src_ip})) {
        printf STDERR ("first: %s %s %.70s\n",
                       $src_ip, $src_port, $descr)  if $debug >= 2;
        $src{$src_ip} = [ { t=>$now, p=>$src_port, c=>1, d=>$descr } ]
      } else {
        my($found) = 0;
        for my $e (@{$src{$src_ip}}) {
          if ($e->{d} eq $descr) {
            $e->{c}++; $e->{p} = '*'; $e->{t} = $now, $found = 1;
            printf STDERR ("deja-vu: %s %d, cnt=%d %.70s\n",
                           $src_ip,$src_port,$e->{c},$descr)  if $debug >= 2;
            last;
          }
        }
        if (!$found) {
          push(@{$src{$src_ip}}, { p=>$src_port, c=>1, d=>$descr });
          printf STDERR ("new: %s %d %.70s\n",
                         $src_ip,$src_port,$descr)  if $debug >= 2;
        }
      }
      if ($cnt_since_cleanup > 50) {
        for my $ip (keys %src) {
          my(@kept) = grep { $_->{t} + $retention_time >= $now } @{$src{$ip}};
          if (!@kept) {
            printf STDERR ("EXPIRED: %s, age = %d s\n",
                           $ip, $now - $src{$ip}[0]{t})  if $debug >= 2;
            delete $src{$ip};
          } elsif (@kept != @{$src{$ip}}) {
            printf STDERR ("SHRUNK: %s\n", $ip)  if $debug >= 2;
            @{$src{$ip}} = @kept;
          }
        }
        $cnt_since_cleanup = 0;
      }
    }
  }
  print STDERR "normal termination\n"  if $debug;
  exit 0;

# Return untainted copy of a string (argument can be a string or a string ref)
sub untaint($) {
  no re 'taint';
  my($str);
  if (defined($_[0])) {
    local($1); # avoid Perl taint bug: tainted global $1 propagates taintedness
    $str = $1  if (ref($_[0]) ? ${$_[0]} : $_[0]) =~ /^(.*)\z/s;
  }
  $str;
}
