--TEST--
DB_oci8::errorNative test
--SKIPIF--
<?php /*include("skipif.inc");*/ print "skip\n"; ?>
--FILE--
<?php
require_once "DB.php";
include("mktable.inc");
$err = $dbh->query("SELECT foo FROM tablethatdoesnotexist");
print $dbh->errorNative() . "\n";
?>
--EXPECT--
