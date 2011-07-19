use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib 't/lib';

use File::Temp ();
use DBICTest;
use DBICTest::Schema;

plan tests => 2;
my $wait_for = 30;  # how many seconds to wait

for my $close (0,1) {

  my $tmp = File::Temp->new(
    UNLINK => 1,
    TMPDIR => 1,
    SUFFIX => '.sqlite',
    EXLOCK => 0,  # important for BSD and derivatives
  );

  my $tmp_fn = $tmp->filename;
  close $tmp if $close;

  local $SIG{ALRM} = sub { die sprintf (
    "Timeout of %d seconds reached (tempfile still open: %s)",
    $wait_for, $close ? 'No' : 'Yes'
  )};

  alarm $wait_for;

  lives_ok (sub {
    my $schema = DBICTest::Schema->connect ("DBI:SQLite:$tmp_fn");
    DBICTest->deploy_schema ($schema);
    #DBICTest->populate_schema ($schema);
  });

  alarm 0;
}
