<?php
// This script prints "skip" unless:
// * the mysql extension is built-in or loadable, AND
// * there is a database called "test" accessible
//   with no username/password, AND
// * we have create/drop privileges on the entire "test"
//   database
if (!extension_loaded("mysql")) {
  $dlext = (substr(PHP_OS, 0, 3) == "WIN") ? ".dll" : ".so";
  @dl("mysql$dlext");
}
if (!extension_loaded("mysql")) {
    die("skip\n");
}
$conn = @mysql_connect("localhost");
if (!is_resource($conn)) {
    die("skip\n");
}
if (!mysql_select_db("test", $conn)) {
    die("skip\n");
}
?>
