<?php
require_once "DB.php";
$dbh = DB::connect("mysql://localhost/test");
if (DB::isError($dbh)) {
    die("connect.inc: ".$dbh->getMessage());
}
?>
