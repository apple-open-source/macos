--TEST--
DB_mysql::errorNative test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require_once "DB.php";
include("mktable.inc");
$dbh->query("syntax error please");
print "error code " . $dbh->errorNative() . "\n";
?>
--EXPECT--
error code 1064
