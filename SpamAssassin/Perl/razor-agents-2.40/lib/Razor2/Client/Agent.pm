#!/usr/bin/perl -sw
##
## Razor2::Client::Agent -- UI routines for razor agents.
##
## Copyright (c) 2002, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: Agent.pm,v 1.1 2004/04/19 17:50:31 dasenbro Exp $

package Razor2::Client::Agent;

use lib qw(lib);
use strict;
use Getopt::Long; 
use IO::File;

use base qw(Razor2::Client::Core);
use base qw(Razor2::Client::Config);
use base qw(Razor2::Logger);
use base qw(Razor2::String); 
use Razor2::Preproc::Manager;
use Razor2::Preproc::VR1;
use Data::Dumper;
use vars qw( $VERSION $PROTOCOL );


$PROTOCOL = $Razor2::Client::Version::PROTOCOL;
$VERSION  = $Razor2::Client::Version::VERSION;



sub new {
    my ($class, $breed) = @_;

    # For Taint Friendliness
    delete $ENV{PATH};
    delete $ENV{BASH_ENV};

    my @valid_program_names = qw(
            razor-check 
            razor-report  
            razor-revoke  
            razor-admin
    );

    my $ok = 0;
    foreach (@valid_program_names) { $breed =~ /$_$/ and $ok = $_; }
    unless ($ok) {
        if ($breed =~ /razor-client$/) {
            create_symlinks(@valid_program_names);
            exit 0;
        }
        die "Invalid program name, must be one of: @valid_program_names\n";
    }

    $ok =~ /razor-(.*)$/; 
    my %me = (
            name_version => "Razor-Agents v$VERSION",  # used in register
            breed        => $1,
            preproc_vr1  => new Razor2::Preproc::VR1,
            preproc      => new Razor2::Preproc::Manager,
            global_razorhome => '/etc/razor',
    );
    
    
    return bless \%me, $class;
}

sub do_conf {
    my $self = shift;

    # parse config-related cmd-line args
    #
    
    # identity is parsed later after razorhome is fully resolved

    if ($self->{opt}->{config}) {
        if ($self->{opt}->{create_conf}) {
            $self->{razorconf} = $self->{opt}->{config};
        } elsif (-r $self->{opt}->{config}) {
            $self->{razorconf} = $self->{opt}->{config};
        } else {
            return $self->error("Can't read conf file: $self->{opt}->{config}") 
        }
    }
    if ($self->{opt}->{razorhome}) {
        if (-d $self->{opt}->{razorhome}) {
            $self->{razorhome} = $self->{opt}->{razorhome};
        } else {
            return $self->error("Can't read: $self->{opt}->{razorhome}") 
                unless $self->{opt}->{create_conf};
        }
    }
    return unless $self->read_conf();

    if ($self->{opt}->{create_conf}) {
        $self->{force_discovery} = 1;
        $self->{force_bootstrap_discovery} = 1;
        $self->log(8," -create will force complete discovery");
    }
    if ($self->{opt}->{force_discovery}) {
        $self->{force_discovery} = 1;
        $self->{force_bootstrap_discovery} = 1;
        $self->log(8," -discover will force complete discovery");
    }
    if ($self->{opt}->{debug} && !$self->{opt}->{debuglevel}) {
        $self->{conf}->{debuglevel} = 9 if $self->{conf}->{debuglevel} < 9;
    }


    #
    # Note: we start logging before we process '-create' ,
    # so logfile will not go into a newly created razorhome
    #
    my $logto = $self->{opt}->{debug} ? "stdout" : "file:$self->{conf}->{logfile}";
    if (exists $self->{conf}->{logfile}) {
        my $debuglevel = exists $self->{conf}->{debuglevel} ? $self->{conf}->{debuglevel} : 9;
        my $logger = new Razor2::Logger ( 
                        LogDebugLevel => $debuglevel,
                        LogTo         => $logto,
                        LogPrefix     => $self->{breed},
                        LogTimestamp  => 1,
                        DontDie       => 1,
                        Log2FileDir   => defined($self->{conf}->{tmp_dir}) ? $self->{conf}->{tmp_dir} : "/tmp",
                     );
        $self->{logref} = ref($logger) ? $logger : 0;
        # log error strings at loglevel 11.  Pick a high number 'cuz
        # if its really an error, it will be in errstr for caller
        $self->{logerrors} = 11; 
    }
    $self->logobj(15,"cmd-line options", $self->{opt});
    $self->{preproc}->{rm}->{log} = $self->{logref};

    #my $chadrules;  # test warning
    #$self->logobj(3,"warn on this $chadrules");

    # creates razorhome, and sets $self->{razorhome} if successful
    return $self->errprefix("Could not create 'razorhome'") unless $self->create_home_conf(); 
    $self->compute_identity;

    $self->log(5,"computed razorhome=$self->{razorhome}, conf=$self->{razorconf}, ident=$self->{identity}");
    return 1;
}

# if a debug log statement requires extra work, check this call before doing it.
sub logll {
    my ($self, $loglevel) = @_;
    return unless $self->{logref};
    return 1 if ($self->{logref}->{LogDebugLevel} >= $loglevel);
    return;
}

sub create_home_conf {
    my $self = shift;

    unless ($self->{opt}->{create_conf}) {
        #
        # if the global razorhome exists, don't create anything 
        # without '-create' option
        #
        return 1 if (-d $self->{global_razorhome});

        #
        # if there is not global razorhome,
        # try to create razorhome one anyway.
        # if it fails, thats ok.
        #
        $self->create_home($self->{razorhome_computed});
        $self->errstrrst;  # nuke error string
        return 1;
    }

    #
    # user passed in 'create' option, so create.
    #
    my $rhome = $self->{opt}->{razorhome} 
              ? $self->{opt}->{razorhome} 
              : $self->{razorhome_computed};

    if ($rhome) {

        if (-d $rhome) {
            $self->log(6,"Not creating razorhome $rhome, already exists");
        } else {
            return unless $self->create_home($rhome);
        }
    }


    if ($self->{opt}->{config}) {

        # if create and conf specified, exit if write is not successful
        #
        $self->{razorconf} = $self->{opt}->{config};
        return $self->write_conf(); 

    } else {

        # else just try and create, if fail ok.
        #
        $self->compute_razorconf();
        $self->{razorconf} ||= $self->{computed_razorconf};
        $self->write_conf();
        $self->errstrrst;  # nuke error string
    }
    return 1;
}

# wrapper for log
sub log {
    my $self = shift;
    my $level = shift;
    my $msg = shift;

    if ($self->{logref}) {
        return  $self->{logref}->log($level, $msg);
    } elsif ($self->{opt}->{debug}) {
    #} else {
        print " Razor-Log: $msg\n" if $self->{opt}->{debug};
    }
}
sub log2file {
    my $self = shift;
    return unless $self->{logref};
    return        $self->{logref}->log2file(@_);
}

sub doit {
    my $self = shift;
    my $args = shift;
    my $r;

    $self->log(2," $self->{name_version} starting razor-$self->{breed} $self->{args}");
    $self->log(9,"uname -a: ". `uname -a`) if $self->logll(9);

    $r = $self->checkit($args)    if $self->{breed} eq 'check';
    $r = $self->adminit($args)    if $self->{breed} eq 'admin';
    $r = $self->reportit($args)   if $self->{breed} eq 'report';
    $r = $self->reportit($args)   if $self->{breed} eq 'revoke';

    # return exit code
    # 0, 1 => ok
    #  > 1 => error  (caller should prolly print $self->errstr)
    #
    if ($r > 1) {
        my $msg = $self->errstr;
        $self->log(1,"razor-$self->{breed} error: ". $msg);
    } else {
        $self->log(8,"razor-$self->{breed} finished successfully.");
    }
    return $r;
}


sub _help {
    my ($self,$breed) = @_;

    chomp(my $all = <<EOFALL);
            -h  Print this usage message.
            -v  Print version number and exit
            -d  Turn on debugging.  Logs to stdout.
            -s  Simulate Only.  Does not connect to server.
    -conf=file  Use this config file instead of <razorhome>/razor.conf
     -home=dir  Use this as razorhome
   -ident=file  Use this identity file instead of <razorhome>/identity
           -rs  Use this razor server instead of reading .lst
EOFALL
    chomp(my $sigs = <<EOFSIGS);
            -H  Compute and print signature.
   -S |  --sig  Accept a signatures to check on the command line
        -e eng  Engine used to compute sig, integer
      -ep4 val  String value required when engine == 4
EOFSIGS

    chomp(my $mbox = <<EOFMBOX);
   -M | --mbox  Accept a mailbox name on the command line (default)
                If no filename, mbox, or signatures, input read from stdin.
EOFMBOX
    
    my %b;
    $b{check} = <<EOFCHECK;

razor-check [options] [ filename | -M mbox | -S signatures | < filename ]
$all
$sigs
$mbox

See razor-check(1) manpage for details.

EOFCHECK

    $b{report} = <<EOFREPORT;

razor-report [options] [ filename | -M mbox | -S signatures -e engine]
$all
$sigs
$mbox
       -i file  Use identity from this file
            -f  Stay in foreground.
            -a  Authenticate only.  Exit 0 if authenticated, 1 if not
                Stays in foreground.

See razor-report(1) manpage for details.

EOFREPORT

    $b{admin} = <<EOFREGISTER;

razor-admin [options] [ -register | -create | -discover ]
$all
       -create  Create razorhome, does discover, does not register
     -discover  Discover Razor servers: write .lst files
     -register  Register a new identity
    -user name  Request 'name' when registering (requires -register)
    -pass pass  Request 'password' when registering (requires -register)
            -l  Make new identity the the default identity.
                Used only when registering.

See razor-admin(1) manpage for details.

EOFREGISTER

    $b{revoke} = <<EOFREVOKE;

razor-revoke [options] filename
$all
$mbox
       -i file  Use identity from this file
            -f  Stay in foreground.
            -a  Authenticate only.  exit 0 if authenticated, 1 if not
                Stays in foreground.

See razor-revoke(1) manpage for details.

EOFREVOKE

    my $future = <<EOFFUTURE;
EOFFUTURE

    return $b{$self->{breed}};
}


# maybe this should be in Client::Config
#
sub read_options { 
    my ($self, $agent) = @_;
    $self->{args} = join ' ', @ARGV;
    Getopt::Long::Configure ("no_ignore_case");
    my %opt;
    #
    # These options override what is loaded in config file
    # the names on the right should match keys in config file
    #
    GetOptions(
        's'   => \$opt{simulate},
        'd'   => \$opt{debug},
  'verbose'   => \$opt{debug},
        'v'   => \$opt{version},
        'h'   => \$opt{usage},
     'help'   => \$opt{usage},
        'H'   => \$opt{printhash},
      'C=s'   => \$opt{printcleaned},
    'sig=s'   => \$opt{sig},
      'S=s'   => \$opt{sig},
      'e=s'   => \$opt{sigengine},
    'ep4=s'   => \$opt{sigep4},
     'mbox'   => \$opt{mbox},
        'M'   => \$opt{mbox},
        'n'   => \$opt{negative},
   'conf=s'   => \$opt{config},
 'config=s'   => \$opt{config},
   'home=s'   => \$opt{razorhome},
        'f'   => \$opt{foreground},
     'noml'   => \$opt{noml},
   'user=s'   => \$opt{user},
      'u=s'   => \$opt{user},
   'pass=s'   => \$opt{pass},
        'a'   => \$opt{authen_only},
     'rs=s'   => \$opt{server},
 'server=s'   => \$opt{server},
        'r'   => \$opt{register},
 'register'   => \$opt{register},
        'l'   => \$opt{symlink},
      'i=s'   => \$opt{identity},
  'ident=s'   => \$opt{identity},
   'create'   => \$opt{create_conf},
'logfile=s'   => \$opt{logfile},
 'discover'   => \$opt{force_discovery},
     'dl=s'   => \$opt{debuglevel},
'debuglevel=s' => \$opt{debuglevel},
'whitelist=s' => \$opt{whitelist},
     'lm=s'   => \$opt{logic_method},
     'le=s'   => \$opt{logic_engines},
    );

    # remove elements not set in the cmd-line
    foreach (keys %opt) { delete $opt{$_} unless defined $opt{$_}; }

    if ($opt{usage}) { 
        $self->error($self->_help);
        return;
    } elsif ($opt{mbox} && $opt{sig}) { 
        $self->error("--mbox and --sig are mutually exclusive.\n"); 
        return;
    } elsif ($opt{sig} && !$opt{sigengine}) { 
        $self->error("--sig requires -e (engine used to generate sig)\n");
        return;
        #
        # fixme - require ep4 if -e 4 is used ?
        #
    } elsif ($opt{version}) { 
        $self->error("Razor Agents $VERSION, protocol version $PROTOCOL");
        return;
    } 
    $self->{opt} = \%opt;
    return 1;
} 



# returns 0 if match (spam)
# returns 1 if no match (legit)
# returns 2 if error
sub checkit {

    my $self = shift;
    my $args = shift;

    # check for spam.
    # input can be one of 
    #   file - single mail
    #   mbox - many  mail
    #   sig  - 1 or more sigs
    #   or a filehandle provided via args
        
    my $objects;
    if ($self->{conf}->{sig}) {
        my @sigs;
        # 
        # cmd-line sigs
        # 
        # prepare 1 mail object per sig 
        # 
        foreach my $sig (split ',', $self->{conf}->{sig}) {
            $sig =~ s/^\s*//;  $sig =~ s/\s*$//;
            my $hr = { 
                eng => $self->{conf}->{sigengine},
                sig => $sig,
            };
            $hr->{ep4} = "7542-10";
            $hr->{ep4} = $self->{conf}->{sigep4} if $self->{conf}->{sigep4};
            push @sigs, $hr;
        }
        $self->log (5,"received ". (scalar @sigs) ." valid cmd-line sigs.");
        $objects = $self->prepare_objects(\@sigs) or return 2;
    } else {
        
        my $mails = $self->parse_mbox($args) or return 2;

        $objects  = $self->prepare_objects($mails) or return 2;
        #
        # if mail is whitelisted, its not spam.
        # flag it so it we don't check it against server
        #
        foreach my $obj (@$objects) {
            if ($self->local_check($obj)) {
                $obj->{skipme} = 1;
                $obj->{spam} = 0;
            } else {
                next;
            }
        }

    }

    # compute_sigs needs server info like ep4, so get_server_info first
    $self->get_server_info()                            or return 2;
    my $printable_sigs = $self->compute_sigs($objects)  or return 2;

    if ($self->{opt}->{printhash}) { 
        my $i = 0;
        foreach (@$printable_sigs) {
            if ($self->{opt}->{sigengine}) {
                next unless (/ e$self->{opt}->{sigengine}: /);
            }
            print "$_\n";
            $i++;
        }
        $self->log (4, "Done. Printed $i sig(s) for ". scalar(@$objects) ." mail(s)");
    }
    if ($self->{opt}->{printcleaned}) { 
        my $totalp = 0;
        my $totalc = 0;
        foreach my $obj (@$objects) {
            my $n = 0;
            `mkdir -p $self->{opt}->{printcleaned}/cleaned`;
            foreach ($obj->{headers}, @{$obj->{bodyparts_cleaned}}) {
                my $fn = "$self->{opt}->{printcleaned}/cleaned/mail$obj->{id}.". $n++;
                $self->write_file($fn, $_);
                $totalc++;
            }
            $n = 0;
            `mkdir -p $self->{opt}->{printcleaned}/uncleaned`;
            foreach ($obj->{headers}, @{$obj->{bodyparts}}) {
                my $fn = "$self->{opt}->{printcleaned}/uncleaned/mail$obj->{id}.". $n++;
                $self->write_file($fn, $_);
                $totalp++;
            }
        }
        $self->log (4, "Done. $totalp uncleaned, $totalc cleaned mails saved in $self->{opt}->{printcleaned}");
        print "Done. $totalp uncleaned, $totalc cleaned mails saved in $self->{opt}->{printcleaned}\n";
        return 1;
            
    }

    return 1 if $self->{opt}->{printhash};

    # only check good objects
    my @goodones;                 # this should be optimized! 
    foreach my $obj (@$objects) {
        next if $obj->{skipme};
        push @goodones, $obj;
    }
    unless (scalar @goodones) {
        $self->log (4,"Done.  No valid mail or signatures to check.");
        return 1;
    }

    if ($self->{conf}->{simulate}) {
        $self->log (4, "Done. (simulate only)");
        return 1;
    }
    #
    # Check against server
    #  
    $self->connect()          or return 2;
    $self->check (\@goodones) or return 2;
    $self->disconnect()       or return 2;


    #
    # print out responses and exit
    #  
    my $only1check = (scalar(@$objects) == 1) ? 1 : 0;
    my $has_spam = 0;
    foreach my $obj (@$objects) {

        $obj->{spam} = 0 if $obj->{skipme};
        $obj->{spam} = 0 unless defined $obj->{spam};

        if ($obj->{spam} > 0) {
            return 0 if $only1check;
            $has_spam = 1;
            print $obj->{id} ."\n";
            next;

        } elsif ($obj->{spam} == 0) {
            return 1 if $only1check;
            print "-". $obj->{id} ."\n" if $self->{conf}->{negative};
            next;

        } else {
            # error
            #
            $self->logobj(1,"bad 'spam' in checkit", $obj);
            return 2 if $only1check;
            print "-". $obj->{id} ."\n" if $self->{conf}->{negative};
            next;
        }
    }
    return 0 if $has_spam;
    return 1;
}



# returns 0 if success
# returns 2 if error
sub adminit { 
    my $self = shift;

    my $done_something = 0;

    if ($self->{opt}->{create_conf}) {
        $done_something++;
        # $self->create_home_conf() is always checked
    }
    
    if (  $self->{opt}->{force_discovery} || 
          $self->{opt}->{create_conf}) {
        $done_something++;
        # get_server_info() calls nextserver() which calls discovery()
        $self->get_server_info()    or return 2;  
    }

    if ($self->{opt}->{register}) {
        $done_something++;
        my $r = $self->registerit();
        return $r if $r;
    }

    unless ($done_something) {
        $self->error("An option needs to be specified,  -h for help.");
        return 2;
    }
    
    return 0;
}

# returns 0 if success
# returns 2 if error
sub registerit { 
    my $self = shift;

    unless ($self->{razorhome} || $self->{opt}->{identity}) {
        $self->errprefix("Unable to register without a valid razorhome or identity");
        return 2;
    }

    my $ident;

    if (exists $self->{opt}->{user} 
        && ($ident = $self->get_ident) 
        && $ident->{user} eq $self->{opt}->{user} ) {
        $self->error("You are already registered as user=$ident->{user} in $self->{razorhome}");
        return 2;
    }
    if ( $self->{conf}->{simulate}) {
        $self->log(5,"Done - simulate only.");
        return 0;
    }

    $self->get_server_info()    or return 2;
    $self->connect()            or return 2;
    if ($self->{opt}->{create_conf}) {
        $self->disconnect()     or return 2;
        $self->log(3, "Register create successful.");
        return 0;
    }

    $ident = $self->register({
                        user => $self->{opt}->{user}, 
                        pass => $self->{opt}->{pass}, 
                            }) or return 2;
    $self->disconnect()         or return 2;


    if (my $fn = $self->save_ident($ident)) {
        my $msg = "Register successful.  Identity stored in $fn";
        $self->log(3, $msg);
        print "$msg\n";
        return 0;
    } else {
        return 2;
    }
}


#
# handles report and revoke
#
# returns 0 if success
# returns 2 if error
sub reportit {

    my ($self, $args) = @_;

    my $ident = $self->get_ident;
    unless ($ident) {
        $self->errprefix("Bootstrap Error: Your Razor2 identity was not found.\n   " . 
                         "  If you haven't registered, please do so:\n" .
                         "     \"razor-admin -register -user=[name/email_address] -pass=[password]\".\n". 
                         "     (Further information can be found in the razor-admin(1) manpage)\n" .
                         "  If you did register, please ensure your identity symlink (or file) is in order.\nmore");

        return 2;
    }

    # background myself 
    unless ($self->{opt}->{foreground}) {
        chdir '/'; 
        fork && return 0; 
        POSIX::setsid;
        # close 0, 1, 2;
    }

    if ($self->{opt}->{authen_only}) { 
        $self->authenticate($ident) or return;
        $self->log(5,"Done - authenticate only.");
        return 0 if $self->{authenticated};
        return 2;
    }

    my $mails   = $self->parse_mbox($args) or return 2;

    my $objects = $self->prepare_objects($mails) or return 2;  

    # compute_sigs needs server info like ep4, so get_server_info first
    $self->get_server_info()                            or return 2;
    my $printable_sigs = $self->compute_sigs($objects)  or return 2;

    if ($self->{opt}->{printhash}) { 
        foreach (@$printable_sigs) {
            if ($self->{opt}->{sigengine}) {
                next unless (/ e$self->{opt}->{sigengine}: /);
            }
            print "$_\n";
        }
        exit 0;
    }

    if ( $self->{conf}->{simulate}) {
        $self->log (4, "Done. (simulate only)");
        exit 0;
    }
    unless (scalar @$objects) {
        $self->log (4,"Done.  No valid mail or signatures to check.");
        exit 1;
    }

    $self->connect()            or return 2;
    $self->authenticate($ident) or return 2; 
    $self->report($objects)     or return 2; 
    $self->disconnect()         or return 2; 


    if ($self->{opt}->{foreground}) {
        foreach my $obj (@$objects) {
            # my $line = debugobj($obj->{r});
            # $line =~ /(\S+=\S+)/s;  # could be res=0|1, err=xxx
            # print "$obj->{id}: $1\n";
            #print "$obj->{id}\n" if $obj->{r}->{res} == '1';
        }
    }
    return 0;
}


sub parse_mbox {
    my ($self, $args) = @_;

    my @mails;
    my @message;
    my $passed_fh = 0;
    my $aref;
    
    # There are different kinds of mbox formats, we just split on simplest case.
    # djb defines mbox, mboxrd, mboxcl, mboxcl2 
    # http://www.qmail.org/qmail-manual-html/man5/mbox.html
    #
    # non-mbox support added, thanx to Aaron Hopkins <aaron@die.net>

    if (exists $$args{"fh"}) { 
        @ARGV = ();
        push @ARGV, $$args{'fh'};
        $passed_fh = 1;
    } elsif (exists $$args{"aref"}) { 
       $aref = $$args{"aref"};
    } elsif (!scalar @ARGV) { 
        push @ARGV, "-"
    }

    if ($$args{'aref'}) { 
        my @foo = (\join'', @{$$args{'aref'}});
        return \@foo;
    }

    foreach my $file (@ARGV) {
        my $fh = new IO::File;
        my @message = ();
        if (ref $file) { 
            $fh = $file
        } else { 
            open $fh, "<$file" or return $self->error("Can't open $file: $!");
        }

        my $line = <$fh>;
        next unless $line;

        if ($line =~ /^From /) {
            $self->log(8,"reading  mbox formatted mail from ". 
                ($file eq '-' ? "<stdin>" : $file));
            while (1) {
                push @message, $line;
                $line = <$fh>;
                if (!defined($line) || $line =~ /^From /) {
                    push @mails, \join ('', @message);
                    @message = ();
                    last unless defined $line;
                }
            }
        } else {
            $self->log(8,"reading straight RFC822 mail from ". 
                ($file eq '-' ? "<stdin>" : $file));
            push @mails, \join ('', map {s/^(>*From )/>$1/; $_} $line, <$fh>);
        }
        close $fh unless $passed_fh;
    }

    my $cnt = scalar @mails; 
    $self->log (6, "read $cnt mail". ($cnt>1 ? 's' : '') );

    return \@mails;
}



sub raise_error { 
    my ($self, $errstr) = @_;;
    my $str;
    if (ref $self) { 
        $str = $self->errstr;
    } 
    $str = $errstr if $errstr;
    my ($code) = $str =~ /Razor Error (\d+):/;
    $code = 255 unless $code; 
    print "FATAL: $str";
    exit $code;
}

# returns 1 if mail should be skipped 
#
sub local_check {
    my ($self, $obj) = @_;
    my ($headers, $body) = split /\n\r*\n/, ${$obj->{orig_mail}}, 2;

    $headers =~ s/\n\s+//sg;  # merge multi-line headers

    if ($self->{conf}->{ignorelist}) { 
        if ($headers =~ /\n((X-)?List-Id[^\n]+)/i) { 
            my $listid = $1;
            my ($line1) = substr(${$obj->{orig_mail}}, 0, 50) =~ /^([^\n]+)/;
            $self->log (5,"Mailing List post; mail ". $obj->{id} ." not spam.");
           #$self->log (5,"Mailing List post; mail ". $obj->{id} ." not spam.\n  $line1\n  $listid");
            return 1;
        }
    }
    return 0 if $self->{no_whitelist};
    if (-s $self->{conf}->{whitelist}) {  
        $self->read_whitelist;
        foreach my $sh (keys %{$self->{whitelist}}) { 
            if ($sh ne 'sha1') { 
                while ($headers =~ /^$sh:\s+(.*)$/img) {
                    last unless $1;
                    my $fc = $1;
                    $self->log (13,"whitelist checking headers for match $sh: $fc");
                    foreach my $address (@{$self->{whitelist}->{$sh}}) {
                        if ($fc =~ /$address/i) { 
                            $self->log (3,"ignoring mail $obj->{id}, whitelisted by rule: $sh: $address");
                            return 1;
                        }
                    }
                }
            }
        }
        $self->log (12,"Whitelist rules did not match mail $obj->{id}");
    } elsif ($self->{conf}->{whitelist}) {
        $self->log (6,"skipping whitelist file (empty?): $self->{conf}->{whitelist}");
        $self->{no_whitelist} = 1;
    }
    return 0;
}



sub read_whitelist { 
    my ($self) = @_; 
    return if $self->{whitelist};

    my %whitelist;
    my $lines = $self->read_file($self->{conf}->{whitelist},0,1);
    for (@$lines) { 
        s/^\s*//;
        next if /^#/;
        chomp;
        my ($type, $value) = split /\s+/, $_, 2; 
        $type =~ y/A-Z/a-z/ if $type;
        push @{$whitelist{$type}}, $value if ($type && $value);
    }
    $self->{whitelist} = \%whitelist;
    $self->log (8,"loaded ". scalar(keys %whitelist) ." different types of whitelist");
    #$self->logobj (15,"loaded whitelist:", \%whitelist);
    return 1;
}   


sub logerr {
    my ($self,$msg) = @_; 
    $msg = $self->errstr unless $msg;
    $self->log(1,"$self->{breed} error: ". $msg);
    return;
}



sub create_symlinks {
    my @progs = @_;
    my $bdir = $0 =~ /^\// ? $0 : "$ENV{PWD}/$0";
    $bdir =~ s|/([^/]+)$||;
    foreach (@progs) {
        if (-e "$bdir/$_") {
            next if -l "$bdir/$_";           # unless a symlink, prolly razor2
            rename ("$bdir/$_", "$bdir/$_.v1")  # rename razor1 
        }
        print "Creating symlink $1 <== $bdir/$_\n";
        symlink($1, "$bdir/$_") if $^O ne 'VMS';
        
        # Do VMS specific stuff.
        if ($^O eq 'VMS' ) {
            my ($disk, $dir, $file) = File::Spec->splitpath($1);
            File::Copy::copy($1,File::Spec->catpath($disk,$dir,"$_"));
        }
     }
}


# see nextserver() for explanation of how data is stored
#
sub get_server_info {
    my $self = shift;

    unless (exists $self->{s}) { $self->{s} = {}; }

    if ($self->{opt}->{server}) {  # cmd-line
        $self->{s}->{list} = [$self->{opt}->{server}];
        $self->log(8,"Using cmd-line server ($self->{opt}->{server}), skipping .lst files");
    } else {
        $self->readservers;
    }
    $self->loadservercache;
    #$self->logobj(6,"find_closest_server server info (before nextserver)", $self->{s});
    $self->{loaded_servers} = 1;
    return $self->nextserver;  # this will connect and get state info if not cached
}


# see nextserver() for explanation of how data is stored
#
sub readservers {
    my $self = shift;

    unless (exists $self->{s}) { $self->{s} = {}; }

    # read .lst files
    foreach my $lf (qw(discovery nomination catalogue)) {

        my $h = $self->read_file($self->{conf}->{"listfile_$lf"},0,1) or next;
        $self->{s}->{$lf} = [];
        foreach (@$h) {
            push @{$self->{s}->{$lf}}, $1
                if /^(([^\.\s]+\.)+[^\.\s]+(:\S+)?)/;  
        }
        if (ref($self->{s}->{$lf})) {
            $self->log(11,"Read ". scalar(@{$self->{s}->{$lf}}) ." from server listfile: ".
                $self->{conf}->{"listfile_$lf"});
        }
    }
    if ($self->{breed} =~ /^check/) {
        $self->{s}->{list} = $self->{s}->{catalogue};
        $self->{s}->{listfile} = $self->{conf}->{listfile_catalogue}; # for discovery()
    } else {
        $self->{s}->{list} = $self->{s}->{nomination};
        $self->{s}->{listfile} = $self->{conf}->{listfile_nomination}; # for discovery()
    }
}

sub loadservercache {
    my $self = shift;

    #
    # Read in server-specific config, using defaults for stuff not found
    #
    # NOTE: this reads all server.*.conf files in razor home, not just those in .lst
    #
    
    # load defaults for .lst servers
    foreach (qw(nomination catalogue)) {
        next unless $self->{s}->{$_};
        foreach my $server (@{$self->{s}->{$_}}) {
            next if $self->{s}->{allconfs}->{$server};  # avoid repeats
            $self->{s}->{allconfs}->{$server} = $self->default_server_conf();
            $self->log(9,"Assigning defaults to $server");
        }
    }
    my @fns;
    my $sep = '\.';
    $sep = '_' if $^O eq 'VMS';
    if (opendir D,$self->{razorhome}) {
        @fns = map {s/_/./g; "$self->{razorhome}/$_";} grep /^server$sep[\S]+\.conf$/, readdir D;
        @fns = map { /^(\S+)$/, $1 } @fns; # untaint
        closedir D;
    }
    foreach (@fns) {
        /server\.(.+)\.conf$/ and my $sn = $1;
        next unless $sn;
        $self->{s}->{allconfs}->{$sn} = $self->read_file($_, $self->{s}->{allconfs}->{$sn} );
        if ($self->{s}->{allconfs}->{$sn}) {
            #$self->log(8,"Loaded server specific conf info for $sn");
        } else {
            $self->log(5,"loadservercache skipping $_");
        }
    }

    return $self;
}


sub writeservers {
    my $self = shift;

    unless ($self->{razorhome}) {
        $self->log(5,"no razorhome, not caching server info to disk");
        return;
    }

    foreach (@{$self->{s}->{modified_lst}}) {
        my $fn = $self->{conf}->{"listfile_$_"};
        $self->write_file($fn, $self->{s}->{$_}, 0, 0, 1)
            || $self->log(5,"writeservers skipping .lst file: $fn");
    }
    $self->log(11,"No bootstrap_discovery (DNS) recently, not recording .lst files")
        unless scalar (@{$self->{s}->{modified_lst}});
    $self->{s}->{modified_lst} = []; 

    foreach (@{$self->{s}->{modified}}) {
        my $fn = "$self->{razorhome}/server.$_.conf";
        my $header = "#\n# Autogenerated by $self->{name_version}, ". localtime() ."\n";
        $self->write_file($fn, $self->{s}->{allconfs}->{$_}, 0, $header) 
            || $self->debug("writeservers skipping $fn");
    }
    $self->{s}->{modified} = []; 
    $self->errstrrst;  # nuke error string if write errors
    return $self;
}


1;
