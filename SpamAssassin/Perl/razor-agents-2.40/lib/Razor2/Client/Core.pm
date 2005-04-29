#!/usr/bin/perl -sw
##
## Razor2::Client::Core - Vipul's Razor Client API
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: Core.pm,v 1.1 2004/04/19 17:50:31 dasenbro Exp $

package Razor2::Client::Core;


    use strict;
    use IO::Socket;
    use IO::Select;
    use Errno qw(:POSIX);

    use Razor2::Client::Version; 
    use Data::Dumper;
    use vars qw( $VERSION $PROTOCOL );
    use base qw(Razor2::String);
    use base qw(Razor2::Logger);
    use base qw(Razor2::Client::Engine);
    use base qw(Razor2::Errorhandler);
    use Razor2::Client::Version;
    use Razor2::String qw(hextobase64 makesis parsesis hmac_sha1 xor_key 
                          prep_mail debugobj to_batched_query 
                          from_batched_query hexbits2hash 
                          fisher_yates_shuffle);


($VERSION) = do { my @r = (q$Revision: 1.1 $ =~ /\d+/g); sprintf "%d."."%02d" x $#r, @r };
$PROTOCOL = $Razor2::Client::Version::PROTOCOL; 


sub new { 
    my ($class, $conf, %params) = @_;
    my $self = {};
    bless $self, $class;
    $self->debug ("Razor Agents $VERSION, protocol version $PROTOCOL.");
    return $self;
} 



#
# We store server-specific config info for each server we know about.
# All info about razor servers is stored in $self->{s}.
#
# Basically we get the server name/ip from {list}, 
# load that server's specific info from {allconfs} into {conf},
# and do stuff.  If server is no good, we get nextserver from {list}
#
# $self->{s}->{list}      ptr to {nomination} if report,revoke; or {catalogue} if check
#                         or the cmd-line server (-rs server)
# $self->{s}->{new_list}  set to 1 when discover gets new lists
# $self->{s}->{catalogue} array ref containing catalogue servers
# $self->{s}->{nomination}array ref containing nomination servers
# $self->{s}->{discovery} array ref containing discovery servers 
#
# $self->{s}->{modified}  array ref containing servers whose .conf needs updating
# $self->{s}->{modified_lst} array ref containing which .lst files need updating
#
# $self->{s}->{ip}        string containing ip (or dns name) of current server from {list})
# $self->{s}->{port}      string containing port, taken from server:port from {list}
# $self->{s}->{engines}   engines supported, derived from {conf}->{se}
# $self->{s}->{conf}      hash ref containing current server's config params
#                         read from $razorhome/server.$ip.conf
#
# $self->{s}->{allconfs}  hash ref of all servers' configs.  key={ip}, val={conf}
#                         as read from server.*.conf file
#
# $self->{s}->{listfile}  string containing path/file of server.lst, either 
#                         nomination or catalogue depending $self->{breed}
# $self->{conf}->{listfile_discovery}  string containing path/file of discovery server
#
# NOTE: if we are razor-check, server is Catalogue Server
#       otherwise server is Nomination server.
#
# everytime we update our server list, $self->{s}->{list};
# we want to write that to disk - $self->{s}->{listfile}  
#
sub nextserver {
    my ($self) = @_;
    $self->log (16,"entered nextserver");

    # see if we need to discover (.lst files might be too old)
    $self->discover() or return $self->errprefix ("nextserver");

    # first time we don't remove from list
    # or if we've rediscovered.
    shift @{$self->{s}->{list}} unless ($self->{s}->{new_list} || !$self->{s}->{ip});
    $self->{s}->{new_list} = 0;

    my $next = ${$self->{s}->{list}}[0];

    # do we ever want to put current back on the end of list?
    # push @{$self->{s}->{list}}, $self->{s}->{ip}; 

    if ($next) {
        ($self->{s}->{port}) = $next =~ /:(.*)$/;
        $next =~ s/:.*$//;         # optional
        $self->{s}->{ip}   = $next;  # ip can be IP or DNS name
        $self->{s}->{port} ||= $self->{conf}->{port} || 2703;
        $self->{s}->{conf} = $self->{s}->{allconfs}->{$next};

        my $svrport = "$self->{s}->{ip}:$self->{s}->{port}";

        # get rid of server specific stuff
        delete $self->{s}->{greeting};

        unless (ref($self->{s}->{conf})) {
            # never used this server before, no cached info. go get it!
            $self->{s}->{conf} = {};
            $self->connect;   # calls parse_greeting which calls compute_server_conf
        } else {
            $self->compute_server_conf(1);  # computes supported engines, logs info
        }
        $self->writeservers();

        my $srl = defined($self->{s}->{conf}->{srl}) ? $self->{s}->{conf}->{srl} : "<unknown>";
        $self->log(8, "Using next closest server $svrport, cached info srl $srl");
        #$self->logobj(11, "Using next closest server $svrport, cached info", $self->{s}->{conf});
        return 1;

    } else {
        return $self->error ("Razor server $self->{opt}->{server} not available at this time")
            if $self->{opt}->{server};
        $self->{force_discovery} = 1;
        if ($self->{done_discovery} && !($self->discover)) {
            return $self->errprefix("No Razor servers available at this time");
        }
        return $self->nextserver;
    }
}

sub load_at_runtime {
    my ($self,$class,$sub,$args) = @_;
    
    $sub = 'new' unless defined $sub;
    $args = ""   unless defined $args;

    eval "use $class";
    if ($@) {
        $self->log(2,"$class not found, please to fix.");
        return $self->error("\n\n$@");
    }
    my $evalstr;
    if ($sub && $sub ne "new") {
        $evalstr = $class ."::$sub($args);";
    } else {
        $evalstr = $class . "->new($args)"; 
    }
    if (my $dude = eval $evalstr) {
        $self->log(12,"Found and evaled $evalstr ==> $dude");
        return $dude;
    } else {
        $self->log(5,"Found but problem (bad args?) with $evalstr");
        return $self->error("Problem with $evalstr");
    }
}

# 
# uses DNS to find Discovery servers
# puts discovery servers in $self->{s}->{discovery}
#
sub bootstrap_discovery {
    my ($self) = @_;
    $self->log (16,"entered bootstrap_discovery");
    
    if ($self->{conf}->{server}) {
        $self->log(8,"no bootstap_discovery when cmd-line server specified");
        return 1;
    }
    unless ($self->{force_bootstrap_discovery}) {
        if (ref($self->{s}->{discovery}) && scalar(@{$self->{s}->{discovery}})) {
            $self->log(8,"already have ". scalar(@{$self->{s}->{discovery}})
                ." discovery servers");
            return 1;
        } elsif ($self->{done_bootstrap}) {
            # if we've done it before {s}->{discovery} should be set
            $self->log(8,"already have done bootstrap_discovery");
            return 1;
        }
    }
    if (-s $self->{conf}->{listfile_discovery}) {
        my $wait = $self->{conf}->{rediscovery_wait_dns} || 604800; # 604800 secs == 7 days
        my $randomize = int(rand($wait/7));
        my $timeleft = ((stat ($self->{conf}->{listfile_discovery}))[9] + $wait - $randomize) - time;
        if ($timeleft > 0) {
            $self->log (7,"$timeleft seconds before soonest DNS discovery");
            return 1 unless $self->{force_bootstrap_discovery};
            $self->log (5,"forcing DNS discovery");
        } else { 
            $self->log (5,"DNS discovery overdue by ". (0-$timeleft) ." seconds"); 
        }
    } else { 
        if (-e $self->{conf}->{listfile_discovery}) {
            $self->log (6,"empty discovery listfile: $self->{conf}->{listfile_discovery}");
        } else { 
            $self->log (6,"no discovery listfile: $self->{conf}->{listfile_discovery}");
        }
    } 

    my $zone = $self->{conf}->{razorzone};
    $self->log(5,"Finding Discovery Servers via DNS in the $zone zone");

    my $resolver = $self->load_at_runtime("Net::DNS::Resolver")
        or return $self->errprefix("bootstrap_discovery");

    my @list;

    BUILDLIST: 
    for ( 'a'..'z' ) { 
        my $query = $resolver->query ($_.'.'.$zone);
        my $rr; next BUILDLIST unless $query;
        foreach $rr ($query->answer) { 
            my $pushed = 0;
            if ($rr->type eq "A") { 
                if ($rr->address =~ m/^(\d+\.\d+\.\d+\.\d+)$/) {
                    push @list, $1; 
                    $pushed = 1;
                }
            } elsif ($rr->type eq "CNAME") { 
                if ($rr->cname eq 'list.terminator') { 
                    pop @list if $pushed;
                    last BUILDLIST;
                } elsif ($rr->cname eq "skip") { 
                    pop @list if $pushed;
                    next BUILDLIST;
                }
            }
        }
    }
    $self->{done_bootstrap} = 1;

    $self->log(6,"Found ". scalar(@list) ." Discovery Servers via DNS in the $zone zone");

    return $self->error("No Razor Discovery servers available at this time")
        unless scalar(@list);

    $self->{s}->{discovery} = \@list;
    push @{$self->{s}->{modified_lst}}, "discovery";
    return 1;
}



# 
# uses Discovery Servers to find closest Nomination/Catalogue Servers.
# called every day or so of if .lst file is empty
#
# puts servers in $self->{s}->{list}
#
sub discover {
    my ($self) = @_; 
    $self->log (16,"entered discover");

    #
    # do we need to discover?
    #

    # no discover if cmd-line server
    return 1 if $self->{opt}->{server};

    #
    # don't discover if conf says turn_off_discovery (unless force_discovery)
    #
    return 1 if $self->{conf}->{turn_off_discovery} && (!($self->{force_discovery}));
    return $self->error ("No Razor servers available at this time")
        if $self->{done_discovery};

    # so if user has their own servers, and they are temporarily down, force_discovery.
    # good: shit will work
    #  bad: it will erase their custom server*.lst file
    #
    if (-s $self->{s}->{listfile}) {
        my $randomize = int(rand($self->{conf}->{rediscovery_wait}/7));
        my $timeleft = ((stat ($self->{s}->{listfile}))[9] + $self->{conf}->{rediscovery_wait} - $randomize) - time;
        if ($timeleft > 0) {
            $self->debug ("$timeleft seconds before closest server discovery");
            return 1 unless $self->{force_discovery};
            $self->debug ("forcing discovery");
        } else { 
            $self->debug ("server discovery overdue by ". (0-$timeleft) ." seconds"); 
        }
    } else { 
        if (-e $self->{s}->{listfile}) {
            $self->debug ("empty listfile: $self->{s}->{listfile}");
        } else { 
            $self->debug ("no listfile: $self->{s}->{listfile}");
        }
    } 

    #
    # we need to discover.
    #


    return $self->errprefix("discover0") unless $self->bootstrap_discovery();


    #
    # Go ahead and do discovery for both csl and nsl.
    #
    my %stype = ( csl => 'catalogue', nsl => 'nomination' );

    my $srvs = {csl => {}, nsl => {} };

    foreach (@{$self->{s}->{discovery}}) {

        $self->log (8,"Checking with Razor Discovery Server $_");

        unless ($self->connect( server => $_, discovery_server => 1 ) ) {
            $self->log (5,"Razor Discovery Server $_ is unreachable");
            next;
        }

        foreach my $querytype (qw(csl nsl)) {
        my $query = "a=g&pm=$querytype\r\n";
        my $resp = $self->_send([$query]);
        unless ($resp) {
            return $self->errprefix("discover1");
            next;
        }

        # from_batched_query wants "-" in beginning, but not ".\r\n" at end
        $resp->[0] =~ s/\.\r\n$//sg;   
        my $h = from_batched_query($resp->[0], {});

        foreach my $href (@$h) {
            next unless $href->{$querytype};
            $self->log (8,"Discovery Server $_ replying with $querytype=$href->{$querytype}");
            $srvs->{$querytype}->{$href->{$querytype}} = 1;
        }
        unless (keys %{$srvs->{$querytype}}) {
            $self->log (5,"Razor Discovery Server $_ had no valid $querytype servers");
            next;
        }
        }
    }
    foreach my $querytype (qw(csl nsl)) {

    my @list = keys %{$srvs->{$querytype}};

    #return $self->error("Could not get valid info from Discovery Servers")
    #    unless @list;
    unless (@list) { 
        $self->log(5, "Couldn't talk to discovery servers.  Will force a bootstrap...");
        $self->{force_bootstrap_discovery} = 1;
        return $self->error("Bootstrap discovery failed. Giving up.") unless $self->bootstrap_discovery();
        return $self->discover();
    }

        # $self->logobj(11,"checking ping times for servers", \@list);

    if ($self->{conf}->{sort_by_distance}) { 
        # sort @list by ping time
        #
        my $pinger = $self->load_at_runtime("Net::Ping", "new", "'tcp', 4")
            or return $self->errprefix("discover2");

        my %times; my $timings;
       
        for (@list) {
            $" = ", ";
            print "Will determine the closest of:\n\t(@list)\n";
            my $t1 = $self->load_at_runtime("Time::HiRes", "gettimeofday");
            $pinger->ping($_);
            my $t2 = $self->load_at_runtime("Time::HiRes", "gettimeofday");
            $times{$_} = $t2 - $t1;
        }
        @list = sort { $times{$a} <=> $times{$b} } keys %times;
        #for (@list) { $timings .= sprintf ("  %4.1fms %s,", $times{$_}*1000, $_);  }
        $timings = join( ', ', map {sprintf ("%4.1fms %s", $times{$_}*1000, $_)} @list);
        $self->log (5,"Sorted (closest first) list of $stype{$querytype} ($querytype) servers & RTTs:$timings");
    } else { 
        fisher_yates_shuffle(\@list);
    }

    $self->{s}->{$stype{$querytype}} = \@list;
    push @{$self->{s}->{modified_lst}}, $stype{$querytype};

    }

    unless ($self->{opt}->{server}) {
        if ($self->{breed} =~ /^check/) {
            $self->{s}->{list} = $self->{s}->{catalogue};
            $self->{s}->{listfile} = $self->{conf}->{listfile_catalogue}; # for discovery()
        } else {
            $self->{s}->{list} = $self->{s}->{nomination};
            $self->{s}->{listfile} = $self->{conf}->{listfile_nomination}; # for discovery()
        }
    }
    $self->{s}->{new_list} = 1;

    $self->{done_discovery} = 1;
    $self->writeservers();
    return $self;
}


# only for debugging and errorchecking
#
sub logobj {
    my ($self, $loglevel, $prefix, @objs) = @_;
    
    return unless $self->logll($loglevel);

    foreach my $obj (@objs) {
        my $line = debugobj($obj);
        $self->log($loglevel, "$prefix:\n $line");
    }
}


#
# Mail Object
#
# Main data type used by check and report is the Mail Object.
# an array of hash ref's, where array order matches mails in mbox (or stdin).
#
#             key = value  (not all defined) 
#
#              id = integer  NOTE: only key guaranteed to exist 
#       orig_mail = ref to string containing orig email (headers+body)
#         headers = headers of orig_email
#            spam = 0, not spam, >1 spam
#          skipme = 0|1  (not checked against server, usually whitelisted mail)
#               p = array ref to mimeparts. see below
#              e1 = similar to p, but special for engine 1
# 
# e1: each mail obj contains a special part for engine 1
# 
#             skipme = 0|1  (ex: 1 if cleaned body goes to 0 len)
#               spam = 0, not spam, >1 spam
#               body = body of orig_mail
#            cleaned = body sent thru razor 1 preproc
#                 e1 = hash using engine 1
#               sent = hash ref sent to server 
#               resp = hash ref of server response
# 
# p: each mail obj contains 1 or more mimeparts, which can contain:
# 
#                 id = string - mailid.part 
#             skipme = 0|1  (ex: 1 if cleaned body goes to 0 len)
#               spam = 0, not spam, >1 spam
#               body = bodyparts (mimeparts) of orig_email, has X-Razor & Content-* headers
#            cleaned = body sent through preprocessors (deHtml, deQP, etc..), debugging use only
#                 e2 = hash using engine 2
#                 e3 = hash using engine 2
#                 e4 = hash using engine 2
#               sent = array ref of hash ref's sent to server 
#               resp = array ref of hash ref's, where hash is parsed sis of server response
#
#
sub prepare_objects {
    my ($self, $objs) = @_;
    my @objects;

    unless ($self->{s}->{engines} || 
            ($self->{s}->{engines} = $self->compute_supported_engines() ) ) {
        $self->log(1, "ALLBAD. supported engines not defined");
    }

    my $i = 1;
    if (ref($objs->[0]) eq 'HASH') { # checking cmd-line signatures
        foreach my $o (@$objs) {
            my $obj = { id => $i++ };
            $obj->{p}->[0]->{id} = "$obj->{id}.0";
            $obj->{p}->[0]->{"e$o->{eng}"} = $o->{sig};
            $obj->{ep4} = $o->{ep4} if $o->{ep4};
            push @objects, $obj;
        }

    } elsif (ref($objs->[0]) eq 'SCALAR') {  # checking/reporting mail
        foreach my $o (@$objs) {
            my $obj = { id => $i++ };
            $obj->{orig_mail} = $o;
            $self->log2file( 16, $o, "$obj->{id}.orig_mail" ); # includes headers and all
            push @objects, $obj;
        }
        $self->prepare_parts(\@objects);
    }
    $self->logobj(14,"prepared objs", \@objects);

    return \@objects;
}

sub prepare_parts {
    my ($self, $objs) = @_;

    my $prep_mail_debug = 0; # debug print, 0=none, 1=split_mime stuff, 2=more verbose
    $prep_mail_debug++ if $self->{conf}->{debuglevel} > 15;
    $prep_mail_debug++ if $self->{conf}->{debuglevel} > 16;

    foreach my $obj (@$objs) {
        next if ($obj->{skipme} || !$obj->{orig_mail});

        #
        # now split up mime parts from orig mail
        #
        my ($headers, @bodyparts) = prep_mail (
                            $obj->{orig_mail}, 
                            $self->{conf}->{report_headers}, 
                            4 * 1024,
                            60 * 1024,
                            15 * 1024,
                            $self->{name_version},
                            $prep_mail_debug,  # $debug, 
                            );

        my $lines = " prep_mail done: mail $obj->{id} headers=". length($$headers);
        foreach (0..$#bodyparts) { $lines .= ", mime$_=". length(${$bodyparts[$_]}); }
        $self->log(8,$lines);

        unless (@bodyparts) {
            $self->log(2,"empty body in mail $obj->{id}, skipping");
            next;
        }

        $$headers =~ s/\r\n/\n/gs;
        $obj->{headers} = $headers;

        $obj->{e1} = { 
            id => "$obj->{id}.e1",
          body => $obj->{orig_mail},
        };

        $obj->{p} = [];
        foreach (0..$#bodyparts) { 
            $bodyparts[$_] =~ s/\r\n/\n/gs;
            $obj->{p}->[$_] = { 
                id => "$obj->{id}.$_",
              body => $bodyparts[$_],
            };
        }
    }
    return 1;
}


# given mail objects, fills out 
#
# - e1
#
# and for each body part of mail object, fills out
#
# - cleaned 
# - e2 
# - e3
# - e4
#
# also returns array ref of sigs suitable for printing 
#
sub compute_sigs {
    my ($self, $objects) = @_;
    my @printable_sigs;

    foreach my $obj (@$objects) {
        next if ($obj->{skipme} || !$obj->{orig_mail});

        if (${$obj->{orig_mail}} =~ /\n(Subject: [^\n]+)\n/) {
            my $subj = substr $1, 0, 70;
            $self->log(8,"mail ". $obj->{id} ." $subj");
        } else {
            $self->log(8,"mail ". $obj->{id} ." has no subject");
        }

        #
        # engine 1 is special case - must be same as razor 1
        #
        my ($hdrs, $body) = split /\n\r*\n/, ${$obj->{orig_mail}}, 2;
        if ($body) {
            $self->log2file( 16, \$body, "$obj->{id}.before.preproc_vr1" );
            $obj->{e1}->{body} = \$body;
            my $cln = $body;
            $self->{preproc_vr1}->doit(\$cln);
            $obj->{e1}->{cleaned} = \$cln;
            $self->log2file( 16, \$cln, "$obj->{id}.after.preproc_vr1" );

            my $sig = $self->compute_engine(1, \$cln );
            $obj->{e1}->{e1} = $sig;
            push @printable_sigs, "$obj->{id}  e1: $sig";
        } else {
            $self->log(6,"body empty for mail $obj->{id}, skipping eng 1 sig.");
        }

        # 
        # clean each bodypart, removing if new length is 0 
        #
        next unless $obj->{p};
        foreach my $objp (@{$obj->{p}}) {
            next if $objp->{skipme};

            my $olen = length(${$objp->{body}});
            my $clnpart = ${$objp->{body}};
            $self->{preproc}->preproc( \$clnpart ); # in da future: $self->{s}->{conf}->{dre}
            $objp->{cleaned} = \$clnpart;
            my $clen = length($clnpart);

            $self->log2file( 15, $objp->{body},    "$objp->{id}.before_preproc.as_reported");
            $self->log2file( 15, $objp->{cleaned}, "$objp->{id}.after_preproc");

            if ($clen eq 0) {
                $self->log(6,"preproc: mail $objp->{id} went from $olen bytes to 0, erasing");
                $objp->{skipme} = 1;
                next;
	    } elsif (($clen < 128)and($clnpart =~ /^(Content\S*:[^\n]*\n\r?)+(Content\S*:[^\n]*)?\s*$/s)) {
                $self->log(6,"preproc: mail $objp->{id} seems empty, erasing");
		$objp->{skipme} = 1;
		next;
            } elsif ($clnpart !~ /\S/) {
                $self->log(6,"preproc: mail $objp->{id} went to all whitespace, erasing");
                $objp->{skipme} = 1;
                next;
            } elsif ($clen eq $olen) {
                $self->log(6,"preproc: mail $objp->{id} unchanged, bytes=$olen");
            } else {
                $self->log(6,"preproc: mail $objp->{id} went from $olen bytes to $clen ");
            }
        }


        # 
        # compute sig for bodyparts that are cleaned.
        #
        if ($self->{s}->{conf}->{ep4}) {
            $obj->{ep4} = $self->{s}->{conf}->{ep4};
        } else {
            $obj->{ep4} = '7542-10';
            $self->log(8,"warning: no ep4 for server $self->{s}->{ip}, using $obj->{ep4}");
        }
        foreach my $objp (@{$obj->{p}}) {
            next if $objp->{skipme};

            $self->log(15, "mail part is [${$objp->{cleaned}}]");

            if (${$objp->{cleaned}} =~ /^\s+$/) { 
                $self->log(6, "mail $objp->{id} is whitespace only; skipping!");
            }

            $self->log(6,"computing sigs for mail $objp->{id}, len ". length(${$objp->{cleaned}}));

            foreach (sort keys %{$self->{s}->{engines}}) {
                next if $_ eq '1';  # engine 1 handled above

                my $sig = $self->compute_engine( $_, $objp->{cleaned},
                     $obj->{ep4} );  # if not engine 4, this is ignored
                $self->log(6,"doh.  no sig for mail $objp->{id} eng $_") unless $sig;
                
                $objp->{"e$_"} = $sig;

                my $line = "$objp->{id} e$_: $sig";
                $line .= ", ep4: $obj->{ep4}" if ($_ eq '4');
                push @printable_sigs, $line;
            }
        }
        $self->logobj(14,"computed sigs for obj", $obj);
    }
    return \@printable_sigs;
}


#
# this function is the only one that has to be aware
# of razor protocol syntax.  (not including random logging)
# the hashes generated here are eventually sent to to_batched_query.
#
sub make_query {
    my ($self, $params) = @_;


    if ($params->{action} =~ /^check/)  {
                    
        my %query = (  a => 'c', 
                    e => $params->{eng},
                    s => $params->{sig},
                 );
        $query{ep4} = $params->{ep4} if $query{e} eq '4';
        return \%query;

    } elsif ($params->{action} =~ /^rcheck/) {

        my %query = (  a => 'r', 
                    e => $params->{eng},
                    s => $params->{sig},
                 );
        $query{ep4} = $params->{ep4} if $query{e} eq '4';
        return \%query;

    } elsif ($params->{action} =~ /(report|revoke)/) {

        # prep_mail already truncated headers and body parts > 64K
        my @dudes;
        my $n = 0;
        while ($params->{obj}->{p}->[$n]) {
            my $line = ${$params->{obj}->{headers}};
            while (1) {
                my $body = $params->{obj}->{p}->[$n]->{body};
                last unless ( (length($$body) + length($line) 
                      < $self->{s}->{conf}->{bqs} * 1024));

                $self->log(11, "bqs=". ($self->{s}->{conf}->{bqs} * 1024) .  
                    " adding to line [len=". length($line) ."] mail $params->{obj}->{p}->[$n]->{id}"
                    ." [len=". length($$body) ."], total len=".  (length($$body) + length($line)) );

                $line .= "\r\n". $$body;
                $n++;
                last unless $params->{obj}->{p}->[$n];
            }
            push @dudes, $line;
        }

        my @queries;
        foreach (@dudes) {
            push @queries, {  a => $params->{action} eq 'report' ? 'r' : 'revoke', 
                        message => $_,
                 };
        }
        return @queries;

    }

}

#
# prepare queries in correct syntax for sending over network
#
sub obj2queries {
    my ($self, $objects, $action) = @_;
    my @queries = ();
    
    foreach my $obj (@$objects) {
        next if $obj->{skipme};
        push @queries, $obj->{e1}->{sent} if $obj->{e1}->{sent}; 
        foreach my $objp (@{$obj->{p}}) {
            next if $objp->{skipme}; 
            #$self->log(8,"not skipping mail part $objp->{id}, sent: ". scalar(@{$objp->{sent}}));
            push @queries, @{$objp->{sent}} if $objp->{sent}; 
        }
    }

    if (scalar(@queries)) {
        $self->log(8,"preparing ". scalar(@queries) ." queries");
    } else {
        $self->log(8,"objects yielded no valid queries");
        return [];
    }
    my $qbatched = to_batched_query( \@queries, 
                                 $self->{s}->{conf}->{bql},
                                 $self->{s}->{conf}->{bqs},
                                 1);
    $self->log(8,"sending ". scalar(@$qbatched) ." batches");

    return $qbatched;
}


#
# Parse response syntax, add info to appropriate object
#
sub queries2obj { 
    my ($self, $objs, $responses, $action) = @_;

    my @resp;
    foreach (@$responses) {
        # from_batched_query wants "-" in beginning, but not ".\r\n" at end
        s/\.\r\n$//sg;   
        my $arrayref = from_batched_query($_);
        push @resp, @$arrayref;
    }

    $self->log(12,"processing ". scalar(@resp) ." responses");
    $self->logobj(14,"from_batched_query", \@resp);

    my $j = 0;
    while (@resp) {
        my $obj = $objs->[$j++];
        return $self->error("more responses than mail objs!") unless $obj;
        next if $obj->{skipme};

        if ($obj->{e1}->{sent} && !$obj->{e1}->{skipme}) {
            $obj->{e1}->{resp} = shift @resp;
            $self->log(12,"adding a resp to mail $obj->{e1}->{id}") ;
        }
        foreach my $objp (@{$obj->{p}}) {
            next unless $objp->{sent};
            # for each part, shift out as many responses as there were queries
            foreach (@{$objp->{sent}}) {
                push @{$objp->{resp}}, shift @resp;
                $self->log(12,"adding a resp to mail $objp->{id}");
            }
        }
        #$self->logobj(13,"end of queries2obj",$obj);
    }
    return 1;
}


sub check_resp {
    my ($self, $me, $sent, $resp, $objp) = @_;

    # default is no contention
    $objp->{ct} = 0;
    $objp->{ct} = $resp->{ct} if exists $resp->{ct}; 

    if (exists $resp->{err}) { 
        $self->logobj(4,"$me: got err $resp->{err} for query", $sent);
        return 0;
    } 
    if ($resp->{p} eq '1') {
        if (exists $resp->{cf}) {
            if ($resp->{cf} < $self->{s}->{min_cf}) { 
                $self->log (6,"$me: Not spam: cf $resp->{cf} < min_cf $self->{s}->{min_cf}");
                return 0;
            } else {
                $self->log (6,"$me: Is spam: cf $resp->{cf} >= min_cf $self->{s}->{min_cf}");
                return 1;
            }
        }
        $self->log (6,"$me: sig found, no cf, ok.");
        return 1;
    }
    if ($resp->{p} eq '0') {
        $self->log (6,"$me: sig not found.");
        return 0;
    }
    # should never get here
    $self->logobj(2,"$me: got bad response from server - sent obj, resp obj", 
            [$sent, $resp] );
    return 0;
}


sub rcheck_resp {
    my ($self, $me, $sent, $resp) = @_;

    $self->log(8,"$me: invalid $sent") unless ref($sent);
    $self->log(8,"$me: invalid $resp") unless ref($resp);
    
    if (exists $resp->{err}) { 
        if ($resp->{err} eq '230') { 
            $self->log(8,"$me: err 230 - server wants mail");
            return 1;
        }
        $self->logobj(4,"$me: got err $resp->{err} for query", $sent);
        return 0;
    } 
    if ($resp->{res} eq '1') {
        $self->log (5,"$me: Server accepted report.");
        return 0;
    }
    if ($resp->{res} eq '0') {
        $self->log (1,"$me: Server did not accept report.  Shame on the server.");
        return 0;
    }
    # should never get here
    $self->logobj(2,"$me: got bad response from server - sent obj, resp obj", 
            [$sent, $resp] );
    return 0;
}


sub check { 
    my ($self, $objects) = @_;

    my $valid = 0;
    foreach my $obj (@$objects) {
        next if $obj->{skipme}; 

        #
        # Logic used in ordering of check queries 
        #
        # queries should go like this: (e=engine, p=part)
        # e1, p0e2, p0e3, p0e4, p1e2, p1e3, p1e4, etc..
        # unless cmd-line sigs are passed.
        #

        # engine 1 is for entire mail, not parts
        if ($obj->{e1} # cmd-line sig checks don't have this
            && $self->{s}->{engines}->{1}) { 
            $obj->{e1}->{sent} = $self->make_query( {
                             action => 'check',
                                sig => $obj->{e1}->{e1},
                                eng => 1 } );
        }
        # rest of engines and mime parts
        foreach my $objp (@{$obj->{p}}) {

            if ($objp->{skipme}) {
                $self->log(8,"mail $objp->{id} skipped in check");
                next;
            }
            $objp->{sent} = [];
            foreach (sort keys %{$self->{s}->{engines} }) {

                next if $_ eq 1; # engine 1 done above
                my $sig = $objp->{"e$_"}; 
                unless ($sig) {
                    $self->log(5,"mail $objp->{id} e$_ got no sig");
                    next;
                }

                unless ($self->{s}->{engines}->{$_}) {
                    # warn if cmd-lig sig check is not supported
                    $self->log(5,"mail $objp->{id} engine $_ is not supported, sig check skipped") 
                        if ($sig && !$obj->{orig_mail});
                    next;
                }


                $self->log(8,"mail $objp->{id} e$_ sig: $sig");

                my $query = $self->make_query( {
                             action => 'check',
                                sig => $sig,
                                ep4 => $obj->{ep4},
                                eng => $_ } );

                $valid++ if $query;
                push @{$objp->{sent}}, $query;
            }
        }
    }
    unless ($valid) {
        $self->log (5,"No queries, no spam");
        return 1;
    }


    # Build query text strings
    #
    my $queries = $self->obj2queries($objects, 'check') or return $self->errprefix("check 1");

    # send to server and store answers in mail obj
    #
    my $response = $self->_send($queries)           or return $self->errprefix("check 2");
    $self->queries2obj($objects, $response, 'check')   or return $self->errprefix("check 3");


    foreach my $obj (@$objects) {

        # check_logic will parse response for each object, decide if its spam
        #
        $self->check_logic($obj);

        $self->log (3,"mail $obj->{id} is ". ($obj->{spam} ? '' : 'not ') ."known spam.");
    }
    return 1;
} 



sub check_logic {
    my ($self, $obj) = @_;

    # default is not spam
    $obj->{spam} = 0;
    if ($obj->{skipme}) {
        next;
    }

    #
    # Logic for Spam
    #
    #
    my $logic_method  = $self->{conf}->{logic_method}  || 4;
    my $logic_engines = $self->{conf}->{logic_engines} || 'any';

    # cmd-line sig checks default to logic_method 1
    $logic_method = 1 unless $obj->{orig_mail};

    my $leng;
    if ($logic_engines eq 'any') {
        $leng = "";  # not a hash ref, implies 'any' logic_engine
    } elsif ($logic_engines eq 'all') {
        $leng = $self->{s}->{engines};
    } elsif ($logic_engines =~ /^(\d\,)+$/) {
        $leng = {};
        foreach (split /,/,$logic_engines) {
            unless ($self->{s}->{engines}->{$_}) {
                $self->log(3, "logic_engine $_ not supported, skipping");
                next;
            }
            $leng->{$_} = 1;
        }
    } else {
        $self->log(3, "invalid logic_engines: $logic_engines, defaulting to 'any'");
        $leng = "";  # not a hash ref, implies 'any' logic_engine
    }
   

    # iterate through sent queries and responses,
    # perform engine analysis (logic_engines).
    #
    # engine 1 case
    my $sent = $obj->{e1}->{sent};
    my $resp = $obj->{e1}->{resp};
    if ($resp && $sent) {
        # if skipme, there would be no resp
        my $logmsg   = "mail $obj->{id} e=1 sig=$sent->{s}";
        $obj->{e1}->{spam} = $self->check_resp($logmsg, $sent, $resp, $obj->{e1});
    }
    # all other engines for all parts
    foreach my $objp (@{$obj->{p}}) {
        $objp->{spam} = 0;
        if ($objp->{skipme}) {
            $self->log(8,"doh. $objp->{id} is skipped, yet has sent") if $objp->{sent};
            next;
        }
        next unless $objp->{sent};
        my $not_spam = 0;
        foreach (0..(scalar(@{$objp->{sent}}) - 1)) {
            $sent = $objp->{sent}->[$_];
            $resp = $objp->{resp}->[$_];
            unless ($resp) {
                $self->log(5,"doh. more sent queries than responses");
                next;
            }
            my $logmsg  = "mail $objp->{id} e=$sent->{e} sig=$sent->{s}";
            my $is_spam = $self->check_resp($logmsg, $sent, $resp, $objp);
            
            if (ref($leng)) {
                if ($leng->{$sent->{e}} && $is_spam) {
                    $self->log(8,"logic_engines requires $sent->{e}, and it is. cool.");
                    $objp->{spam} = 1;
                } elsif ($leng->{$sent->{e}} && !$is_spam) {
                    $self->log(8,"logic_engines requires $sent->{e}, and it is not, part not spam");
                    $not_spam = 1;
                } else {
                    $self->log(8,"logic_engines doesn't care about $sent->{e}, skipping");
                }
            } else {
                # not a hash ref, implies 'any' logic_engine
                $objp->{spam} += $is_spam;
            }
            $objp->{spam} = 0 if $not_spam;
        }
    }

    # mime part analysis (logic_methods)
    #
    if ($logic_method == 1) {

        $obj->{spam} = 0;
        if ($obj->{e1}) {
            $obj->{spam} += $obj->{e1}->{spam} if $obj->{e1}->{spam};
        }
        foreach my $objp (@{$obj->{p}}) {
            $obj->{spam} += $objp->{spam} if $objp->{spam};
        }


    } elsif ($logic_method =~ /^(2|3)$/) {
        # logic_methods > 1

        foreach my $objp (@{$obj->{p}}) {
            next if $objp->{skipme};
            next unless $objp->{body};
            my ($hdrs, $body) = split /\n\n/, ${$objp->{body}}, 2;
            $hdrs .= "\n";

            #$self->log(8,"$objp->{id} hdrs:\n$hdrs");
            my $type = "<type unknown>";
            $objp->{is_text} = 0;
            $objp->{is_inline} = 0;
            $objp->{is_inline} = 1 if $hdrs =~ /Content-Disposition: inline/i;
            #$type = $1 if $hdrs =~ /Content-Type:\s([^\;\n]+)/i;
            $type = $1 if $hdrs =~ /Content-Type:\s([^\n]+)/i;
            $objp->{is_text} = 1 if $type =~ /text\//i;
            $objp->{is_text} = 1 if $type =~ /type unknown/;  # assume text ?

            $self->log(8,"mail $objp->{id} Type $objp->{is_text},$objp->{is_inline} $type");
        }
    }

    if ($logic_method == 2) {

        # in this method, only 1 dude decides if mail is spam. decider.
        
        # the first part is the default decider. can be overwritten, tho.
        my $decider = $obj->{p}->[0];

        # basically the first inline text/* becomes the decider.
        # however, if no inline, the first text/* is used
        my $found = 0;
        foreach my $objp (@{$obj->{p}}) {
            next if $objp->{skipme};
            
            if ($objp->{is_inline} && $objp->{is_text}) {
                $decider = $objp;
                last;
            }
            if (!$found && $objp->{is_text}) {
                $decider = $objp;
                $found = 1;
            }
        }
        $self->log (7,"method 2: $decider->{id} is the spam decider");
        $obj->{spam} = $decider->{spam};

    } elsif ($logic_method == 3) {

        # in this method, all text/* parts must be spam for obj to be spam
        # non-text parts are ignored

        my $found = 0;
        foreach my $objp (@{$obj->{p}}) {
            next if $objp->{skipme};
            next unless $objp->{is_text};
            $found = 1;    
            $obj->{spam} = $objp->{spam};
            unless ($objp->{spam}) {
                $self->log (7,"method 3: $objp->{id} is_text but not spam, mail not spam");
                last;
            }
        }
        $self->log (7,"method 3: mail $obj->{id}: all is_text parts spam, mail spam") if $obj->{spam};

        # if no parts where text, use the first part as spam indicator
        unless ($found) {
            $self->log (6,"method 3: mail $obj->{id}: no is_text, using part 1");
            $obj->{spam} = 1 if $obj->{p}->[0]->{spam};
        }

    } elsif ($logic_method == 4) {

        # in this method, if any non-contention parts is spam, mail obj is spam
        # contention parts are ignored.

        $obj->{spam} = 0;
        foreach my $objp (@{$obj->{p}}) {
            next if $objp->{skipme};
            if ($objp->{ct}) {
                $self->log (7,"method 4: mail $objp->{id}: contention part, skipping");
            } else {
                $self->log (7,"method 4: mail $objp->{id}: no-contention part, spam=$objp->{spam}");
                $obj->{spam} = 1 if $objp->{spam};
            }
        }
        if ($obj->{spam}) {
            $self->log (7,"method 4: mail $obj->{id}: a non-contention part was spam, mail spam");
        } else {
            $self->log (7,"method 4: mail $obj->{id}: all non-contention parts not spam, mail not spam");
        }

    } elsif ($logic_method == 5) {

        # in this method, all non-contention parts must be spam for obj to be spam
        # contention parts are ignored.

        my $not_spam = 0;
        my $is_spam = 0;
        foreach my $objp (@{$obj->{p}}) {
            next if $objp->{skipme};
            if ($objp->{ct}) {
                $self->log (7,"method 5: mail $objp->{id}: contention part, skipping");
                next;
            } else {
                $self->log (7,"method 5: mail $objp->{id}: no-contention part, spam=$objp->{spam}");
            }
            if ($objp->{spam}) {
                $is_spam = 1;
            } else {
                $not_spam = 1;
            }
        }
        if ($is_spam && !$not_spam) {
            $obj->{spam} = 1;
            $self->log (7,"method 5: mail $obj->{id}: all non-contention parts spam, mail spam");
        } else {
            $self->log (7,"method 5: mail $obj->{id}: a non-contention part not spam, mail not spam");
            $obj->{spam} = 0;
        }

    }
    return 1;
} 



# returns hash ref if successfully registered
# returns 0 if not
sub register {
    my ($self, $p,) = @_;
    my @queries;

    my $registrar = $self->{name_version};
    my %qr = ( a => 'reg', registrar => $registrar );
    $qr{user} = $p->{user} if $p->{user};
    $qr{pass} = $p->{pass} if $p->{pass};
    $queries[0] = makesis(%qr);

    my $response = $self->_send(\@queries) or return $self->errprefix("register");

    my %resp = parsesis($$response[0]);
 
    return $self->error("Error $resp{err}: User exists. Try another name. aborting.\n")
        if ($resp{err} && $resp{err} eq '210'); 
    return $self->error("Error $resp{err} while performing register, aborting.\n")
        if ($resp{err});

    return $self->error("No success (res=$resp{res}) while performing register, aborting.\n")
        if ($resp{res} ne '1');

    $self->log(6,"Successfully registered with $self->{s}->{ip} identity: $resp{user}");

    # otherwise return hash containing 'user' and 'pass'
    delete $resp{res};
    return \%resp;
}


sub authenticate {
    my ($self, $options) = @_;
    my @queries;

    unless (($options->{user} =~ /\S/) && ($options->{pass} =~ /\S/)) {
        return $self->error("authenticate did not get valid user + pass");
    }

    my %qr = ( a => 'ai', user => $options->{user}, cn => 'razor-agents', cv => $Razor2::Client::Version::VERSION );
    $queries[0] = makesis(%qr);

    my $response = $self->_send(\@queries) or return $self->errprefix("authenticate 1");

    my %resp = parsesis($$response[0]);
    if ($resp{err}) {
        if (($resp{err} eq '213') && !defined($self->{reregistered})) {
            # 213 = unknown user.  
            # Try to register with current user+pass and continue with authenticate
            $self->log (8,"unknown user, attempting to re-register");
            
            my $id = $self->register($options);
            $self->{reregistered} = 1;
            if (($id->{user} eq $options->{user}) &&
                ($id->{pass} eq $options->{pass})) {
                $self->log (5,"re-registered user $id->{user} with $self->{s}->{ip}");
                return $self->authenticate($options);
            } else {
                return $self->error("Error 213 while authenticating, aborting.\n") 
            }
        } else {
            return $self->error("Error $resp{err} while authenticating, aborting.\n") 
        }
    }
    my ($iv1, $iv2) = xor_key($options->{pass});
    my ($my_digest) = hmac_sha1($resp{achal}, $iv1, $iv2);

    %qr = ( a => 'auth', aresp => $my_digest );
    $queries[0] = makesis(%qr);

    $response = $self->_send(\@queries) or return $self->errprefix("authenticate 2");

    %resp = parsesis($$response[0]);
    return $self->error("Error $resp{err} while authenticating, aborting.\n") if ($resp{err});
    return $self->error("Authentication failed for user=$options->{user}")
        if ($resp{res} ne '1');

    $self->log (5,"Authenticated user=$options->{user}");
    $self->{authenticated} = 1;
    return 1;
}


#
# handles report and revoke
#
sub report {

    my ($self, $objs) = @_;
    return $self->error("report: Not Authenticated") unless $self->{authenticated};

    return $self->error("report/revoke for engine 1 not supported")
            if ($self->{s}->{conf}->{dre} == 1);

    my @robjs;
    my $valid = 0;
    if ($self->{breed} eq 'report') {

        #
        # Before reporting entire email, check to see if server already has it
        #
    
        unless ($self->{s}->{conf}->{dre}) {
            $self->logobj(8,"server has no default dre, using 4", $self->{s}->{conf});
            $self->{s}->{conf}->{dre} = 4;
        }
    
        foreach my $obj (@$objs) {
            next if $obj->{skipme};

            # handle special case for engine  1
            # note: razor 1 does not store emails in its db, just sigs.
            # so we should never get a res=230 for e=1 a=r sig=xxx
            #
            $obj->{e1}->{sent} = $self->make_query( {
                         action => 'rcheck',
                            sig => $obj->{e1}->{e1},
                            eng => 1, } );
            $valid++ if $obj->{e1}->{sent};

            # rest of engines and mime parts
            foreach my $objp (@{$obj->{p}}) {

                if ($objp->{skipme}) {
                    $self->log(13,"mail $objp->{id} skipped in report");
                    next;
                }
                my $q = $self->make_query( {
                         action => 'rcheck',
                            sig => $objp->{"e$self->{s}->{conf}->{dre}"},
                            ep4 => $obj->{ep4},
                            eng => $self->{s}->{conf}->{dre}, } );
                $objp->{sent} = [$q];
                $valid++;
            }
        }
        unless ($valid) {
            $self->log (5,"No report check queries, no spam");
            return 1;
        }
        $valid = 0;


        # Build query text strings - signatures computed already (see reportit)
        my $queries = $self->obj2queries($objs,'rcheck') or return $self->errprefix("report1");
    
        # send to server and store answers in mail obj
        my $response = $self->_send($queries)            or return $self->errprefix("report2");
        $self->queries2obj($objs, $response, 'rcheck')   or return $self->errprefix("report3");
                    
    
        #
        # If server wants email or certain body parts, 
        # create new {sent} and add obj to @robjs
        #
        foreach my $obj (@$objs) {
            next if $obj->{skipme};

            #$self->log(12,"mail $obj->{id} read ". scalar(@{$obj->{resp}}) ." queries");
            # handle engine 1 special case
            if ( !$obj->{e1}->{skipme} && $self->rcheck_resp(
                    "mail ". $obj->{id} .", orig_email, special case eng 1",
                    $obj->{e1}->{sent}, 
                    $obj->{e1}->{resp} 
                    ) ) {
                $self->log(5,"doh.  Server should not send res=230 for eng=1 report");
            }
            delete $obj->{e1}->{sent};

            my $wants_orig_mail = 0;
            foreach my $objp (@{$obj->{p}}) {
                next if $objp->{skipme};
    
                $self->logobj(14,"checking response for $objp->{id}", $objp);
                unless ( $self->rcheck_resp(
                        "mail $objp->{id}, eng $self->{s}->{conf}->{dre}",
                        $objp->{sent}->[0], $objp->{resp}->[0] )) {
                    $objp->{skipme} = 1;
                } else {
                    $wants_orig_mail++;
                }
                $objp->{resp} = [];  # clear responses from rcheck
                $objp->{sent} = [];
            }
            if ($wants_orig_mail) {
                # reports are special, all parts need to be together, so use part 0's sent
                my $objp = $obj->{p}->[0];  
                $objp->{skipme} = 0 if $objp->{skipme};
                push @{$objp->{sent}}, $self->make_query( {
                          action => 'report',
                             obj => $obj,
                                } );
                push @robjs, $obj;
            }
            $valid += $wants_orig_mail;
        }

    } else {  # revoke

        foreach my $obj (@$objs) {
            # don't revoke eng 1

            # engines > 1 we send all the body parts, use part 0 to store sent
            my $objp = $obj->{p}->[0];
            $objp->{sent} = [];

            push @{$objp->{sent}}, $self->make_query( {
                               action => 'revoke',
                                  obj => $obj,
                             } );
            $valid++ if scalar(@{$objp->{sent}});
            $self->log (9,"revoke sent:". scalar(@{$objp->{sent}}));
            push @robjs, $obj;
        }

    }

    unless ($valid && scalar(@robjs)) {
        $self->log (3,"Finished $self->{breed}.");
        return 1;
    }

    #$self->logobj(14,"report objs", \@robjs);

    #
    # send server mails/body parts either
    # revoked, or requested if reporting
    #
    my $queries = $self->obj2queries( \@robjs,$self->{breed}) or return $self->errprefix("report4");
    my $response = $self->_send( $queries )                   or return $self->errprefix("report5");
    $self->queries2obj( \@robjs, $response,$self->{breed})    or return $self->errprefix("report6");

    # we just do this to log server's response
    #
    foreach my $obj (@robjs) {
        my $objp = $obj->{p}->[0];
        my $cur = -1;
        while ($objp->{sent}->[++$cur]) {
            $self->rcheck_resp(
                "$self->{breed}: mail $obj->{id}, $cur",
                $objp->{sent}->[$cur], 
                $objp->{resp}->[$cur] ) unless ($objp->{skipme});
        }
    }
    $self->logobj(14,"report objs", \@robjs);
    $self->log (3,"Sent $self->{breed}.");
    return 1;

}



sub _send { 
    my ($self, $msg, $closesock, $skipread) = @_;
    $self->log (16,"entered _send");
   
    unless ($self->{connected_to}) {
        $self->connect() or return $self->errprefix("_send");
    }

    my @response;
    my $select = $self->{select};
    my $sock  = ($select->handles)[0];
    $self->{sent_cnt} = 0 unless $self->{sent_cnt};
    foreach my $i (0 .. ((scalar @$msg) -1) ) {
        my @handles = $select->can_write (15);
        if ($handles[0]) {
            $self->log (4,"$self->{connected_to} << ". length($$msg[$i]) );
            if ($$msg[$i] =~ /message/) { 
                my $line = debugobj($$msg[$i]);
                $self->log (6, $line );
                $self->log2file(16, \$$msg[$i], "sent_to.". $self->{sent_cnt});
            } else {
                $self->log (6, $$msg[$i] );
            }
            local $\;
            undef $\; 
            print $sock $$msg[$i];
            $self->{sent_cnt}++;
        } else {
            return $self->error("Timed out (15 sec) while writing to $self->{s}->{ip}");
        }
        next if $skipread;
        @handles = $select->can_read (15);
        if ($sock=$handles[0]) {
            local $/;
            undef $/; 
            $response[$i] = $self->_read($sock) or return $self->error("Error reading socket");
            $self->log (4,"$self->{connected_to} >> ". length($response[$i]) );
            $self->log (6,"response to sent.$self->{sent_cnt}\n". $response[$i]); 
        } else {
            return $self->error("Timed out (15 sec) while reading from $self->{s}->{ip}");
        }
    }
    if ($closesock) {
        $select->remove($sock);
        close $sock;
    }
    return \@response;
}


sub _read {
    my ($self, $socket) = @_;
    my ($query, $read);

        # fixme - need to trim this down (copied from server)
        #

        unless ($read = sysread($socket, $query, 1024)) {

            # There was an error on sysread(), could be a real error or a
            # blocking error.

            if ($! == EWOULDBLOCK) {
                # write would block, so we try again later
                $self->debug ("_read: EWOULDBLOCK");
                return;
            } elsif ($! == EINTR or $! == EIO) {
                # sysread() got interupted by a signal.
                # we will process this socket on next wheelwalk.
                $self->debug ("_read: EINTR");
                return;
            } elsif ($! == EPIPE or $! == EISDIR or $! == EBADF or $! == EINVAL or $! == EFAULT) {
                $self->debug ("_read: EPIPE");
                return;
            } else {
                # This happens when client breaks the connection.
                # Find out why we don't get an EPIPE instead. FIX!
                $self->debug ("_read: connection_closed");
                return;
            }
        }

        if ($read > 0) {

            # Now we are absolutely sure there is data on the socket.

            return $query;

        } else {

            # Otherwise we got an EOF, expire the socket

            $self->debug ("_read: EOF, connection_closed");
            return;

        }
}



sub connect {
    my ($self, %params) = @_;
    my $sock;
    $self->log (16,"entered connect");

    if ($self->{simulate}) { 
        return $self->error ("Razor Error 4: This is a simulation. Won't connect to $self->{s}->{ip}.");
    }

    my $server = $params{server} || $self->{s}->{ip};

    if ($self->{sock} && $self->{connected_to}) {
        unless ($server) {
            $self->log (13,"no server specified, using already connected server $self->{connected_to}");
            return 1;
        }
        if ($server eq $self->{connected_to}) {
            $self->log (15,"already connected to server $self->{connected_to}");
            return 1;
        }
        return 1 if $self->{disconnecting};
        $self->log(6,"losing old server connection, $self->{connected_to}, for new server, $server");
        $self->disconnect;
    }
    unless ($server) {
        $self->log (6,"no server specified, not connecting");
        return;
    }

    my $port   = $params{port}  || $self->{s}->{port};
    unless (defined($port) && $port =~ /^\d+$/) {
        my $portlog = defined($port) ? " ($port)" : "";
        $self->log (6, "No port specified$portlog, using 2703"); # bootstrap_discovery will come here
        $port  = 2703;
    }
    $self->log (5,"Connecting to $server ...");
    if (my $proxy = $self->{conf}->{proxy}) {
        # 
        # Proxy stuff never been tested 
        # 
        $proxy =~ s!^http://!!;
        $proxy =~ s!:(\d+)/?$!!;
        my $pport = $1 || 80;
        $self->debug ("HTTP tunneling through $proxy:$pport.");
        $sock = IO::Socket::INET->new( 
                            PeerAddr => $proxy,
                            PeerPort => $pport,
                            Proto    => 'tcp',
                            Timeout  => 20,
                     );
        unless ( $sock ) {
            $self->debug ("Unable to connect to proxy $proxy:$pport; Reason: $!.");
        } else {
            $sock->printf( "CONNECT %s:%d HTTP/1.0\r\n\r\n", $server, $port );
            if( $sock->getline =~ m!^HTTP/1\.\d+ 200 ! ){
                # Skip through remaining part of MIME header.
                while( $sock->getline !~ m!^\r! ){ ; }
            } else {
                $self->log (4, "HTTP tunneling is disabled at $proxy.");
                $sock = undef;
            }
        }
    }

    # if proxy, we already might have a $sock.
    # if proxy failed to connect, try without proxy.
    #

    if ($self->{conf}->{socks_server}) { 

        my $socks_module = "Net::SOCKS";
        eval "require $socks_module";

        $self->log(6, "Will try to connect through the SOCKS server on $$self{conf}{socks_server}...");

        my $socks_sock = Net::SOCKS->new (
            socks_addr => $$self{conf}{socks_server},
            socks_port => 1080,
            protocol_version => 4 
        );
        
        if ($socks_sock) { 

            $sock = $socks_sock->connect(peer_addr => $server, peer_port => $port);
            if ($sock) {
                $self->log(6, "Connected to $server via SOCKS server $$self{conf}{socks_server}.");
            }

        }

    }


    unless ($sock) {
        $sock = IO::Socket::INET->new( 
                            PeerAddr => $server,
                            PeerPort => $port,
                            Proto    => 'tcp',
                            Timeout  => 20,
                     );
        unless ( $sock ) {
            $self->log (3,"Unable to connect to $server:$port; Reason: $!.");
            return if $params{discovery_server};
            $self->nextserver or do { return $self->errprefix("connect1"); };
            return $self->connect;
        }    
    } 

    my $select = new IO::Select ($sock);
    my @handles = $select->can_read (15);
    if ($handles[0]) {
        $self->log (8,"Connection established");
        my $greeting = <$sock>;
        # $sock->autoflush; # this is on by default as of IO::Socket 1.18
        $self->{sock} = $sock;
        $self->{connected_to} = $server;
        $self->{select} = $select;
        $self->log(4,"$server >> ". length($greeting) ." server greeting: $greeting");

        return 1 if $params{discovery_server};
        unless ($self->parse_greeting($greeting) ) {
            $self->nextserver or return $self->errprefix("connect2");
            return $self->connect;
        }
        return 1;
    } else {
        $self->log (3, "Timed out (15 sec) while reading from $self->{s}->{ip}.");
        $select->remove($sock);
        $sock->close();
        return $self->errprefix("connect3") if $params{skip_greeting};
        $self->nextserver or return $self->errprefix("connect4");
        return $self->connect;
    }
}

sub disconnect {
    my $self = shift;

    unless ($self->{sock}) {
        $self->log (5,"already disconnected from server ". $self->{connected_to});
        return 1;
    }

    $self->log (5,"disconnecting from server ". $self->{connected_to});

    $self->{disconnecting} = 1;
    $self->_send(["a=q\r\n"], 0, 1);
    delete $self->{disconnecting};

    delete $self->{sock};  # _send closes socket


    return 1;
}


sub parse_greeting {
    my ($self, $greeting) = @_;
    $self->log (16,"entered parse_greeting($greeting)");

    my %server_greeting = parsesis($greeting); 
    $self->{s}->{greeting} = \%server_greeting;

    unless ($self->{s}->{greeting} && $self->{s}->{greeting}->{sn}) { 
        $self->log(1,"Couldn't parse server greeting\n"); 
        return;
    }

    # server greeting must contain: sn, srl
    # server greeting may  contain: ep4, redirect, a,

    #
    # fixme - add support for redirect, etc.
    #

    #
    # current server config info is stored in $self->{s}->{conf}
    # see nextserver for more info
    #
    # If server greeting says there are new values
    # (which we know if greeting's srl > conf's srl)
    # we ask server for new values, update conf, then
    # put that server on modified list so it gets recorded to disk
    #
    # fixme - in the future, we could have a key with no value
    #         in .conf file - forcing client to ask server 'a=g&pm=key' 
    #

    if ($self->{s}->{greeting}->{a} eq 'cg') { 
        my $version = $Razor2::Client::Version::VERSION;
        my @cg = ("cn=razor-agents&cv=$version\r\n");
        $self->_send(\@cg, 0, 1);
    }

    if (defined($self->{s}->{greeting}->{srl}) &&
        defined($self->{s}->{conf}->{srl})  &&
        $self->{s}->{greeting}->{srl} <= $self->{s}->{conf}->{srl}) { 

        $self->compute_server_conf;
        return 1 ;
    }

    # srl > our cached srl, request update  (a=g&pm=state)
    # and rediscover
    #

    my @queries = ("a=g&pm=state\r\n");

    my $response = $self->_send(\@queries) or return $self->errprefix("parse_greeting");

    # should be just one response
    # from_batched_query wants "-" in beginning, but not ".\r\n" at end
    $response->[0] =~ s/\.\r\n$//sg;   
    my $h = from_batched_query($response->[0], {});

    foreach my $href (@$h) {
        foreach (sort keys %$href) {
            $self->{s}->{conf}->{$_} = $href->{$_};
            #$self->log(8,"updated: $_=$href->{$_}");
        } 
    } 
    $self->log(1,"Bad info while trying to get server state (a=g&pm=state)") 
        unless scalar(@$h);

    $self->{s}->{conf}->{srl} = $self->{s}->{greeting}->{srl};
    push @{$self->{s}->{modified}}, $self->{s}->{ip};
    $self->{s}->{allconfs}->{$self->{s}->{ip}} = $self->{s}->{conf}; # in case new server

    # now we're up to date
    $self->log(5,"Updated to new server state srl ". $self->{s}->{conf}->{srl}
                   ." for server ". $self->{s}->{ip});

    $self->compute_server_conf();
    $self->writeservers;  # writes to disk servers listed in $self->{s}->{modified}

    $self->log(5,"srl was updated, forcing discovery ...");
    $self->{done_discovery} = 0;
    $self->{force_discovery} = 1;
    $self->discover();
     
    return 1;
}


# Returns engines supported 
#
# can be called with no paramaters or 
# with hash of server supported engines
sub compute_supported_engines {
    my ($self, $orig) = @_;

    my %all;
    my $se = $self->supported_engines(); # local supported engines
    foreach (@{$self->{conf}->{use_engines}}) {
        if ($orig) {
            $all{$_} = 1 if (exists $se->{$_}) && (exists $orig->{$_});
        } else {
            $all{$_} = 1 if exists $se->{$_};
        }
    }
    if ($orig) {
        $self->log(8, "Computed supported_engines: ". join(' ', sort(keys %all)) );
    } else {
        $self->log(8, "Client supported_engines: ". join(' ', sort(keys %all)) );
    }
    return \%all;
}


# called when we need to parse server conf
# - after initial parse_greeting
# - if state (srl) changes 
# - when we switch to cached server conf info in nextserver
#
sub compute_server_conf {
    my ($self, $cached) = @_;

    #
    # compute a confindence (cf) from razor-agent.conf's 'min_cf'
    # and server's average confidence (ac)
    #
    # min_cf can be 'n', 'ac', 'ac + n', or 'ac - n'
    # where 'n' can be 1..100
    # 
    my $cf     = $self->{s}->{conf}->{ac}; # default is server's ac
    my $min_cf = $self->{conf}->{min_cf};
    $min_cf =~ s/\s//g;

    if ($min_cf =~ /^ac\+(\d+)$/) {
        $cf = $self->{s}->{conf}->{ac} + $1;

    } elsif ($min_cf =~ /^ac-(\d+)$/) {
        $cf = $self->{s}->{conf}->{ac} - $1;

    } elsif ($min_cf =~ /^ac$/) {
        $cf = $self->{s}->{conf}->{ac};

    } elsif ($min_cf =~ /^(\d+)$/) {
        $cf = $min_cf;
    } else {
        $self->log(5,"Invalid min_cf $self->{conf}->{min_cf}");
    }
    $cf = 100 if $cf > 100;
    $cf = 0   if $cf < 0;
    $self->{s}->{min_cf} = $cf;

    #
    # ep4 - special for vr4
    #
    $self->{s}->{conf}->{ep4} = $self->{s}->{greeting}->{ep4} 
                             if $self->{s}->{greeting}->{ep4};

    my $info = $cached ? $self->{s}->{conf} : $self->{s}->{greeting};
    my $name = "Unknown-Type: ";
    if ($info->{sn}) {
        $name .= $info->{sn};
        $name = "Nomination" if $info->{sn} =~ /N/;
        $name = "Catalogue"  if $info->{sn} =~ /C/;
        $name = "Catalogue"  if $info->{sn} =~ /S/;
    }
    $self->log (6, $self->{s}->{ip} ." is a $name Server srl ". 
        $self->{s}->{conf}->{srl} ."; computed min_cf=$cf, Server se: $self->{s}->{conf}->{se}");

    #
    # Supported Engines - greeting contains hex of bits
    # we turn into a hash so we can just quickly do
    # do_eng3_stuff if $self->{s}->{engines}->{3};
    #

    # if we're just computing hashes locally, ignore what engines server currently supports
    # fixme - this prolly should be done somewhere else
    if ($self->{opt}->{printhash}) {
        $self->log (6, "Ignore what engines server supports for -H");
        $self->{s}->{engines} = $self->compute_supported_engines();
    } else { 
        my $se = hexbits2hash($self->{s}->{conf}->{se} );
        $self->{s}->{engines} = $self->compute_supported_engines($se);
    } 
}


# sub log2file moved to Agent.pm


sub debug { 
    my ($self, $message) = @_;
    $self->log(5,$message);
}


sub DESTROY { 
    my $self = shift;
    #$self->debug ("Agent terminated");
}


sub zonename { 
    my ($zone, $type) = @_;
    my ($sub, $dom) = split /\./, $zone, 2; 
    return "$sub-$type.$dom";
}


1;


