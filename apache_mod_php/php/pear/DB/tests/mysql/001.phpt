--TEST--
DB_mysql::connect test
--SKIPIF--
<?php require "skipif.inc"; ?>
--FILE--
<?php
require "connect.inc";
if (is_object($dbh)) {
    print "\$dbh is an object\n";
}
if (is_resource($dbh->connection)) {
    print "\$dbh is connected\n";
}
?>
--EXPECT--
$dbh is an object
$dbh is connected
