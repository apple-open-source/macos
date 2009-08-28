#!/usr/bin/perl

# loclist.pl -- check a list of hostnames for LOC records

#  -v  -- verbose output (include NO results).  used to be the default
#  -n  -- try looking for network LOC records as well (slower)
#  -r  -- try doing reverse-resolution on IP-appearing hosts
#  -d  -- debugging output

# egrep 'loc2earth.*host' /serv/www/logs/wn.log |
#    perl -pe 's/^.*host=//; s/([a-zA-Z0-9.-]+).*/$1/' |
#    sort -u | ~/loclist.pl > loc.sites

use Net::DNS '0.08';
use Getopt::Std;

getopts('vnrd');

$res = new Net::DNS::Resolver;

line:
    foreach $_ (<>) {
      chomp;
      $foundloc = $namefound = 0;

      next line if m/^$/;
      next line if m/[^\w.-\/+_]/; # /, +, _ not actually valid in hostnames

      print STDERR "$_ DEBUG looking up...\n" if $opt_d;

      if (m/^\d+\.\d+\.\d+\.\d+$/) {
	if ($opt_r) {
	  $query = $res->query($_);
	  
	  if (defined ($query)) {
	    foreach $ans ($query->answer) {
	      if ($ans->type eq "PTR") {
		$_ = $ans->ptrdname;
		$namefound++;
	      }
	    }
	  }
	}
	next line unless $namefound;
      }

      $query = $res->query($_,"LOC");

      if (defined ($query)) {	# then we got an answer of some sort
	foreach $ans ($query->answer) {
	  if ($ans->type eq "LOC") {
	    print "$_ YES ",$ans->rdatastr,"\n";
	    $foundloc++;
	  }
	}
      }
      if ($opt_n && !$foundloc) {		# try the RFC 1101 search bit
	@addrs = @netnames = ();
	$query = $res->query($_,"A");
	if (defined ($query)) {
	  foreach $ans ($query->answer) {
	    if ($ans->type eq "A") {
	      push(@addrs,$ans->address);
	    }
	  }
	}
	if (@addrs) {
	checkaddrs:
	  foreach $ipstr (@addrs) {
	    $ipnum = unpack("N",pack("CCCC",split(/\./,$ipstr,4)));
	    ($ip1) = split(/\./,$ipstr);
	    if ($ip1 >= 224) { # class D/E, treat as host addr
	      $mask = 0xFFFFFFFF;
	    } elsif ($ip1 >= 192) { # "class C"
	      $mask = 0xFFFFFF00;
	    } elsif ($ip1 >= 128) { # "class B"
	      $mask = 0xFFFF0000;
	    } else {	# class A
	      $mask = 0xFF000000;
	    }
	    $oldmask = 0;
	    while ($oldmask != $mask) {
	      $oldmask = $mask;
	      $querystr =
		  join(".", reverse (unpack("CCCC",pack("N",$ipnum & $mask))))
		      . ".in-addr.arpa";
	      $query = $res->query($querystr,"PTR");
	      if (defined ($query)) {
		foreach $ans ($query->answer) {
		  if ($ans->type eq "PTR") {
		    # we want the list in LIFO order
		    unshift(@netnames,$ans->ptrdname);
		  }
		}
		$query = $res->query($querystr,"A");
		if (defined ($query)) {
		  foreach $ans ($query->answer) {
		    if ($ans->type eq "A") {
		      $mask = unpack("L",pack("CCCC",
					      split(/\./,$ans->address,4)));
		    }
		  }
		}
	      }
	    }
	    if (@netnames) {
	      foreach $network (@netnames) {
		$query = $res->query($network,"LOC");
		if (defined ($query)) {
		  foreach $ans ($query->answer) {
		    if ($ans->type eq "LOC") {
		      print "$_ YES ",$ans->rdatastr,"\n";
		      $foundloc++;
		      last checkaddrs;
		    } elsif ($ans->type eq "CNAME") {
		      # XXX should follow CNAME chains here
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
      if ($opt_v && !$foundloc) {
	print "$_ NO\n";
      }
    }
