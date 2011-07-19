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
  use Errno qw(EAGAIN EINTR);
  use Socket;
  use vars qw($VERSION);
  $VERSION = '1.400';

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

  do_log(1, "p0f-analyzer version %s starting", $VERSION);
  do_log(1, "listening on UDP port %s, allowed queries from: %s",
            $port, join(", ",@inet_acl));
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
      if (!defined($paddr)) {
        if ($!==EAGAIN || $!==EINTR) {
          # false alarm, nothing can be read
        } else {
          die "recv: $!";
        }
      } else {
        ($port,$iaddr) = sockaddr_in($paddr)  if $paddr ne '';
        $clientaddr = inet_ntoa($iaddr)  if defined $iaddr;
        if (!defined($clientaddr)) {
          do_log(1, "query from unknown client");
        } elsif (!grep {$_ eq $clientaddr} @inet_acl) {
          do_log(1, "query from non-approved client: %s:%s",$clientaddr,$port);
        } elsif ($port < 1024 || $port == 2049 || $port > 65535) {
          do_log(1, "query from questionable port: %s:%s",  $clientaddr,$port);
        } elsif ($inbuf !~ /^([^ ]+) (.*)$/s) {
          do_log(1, "invalid query syntax from %s:%s", $clientaddr,$port);
        } else {
          my($query,$nonce) = ($1,$2);  my($src_ip,$src_port);
          if ($query =~ /^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\z/s) {
            $src_ip = $query; $src_port = 0;  # old style query
          } elsif ($query =~ /^ \[ ([^\]]*) \] (?: : (\d{1,5}) )? \z/xs) {
            $src_ip = $1; $src_port = $2;
            if ($src_ip =~ /^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\z/) {
              # IPv4
            } elsif ($src_ip =~ /^ (?: IPv6: )? [0-9a-f]{0,4}
                       (?: : [0-9a-f]{0,4} | \. [0-9]{1,3} ){2,8} \z/xsi) {
              $src_ip =~ s/^IPv6://i;
            } else { undef $src_ip }
          }
          $src_port = 0  if !defined($src_port);
          if (length($nonce) > 1024) {
            do_log(1, "invalid query from %s:%s, nonce too long: %d chrs",
                      $clientaddr,$port,length($nonce));
          } elsif ($nonce !~ /^([\040-\177].*)\z/s) {
            do_log(1, "invalid query from %s:%s, forbidden char in nonce",
                      $clientaddr,$port);
          } elsif (!defined($src_ip) || $src_port > 65535) {
            do_log(1, "invalid query from %s:%s, bad IP address or port: %s",
                      $clientaddr,$port,$query);
          } else {
            do_log(1, "query from  %s:%s: %s", $clientaddr,$port,$inbuf);
            my($resp) = '';
            if (exists($src{$src_ip})) {
              if ($src_port > 0) {  # source port known, must match exactly
                $resp = $src{"[$src_ip]:$src_port"}{d}
                  if exists $src{"[$src_ip]:$src_port"};
              } else {  # source port not known, find the closest match
                for my $e (@{$src{$src_ip}}) {
                  if ($resp eq '') { $resp = $e->{d} }
                  elsif ($e->{d} eq $resp) {}
                  else {  # keep the longest common string head
                    my($j);  my($resp_l) = length($resp);
                    for ($j=0; $j<$resp_l; $j++)
                      { last  if substr($e->{d},$j,1) ne substr($resp,$j,1) }
                    if ($j < $resp_l) {
#                     do_log(1, "TRUNCATED to %d: %s %s => /%s/",
#                               $j, $resp, $e->{d}, substr($resp,0,$j));
                      $resp = substr($resp,0,$j);
                    }
                  }
                  last;
                }
              }
            }
            $resp = $query.' '.$nonce.' '.$resp;
            do_log(1, "response to %s:%s: %s", $clientaddr,$port,$resp);
            defined(send(S, $resp."\015\012", 0, $paddr)) or die "send: $!";
          }
        }
      }
    }
    if (vec($rout,$fn_stdin,1)) {
      $cnt_since_cleanup++; my($line); $! = 0;
      my($nbytes) = sysread(STDIN,$line,1024);
      if (!defined($nbytes)) {
        if ($!==EAGAIN || $!==EINTR) {
          # false alarm, nothing can be read
        } else {
          die "Read: $!";
        }
      } elsif ($nbytes < 1) {  # sysread returns 0 at eof
        last;  # eof
      } else {
        chomp($line);
        local($1,$2,$3,$4,$5,$6);
        $line =~ /^(\d+\.\d+\.\d+\.\d+):(\d+)[ -]*(.*)
                   \ ->\ (\d+\.\d+\.\d+\.\d+):(\d+)\s*(.*)$/x or next;
        my($src_ip,$src_port,$src_t,$dst_ip,$dst_port,$src_d) =
          ($1,$2,$3,$4,$5,$6);
        my($descr) = "$src_t, $src_d";
        my($entry) = { t=>$now, p=>$src_port, c=>1, d=>$descr };
        $src{"[$src_ip]:$src_port"} = $entry;
        if (!exists($src{$src_ip})) {
          do_log(2, "first: [%s]:%s %.70s", $src_ip,$src_port,$descr);
          $src{$src_ip} = [ $entry ];
        } else {
          my($found) = 0;
          for my $e (@{$src{$src_ip}}) {
            if ($e->{d} eq $descr) {
              $e->{c}++; $e->{p} = '*'; $e->{t} = $now, $found = 1;
              do_log(2, "deja-vu: [%s]:%s, cnt=%d %.70s",
                        $src_ip,$src_port,$e->{c},$descr);
              last;
            }
          }
          if (!$found) {
            push(@{$src{$src_ip}}, $entry);
            do_log(2, "stored: [%s]:%d %.70s", $src_ip,$src_port,$descr);
          }
        }
      }
      if ($cnt_since_cleanup > 50) {
        for my $k (keys %src) {
          if (ref($src{$k}) ne 'ARRAY') {
            if ($src{$k}{t} + $retention_time < $now) {
              do_log(2, "EXPIRED: %s, age = %d s", $k, $now - $src{$k}{t});
              delete $src{$k};
            }
          } else {
            my(@kept) = grep { $_->{t} + $retention_time >= $now } @{$src{$k}};
            if (!@kept) {
              do_log(2, "EXPIRED: %s, age = %d s", $k, $now - $src{$k}[0]{t});
              delete $src{$k};
            } elsif (@kept != @{$src{$k}}) {
              do_log(2, "SHRUNK: %s, %d -> %d",
                        $k, scalar(@{$src{$k}}), scalar(@kept));
              @{$src{$k}} = @kept;
            }
          }
        }
        $cnt_since_cleanup = 0;
      }
    }
  }
  do_log(1, "normal termination");
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

# write log entry
sub do_log($$;@) {
  my($level,$errmsg,@args) = @_;
  if ($level <= $debug) {
    $errmsg = sprintf($errmsg,@args)  if @args;
    print STDERR $errmsg,"\n";
  }
  1;
}
