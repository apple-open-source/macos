BEGIN {

  foreach (qw(my.cfg test.cfg)) {
    -f and require "$_" and last;
  }

  undef $SERVER_EXE unless $SERVER_EXE and -x $SERVER_EXE;

  # If your host cannot be contacted as localhost, change this
  $HOST     ||= '127.0.0.1';

  # Where to put temporary files while testing
  # the Makefile is setup to delete temp/ when make clean is run
  $TEMPDIR  = "./temp";
  $SCHEMA_DIR ||= "./data";
  $SLAPD_DB ||= 'ldbm';

  $TESTDB   = "$TEMPDIR/test-db";
  $CONF     = "$TEMPDIR/conf";
  $PASSWD   = 'secret';
  $BASEDN   = "o=University of Michigan, c=US";
  $MANAGERDN= "cn=Manager, o=University of Michigan, c=US";
  $JAJDN    = "cn=James A Jones 1, ou=Alumni Association, ou=People, o=University of Michigan, c=US";
  $BABSDN   = "cn=Barbara Jensen, ou=Information Technology Division, ou=People, o=University of Michigan, c=US";
  $PORT     = 9009;
  @URL      = ();

  my @server_opts;
  ($SERVER_TYPE,@server_opts) = split(/\+/, $SERVER_TYPE || 'none');

  if ($SERVER_TYPE eq 'openldap1') {
    $CONF_IN	  = "./data/slapd-conf.in";
    @LDAPD	  = ($SERVER_EXE, '-f',$CONF,'-p',$PORT,qw(-d 1));
    $LDAP_VERSION = 2;
  }
  elsif ($SERVER_TYPE eq 'openldap2') {
    $SSL_PORT = 9010 if grep { $_ eq 'ssl' } @server_opts
      and eval { require IO::Socket::SSL; 1};
    ($IPC_SOCK = "$TEMPDIR/ldapi_sock") =~ s,/,%2f,g if grep { $_ eq 'ipc' } @server_opts;
    $SASL = 1 if grep { $_ eq 'sasl' } @server_opts
      and eval { require Authen::SASL; 1 };
    $CONF_IN	  = "./data/slapd2-conf.in";
    push @URL, "ldap://${HOST}:$PORT/";
    push @URL, "ldaps://${HOST}:$SSL_PORT/" if $SSL_PORT;
    push @URL, "ldapi://$IPC_SOCK/" if $IPC_SOCK;
    @LDAPD	  = ($SERVER_EXE, '-f',$CONF,'-h', "@URL",qw(-d 1));
    $LDAP_VERSION = 3;
  }

  $LDAP_VERSION ||= 2;
  mkdir($TEMPDIR,0777);
  die "$TEMPDIR is not a directory" unless -d $TEMPDIR;
}

use Net::LDAP;
use Net::LDAP::LDIF;
use Net::LDAP::Util qw(canonical_dn);
use File::Path qw(rmtree);
use File::Basename qw(basename);

my $pid;

sub start_server {
  my %arg = (version => 2, @_);

  unless ($LDAP_VERSION >= $arg{version}
	and $LDAPD[0] and -x $LDAPD[0]
	and (!$arg{ssl} or $SSL_PORT)
	and (!$arg{ipc} or $IPC_SOCK))
  {
    print "1..0 # Skip No server\n";
    exit;
  }

  if ($CONF_IN and -f $CONF_IN) {
    # Create slapd config file
    open(CONFI,"<$CONF_IN") or die "$!";
    open(CONFO,">$CONF") or die "$!";
    while(<CONFI>) {
      s/\$([A-Z]\w*)/${$1}/g;
      s/^TLS/#TLS/ unless $SSL_PORT;
      s/^(sasl.*)/#$1/ unless $SASL;
      print CONFO;
    }
    close(CONFI);
    close(CONFO);
  }

  rmtree($TESTDB) if ( -d $TESTDB );
  mkdir($TESTDB,0777);
  die "$TESTDB is not a directory" unless -d $TESTDB;

  warn "@LDAPD" if $ENV{TEST_VERBOSE};

  my $log = $TEMPDIR . "/" . basename($0,'.t');

  unless ($pid = fork) {
    die "fork: $!" unless defined $pid;

    open(STDERR,">$log");
    open(STDOUT,">&STDERR");
    close(STDIN);

    exec(@LDAPD) or die "cannot exec @LDAPD";
  }

  sleep 2; # wait for server to start
}

sub kill_server {
  if ($pid) {
    kill 9, $pid;
    sleep 2;
    undef $pid;
  }
}

END {
  kill_server();
}

sub client {
  my %arg = @_;
  my $ldap;
  my $count;
  local $^W = 0;
  if ($arg{ssl}) {
    require Net::LDAPS;
    until($ldap = Net::LDAPS->new($HOST, port => $SSL_PORT, version => 3)) {
      die "ldaps://$HOST:$SSL_PORT/ $@" if ++$count > 10;
      sleep 1;
    }
  }
  elsif ($arg{ipc}) {
    require Net::LDAPI;
    until($ldap = Net::LDAPI->new($IPC_SOCK)) {
      die "ldapi://$IPC_SOCK/ $@" if ++$count > 10;
      sleep 1;
    }
  }
  elsif ($arg{url}) {
    print "Trying $arg{url}\n";
    until($ldap = Net::LDAP->new($arg{url})) {
      die "$arg{url} $@" if ++$count > 10;
      sleep 1;
    }
  }
  else {
    until($ldap = Net::LDAP->new($HOST, port => $PORT, version => $LDAP_VERSION)) {
      die "ldap://$HOST:$PORT/ $@" if ++$count > 10;
      sleep 1;
    }
  }
  $ldap;
}

sub compare_ldif {
  my($test,$mesg) = splice(@_,0,2);

  unless (ok(!$mesg->code, $mesg->error)) {
    skip(2, $mesg->error);
    return;
  }

  my $ldif = Net::LDAP::LDIF->new("$TEMPDIR/${test}-out.ldif","w", lowercase => 1);
  unless (ok($ldif, "Read ${test}-out.ldif")) {
    skip(1,"Read error");
    return;
  }

  my @canon_opt = (casefold => 'lower', separator => ', ');
  foreach $entry (@_) {
    $entry->dn(canonical_dn($entry->dn, @canon_opt));
    foreach $attr ($entry->attributes) {
      $entry->delete($attr) if $attr =~ /^(modifiersname|modifytimestamp|creatorsname|createtimestamp)$/i;
      if ($attr =~ /^(seealso|member|owner)$/i) {
	$entry->replace($attr => [ map { canonical_dn($_, @canon_opt) } $entry->get_value($attr) ]);
      }
    }
    $ldif->write($entry);
  }

  $ldif->done; # close the file;

  ok(!compare("$TEMPDIR/${test}-out.ldif","data/${test}-cmp.ldif"), "data/${test}-cmp.ldif");
}

require File::Compare;

sub compare($$) {
  local(*FH1,*FH2);
  not( open(FH1,"<".$_[0])
       && open(FH2,"<".$_[1])
       && 0 == File::Compare::compare(*FH1,*FH2, -s FH1)
  );
}

sub ldif_populate {
  my ($ldap, $file, $change) = @_;
  my $ok = 1;

  my $ldif = Net::LDAP::LDIF->new($file,"r", changetype => $change || 'add')
	or return;

  while (my $e = $ldif->read_entry) {
    $mesg = $e->update($ldap);
    if ($mesg->code) {
      $ok = 0;
      Net::LDAP::LDIF->new(qw(- w))->write_entry($e);
      print "# ",$mesg->code,": ",$mesg->error,"\n";
    }
  }
  $ok;
}

my $number = 0;
sub ok {
	my ($condition, $name) = @_;

	my $message = $condition ? "ok " : "not ok ";
	$message .= ++$number;
	$message .= " # $name" if defined $name;
	print $message, "\n";
	return $condition;
}

sub is {
	my ($got, $expected, $name) = @_;

	for ($got, $expected) {
		$_ = 'undef' unless defined $_;
	}

	unless (ok($got eq $expected, $name)) {
		warn "Got: '$got'\nExpected: '$expected'\n" . join(' ', caller) . "\n";
	}
}

sub skip {
	my ($reason, $num) = @_;
	$reason ||= '';
	$number ||= 1;

	for (1 .. $num) {
		$number++;
		print "ok $number # skip $reason\n";
	}
}

1;
