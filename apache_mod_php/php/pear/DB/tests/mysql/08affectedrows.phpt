--TEST--
DB_mysql::affectedRows test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require_once "DB.php";
include("mktable.inc");
$dbh->query("INSERT INTO phptest (a,b) VALUES(1, 'test')");
$dbh->query("INSERT INTO phptest (a,b) VALUES(2, 'test')");
printf("%d after insert\n", $dbh->affectedRows());
$dbh->query("SELECT * FROM phptest");
printf("%d after select\n", $dbh->affectedRows());
$dbh->query("DELETE FROM phptest WHERE b = 'test'");
printf("%d after delete\n", $dbh->affectedRows());
$dbh->query("INSERT INTO phptest (a,b) VALUES(1, 'test')");
$dbh->query("INSERT INTO phptest (a,b) VALUES(2, 'test')");
$dbh->query("DELETE FROM phptest");
printf("%d after delete all (optimize=%s)\n", $dbh->affectedRows(),
       $dbh->getOption("optimize"));
$dbh->query("INSERT INTO phptest (a,b) VALUES(1, 'test')");
$dbh->query("INSERT INTO phptest (a,b) VALUES(2, 'test')");
$dbh->setOption("optimize", "portability");
$dbh->query("DELETE FROM phptest");
printf("%d after delete all (optimize=%s)\n", $dbh->affectedRows(),
       $dbh->getOption("optimize"));
?>
--EXPECT--
1 after insert
0 after select
2 after delete
0 after delete all (optimize=performance)
2 after delete all (optimize=portability)
