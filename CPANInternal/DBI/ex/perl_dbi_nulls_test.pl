#! /usr/bin/perl -w

# This script checks which style of WHERE clause(s) will support both
# null and non-null values.  Refer to the NULL Values sub-section
# of the "Placeholders and Bind Values" section in the DBI
# documention for more information on this issue.  The clause styles
# and their numbering (0-6) map directly to the examples in the
# documentation.
#
# To use this script:
#
# 1) If you are not using the DBI_DSN env variable, then update the
#    connect method arguments to support your database engine and
#    database, and remove the nearby check for DBI_DSN.
# 2) Set PrintError to 1 in the connect method if you want see the
#    engine's reason WHY your engine won't support a particular
#    style.
# 3) If your database does not support NULL columns by default
#    (e.g. Sybase) find and edit the CREATE TABLE statement
#    accordingly.
# 4) To properly test style #5, you need the capability to create the
#    stored procedure SP_ISNULL that acts as a function: it tests its
#    argument and returns 1 if it is null, 0 otherwise.  For example,
#    using Informix IDS engine, a definition would look like:
#
# CREATE PROCEDURE SP_ISNULL (arg VARCHAR(32)) RETURNING INTEGER;
#     IF arg IS NULL THEN RETURN 1; 
#     ELSE                RETURN 0;
#     END IF;
# END PROCEDURE;
#
# Warning: This script will attempt to create a table named by the
# $tablename variable (default dbi__null_test_tmp) and WILL DESTROY
# any pre-existing table so named.

use strict;
use DBI;

# The array represents the values that will be stored in the char column of our table.
# One array element per row.
# We expect the non-null test to return row 3 (Marge)
# and the null test to return rows 2 and 4 (the undefs).
		
my $homer = "Homer";
my $marge = "Marge";

my @char_column_values = (
  $homer,   # 1
  undef,    # 2
  $marge,   # 3
  undef,    # 4
);

# Define the SQL statements with the various WHERE clause styles we want to test
# and the parameters we'll substitute.

my @select_clauses =
(
  {clause=>qq{WHERE mycol = ?},                                         nonnull=>[$marge], null=>[undef]},
  {clause=>qq{WHERE NVL(mycol, '-') = NVL(?, '-')},                     nonnull=>[$marge], null=>[undef]},
  {clause=>qq{WHERE ISNULL(mycol, '-') = ISNULL(?, '-')},               nonnull=>[$marge], null=>[undef]},
  {clause=>qq{WHERE DECODE(mycol, ?, 1, 0) = 1},                        nonnull=>[$marge], null=>[undef]},
  {clause=>qq{WHERE mycol = ? OR (mycol IS NULL AND ? IS NULL)},        nonnull=>[$marge,$marge], null=>[undef,undef]},
  {clause=>qq{WHERE mycol = ? OR (mycol IS NULL AND SP_ISNULL(?) = 1)}, nonnull=>[$marge,$marge], null=>[undef,undef]},
  {clause=>qq{WHERE mycol = ? OR (mycol IS NULL AND ? = 1)},            nonnull=>[$marge,0],      null=>[undef,1]},
);

# This is the table we'll create and use for these tests.
# If it exists, we'll DESTROY it too.  So the name must be obscure.

my $tablename = "dbi__null_test_tmp"; 

# Remove this if you are not using the DBI_DSN env variable,
# and update the connect statement below.

die "DBI_DSN environment variable not defined"
	unless $ENV{DBI_DSN};

my $dbh = DBI->connect(undef, undef, undef,
  {
	  RaiseError => 0,
	  PrintError => 1
  }
) || die DBI->errstr;

printf "Using %s, db version: %s\n", $ENV{DBI_DSN} || "connect arguments", $dbh->get_info(18) || "(unknown)";

my $sth;
my @ok;

print "=> Drop table '$tablename', if it already exists...\n";
do { local $dbh->{PrintError}=0; $dbh->do("DROP TABLE $tablename"); };

print "=> Create table '$tablename'...\n";
$dbh->do("CREATE TABLE $tablename (myid int NOT NULL, mycol char(5))");
# Use this if your database does not support NULL columns by default:
#$dbh->do("CREATE TABLE $tablename (myid int NOT NULL, mycol char(5) NULL)");

print "=> Insert 4 rows into the table...\n";

$sth = $dbh->prepare("INSERT INTO $tablename (myid, mycol) VALUES (?,?)");
for my $i (0..$#char_column_values)
{
    my $val = $char_column_values[$i];
    printf "   Inserting values (%d, %s)\n", $i+1, $dbh->quote($val);
    $sth->execute($i+1, $val);
}
print "(Driver bug: statement handle should not be Active after an INSERT.)\n"
    if $sth->{Active};

# Run the tests...

for my $i (0..$#select_clauses)
{
    my $sel = $select_clauses[$i];
    print "\n=> Testing clause style $i: ".$sel->{clause}."...\n";
    
    $sth = $dbh->prepare("SELECT myid,mycol FROM $tablename ".$sel->{clause})
	or next;

    print "   Selecting row with $marge\n";
    $sth->execute(@{$sel->{nonnull}})
	or next;
    my $r1 = $sth->fetchall_arrayref();
    my $n1_rows = $sth->rows;
    my $n1 = @$r1;
    
    print "   Selecting rows with NULL\n";
    $sth->execute(@{$sel->{null}})
	or next;
    my $r2 = $sth->fetchall_arrayref();
    my $n2_rows = $sth->rows;
    my $n2 = @$r2;
    
    # Complain a bit...
    
    print "\n=>Your DBD driver doesn't support the 'rows' method very well.\n\n"
       unless ($n1_rows == $n1 && $n2_rows == $n2);
       
    # Did we get back the expected "n"umber of rows?
    # Did we get back the specific "r"ows we expected as identifed by the myid column?
    
    if (   $n1 == 1     # one row for Marge
        && $n2 == 2     # two rows for nulls
        && $r1->[0][0] == 3 # Marge is myid 3
        && $r2->[0][0] == 2 # NULL for myid 2
        && $r2->[1][0] == 4 # NULL for myid 4
    ) {
      print "=> WHERE clause style $i is supported.\n";
      push @ok, "\tStyle $i: ".$sel->{clause};
    }
    else
    {
      print "=> WHERE clause style $i returned incorrect results.\n";
      if ($n1 > 0 || $n2 > 0)
      {
        print "   Non-NULL test rows returned these row ids: ".
            join(", ", map { $r1->[$_][0] } (0..$#{$r1}))."\n";
        print "   The NULL test rows returned these row ids: ".
            join(", ", map { $r2->[$_][0] } (0..$#{$r2}))."\n";
      }
    }
}

$dbh->disconnect();
print "\n";
print "-" x 72, "\n";
printf "%d styles are supported:\n", scalar @ok;
print "$_\n" for @ok;
print "-" x 72, "\n";
print "\n";
print "If these results don't match what's in the 'Placeholders and Bind Values'\n";
print "section of the DBI documentation, or are for a database that not already\n";
print "listed, please email the results to dbi-users\@perl.org. Thank you.\n";

exit 0;
