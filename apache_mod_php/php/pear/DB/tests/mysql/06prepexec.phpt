--TEST--
DB_mysql::prepare/execute test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require_once "DB.php";
include("mktable.inc");
include("../prepexe.inc");
?>
--EXPECT--
sth1,sth2,sth3 created
sth1 executed
sth2 executed
sth3 executed
results:
72 -  -  - 
72 - bing -  - 
72 - gazonk - opaque
placeholder
test - 
