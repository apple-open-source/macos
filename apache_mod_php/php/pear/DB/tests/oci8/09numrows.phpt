--TEST--
DB_oci8::numRows test
--SKIPIF--
<?php require "skipif.inc"; ?>
--FILE--
<?php
require "connect.inc";
require "mktable.inc";
$dbh->autoCommit(false);
$dbh->setOption("optimize", "portability");
$test_error_mode = PEAR_ERROR_PRINT;
include "../numrows.inc";
$dbh->rollback();
$dbh->setOption("optimize", "performance");
include "../numrows.inc";
$dbh->rollback();
?>
--EXPECT--
1
2
3
4
5
6
2
0
DB Error: DB backend not capable
DB Error: DB backend not capable
DB Error: DB backend not capable
DB Error: DB backend not capable
DB Error: DB backend not capable
DB Error: DB backend not capable
DB Error: DB backend not capable
DB Error: DB backend not capable
DB Error: DB backend not capable
DB Error: DB backend not capable
