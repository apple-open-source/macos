#!/usr/bin/perl
use Test::More skip_all => 'not fixed yet';

exit;

use Data::Dumper;
our %stack = ();


use SOAP::Lite +trace => 'all';
use Storable qw(nstore retrieve);

use B      'svref_2object';
use Symbol 'qualify_to_ref';

sub change_depth_warn {
  my($subname, $limit) = @_;
  my $subref = \&$subname;
  my $gv     = svref_2object($subref)->GV;
  my $lineno = 0;

  no warnings 'redefine';
  *{ qualify_to_ref $subname } = sub {
     if( $gv->CV->DEPTH % $limit == 0 ) {
     $lineno = do {
       my $i = 0;
       1 while caller $i++;
     (caller($i - 2))[2]
   } unless $lineno;
   die sprintf "Deep recursion on subroutine '%s' at %s line %d.\n",  join('::', $gv->STASH->NAME, $gv->NAME), $0, $lineno;
  }
  &$subref(@_);
  };
 }

my $obj = retrieve("$0.stored");
print Dumper $obj;

change_depth_warn("SOAP::Serializer::encode_object", 100);

my $ser = SOAP::Serializer->new( readable => 1);
$ser->autotype(0);
print $ser->freeform($obj);

ok(1);
done_testing();
