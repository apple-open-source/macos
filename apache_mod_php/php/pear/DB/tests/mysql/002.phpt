--TEST--
MySQL DB fetch modes
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require_once "DB.php";
include("mktable.inc");
include("../fetchmodes.inc");
?>
--EXPECT--
0 1 2 3
0 1 2 3
a b c d
a b c d
0 1 2 3
