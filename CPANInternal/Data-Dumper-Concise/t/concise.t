use strict;
use warnings;
use Data::Dumper ();
use Data::Dumper::Concise;
use Test::More qw(no_plan);

my $dd = Data::Dumper->new([])
                     ->Terse(1)
                     ->Indent(1)
                     ->Useqq(1)
                     ->Deparse(1)
                     ->Quotekeys(0)
                     ->Sortkeys(1);

my $dd_c = Dumper;

foreach my $to_dump (
  [ { foo => "bar\nbaz", quux => sub { "fleem" }  } ],
  [ 'one', 'two' ]
) {

  $dd_c->Values([ @$to_dump ]);
  $dd->Values([ @$to_dump ]);
  
  my $example = do {
    local $Data::Dumper::Terse = 1;
    local $Data::Dumper::Indent = 1;
    local $Data::Dumper::Useqq = 1;
    local $Data::Dumper::Deparse = 1;
    local $Data::Dumper::Quotekeys = 0;
    local $Data::Dumper::Sortkeys = 1;
    Data::Dumper::Dumper(@$to_dump);
  };
  
  is($example, $dd->Dump, 'Both Data::Dumper usages equivalent');
  
  is($example, $dd_c->Dump, 'Returned object usage equivalent');
  
  is($example, Dumper(@$to_dump), 'Subroutine call usage equivalent');
}
