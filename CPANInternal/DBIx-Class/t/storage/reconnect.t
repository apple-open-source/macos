use strict;
use warnings;  

use FindBin;
use File::Copy;
use Test::More;
use lib qw(t/lib);
use DBICTest;

plan tests => 6;

my $db_orig = "$FindBin::Bin/../var/DBIxClass.db";
my $db_tmp  = "$db_orig.tmp";

# Set up the "usual" sqlite for DBICTest
my $schema = DBICTest->init_schema( sqlite_use_file => 1 );

# Make sure we're connected by doing something
my @art = $schema->resultset("Artist")->search({ }, { order_by => 'name DESC'});
cmp_ok(@art, '==', 3, "Three artists returned");

# Disconnect the dbh, and be sneaky about it
# Also test if DBD::SQLite finaly knows how to ->disconnect properly
{
  my $w;
  local $SIG{__WARN__} = sub { $w = shift };
  $schema->storage->_dbh->disconnect;
  ok ($w !~ /active statement handles/, 'SQLite can disconnect properly');
}

# Try the operation again - What should happen here is:
#   1. S::DBI blindly attempts the SELECT, which throws an exception
#   2. It catches the exception, checks ->{Active}/->ping, sees the disconnected state...
#   3. Reconnects, and retries the operation
#   4. Success!
my @art_two = $schema->resultset("Artist")->search({ }, { order_by => 'name DESC'});
cmp_ok(@art_two, '==', 3, "Three artists returned");

### Now, disconnect the dbh, and move the db file;
# create a new one and chmod 000 to prevent SQLite from connecting.
$schema->storage->_dbh->disconnect;
move( $db_orig, $db_tmp );
open DBFILE, '>', $db_orig;
print DBFILE 'THIS IS NOT A REAL DATABASE';
close DBFILE;
chmod 0000, $db_orig;

### Try the operation again... it should fail, since there's no db
{
    # Catch the DBI connection error
    local $SIG{__WARN__} = sub {};
    eval {
        my @art_three = $schema->resultset("Artist")->search( {}, { order_by => 'name DESC' } );
    };
    ok( $@, 'The operation failed' );
}

### Now, move the db file back to the correct name
unlink($db_orig);
move( $db_tmp, $db_orig );

SKIP: {
    skip "Cannot reconnect if original connection didn't fail", 2
        if ( $@ =~ /encrypted or is not a database/ );

    ### Try the operation again... this time, it should succeed
    my @art_four;
    eval {
        @art_four = $schema->resultset("Artist")->search( {}, { order_by => 'name DESC' } );
    };
    ok( !$@, 'The operation succeeded' );
    cmp_ok( @art_four, '==', 3, "Three artists returned" );
}
