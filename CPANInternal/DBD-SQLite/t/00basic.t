use Test;
BEGIN { plan tests => 1 }
END { ok($loaded) }
use DBD::SQLite;
$loaded++;

unlink("foo", "output/foo", "output/database", "output/datbase");

