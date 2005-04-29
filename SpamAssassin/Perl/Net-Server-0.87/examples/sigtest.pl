#!/usr/bin/perl -w

use IO::Select ();
use IO::Socket ();
use Net::Server::SIG qw(register_sig check_sigs);
use POSIX ();

print "Usage: $0 SIGNAME SAFE|UNSAFE
  (SIGNAME is a standard signal - default is USR1)
  (SAFE will use Net::Server::SIG, UNSAFE uses \$SIG{} - default is SAFE)
  If the child isn't saying anything, the test is invalid.
  If the child dies, look for a core file.
";

my $SIG = shift() || 'USR1';
my $safe = shift() || 'SAFE';
$safe = uc($safe) eq 'UNSAFE' ? undef : 1;
my $x = 0;
my %hash = ();

### set up a pipe
pipe(READ,WRITE);
READ->autoflush(1);
WRITE->autoflush(1);
STDOUT->autoflush(1);

my $pid = fork();
die "Couldn't fork [$!]" unless defined $pid;

### see if child left
$SIG{CHLD} = sub {
  print "P ($$): Child died (\$?=$?)\n"
    while (waitpid(-1, POSIX::WNOHANG()) > 0);
};

### let the parent try to kill the child
if( $pid ){

  sleep(2);

  ### for off children to help bombard the child
  for(1..4){
    my $pid2 = fork();
    unless( defined $pid2 ){
      kill 9, $pid;
      die "Couldn't fork [$!]";
    }
    unless( $pid2 ){
      $SIG{CHLD} = 'DEFAULT';
      last;
    }
  }    

  print "P ($$): Starting up!\n";

  ### kill the child with that signal
  my $n = 50000;
  while (1){
    last unless kill $SIG, $pid;
    unless( ++$x % $n ){
      print "P ($$): $x SIG_$SIG\'s sent.\n";
      print WRITE "$n\n";
    }
  }


### let the child try to stay alive
}else{

  print "C ($$): Starting up!\n";

  my $select = IO::Select->new();
  $select->add(\*READ);

  ### do some variable manipulation in the signal handler
  my $subroutine = sub {
    $hash{foo} = "abcde"x10000;
    $hash{bar} ++;
    delete $hash{baz};
    delete $hash{bar};
  };

  ### register a signal and see if it will bounce off of the can_read
  if( $safe ){
    print "C ($$): Using SAFE signal handler.\n";
    register_sig($SIG => $subroutine);

  ### This is an unsafe signal handler. See how long
  ### it can take signals.
  }else{
    print "C ($$): Using UNSAFE signal handler.\n";
    $SIG{$SIG} = $subroutine;

  }

  my $total = 0;

  ### loop forever trying to stay alive
  while ( 1 ){

    my @fh = $select->can_read(10);

    my $key;
    my $val;

    ### this is the handler for safe (fine under unsafe also)
    next if &check_sigs() && ! @fh;

    ### do some hash manipulation
    delete $hash{foo};
    $hash{bar} = 0;
    $hash{baz} = "abcde"x100000;


    next unless @fh;
    my $line = <READ>;
    chomp($line);
    $total += $line;
    print "C ($$): P said \"$line\"\n";
    
    unless( ++$x % 5 ){
      print "C ($$): $x lines read. $total SIG's received\n";
    }
  }

  print "Child is done\n";
}    

