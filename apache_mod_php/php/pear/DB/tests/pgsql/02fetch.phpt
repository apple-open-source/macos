--TEST--
DB_pgsql fetch test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require_once "DB.php";
include("mktable.inc");
include("../fetchrow.inc");
include("../fetchmodes.inc");
?>
--EXPECT--
testing fetchrow:
row 1: 42, bing, This is a test, 11-21-1999
row 2: 1, one, One, 02-16-2001
row 3: 2, two, Two, 02-15-2001
row 4: 3, three, Three, 02-14-2001
row 5: NULL
testing fetchmodes: fetchrow default default
0 1 2 3
testing fetchmodes: fetchinto default default
0 1 2 3
testing fetchmodes: fetchrow ordered default
0 1 2 3
testing fetchmodes: fetchrow assoc default
a b c d
testing fetchmodes: fetchrow ordered default with assoc specified
a b c d
testing fetchmodes: fetchrow assoc default with ordered specified
0 1 2 3
testing fetchmodes: fetchinto ordered default
0 1 2 3
testing fetchmodes: fetchinto assoc default
a b c d
testing fetchmodes: fetchinto ordered default with assoc specified
a b c d
testing fetchmodes: fetchinto assoc default with ordered specified
0 1 2 3
