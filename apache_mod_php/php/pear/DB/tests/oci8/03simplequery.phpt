--TEST--
DB_oci8::simpleQuery test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require_once "DB.php";
include("mktable.inc");
include("../simplequery.inc");
?>
--EXPECT--
resource
